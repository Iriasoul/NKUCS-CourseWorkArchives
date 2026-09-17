"""
个性化推荐
============================================
A. 搜索联想  suggest(prefix, user_id)   用户输入前缀, 前缀补全
     - 登录用户: 个人历史 (前缀过滤+去重+按最近) 置顶, 再用全局热词补满到 8 条
     - 匿名用户: 只用全局热词 (按词频)
B. 内容推荐  recommend_for_user(uid)    用户没输入时, 直接基于历史推荐
"""
from __future__ import annotations

import os
import sys
from typing import Any

from elasticsearch import Elasticsearch

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config
from indexer.es_indexer import get_es
from search.search_service import get_user_logs, build_user_profile


# A. 搜索联想 (用户输入了前缀)
def suggest(prefix: str, size: int = 8, user_id: str | None = None,
            es: Elasticsearch | None = None) -> list[str]:
    """搜索联想 (前缀补全), 最多 size 条 (默认 8)。

    排序两档:
      第一档 个人历史 (仅登录用户): 该用户搜过的、以 prefix 开头的查询,
            去重 (同词只留最近一次), 按最近时间排, 置顶。
      第二档 全局热词: 用 nku_suggest 词典 (completion suggester, 按词频排),
            前缀过滤, 把列表补满到 size 条; 跨档去重 (已在个人历史的词跳过)。

    匿名用户 (user_id=None) 只有第二档。
    若全局词典还没建, 退化为对标题做 match_phrase_prefix。
    """
    es = es or get_es()
    prefix = prefix.strip()
    if not prefix:
        return []

    results: list[str] = []
    seen: set[str] = set()

    # 第一档: 个人历史 (前缀过滤 + 去重 + 按最近)
    if user_id:
        for q in _personal_history(user_id, prefix, size=size, es=es):
            if q not in seen:
                seen.add(q)
                results.append(q)
            if len(results) >= size:
                return results

    # 第二档: 全局热词 (前缀过滤, 按词频), 补满到 size
    need = size - len(results)
    if need > 0:
        for w in _global_hot(prefix, size=size, es=es):
            if w not in seen:                 # 跨档去重
                seen.add(w)
                results.append(w)
            if len(results) >= size:
                break

    return results


def _personal_history(user_id: str, prefix: str, size: int = 8,
                      es: Elasticsearch | None = None) -> list[str]:
    """该用户、以 prefix 开头的历史查询, 去重 (同词留最近), 按最近时间排。"""
    es = es or get_es()
    try:
        resp = es.search(
            index=config.INDEX_LOGS,
            query={
                "bool": {
                    "filter": [
                        {"term": {"user_id": user_id}},
                        {"prefix": {"query.raw": prefix}},
                    ]
                }
            },
            sort=[{"ts": {"order": "desc"}}],
            _source=["query", "ts"],
            size=200,   # 单用户历史量小, 取够了在 Python 里去重
        )
        out, seen = [], set()
        for h in resp.body.get("hits", {}).get("hits", []):
            q = (h["_source"].get("query") or "").strip()
            if q and q not in seen:           # 已按时间倒序, 首次出现即最近一次
                seen.add(q)
                out.append(q)
            if len(out) >= size:
                break
        return out
    except Exception as e:
        print(f"[suggest] personal history failed: {e}")
        return []


def _global_hot(prefix: str, size: int = 8,
                es: Elasticsearch | None = None) -> list[str]:
    """全局热词: nku_suggest 词典前缀补全 (按词频), 失败则退化标题匹配。"""
    es = es or get_es()
    try:
        resp = es.search(
            index=config.INDEX_SUGGEST,
            suggest={
                "sug": {
                    "prefix": prefix,
                    "completion": {
                        "field": "suggest",
                        "size": size,
                        "skip_duplicates": True,
                    },
                }
            },
            _source=False,
        )
        options = resp.body.get("suggest", {}).get("sug", [{}])[0].get("options", [])
        results = [o["text"] for o in options]
        if results:
            return results
    except Exception:
        pass
    # 兜底: 标题前缀匹配
    return _suggest_by_title(prefix, size=size, es=es)


def _suggest_by_title(prefix: str, size: int = 8,
                      es: Elasticsearch | None = None) -> list[str]:
    es = es or get_es()
    try:
        resp = es.search(
            index=config.INDEX_PAGES,
            query={
                "bool": {
                    "should": [
                        {"match_phrase_prefix": {"title": {"query": prefix, "boost": 3}}},
                        {"wildcard": {"title.raw": {"value": f"*{prefix}*"}}},
                    ],
                    "minimum_should_match": 1,
                }
            },
            _source=["title"],
            size=size * 3,
        )
        titles, seen = [], set()
        for h in resp.body.get("hits", {}).get("hits", []):
            t = (h["_source"].get("title") or "").strip()
            if t and t not in seen:
                seen.add(t)
                titles.append(t)
            if len(titles) >= size:
                break
        return titles
    except Exception as e:
        print(f"[suggest] fallback failed: {e}")
        return []


# B. 用户没输入时的内容推荐
def recommend_for_user(user_id: str | None, size: int = 8,
                       es: Elasticsearch | None = None) -> list[dict]:
    """基于用户最近 N 条查询, 用 more_like_this 召回相似页面.
    匿名用户则返回 pagerank 最高的几个站内热门页"""
    es = es or get_es()

    # 匿名 : 返回 pagerank top
    if not user_id:
        resp = es.search(
            index=config.INDEX_PAGES,
            query={"match_all": {}},
            sort=[{"pagerank": {"order": "desc"}}],
            _source=["title", "url", "site"],
            size=size,
        )
        return [{"title": h["_source"].get("title"),
                 "url":   h["_source"].get("url"),
                 "site":  h["_source"].get("site"),
                 "reason": "热门"}
                for h in resp.body.get("hits", {}).get("hits", [])]

    # 登录用户 : 用历史查询做 more_like_this
    logs = get_user_logs(user_id, size=10, es=es)
    if not logs:
        return recommend_for_user(None, size=size, es=es)

    like_text = " ".join((l.get("query") or "") for l in logs)
    try:
        resp = es.search(
            index=config.INDEX_PAGES,
            query={
                "more_like_this": {
                    "fields": ["title", "body", "anchor_text"],
                    "like": like_text,
                    "min_term_freq": 1,
                    "min_doc_freq": 1,
                    "max_query_terms": 12,
                }
            },
            _source=["title", "url", "site"],
            size=size,
        )
        hits = resp.body.get("hits", {}).get("hits", [])
        if hits:
            return [{"title": h["_source"].get("title"),
                     "url":   h["_source"].get("url"),
                     "site":  h["_source"].get("site"),
                     "reason": "根据你的搜索历史"}
                    for h in hits]
    except Exception as e:
        print(f"[recommend] more_like_this failed: {e}")

    # 失败兜底
    return recommend_for_user(None, size=size, es=es)
