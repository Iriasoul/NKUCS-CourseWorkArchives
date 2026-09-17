"""
查询服务
==================
六大查询功能:
    1. 站内查询    site_search(q)
    2. 文档查询    doc_search(q)               搜文档 (pdf/doc/xls)
    3. 短语查询    phrase_search(q)            连续匹配
    4. 通配查询    wildcard_search(q)          支持 * 与 ?
    5. 查询日志    log_query / get_user_logs
    6. 网页快照    见 webapp/app.py

排序 (两阶段 retrieve-then-rerank):
    阶段一: ES 用 BM25 (向量空间模型) 召回候选池 (CANDIDATE_POOL 条)
    阶段二: search/ranking.py 按混合公式重排:
        final = 0.4*个性化 + 0.3*时效 + 0.2*PageRank + 0.1*BM25  (登录用户)
    匿名用户用另一套权重 (个性化=0, 其余重分配)。
"""
from __future__ import annotations

import time
from typing import Any, Optional

from elasticsearch import Elasticsearch

import config
from indexer.es_indexer import get_es
from search.ranking import rerank


# 工具
def _has_wildcard(q: str) -> bool:
    return any(c in q for c in "*?")


def _highlight() -> dict:
    """ES 高亮配置: 让前端显示飘红 snippet."""
    return {
        "pre_tags": ["<em>"], "post_tags": ["</em>"],
        "fields": {
            "title":       {"number_of_fragments": 0},
            "body":        {"fragment_size": 150, "number_of_fragments": 2},
            "anchor_text": {"number_of_fragments": 0},
            "content":     {"fragment_size": 150, "number_of_fragments": 2},
        }
    }


def _format_hits(resp: dict) -> list[dict]:
    out = []
    for h in resp.get("hits", {}).get("hits", []):
        src = h["_source"]
        item = {
            "doc_id":   src.get("doc_id"),
            "url":      src.get("url"),
            "title":    src.get("title") or src.get("url"),
            "site":     src.get("site", ""),
            "snippet":  "",
            "score":    h.get("_score"),         # BM25 原始分
            "snapshot_path": src.get("snapshot_path", ""),
            "ext":      src.get("ext", ""),
            "local_path": src.get("local_path", ""),
            "pagerank": src.get("pagerank", 0.0),
            "publish_time": src.get("publish_time", 0),
            "body":     src.get("body", "") or src.get("content", ""),
        }
        # 选高亮 (优先 body / content)
        hl = h.get("highlight", {})
        for key in ("body", "content", "anchor_text", "title"):
            if key in hl:
                item["snippet"] = " … ".join(hl[key])
                break
        if not item["snippet"]:
            body = src.get("body") or src.get("content") or ""
            item["snippet"] = body[:160]
        out.append(item)
    return out


def _run(es: Elasticsearch, query: dict, *, user_profile: dict | None,
         size: int, pool: int | None = None,
         do_rerank: bool = True, site: str | None = None) -> list[dict]:
    """执行 ES 查询 -> 取候选池 -> (可选) 混合重排 -> 返回 size 条。
    site 非空时, 把查询包一层 bool, filter 限定到该站点 (站内查询)。
    """
    pool = pool or config.CANDIDATE_POOL
    if site:
        query = {"bool": {"must": [query], "filter": [{"term": {"site": site}}]}}
    resp = es.search(index=config.INDEX_PAGES, query=query, size=pool,
                     highlight=_highlight())
    hits = _format_hits(resp.body)
    if do_rerank:
        hits = rerank(hits, user_profile=user_profile, top_k=size)
    else:
        hits = hits[:size]
    return hits


# 1. 站内查询
def site_search(q: str, *, user_profile: dict | None = None,
                site: str | None = None,
                size: int = 20, es: Elasticsearch | None = None) -> list[dict]:
    es = es or get_es()
    base = {
        "multi_match": {
            "query": q,
            "fields": ["title^4", "anchor_text^2", "body"],
            "type": "best_fields",
            "tie_breaker": 0.3,
        }
    }
    return _run(es, base, user_profile=user_profile, size=size, site=site)


# 2. 文档查询 (pdf/doc/xls 等) , 文档无 pagerank/时效, 直接按 BM25
def doc_search(q: str, *, ext: str | None = None,
               size: int = 20, es: Elasticsearch | None = None) -> list[dict]:
    es = es or get_es()
    base = {
        "multi_match": {
            "query": q,
            "fields": ["title^3", "content"],
        }
    }
    if ext:
        exts = [e.strip() for e in ext.split(",") if e.strip()]
        if exts:
            base = {"bool": {"must": [base], "filter": [{"terms": {"ext": exts}}]}}
    resp = es.search(index=config.INDEX_DOCS, query=base, size=size,
                     highlight=_highlight())
    return _format_hits(resp.body)


