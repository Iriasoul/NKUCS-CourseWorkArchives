import collections
import re
import math
import numpy as np
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.metrics.pairwise import cosine_similarity


# 停用词
STOPWORDS = {
    'the', 'a', 'an', 'is', 'in', 'of', 'to', 'and', 'for',
    'with', 'on', 'at', 'by', 'this', 'that', 'are', 'was',
    'be', 'as', 'it', 'its', 'from', 'or', 'but', 'not',
    'have', 'had', 'has', 'he', 'she', 'they', 'we', 'you',
    'i', 'do', 'did', 'will', 'can', 'which', 'who', 'their',
}


class AdvancedSearchEngine:
    def __init__(self):
        # 稀疏路
        self.inverted_index: dict[str, set] = collections.defaultdict(set)
        self.doc_lengths: list[int] = []
        self.avgdl: float = 1.0

        # 稠密路
        self.tfidf_vectorizer: TfidfVectorizer | None = None
        self.tfidf_matrix = None          # scipy sparse matrix

        # 文档库
        self.all_docs: list[dict] = []

    # 索引构建
    def build_index(self, docs: list[dict]):
        self.all_docs = docs

        for idx, doc in enumerate(docs):
            words = re.findall(r'\w+', doc['content'])
            filtered = [w for w in words if w not in STOPWORDS and len(w) > 1]
            self.doc_lengths.append(len(filtered))
            for word in filtered:
                self.inverted_index[word].add(idx)

        self.avgdl = (sum(self.doc_lengths) / len(self.doc_lengths)
                      if self.doc_lengths else 1.0)

        contents = [doc['content'] for doc in docs]
        self.tfidf_vectorizer = TfidfVectorizer(
            stop_words='english',
            max_features=20_000,
            sublinear_tf=True,        # 1+log(tf)
        )
        self.tfidf_matrix = self.tfidf_vectorizer.fit_transform(contents)

    # 编辑距离（dp算）
    @staticmethod
    def _edit_distance(a: str, b: str) -> int:
        dp = list(range(len(b) + 1))
        for i, ca in enumerate(a):
            ndp = [i + 1]
            for j, cb in enumerate(b):
                ndp.append(min(
                    dp[j]     + (ca != cb),   # 替换
                    dp[j + 1] + 1,            # 删除
                    ndp[-1]   + 1,            # 插入
                ))
            dp = ndp
        return dp[-1]

    # 稀疏路：子串/前缀匹配 + 编辑距离
    def sparse_recall(self, query: str, edit_threshold: int = 2) -> set:
        q = query.lower()
        matched: set = set()
        for key in self.inverted_index:
            # 子串/前缀
            if q in key:
                matched.update(self.inverted_index[key])
            # 编辑距离
            elif abs(len(key) - len(q)) <= edit_threshold:
                if self._edit_distance(q, key) <= edit_threshold:
                    matched.update(self.inverted_index[key])
        return matched

    # 稠密路：TF-IDF 余弦相似度
    def dense_recall(self, query: str, top_k: int = 20) -> tuple[set, np.ndarray]:
        query_vec = self.tfidf_vectorizer.transform([query.lower()])
        scores    = cosine_similarity(query_vec, self.tfidf_matrix).flatten()
        top_idx   = np.argsort(scores)[::-1][:top_k]
        recall_set = set(int(i) for i in top_idx if scores[i] > 0)
        return recall_set, scores

    # RRF融合
    def dual_recall_and_rank(
        self,
        query: str,
        k1: float = 1.5,
        b:  float = 0.75,
        rrf_k: int = 60,
    ) -> list[tuple[dict, float, str]]:
        sparse_set         = self.sparse_recall(query)
        dense_set, d_scores = self.dense_recall(query)
        all_idx            = sparse_set | dense_set

        if not all_idx:
            return []

        bm25_ranked = self._bm25_rank(list(all_idx), query, k1, b)
        sparse_rank  = {doc_idx: rank for rank, (doc_idx, _) in enumerate(bm25_ranked)}

        dense_sorted = sorted(all_idx, key=lambda i: d_scores[i], reverse=True)
        dense_rank   = {doc_idx: rank for rank, doc_idx in enumerate(dense_sorted)}

        fused: dict[int, float] = {}
        for idx in all_idx:
            r_sparse = sparse_rank.get(idx, len(all_idx))
            r_dense  = dense_rank.get(idx, len(all_idx))
            fused[idx] = 1.0 / (rrf_k + r_sparse) + 1.0 / (rrf_k + r_dense)

        results = []
        for idx, score in sorted(fused.items(), key=lambda x: x[1], reverse=True):
            in_sparse = idx in sparse_set
            in_dense  = idx in dense_set
            if   in_sparse and in_dense: source = 'both'
            elif in_sparse:              source = 'sparse'
            else:                        source = 'dense'
            results.append((self.all_docs[idx], round(score, 6), source))

        return results

    # BM25 打分
    def _bm25_rank(
        self, indices: list[int], query: str, k1: float, b: float
    ) -> list[tuple[int, float]]:
        tokens = [t for t in re.findall(r'\w+', query.lower()) if t not in STOPWORDS]
        N      = len(self.all_docs)
        ranked = []
        for idx in indices:
            doc     = self.all_docs[idx]
            dl      = self.doc_lengths[idx]
            content = doc['content']
            score   = 0.0
            for t in tokens:
                tf = content.count(t)
                df = len(self.inverted_index.get(t, set()))
                idf = math.log((N - df + 0.5) / (df + 0.5) + 1)
                score += idf * (tf * (k1 + 1)) / (
                    tf + k1 * (1 - b + b * dl / self.avgdl)
                )
            ranked.append((idx, round(score, 4)))
        return sorted(ranked, key=lambda x: x[1], reverse=True)

    # 片段高亮
    @staticmethod
    def get_snippet(raw_content: str, query: str, window: int = 140) -> str:
        lower_content = raw_content.lower()
        pos = lower_content.find(query.lower())

        if pos == -1:
            snippet = raw_content[:window].replace('\n', ' ').strip()
            return f"{snippet}..."

        start   = max(0, pos - window // 2)
        end     = min(len(raw_content), pos + window // 2)
        snippet = raw_content[start:end].replace('\n', ' ').strip()

        # 高亮
        highlighted = re.sub(
            re.escape(query),
            f"\033[1;33m{query.upper()}\033[0m",  # 黄色加粗
            snippet,
            flags=re.IGNORECASE,
        )
        return f"...{highlighted}..."