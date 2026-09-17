"""
PageRank 计算 (链接分析)
========================
- 读取 pages.jsonl 中的 url + out_links
- 构造有向图, 用 networkx 计算 pagerank
- 写入 data/pagerank.json (url -> score)
"""
from __future__ import annotations

import json
import os
import sys

import networkx as nx

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config


def build_graph(jsonl_path: str) -> nx.DiGraph:
    g = nx.DiGraph()
    with open(jsonl_path, "r", encoding="utf-8") as f:
        for line in f:
            rec = json.loads(line)
            u = rec["url"]
            g.add_node(u)
            for v in rec.get("out_links", []):
                g.add_edge(u, v)
    return g


def compute_and_save(jsonl_path: str = None,
                     out_path: str = None,
                     alpha: float = 0.85,
                     max_iter: int = 50):
    jsonl_path = jsonl_path or os.path.join(config.DATA_DIR, "pages.jsonl")
    out_path = out_path or config.PAGERANK_PATH

    print(f"[pagerank] building graph from {jsonl_path}")
    g = build_graph(jsonl_path)
    print(f"[pagerank] nodes={g.number_of_nodes()} edges={g.number_of_edges()}")

    print("[pagerank] computing...")
    pr = nx.pagerank(g, alpha=alpha, max_iter=max_iter, tol=1e-5)

    # 归一化到 0-1
    if pr:
        mx = max(pr.values())
        if mx > 0:
            pr = {k: v / mx for k, v in pr.items()}

    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(pr, f, ensure_ascii=False)
    print(f"[pagerank] saved {len(pr)} scores -> {out_path}")


if __name__ == "__main__":
    compute_and_save()
