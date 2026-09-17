import fitz  # PyMuPDF
import os
import re


def _extract_year(doc, text):
    # 从 PDF 元数据 creationDate 提取
    meta = doc.metadata
    if meta.get('creationDate') and len(meta['creationDate']) > 5:
        year_str = meta['creationDate'][2:6]
        if year_str.isdigit() and 1900 < int(year_str) <= 2030:
            return int(year_str)

    # 从正文前 500 字中用正则提取四位年份，优先匹配括号
    header = text[:500]
    bracket_years = re.findall(r'\((\d{4})\)', header)
    plain_years   = re.findall(r'\b(20[0-2]\d|19[89]\d)\b', header)
    candidates    = bracket_years + plain_years
    if candidates:
        return int(candidates[0])

    return 2024  # 默认值


def _extract_title(doc, text, filename):
    # PDF 元数据 title 字段
    meta = doc.metadata
    if meta.get('title') and len(meta['title'].strip()) > 3:
        return meta['title'].strip()

    # 取正文第一个有实质内容的非空行
    for line in text.split('\n'):
        line = line.strip()
        if len(line) > 5:
            return line[:120]

    # 退化到文件名
    return os.path.splitext(filename)[0]


def extract_pdf_data(folder_path):
    documents = []
    if not os.path.exists(folder_path):
        os.makedirs(folder_path)
        print(f"！！！文件夹 {folder_path} 不存在，已自动创建，请放入 PDF 文件后重新运行。")
        return documents

    pdf_files = [f for f in os.listdir(folder_path) if f.endswith(".pdf")]
    if not pdf_files:
        print(f"！！！{folder_path} 中未找到任何 PDF 文件。")
        return documents

    for filename in pdf_files:
        path = os.path.join(folder_path, filename)
        try:
            doc  = fitz.open(path)
            text = "".join(page.get_text() for page in doc)

            documents.append({
                "title":       _extract_title(doc, text, filename),
                "path":        os.path.abspath(path),
                "content":     text.lower(),       # 用于索引构建
                "raw_content": text,               # 原始大小写，用于片段高亮
                "year":        _extract_year(doc, text),
            })
            doc.close()
            print(f"已解析: {filename}")
        except Exception as e:
            print(f"！！！解析失败 {filename}: {e}")

    return documents