"""一键计算 PageRank, 输出到 data/pagerank.json。"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from search.pagerank import compute_and_save
if __name__ == "__main__":
    compute_and_save()
