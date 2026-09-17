"""收藏相关的数据访问层"""
from ..db import get_cursor


def get_folder(folder_id):
    """查单个收藏夹,不存在返回 None"""
    with get_cursor() as cur:
        cur.execute('SELECT * FROM favorite_folder WHERE folder_id = %s', (folder_id,))
        return cur.fetchone()

def get_folders_by_user(user_id):
    """获取用户所有收藏夹,附带每个收藏夹的帖子数量"""
    with get_cursor() as cur:
        cur.execute("""
            SELECT ff.folder_id, ff.folder_name, ff.remark,
                   COUNT(fp.post_id) AS post_count
            FROM favorite_folder ff
            LEFT JOIN favorite_post fp ON ff.folder_id = fp.folder_id
            WHERE ff.user_id = %s
            GROUP BY ff.folder_id, ff.folder_name, ff.remark
            ORDER BY ff.create_time ASC
        """, (user_id,))
        return cur.fetchall()


def create_folder(user_id, folder_name, remark=None):
    """新建收藏夹,返回 folder_id。重名时抛 IntegrityError"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'INSERT INTO favorite_folder (user_id, folder_name, remark) '
            'VALUES (%s, %s, %s)',
            (user_id, folder_name, remark)
        )
        return cur.lastrowid


def is_post_in_folder(folder_id, post_id):
    with get_cursor() as cur:
        cur.execute(
            'SELECT 1 FROM favorite_post WHERE folder_id = %s AND post_id = %s',
            (folder_id, post_id)
        )
        return cur.fetchone() is not None


def add_post_to_folder(folder_id, post_id):
    """收藏帖子,已收藏时静默忽略"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'INSERT IGNORE INTO favorite_post (folder_id, post_id) VALUES (%s, %s)',
            (folder_id, post_id)
        )


def remove_post_from_folder(folder_id, post_id):
    """取消收藏"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'DELETE FROM favorite_post WHERE folder_id = %s AND post_id = %s',
            (folder_id, post_id)
        )


def get_posts_in_folder(folder_id):
    """获取某收藏夹内所有帖子,附带作者和板块"""
    with get_cursor() as cur:
        cur.execute("""
            SELECT p.post_id, p.title, p.publish_time,
                   u.username AS author, s.section_name,
                   fp.favorite_time
            FROM favorite_post fp
            JOIN post p    ON fp.post_id = p.post_id
            JOIN user u    ON p.user_id = u.user_id
            JOIN section s ON p.section_id = s.section_id
            WHERE fp.folder_id = %s
            ORDER BY fp.favorite_time DESC
        """, (folder_id,))
        return cur.fetchall()
    
def get_folder_ids_containing_post(post_id, user_id):
    """返回该用户的收藏夹中已包含此帖子的 folder_id"""
    with get_cursor() as cur:
        cur.execute("""
            SELECT fp.folder_id
            FROM favorite_post fp
            JOIN favorite_folder ff ON fp.folder_id = ff.folder_id
            WHERE fp.post_id = %s AND ff.user_id = %s
        """, (post_id, user_id))
        rows = cur.fetchall()
        return {r['folder_id'] for r in rows}   # 返回集合，模板里 in 判断