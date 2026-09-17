"""Section 表的数据访问层"""
from ..db import get_cursor


def find_by_id(section_id):
    """按 ID 查板块,不存在返回 None"""
    with get_cursor() as cur:
        cur.execute('SELECT * FROM section WHERE section_id = %s', (section_id,))
        return cur.fetchone()


def list_all():
    """列出所有板块"""
    with get_cursor() as cur:
        cur.execute('SELECT * FROM section ORDER BY section_id')
        return cur.fetchall()