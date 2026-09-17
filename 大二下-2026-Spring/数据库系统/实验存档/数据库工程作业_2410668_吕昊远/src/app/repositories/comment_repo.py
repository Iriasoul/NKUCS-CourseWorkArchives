"""Comment 表的数据访问层"""
from ..db import get_cursor


def list_by_post(post_id):
    """获取帖子下所有评论,包含作者信息,按时间升序"""
    with get_cursor() as cur:
        cur.execute("""
            SELECT c.comment_id, c.content, c.publish_time,
                   c.parent_comment_id,
                   u.username AS author, u.user_id AS author_id
            FROM comment c
            JOIN user u ON c.user_id = u.user_id
            WHERE c.post_id = %s
            ORDER BY c.publish_time ASC
        """, (post_id,))
        return cur.fetchall()


def create(content, user_id, post_id, parent_comment_id=None):
    """发评论或回复,返回新评论的 comment_id"""
    with get_cursor(commit=True) as cur:
        cur.execute(
            'INSERT INTO comment (content, user_id, post_id, parent_comment_id) '
            'VALUES (%s, %s, %s, %s)',
            (content, user_id, post_id, parent_comment_id)
        )
        return cur.lastrowid