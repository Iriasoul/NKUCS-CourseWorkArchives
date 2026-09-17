"""User 表的数据访问层"""
from ..db import get_cursor


def find_by_id(user_id):
    """按 ID 查用户,不存在返回 None"""
    with get_cursor() as cur:
        cur.execute('SELECT * FROM user WHERE user_id = %s', (user_id,))
        return cur.fetchone()


def find_by_username(username):
    """按用户名查,不存在返回 None。登录时用"""
    with get_cursor() as cur:
        cur.execute('SELECT * FROM user WHERE username = %s', (username,))
        return cur.fetchone()


def list_active_students():
    """所有正常状态的学生用户"""
    with get_cursor() as cur:
        cur.execute(
            'SELECT user_id, username, college FROM user '
            'WHERE user_type = 1 AND status = 1'
        )
        return cur.fetchall()


def create(username, password_hash, user_type, college=None):
    """创建用户,返回新用户的 user_id"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'INSERT INTO user (username, password_hash, user_type, college) '
            'VALUES (%s, %s, %s, %s)',
            (username, password_hash, user_type, college),
        )
        return cur.lastrowid  # 自增主键的值
    
def exists_by_username(username):
    """用户名是否已被占用"""
    with get_cursor() as cur:
        cur.execute('SELECT 1 FROM user WHERE username = %s', (username,))
        return cur.fetchone() is not None
    
    
def deactivate_user(user_id, recipient_id):
    """调用存储过程注销用户,转移其帖子"""
    with get_cursor(commit=True) as cur:
        cur.callproc('sp_deactivate_user', (user_id, recipient_id))
        
        
def list_all_brief():
    """列出所有用户的简要信息(admin)"""
    with get_cursor() as cur:
        cur.execute(
            'SELECT user_id, username, user_type, status, post_count '
            'FROM user ORDER BY user_id'
        )
        return cur.fetchall()
    
def get_system_account_id():
    """找到定好的的注销用户的ID"""
    user = find_by_username('已注销用户')
    return user['user_id'] if user else None