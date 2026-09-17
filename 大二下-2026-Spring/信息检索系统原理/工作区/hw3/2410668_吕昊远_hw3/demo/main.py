from pdf_processor import extract_pdf_data
from search_core import AdvancedSearchEngine


# 来源标签渲染
SOURCE_LABEL = {
    'sparse': '稀疏路(BM25)',
    'dense' : '稠密路(TF-IDF)',
    'both'  : '双路均命中',
}


def main():
    DATA_FOLDER = "./data"

    print("=" * 50)
    print("文献检索系统")
    print("=" * 50)
    print("\n构建索引...")

    raw_docs = extract_pdf_data(DATA_FOLDER)
    if not raw_docs:
        print("未找到文档，退出。")
        return

    engine = AdvancedSearchEngine()
    engine.build_index(raw_docs)

    print(f"\n索引构建完成，共载入 {len(raw_docs)} 份文档。")

    while True:
        print("=" * 50)
        query = input("输入搜索词 (quit 退出): ").strip()
        if not query or query.lower() in ('quit', 'exit'):
            print("感谢使用，再见！")
            break

        year_input = input("年份过滤 (直接回车跳过): ").strip()
        year_filter = int(year_input) if year_input.isdigit() else None

        top_n_input = input("显示前 N 条结果 (默认 10): ").strip()
        top_n = int(top_n_input) if top_n_input.isdigit() else 10

        # 双路召回 + RRF 融合排序
        results = engine.dual_recall_and_rank(query)

        if not results:
            print("\n未找到任何匹配文档。")
            continue

        # 年份过滤
        if year_filter:
            results = [(doc, score, src) for doc, score, src in results
                       if doc['year'] >= year_filter]

        if not results:
            print(f"\n过滤年份 ≥ {year_filter} 后无结果。")
            continue

        print(f"\n共找到 {len(results)} 条结果，显示前 {top_n} 条：\n")

        for rank, (doc, score, source) in enumerate(results[:top_n], start=1):
            print(f"┌─ [{rank}]  {SOURCE_LABEL[source]}  |  RRF 得分: {score}")
            print(f"│  论文标题: {doc['title']}")
            snippet = engine.get_snippet(doc['raw_content'], query)
            print(f"│  检索片段: {snippet}")
            print(f"│  发表年份: {doc['year']}")
            print(f"│  文件路径: {doc['path']}")
            print("└" + "─" * 50)

        print()


if __name__ == "__main__":
    main()