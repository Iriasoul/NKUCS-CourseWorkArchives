"""一键构建 Elasticsearch 索引 (含 pages + docs + 日志)。"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import argparse
from indexer.es_indexer import main

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--recreate", action="store_true",
                   help="先删除已有索引再重建。")
    args = p.parse_args()
    main(recreate=args.recreate)
