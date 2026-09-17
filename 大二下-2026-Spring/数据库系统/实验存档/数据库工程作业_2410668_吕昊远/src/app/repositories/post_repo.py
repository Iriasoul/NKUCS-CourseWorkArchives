"""Post 表的数据访问层"""
from ..db import get_cursor


def find_by_id(post_id):
    with get_cursor() as cur:
        cur.execute('SELECT * FROM post WHERE post_id = %s', (post_id,))
        return cur.fetchone()

def search_by_keyword(keyword):
    """关键字搜索帖子(标题或内容包含)"""
    pattern = f'%{keyword}%'  # SQL LIKE 的通配符
    with get_cursor() as cur:
        cur.execute(
            'SELECT post_id, title, publish_time FROM post '
            'WHERE title LIKE %s OR content LIKE %s '
            'ORDER BY publish_time DESC',
            (pattern, pattern),
        )
        return cur.fetchall()


def create(title, content, user_id, section_id):
    """新建帖子,同步增加作者发帖数,返回 post_id"""
    from ..db import get_connection
    with get_connection() as conn:
        try:
            with conn.cursor() as cur:
                cur.execute(
                    'INSERT INTO post (title, content, user_id, section_id) '
                    'VALUES (%s, %s, %s, %s)',
                    (title, content, user_id, section_id)
                )
                new_id = cur.lastrowid
                cur.execute(
                    'UPDATE user SET post_count = post_count + 1 WHERE user_id = %s',
                    (user_id,)
                )
            conn.commit()
            return new_id
        except Exception:
            conn.rollback()
            raise
    

def find_by_id_with_detail(post_id):
    """查单篇帖子,包含作者用户名和板块名称"""
    with get_cursor() as cur:
        cur.execute("""
            SELECT p.post_id, p.title, p.content, p.publish_time,
                   u.username AS author, u.user_id AS author_id,
                   s.section_name, s.section_id,
                   COUNT(pl.user_id) AS like_count
            FROM post p
            JOIN user u    ON p.user_id = u.user_id
            JOIN section s ON p.section_id = s.section_id
            LEFT JOIN post_like pl ON p.post_id = pl.post_id
            WHERE p.post_id = %s
            GROUP BY p.post_id, p.title, p.content, p.publish_time,
                     u.username, u.user_id, s.section_name, s.section_id
        """, (post_id,))
        return cur.fetchone()


def list_with_stats(section_id=None, keyword=None, limit=20):
    """
    列出帖子(支持按板块过滤 + 关键字搜索,两者可同时使用)
    这里其实相当于把原来两个函数合并成一个
    """
    sql = """
        SELECT p.post_id, p.title, p.publish_time,
               u.username AS author,
               s.section_name, s.section_id,
               COUNT(pl.user_id) AS like_count
        FROM post p
        JOIN user u    ON p.user_id = u.user_id
        JOIN section s ON p.section_id = s.section_id
        LEFT JOIN post_like pl ON p.post_id = pl.post_id
    """
    conditions = []
    params = []

    if section_id is not None:
        conditions.append('p.section_id = %s')
        params.append(section_id)
    if keyword:
        conditions.append('(p.title LIKE %s OR p.content LIKE %s)')
        params.extend([f'%{keyword}%', f'%{keyword}%'])

    if conditions:
        sql += ' WHERE ' + ' AND '.join(conditions)

    sql += """
        GROUP BY p.post_id, p.title, p.publish_time,
                 u.username, s.section_name, s.section_id
        ORDER BY p.publish_time DESC
        LIMIT %s
    """
    params.append(limit)

    with get_cursor() as cur:
        cur.execute(sql, params)
        return cur.fetchall()
    

def is_liked_by(post_id, user_id):
    """判断用户是否点赞了某帖子"""
    with get_cursor() as cur:
        cur.execute(
            'SELECT 1 FROM post_like WHERE post_id = %s AND user_id = %s',
            (post_id, user_id)
        )
        return cur.fetchone() is not None


def add_like(post_id, user_id):
    """点赞,已点赞时静默忽略"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'INSERT IGNORE INTO post_like (post_id, user_id) VALUES (%s, %s)',
            (post_id, user_id)
        )


def remove_like(post_id, user_id):
    """取消点赞"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'DELETE FROM post_like WHERE post_id = %s AND user_id = %s',
            (post_id, user_id)
        )
        
def list_from_view(section_name=None):
    """通过视图查询帖子总览"""
    sql = 'SELECT * FROM v_post_overview'
    params = []
    if section_name:
        sql += ' WHERE section_name = %s'
        params.append(section_name)
    sql += ' ORDER BY like_count DESC'
    with get_cursor() as cur:
        cur.execute(sql, params)
        return cur.fetchall()
    
    
def delete_with_count(post_id):
    """
    删除帖子,并同步减少作者的发帖计数。
    两步操作放在一个事务里,保证 post_count 与真实帖子数一致。
    返回 True 表示删除成功,False 表示帖子不存在。
    """
    from ..db import get_connection
    with get_connection() as conn:
        try:
            with conn.cursor() as cur:
                # 先查出作者是谁(删除后就查不到了)
                cur.execute('SELECT user_id FROM post WHERE post_id = %s', (post_id,))
                row = cur.fetchone()
                if not row:
                    return False           # 帖子不存在
                author_id = row['user_id']

                # 删除帖子(评论/点赞/收藏由外键级联自动删除)
                cur.execute('DELETE FROM post WHERE post_id = %s', (post_id,))

                # 作者发帖数 -1
                cur.execute(
                    'UPDATE user SET post_count = post_count - 1 WHERE user_id = %s',
                    (author_id,)
                )
            conn.commit()                  # 两步都成功,一起提交
            return True
        except Exception:
            conn.rollback()                # 任一步失败,全部回滚
            raise