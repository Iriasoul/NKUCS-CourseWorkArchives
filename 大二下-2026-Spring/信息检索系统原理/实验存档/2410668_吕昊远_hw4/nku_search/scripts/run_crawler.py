"""一键启动爬虫的封装脚本。"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from crawler.crawler import main
if __name__ == "__main__":
    main()
