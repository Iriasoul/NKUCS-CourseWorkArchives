"""
Elasticsearch 索引 mapping 定义
==============================
- nku_pages   : 网页正排索引 (含 title / body / anchor / url / pagerank)
- nku_docs    : 文档索引 (pdf/doc/xls)
- nku_querylog: 查询日志 (用户搜索历史)
- nku_suggest : 自动补全词典 (completion suggester)
"""

# 中文分词器 (建索引 / 查询)
ANALYZER = "ik_max_word"
SEARCH_ANALYZER = "ik_smart"

_TXT = {"type": "text", "analyzer": ANALYZER, "search_analyzer": SEARCH_ANALYZER}

PAGE_MAPPING = {
    "settings": {"number_of_shards": 1, "number_of_replicas": 0},
    "mappings": {"properties": {
        "doc_id":        {"type": "keyword"},
        "url":           {"type": "keyword"},
        "site":          {"type": "keyword"},
        "title": {"type": "text", "analyzer": ANALYZER,
                  "search_analyzer": SEARCH_ANALYZER,
                  "fields": {"raw": {"type": "keyword"}}},
        "body":          dict(_TXT),
        "anchor_text":   dict(_TXT),
        "out_links":     {"type": "keyword"},
        "snapshot_path": {"type": "keyword"},
        "crawl_time":    {"type": "date", "format": "epoch_second"},
        "publish_time":  {"type": "date", "format": "epoch_second"},
        "pagerank":      {"type": "float"},
    }}
}

DOC_MAPPING = {
    "settings": {"number_of_shards": 1, "number_of_replicas": 0},
    "mappings": {"properties": {
        "doc_id":     {"type": "keyword"},
        "url":        {"type": "keyword"},
        "site":       {"type": "keyword"},
        "title": {"type": "text", "analyzer": ANALYZER,
                  "search_analyzer": SEARCH_ANALYZER,
                  "fields": {"raw": {"type": "keyword"}}},
        "content":    dict(_TXT),
        "ext":        {"type": "keyword"},
        "local_path": {"type": "keyword"},
        "referer":    {"type": "keyword"},
        "crawl_time": {"type": "date", "format": "epoch_second"},
    }}
}


LOG_MAPPING = {
    "settings": {"number_of_shards": 1, "number_of_replicas": 0},
    "mappings": {
        "properties": {
            "user_id":  {"type": "keyword"},
            "query":    {"type": "text", "fields": {"raw": {"type": "keyword"}}},
            "ts":       {"type": "date", "format": "epoch_second"},
            "result_clicked": {"type": "keyword"}
        }
    }
}


# 自动补全词典: completion 字段底层为 FST, 支持毫秒级前缀补全
SUGGEST_MAPPING = {
    "settings": {"number_of_shards": 1, "number_of_replicas": 0},
    "mappings": {
        "properties": {
            "text":    {"type": "keyword"},
            "source":  {"type": "keyword"},
            "suggest": {"type": "completion"}
        }
    }
}
