"""
构建查询自动补全词典 (completion suggester)
===========================================
词典来源 (两路, 加权合并):
    1. 语料抽词: 对 pages.jsonl 的标题做 jieba 分词
       - 保留长度 >= 2 的词 (一元词)
       - 相邻 token 合成 bigram, 补充 jieba 切碎的复合词 (如 副/教授 -> 副教授)
    2. 热门查询: 从 nku_querylog 统计用户真实搜过的词, 权重放大 SUGGEST_QUERY_BOOST 倍

按总频次取 top-N (SUGGEST_MAX_TERMS) 写入 nku_suggest 的 completion 字段。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config
from indexer.es_indexer import get_es, ensure_index
from indexer.mapping import SUGGEST_MAPPING

from elasticsearch import helpers

# 一个 token 是否值得进补全词典
_CJK = re.compile(r"[\u4e00-\u9fff]")
# 含任何非(中文/英文/数字)字符即拒绝
_BAD_CHAR = re.compile(r"[^0-9A-Za-z\u4e00-\u9fff]")


def _valid_term(t: str) -> bool:
    t = t.strip()
    if not (config.SUGGEST_MIN_LEN <= len(t) <= config.SUGGEST_MAX_LEN):
        return False
    if t in config.SUGGEST_STOPWORDS:
        return False
    if _BAD_CHAR.search(t):                  # 含标点/括号/空白
        return False
    # 必须含中文, 或是长度>=3 的英文词 (如 python / nankai)
    if _CJK.search(t):
        return True
    if t.isascii() and t.isalpha() and len(t) >= 3:
        return True
    return False


def _collect_corpus_terms(pages_path: str) -> Counter:
    import jieba
    for w in config.ACADEMIC_DICT:
        jieba.add_word(w)               # 保证复合词不被切碎

    counter: Counter = Counter()
    n = 0
    with open(pages_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except Exception:
                continue
            title = (rec.get("title") or "").strip()
            if not title:
                continue
            tokens = [t.strip() for t in jieba.cut(title) if t.strip()]
            # 一元词
            for tk in tokens:
                if _valid_term(tk):
                    counter[tk] += 1
            # 相邻 bigram (补复合词)
            for a, b in zip(tokens, tokens[1:]):
                ph = a + b
                if _valid_term(ph):
                    counter[ph] += config.SUGGEST_BIGRAM_WEIGHT
            # 相邻 trigram (让联想更像完整搜索短语)
            for a, b, c in zip(tokens, tokens[1:], tokens[2:]):
                ph = a + b + c
                if _valid_term(ph):
                    counter[ph] += config.SUGGEST_TRIGRAM_WEIGHT
            n += 1
            if n % 5000 == 0:
                print(f"[suggest] scanned {n} titles, vocab={len(counter)}")
    print(f"[suggest] corpus done: {n} titles, vocab={len(counter)}")
    return counter


def _collect_query_terms(es) -> Counter:
    """统计查询日志里的热门查询 (整条 query 作为一个补全条目)。"""
    counter: Counter = Counter()
    try:
        for doc in helpers.scan(es, index=config.INDEX_LOGS,
                                query={"query": {"match_all": {}}},
                                _source=["query"]):
            q = (doc["_source"].get("query") or "").strip()
            if q and config.SUGGEST_MIN_LEN <= len(q) <= 40:
                counter[q] += 1
    except Exception as e:
        print(f"[suggest] no query log yet ({e})")
    # 放大权重
    for k in list(counter):
        counter[k] *= config.SUGGEST_QUERY_BOOST
    print(f"[suggest] hot queries: {len(counter)}")
    return counter


def _gen_actions(merged: list[tuple[str, int]]):
    for term, weight in merged:
        # completion weight 必须是正整数, 上限保护
        w = max(1, min(int(weight), 1_000_000))
        yield {
            "_index": config.INDEX_SUGGEST,
            "_id": term,
            "_source": {
                "text": term,
                "source": "merged",
                # 前缀补全: 一个词条一个输入即可
                "suggest": {"input": [term], "weight": w},
            },
        }


def build(recreate: bool = True):
    es = get_es()
    pages_path = os.path.join(config.DATA_DIR, "pages.jsonl")
    if not os.path.exists(pages_path):
        print(f"[suggest] {pages_path} 不存在, 请先跑爬虫。")
        return

    corpus = _collect_corpus_terms(pages_path)
    queries = _collect_query_terms(es)

    # 合并: 查询权重已放大, 直接相加
    total = corpus.copy()
    for k, v in queries.items():
        total[k] += v

    top = total.most_common(config.SUGGEST_MAX_TERMS)
    print(f"[suggest] 收录 {len(top)} 个词条 (示例: "
          f"{[t for t, _ in top[:10]]})")

    ensure_index(es, config.INDEX_SUGGEST, SUGGEST_MAPPING, recreate=recreate)

    ok, fail = 0, 0
    for success, _ in helpers.streaming_bulk(es, _gen_actions(top),
                                             chunk_size=1000,
                                             raise_on_error=False):
        ok += 1 if success else 0
        fail += 0 if success else 1
    print(f"[suggest] DONE indexed ok={ok} fail={fail}")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--recreate", action="store_true", default=True)
    args = p.parse_args()
    build(recreate=args.recreate)
