"""
全局配置文件
"""
import os

# 路径配置
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data")
SNAPSHOT_DIR = os.path.join(DATA_DIR, "snapshots")        # 网页快照存放目录
DOCS_DIR = os.path.join(DATA_DIR, "docs")                 # PDF/Word 等文档下载目录
DB_PATH = os.path.join(DATA_DIR, "app.db")                # SQLite (用户+日志)
PAGERANK_PATH = os.path.join(DATA_DIR, "pagerank.json")   # PageRank 缓存

for _d in (DATA_DIR, SNAPSHOT_DIR, DOCS_DIR):
    os.makedirs(_d, exist_ok=True)

# Elasticsearch 配置
ES_HOST = os.getenv("ES_HOST", "http://localhost:9200")
ES_USERNAME = os.getenv("ES_USERNAME", "")
ES_PASSWORD = os.getenv("ES_PASSWORD", "")

# 索引名
INDEX_PAGES = "nku_pages"      # 网页索引
INDEX_DOCS = "nku_docs"        # 文档索引 (pdf/doc/...)
INDEX_LOGS = "nku_querylog"    # 查询日志
INDEX_SUGGEST = "nku_suggest"  # 自动补全词典 (completion suggester)

# 爬虫配置
# 种子 URL: 这里以南开新闻+主站为主, 可扩展。
SEED_URLS = [
    "https://news.nankai.edu.cn/",
    "https://www.nankai.edu.cn/",
    "https://cc.nankai.edu.cn/",        # 计算机学院
    "https://cyber.nankai.edu.cn/",     # 网安学院
    "https://math.nankai.edu.cn/",      # 数院
    "https://history.nankai.edu.cn/",   # 历史学院
    "https://lib.nankai.edu.cn/",       # 图书馆
    "https://graduate.nankai.edu.cn/",  # 研究生院
]

# 只爬取域名以这些为后缀的链接 (校内资源限制)
ALLOWED_DOMAIN_SUFFIX = ("nankai.edu.cn", "nankai.cn", "12club.nankai.edu.cn")

MAX_PAGES = 100_000              # 抓取页面数量上限 (作业要求至少 10 万)
MAX_DOCS = 2_000                 # 文档(pdf/doc/xls) 最多抓多少
REQUEST_TIMEOUT = 15             # 单次请求超时
CRAWL_DELAY = 0.2                # 礼貌爬取: 每个线程的请求间隔
NUM_WORKERS = 16                 # 并发线程数
USER_AGENT = (
    "Mozilla/5.0 (compatible; NKU-IR-Coursework-Bot/1.0; "
    "+for academic use only)"
)

# 文件扩展名
DOC_EXTENSIONS = {".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx"}
HTML_EXTENSIONS = {"", ".html", ".htm", ".shtml", ".asp", ".aspx", ".jsp", ".php"}

# Flask 配置
SECRET_KEY = os.getenv("SECRET_KEY", "nku-ir-2026-change-me")


# 混合排序 + 时效性 配置
# 最终得分 = w_personal*个性化 + w_time*时效 + w_pagerank*PR + w_bm25*BM25
# 登录用户用 LOGGED 这套权重; 匿名用户没有个性化, 用 ANON 这套 (权重重分配)
RANK_WEIGHTS_LOGGED = {"personal": 0.25, "time": 0.10, "pagerank": 0.10, "bm25": 0.55}
RANK_WEIGHTS_ANON   = {"personal": 0.0,  "time": 0.10, "pagerank": 0.15, "bm25": 0.75}

# ES 取候选池大小 (先用 BM25 取这么多, 再在 Python 里按公式重排)
CANDIDATE_POOL = 100

# 时效性分箱: (距今秒数上限, 分数) 
import math as _math
_DAY = 86400
TIME_BINS = [
    (1 * _DAY,   1.0),   # 1 天内
    (7 * _DAY,   0.9),   # 1 周内
    (30 * _DAY,  0.8),   # 1 月内
    (90 * _DAY,  0.6),   # 3 月内
    (365 * _DAY, 0.4),   # 1 年内
    (float("inf"), 0.2), # 更久
]
TIME_SCORE_NO_DATE = 0.3   # 无发布日期时的默认时效分

