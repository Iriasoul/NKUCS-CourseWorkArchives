""" 数据自检, 统计爬取产物的关键指标 """
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config


def _pct(part: int, whole: int) -> str:
    return f"{(100.0 * part / whole):.1f}%" if whole else "—"


def main():
    pages_path = os.path.join(config.DATA_DIR, "pages.jsonl")
    docs_path = os.path.join(config.DATA_DIR, "docs.jsonl")

    n = anchor_nonempty = pub_known = snap_marked = title_nonempty = 0
    if os.path.exists(pages_path):
        with open(pages_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    rec = json.loads(line)
                except Exception:
                    continue
                n += 1
                if (rec.get("anchor_text") or "").strip():
                    anchor_nonempty += 1
                if rec.get("publish_time"):
                    pub_known += 1
                if (rec.get("snapshot_path") or "").strip():
                    snap_marked += 1
                if (rec.get("title") or "").strip():
                    title_nonempty += 1
    else:
        print(f"[check] 找不到 {pages_path}, 请先跑爬虫。")

    dn = 0
    if os.path.exists(docs_path):
        with open(docs_path, "r", encoding="utf-8") as f:
            dn = sum(1 for line in f if line.strip())

    snap_files = (len([x for x in os.listdir(config.SNAPSHOT_DIR)
                       if x.endswith(".html")])
                  if os.path.exists(config.SNAPSHOT_DIR) else 0)
    doc_files = (len(os.listdir(config.DOCS_DIR))
                 if os.path.exists(config.DOCS_DIR) else 0)

    print("=" * 48)
    print("  NKU 搜索引擎 · 数据自检")
    print("=" * 48)
    print(f"页面总数 (pages.jsonl) : {n}"
          f"{'   ！未达 10 万' if n < 100000 else '  '}")
    print(f"  标题非空            : {title_nonempty:>7}  ({_pct(title_nonempty, n)})")
    print(f"  锚文本非空          : {anchor_nonempty:>7}  ({_pct(anchor_nonempty, n)})")
    print(f"  含发布时间          : {pub_known:>7}  ({_pct(pub_known, n)})")
    print(f"  标记了快照          : {snap_marked:>7}")
    print(f"快照文件数 (snapshots/): {snap_files}")
    print(f"文档条目 (docs.jsonl) : {dn}")
    print(f"文档文件数 (docs/)    : {doc_files}")
    print("=" * 48)


if __name__ == "__main__":
    main()
