"""数据库连接管理"""
import pymysql
from contextlib import contextmanager
from .config import Config


@contextmanager
def get_connection():
    """获取数据库连接,使用完自动关闭"""
    conn = pymysql.connect(**Config.db_kwargs())
    try:
        yield conn
    finally:
        conn.close()


@contextmanager
def get_cursor(commit=False):
    """
    获取游标,自动管理事务。
    - 读操作:with get_cursor() as cur: ...
    - 写操作:with get_cursor(commit=True) as cur: ...
    """
    with get_connection() as conn:
        cursor = conn.cursor()
        try:
            yield cursor
            if commit:
                conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            cursor.close()