# 3. 短语查询
def phrase_search(q: str, *, slop: int = 0, size: int = 20,
                  user_profile: dict | None = None,
                  site: str | None = None,
                  es: Elasticsearch | None = None) -> list[dict]:
    es = es or get_es()
    base = {
        "bool": {
            "should": [
                {"match_phrase": {"title": {"query": q, "slop": slop, "boost": 4}}},
                {"match_phrase": {"anchor_text": {"query": q, "slop": slop, "boost": 2}}},
                {"match_phrase": {"body":  {"query": q, "slop": slop}}},
            ],
            "minimum_should_match": 1,
        }
    }
    return _run(es, base, user_profile=user_profile, size=size, site=site)


# 4. 通配查询
def wildcard_search(q: str, *, size: int = 20,
                    user_profile: dict | None = None,
                    site: str | None = None,
                    es: Elasticsearch | None = None) -> list[dict]:
    """支持 * 和 ? """
    es = es or get_es()
    pattern = q.strip()
    query = {
        "bool": {
            "should": [
                {"wildcard": {"title.raw": {"value": pattern, "boost": 3,
                                            "case_insensitive": True}}},
                {"wildcard": {"title":     {"value": pattern, "boost": 2,
                                            "case_insensitive": True}}},
                {"wildcard": {"anchor_text": {"value": pattern,
                                              "case_insensitive": True}}},
            ],
            "minimum_should_match": 1,
        }
    }
    return _run(es, query, user_profile=user_profile, size=size, site=site)


# 统一入口
def smart_search(q: str, *, user_profile: dict | None = None,
                 size: int = 20, site: str | None = None,
                 es: Elasticsearch | None = None) -> dict:
    """根据 q 形态自动调度:
        - 含 * 或 ?              -> wildcard
        - "xxx" 双引号           -> phrase
        - 否则                   -> 站内查询
    site 非空时各分支都会限定到该站点。
    """
    es = es or get_es()
    q = q.strip()
    result: dict[str, Any] = {"type": "site", "hits": [], "docs": []}
    if not q:
        return result

    if _has_wildcard(q):
        result["type"] = "wildcard"
        result["hits"] = wildcard_search(q, user_profile=user_profile,
                                         size=size, site=site, es=es)
    elif q.startswith('"') and q.endswith('"') and len(q) >= 3:
        phrase = q[1:-1]
        result["type"] = "phrase"
        result["hits"] = phrase_search(phrase, user_profile=user_profile,
                                       size=size, site=site, es=es)
    else:
        result["type"] = "site"
        result["hits"] = site_search(q, user_profile=user_profile,
                                     size=size, site=site, es=es)

    try:
        result["docs"] = doc_search(q, size=min(size, 5), es=es)
    except Exception:
        result["docs"] = []
    return result


# 查询日志
def log_query(user_id: str, query: str, es: Elasticsearch | None = None):
    es = es or get_es()
    try:
        es.index(index=config.INDEX_LOGS, document={
            "user_id": user_id,
            "query": query,
            "ts": int(time.time()),
        })
    except Exception as e:
        print(f"[log] failed: {e}")


def get_user_logs(user_id: str, size: int = 20,
                  es: Elasticsearch | None = None) -> list[dict]:
    es = es or get_es()
    try:
        resp = es.search(
            index=config.INDEX_LOGS,
            query={"term": {"user_id": user_id}},
            sort=[{"ts": {"order": "desc"}}],
            size=size,
        )
        return [h["_source"] for h in resp.body.get("hits", {}).get("hits", [])]
    except Exception:
        return []


# 用户画像 (合并显式偏好 + 历史隐式偏好)
def build_user_profile(user_id: str, base_profile: dict | None = None,
                       es: Elasticsearch | None = None) -> dict:
    """base_profile 来自注册资料 (college / role); 这里再补 prefer_terms。"""
    profile = dict(base_profile or {})
    logs = get_user_logs(user_id, size=50, es=es)
    if logs:
        from collections import Counter
        import jieba
        words: list[str] = []
        for l in logs:
            q = (l.get("query") or "").strip()
            if not q:
                continue
            for w in jieba.cut(q):
                w = w.strip()
                if len(w) >= 2:
                    words.append(w)
        top = [w for w, _ in Counter(words).most_common(5)]
        profile["prefer_terms"] = top
    return profile
