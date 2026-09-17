"""
混合排序 
============================
两阶段: ES 先用 BM25 召回候选池, 这里在 Python 里按下式重排:

    final = w_personal * personal_score
          + w_time     * time_score
          + w_pagerank * pagerank
          + w_bm25      * bm25_norm

- personal_score: 学院关键词命中 (标题命中权重高, 正文低), 归一化到 [0,1]
- time_score:     按发布时间分箱 (1天1.0 ... 1年以上0.2), 再乘身份系数
- pagerank:       建索引时已归一化到 [0,1]
- bm25_norm:      ES _score 除以候选池里的最大分, 归一化到 [0,1]

"""
from __future__ import annotations

import time as _time
from typing import Any

import config


# 时效性
def time_score(publish_ts: int | float | None, role: str = "",
               now: float | None = None) -> float:
    """根据发布时间(epoch 秒)算时效分, 并按身份调整。"""
    base = config.TIME_SCORE_NO_DATE
    if publish_ts:
        now = now or _time.time()
        age = max(0.0, now - float(publish_ts))
        for upper, score in config.TIME_BINS:
            if age <= upper:
                base = score
                break
    factor = config.ROLE_TIME_FACTOR.get(role, 1.0)
    # 乘系数后裁剪回 [0,1]
    return max(0.0, min(1.0, base * factor))


# 个性化 (学院关键词匹配)
def personal_score(title: str, body: str, college: str | None,
                   extra_terms: list[str] | None = None) -> float:
    """学院关键词在标题/正文里的命中度, 归一化到 [0,1]。
    标题命中记 1.0, 正文命中记 0.3, extra_terms (来自历史) 各 0.5。
    """
    if not college and not extra_terms:
        return 0.0
    title = title or ""
    body = (body or "")[:2000]   # 只看正文前 2000 字, 控速
    keywords = list(config.COLLEGE_KEYWORDS.get(college or "", []))

    raw = 0.0
    for kw in keywords:
        if kw in title:
            raw += 1.0
        elif kw in body:
            raw += 0.3
    for t in (extra_terms or []):
        if t and (t in title or t in body):
            raw += 0.5

    # 命中越多分越高, 但用饱和函数压到 [0,1]
    return min(1.0, raw / 5.0)


# 候选池重排
def rerank(hits: list[dict], *, user_profile: dict | None,
           weights: dict | None = None, top_k: int = 20) -> list[dict]:
    """ 对 ES 候选池做混合重排, 返回前 top_k 条 """
    if not hits:
        return []

    profile = user_profile or {}
    college = profile.get("college")
    role = profile.get("role", "")
    extra_terms = profile.get("prefer_terms", [])

    if weights is None:
        weights = (config.RANK_WEIGHTS_LOGGED if user_profile
                   else config.RANK_WEIGHTS_ANON)

    # 归一化 BM25
    max_bm25 = max((h.get("score") or 0.0) for h in hits) or 1.0

    now = _time.time()
    for h in hits:
        bm25_norm = (h.get("score") or 0.0) / max_bm25
        pr = float(h.get("pagerank") or 0.0)
        ts = h.get("publish_time")
        t_s = time_score(ts, role=role, now=now)
        p_s = personal_score(h.get("title", ""), h.get("body", ""),
                             college, extra_terms)

        final = (weights["personal"] * p_s
                 + weights["time"] * t_s
                 + weights["pagerank"] * pr
                 + weights["bm25"] * bm25_norm)

        # 存各分量, 便于前端展示 / 调试
        h["final_score"] = round(final, 4)
        h["_components"] = {
            "personal": round(p_s, 3),
            "time": round(t_s, 3),
            "pagerank": round(pr, 3),
            "bm25": round(bm25_norm, 3),
        }

    hits.sort(key=lambda x: x.get("final_score", 0.0), reverse=True)
    return hits[:top_k]
