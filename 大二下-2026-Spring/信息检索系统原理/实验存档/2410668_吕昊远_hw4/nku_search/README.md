# 南开搜索 · NKU Web Search Engine

> 南开大学《信息检索系统原理》HW4 课程大作业 · 校内资源搜索引擎

---

## 1. 环境准备

### 1.1 Python 依赖
```bash
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

### 1.2 启动 Elasticsearch
下载 ES 8.x 的 zip 包解压运行 `bin/elasticsearch`,
然后在 ES 的 `config/elasticsearch.yml` 里加 `xpack.security.enabled: false`。

> 安装 IK
> ```
> bin\elasticsearch-plugin.bat install https://get.infini.cloud/elasticsearch/analysis-ik/8.15.1
> ```
> 装完**重启 ES**, 再 `build_index.py --recreate` 重建索引 + `build_suggest.py --recreate`

---

## 2. 运行

### Step 1. 抓取
```bash
python -m crawler.crawler --max-pages 100000 --workers 16
```
产出: `data/pages.jsonl`, `data/docs.jsonl`, `data/snapshots/*.html`,
`data/docs/*.{pdf,docx,xlsx,...}`。

### Step 2. 链接分析 + 建索引
```bash
python scripts/compute_pagerank.py    # 算 PageRank
python scripts/build_index.py --recreate    # 灌入 ES (网页 + 文档)
python scripts/build_suggest.py --recreate    # 建查询自动补全词典
```

### Step 3. 启动 Web
```bash
python scripts/run_web.py
# 浏览器打开 http://127.0.0.1:5000
```

---
