from __future__ import annotations

import argparse
import hashlib
import json
import os
import queue
import re
import threading
import time
from collections import deque
from dataclasses import asdict, dataclass, field
from typing import Iterable, Optional
from urllib.parse import urldefrag, urljoin, urlparse

import requests
from bs4 import BeautifulSoup

import sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config


# 数据结构
@dataclass
class PageRecord:
    doc_id: str
    url: str
    title: str
    body: str
    anchor_text: str            # 别的页面里指向该 URL 的锚文本拼接
    out_links: list             # 出链 URL 列表 (用于 PageRank)
    crawl_time: float
    publish_time: int = 0       # 发布时间 (epoch 秒, 0=未知), 用于时效排序
    snapshot_path: str = ""     # 网页快照路径 (仅部分网页保存)
    site: str = ""              # 站点域名

@dataclass
class DocRecord:
    doc_id: str
    url: str
    title: str
    ext: str
    local_path: str
    referer: str
    crawl_time: float
    site: str = ""


# 工具函数
def url_id(url: str) -> str:
    """对 URL 做 MD5, 作为文档 ID."""
    return hashlib.md5(url.encode("utf-8")).hexdigest()


def normalize_url(url: str) -> str:
    """去掉 fragment, 简单规范化."""
    url, _ = urldefrag(url.strip())
    return url


def is_allowed(url: str) -> bool:
    """是否属于校内允许域名."""
    try:
        host = urlparse(url).hostname or ""
    except Exception:
        return False
    return any(host.endswith(s) for s in config.ALLOWED_DOMAIN_SUFFIX)


def guess_ext(url: str) -> str:
    """从 URL 猜扩展名."""
    path = urlparse(url).path
    _, ext = os.path.splitext(path)
    return ext.lower()


def is_doc_url(url: str) -> bool:
    return guess_ext(url) in config.DOC_EXTENSIONS


def is_html_url(url: str) -> bool:
    return guess_ext(url) in config.HTML_EXTENSIONS


def safe_get_text(soup: BeautifulSoup, max_len: int = 50_000) -> str:
    """从 HTML 抽正文 (粗糙但够用)."""
    # 去掉脚本/样式
    for s in soup(["script", "style", "noscript"]):
        s.decompose()
    text = soup.get_text(separator=" ", strip=True)
    text = re.sub(r"\s+", " ", text)
    return text[:max_len]


# 常见日期格式
_DATE_PATTERNS = [
    re.compile(r"(20\d{2})[-/.](\d{1,2})[-/.](\d{1,2})"),
    re.compile(r"(20\d{2})年(\d{1,2})月(\d{1,2})日"),
]


def extract_publish_time(soup: BeautifulSoup, body_text: str) -> int:
    """尽力从网页里抽发布时间, 返回 epoch 秒, 抽不到返回 0。

    优先级: <meta> 标签  >>  <time> 标签  >>  正文里的日期模式。
    南开新闻网正文里一般有 "发布时间: 2026-03-01" 这种串。
    """
    import calendar
    candidates: list[str] = []

    # meta
    for name in ("article:published_time", "publishdate", "PubDate",
                 "og:published_time", "date"):
        tag = soup.find("meta", attrs={"property": name}) or \
              soup.find("meta", attrs={"name": name})
        if tag and tag.get("content"):
            candidates.append(tag["content"])

    # <time datetime=...>
    for t in soup.find_all("time"):
        if t.get("datetime"):
            candidates.append(t["datetime"])

    # 正文前 600 字里找日期 
    candidates.append(body_text[:600])

    for c in candidates:
        for pat in _DATE_PATTERNS:
            m = pat.search(c)
            if m:
                y, mo, d = int(m.group(1)), int(m.group(2)), int(m.group(3))
                if 2000 <= y <= 2030 and 1 <= mo <= 12 and 1 <= d <= 31:
                    try:
                        return int(calendar.timegm((y, mo, d, 0, 0, 0, 0, 0, 0)))
                    except Exception:
                        continue
    return 0