# 身份对时效权重的调整系数 
ROLE_TIME_FACTOR = {
    "undergraduate": 1.3,   # 本科生
    "graduate":      1.0,   # 研究生
    "teacher":       0.6,   # 教师
    "":              1.0,
}

# 学院 与 关键词词库 (个性化内容匹配)
# 命中标题权重高, 命中正文权重低; 详见 search/ranking.py
COLLEGE_KEYWORDS = {
    "计算机与网络空间安全": [
        "计算机", "软件", "人工智能", "算法", "编程", "数据", "网络",
        "AI", "机器学习", "网络安全", "信息安全", "系统", "代码", "深度学习",
    ],
    "金融与经济": [
        "金融", "经济", "银行", "投资", "股票", "基金", "证券", "货币",
        "财经", "市场", "保险", "财务", "会计", "贸易",
    ],
    "数学与统计": [
        "数学", "统计", "概率", "代数", "几何", "分析", "拓扑", "方程",
        "建模", "运筹", "微分", "数理",
    ],
    "生命科学与医学": [
        "生物", "细胞", "基因", "DNA", "生态", "进化", "医学", "植物",
        "动物", "蛋白", "药物", "临床", "病毒", "免疫",
    ],
    "物理与化学": [
        "物理", "化学", "材料", "光学", "量子", "粒子", "分子", "原子",
        "催化", "实验", "能源", "半导体",
    ],
    "文学与历史": [
        "文学", "历史", "哲学", "文化", "语言", "考古", "文献", "古代",
        "现代", "诗歌", "小说", "思想", "汉语",
    ],
    "外国语": [
        "英语", "日语", "翻译", "语言", "外国", "文化", "国际", "口译",
        "笔译", "外语",
    ],
    "商学院": [
        "管理", "营销", "商业", "企业", "战略", "运营", "创业", "供应链",
        "市场", "人力资源", "MBA",
    ],
    "法学院": [
        "法律", "法学", "宪法", "民法", "刑法", "诉讼", "权利", "司法",
        "合同", "知识产权", "国际法",
    ],
}

# 用户身份选项 (注册/资料页用)
ROLE_CHOICES = [
    ("undergraduate", "本科生"),
    ("graduate", "研究生"),
    ("teacher", "教师"),
]


# 查询自动补全 (completion suggester) 配置
# 词典最多收录多少个词条 (按频次取 top-N)
SUGGEST_MAX_TERMS = 50_000
# 查询日志里的词权重放大倍数 (真实查询比语料抽词更可信)
SUGGEST_QUERY_BOOST = 5
# 语料里相邻 token 合成的 bigram 短语的权重 (补充 jieba 切碎的复合词)
SUGGEST_BIGRAM_WEIGHT = 1
# trigram (三词短语) 权重: 让联想弹出更像完整搜索的短语
SUGGEST_TRIGRAM_WEIGHT = 1
# 自定义词典: jieba 默认词典里没有这些复合词
ACADEMIC_DICT = [
    "副教授", "教授", "讲师", "助理教授", "特聘教授", "讲席教授",
    "研究员", "副研究员", "助理研究员", "博士生导师", "硕士生导师",
    "院士", "长江学者", "杰出青年", "南开大学", "周恩来",
    "人工智能", "机器学习", "深度学习", "网络空间安全", "计算机科学",
    "金融工程", "生物科学", "化学学院", "物理学院", "文学院",
    "招生简章", "招生章程", "保研", "夏令营", "奖学金", "研究生院",
]
# 词条最短/最长字符数
SUGGEST_MIN_LEN = 2
SUGGEST_MAX_LEN = 20
# 2 字停用词
SUGGEST_STOPWORDS = {
    "我们", "你们", "他们", "这个", "那个", "什么", "可以", "没有",
    "就是", "这样", "以及", "还有", "这些", "那些", "因为", "所以",
    "但是", "如果", "或者", "通过", "进行", "目前", "已经", "现在",
    "包括", "对于", "关于", "其中", "一个", "以上", "以下", "如下",
}