# 主爬虫
class NKUCrawler:
    def __init__(
        self,
        seeds: Iterable[str],
        max_pages: int = config.MAX_PAGES,
        max_docs: int = config.MAX_DOCS,
        num_workers: int = config.NUM_WORKERS,
        output_dir: str = config.DATA_DIR,
        snapshot_sample_rate: float = 0.01,  # 1% 网页保存完整快照
    ) -> None:
        self.seeds = [normalize_url(s) for s in seeds]
        self.max_pages = max_pages
        self.max_docs = max_docs
        self.num_workers = num_workers
        self.output_dir = output_dir
        self.snapshot_sample_rate = snapshot_sample_rate

        # BFS 队列
        self.queue: "queue.Queue[str]" = queue.Queue()
        for u in self.seeds:
            self.queue.put(u)

        # 已访问集合 & 锚文本聚合
        self.visited: set[str] = set()
        self.anchor_map: dict[str, list[str]] = {}
        self.lock = threading.Lock()

        # 计数
        self.pages_count = 0
        self.docs_count = 0

        # 输出文件 (jsonl)
        self.pages_fp = open(os.path.join(output_dir, "pages.jsonl"), "w",
                             encoding="utf-8")
        self.docs_fp = open(os.path.join(output_dir, "docs.jsonl"), "w",
                            encoding="utf-8")
        self.write_lock = threading.Lock()

        # session: 共享 connection pool
        self.session = requests.Session()
        self.session.headers.update({"User-Agent": config.USER_AGENT})

        # 终止信号
        self.stop_event = threading.Event()

    # 网络抓取
    def fetch(self, url: str) -> Optional[requests.Response]:
        try:
            resp = self.session.get(url, timeout=config.REQUEST_TIMEOUT,
                                    allow_redirects=True)
            if resp.status_code != 200:
                return None
            return resp
        except Exception:
            return None

    # 解析 HTML
    def parse_html(self, resp: requests.Response, base_url: str) -> tuple[str, str, int, list[tuple[str, str]]]:
        """返回 (title, body_text, publish_time, [(child_url, anchor_text), ...])."""
        # 编码探测
        if resp.encoding is None or resp.encoding.lower() == "iso-8859-1":
            resp.encoding = resp.apparent_encoding or "utf-8"
        html = resp.text
        soup = BeautifulSoup(html, "lxml")

        title = (soup.title.string or "").strip() if soup.title else ""
        body = safe_get_text(soup)
        publish_time = extract_publish_time(soup, body)

        links: list[tuple[str, str]] = []
        for a in soup.find_all("a", href=True):
            href = a["href"].strip()
            if not href or href.startswith(("javascript:", "mailto:", "#")):
                continue
            child = normalize_url(urljoin(base_url, href))
            anchor = a.get_text(strip=True)[:80]
            links.append((child, anchor))

        return title, body, publish_time, links

    # 文档下载
    def _ext_from_ctype(ctype):
        for key, e in [("pdf",".pdf"), ("wordprocessingml",".docx"), ("msword",".doc"),
                    ("spreadsheetml",".xlsx"), ("ms-excel",".xls"),
                    ("presentationml",".pptx"), ("ms-powerpoint",".ppt")]:
            if key in ctype.lower():
                return e
        return ".bin"
    def save_doc(self, resp: requests.Response, url: str, referer: str) -> None:
        ext = guess_ext(url) or _ext_from_ctype(ctype)
        doc_id = url_id(url)
        local_path = os.path.join(config.DOCS_DIR, doc_id + ext)
        try:
            with open(local_path, "wb") as f:
                f.write(resp.content)
        except Exception:
            return
        # title: 用 referer 上下文的锚文本, 没有则用文件名
        with self.lock:
            anchors = self.anchor_map.get(url, [])
        title = anchors[0] if anchors else os.path.basename(urlparse(url).path)

        record = DocRecord(
            doc_id=doc_id,
            url=url,
            title=title,
            ext=ext.lstrip("."),
            local_path=local_path,
            referer=referer,
            crawl_time=time.time(),
            site=urlparse(url).hostname or "",
        )
        with self.write_lock:
            self.docs_fp.write(json.dumps(asdict(record), ensure_ascii=False) + "\n")
            self.docs_count += 1

    # 处理一个 URL
    def process(self, url: str) -> None:
        if self.stop_event.is_set():
            return

        # 文档: 单独处理
        if is_doc_url(url):
            with self.lock:
                if self.docs_count >= self.max_docs:
                    return
            resp = self.fetch(url)
            if resp is None:
                return
            self.save_doc(resp, url, referer="")
            return

        # HTML
        resp = self.fetch(url)
        if resp is None:
            return
        ctype = resp.headers.get("Content-Type", "").lower()
        DOC_CTYPES = ("application/pdf", "msword", "ms-excel", "ms-powerpoint",
                    "officedocument")          # docx/xlsx/pptx 都含 officedocument
        if any(t in ctype for t in DOC_CTYPES):
            with self.lock:
                if self.docs_count >= self.max_docs:
                    return
            self.save_doc(resp, url, referer="", ctype=ctype)
            return

        if "html" not in ctype and not is_html_url(url):
            return

        title, body, publish_time, links = self.parse_html(resp, url)
        doc_id = url_id(url)

        # 保存快照 (按抽样率)
        snapshot_path = ""
        # 保证至少有 N 个完整快照
        do_snapshot = (
            (self.pages_count < 50)  # 前 50 条全保存, 保证演示用
            or (hash(doc_id) % 100 < int(self.snapshot_sample_rate * 100))
        )
        if do_snapshot:
            snapshot_path = os.path.join(config.SNAPSHOT_DIR, doc_id + ".html")
            try:
                with open(snapshot_path, "w", encoding="utf-8") as f:
                    f.write(resp.text)
            except Exception:
                snapshot_path = ""

        with self.lock:
            anchor_text = " ".join(self.anchor_map.get(url, []))[:500]

        out_links_in_domain = []
        # 写入子链接到队列, 并聚合锚文本
        for child, anchor in links:
            if not is_allowed(child):
                continue
            out_links_in_domain.append(child)
            with self.lock:
                if anchor:
                    self.anchor_map.setdefault(child, []).append(anchor)
                if child not in self.visited:
                    self.visited.add(child)
                    self.queue.put(child)

        record = PageRecord(
            doc_id=doc_id,
            url=url,
            title=title,
            body=body,
            anchor_text=anchor_text,
            out_links=out_links_in_domain[:200],
            crawl_time=time.time(),
            publish_time=publish_time,
            snapshot_path=snapshot_path,
            site=urlparse(url).hostname or "",
        )
        with self.write_lock:
            self.pages_fp.write(json.dumps(asdict(record), ensure_ascii=False) + "\n")
            self.pages_count += 1
            if self.pages_count % 500 == 0:
                print(f"[crawler] pages={self.pages_count} "
                      f"docs={self.docs_count} queue={self.queue.qsize()}",
                      flush=True)

        if self.pages_count >= self.max_pages:
            self.stop_event.set()

        time.sleep(config.CRAWL_DELAY)

    # worker
    def worker(self) -> None:
        while not self.stop_event.is_set():
            try:
                url = self.queue.get(timeout=10)
            except queue.Empty:
                return
            try:
                self.process(url)
            except Exception as e:
                # 单条失败不影响整体
                pass
            finally:
                self.queue.task_done()

    # run
    def run(self) -> None:
        with self.lock:
            for u in self.seeds:
                self.visited.add(u)

        threads = [threading.Thread(target=self.worker, daemon=True)
                   for _ in range(self.num_workers)]
        for t in threads:
            t.start()

        try:
            while not self.stop_event.is_set():
                time.sleep(1)
                # 全部 worker 都拿不到任务时退出
                if self.queue.empty():
                    # 等待 30s 看是否还有新增
                    time.sleep(5)
                    if self.queue.empty():
                        break
        except KeyboardInterrupt:
            print("[crawler] interrupted, stopping...")
            self.stop_event.set()

        # 等剩余 worker 退出
        for t in threads:
            t.join(timeout=2)

        self.pages_fp.close()
        self.docs_fp.close()
        print(f"[crawler] DONE pages={self.pages_count} docs={self.docs_count}")


# CLI
def main():
    parser = argparse.ArgumentParser(description="NKU campus crawler")
    parser.add_argument("--max-pages", type=int, default=config.MAX_PAGES)
    parser.add_argument("--max-docs", type=int, default=config.MAX_DOCS)
    parser.add_argument("--workers", type=int, default=config.NUM_WORKERS)
    parser.add_argument("--seeds", nargs="*", default=config.SEED_URLS)
    args = parser.parse_args()

    crawler = NKUCrawler(
        seeds=args.seeds,
        max_pages=args.max_pages,
        max_docs=args.max_docs,
        num_workers=args.workers,
    )
    crawler.run()


if __name__ == "__main__":
    main()
