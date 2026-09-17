"""帖子相关路由:列表、详情、发帖、评论"""
from flask import Blueprint, render_template, request, redirect, url_for, flash, session
from ..repositories import post_repo, section_repo, comment_repo
from ..utils import login_required
from ..repositories import favorite_repo  


post_bp = Blueprint('post', __name__)


@post_bp.route('/')
def index():
    """首页:帖子列表,支持板块过滤和关键字搜索"""
    section_id = request.args.get('section_id', type=int)  # URL 参数 ?section_id=1
    keyword    = request.args.get('keyword', '').strip()

    posts    = post_repo.list_with_stats(section_id=section_id, keyword=keyword or None)
    sections = section_repo.list_all()

    return render_template('index.html',
                           posts=posts,
                           sections=sections,
                           current_section_id=section_id,
                           keyword=keyword)
    
    


@post_bp.route('/posts/<int:post_id>/comment', methods=['POST'])
@login_required
def add_comment(post_id):
    import pymysql
    content = request.form.get('content', '').strip()
    parent_comment_id = request.form.get('parent_comment_id', type=int)

    if not content:
        flash('评论内容不能为空', 'error')
    elif len(content) > 1000:
        flash('评论不能超过 1000 字', 'error')
    else:
        try:
            comment_repo.create(
                content=content,
                user_id=session['user_id'],
                post_id=post_id,
                parent_comment_id=parent_comment_id
            )
        except pymysql.err.OperationalError as e:
            # 捕获触发器抛出的 SIGNAL 异常
            # e.args = (1644, '回复的评论必须属于同一帖子')
            flash(e.args[1] if len(e.args) > 1 else '评论失败', 'error')

    return redirect(url_for('post.detail', post_id=post_id))


@post_bp.route('/posts/create', methods=['GET', 'POST'])
@login_required
def create():
    """发帖页"""
    sections = section_repo.list_all()

    if request.method == 'POST':
        title      = request.form.get('title', '').strip()
        content    = request.form.get('content', '').strip()
        section_id = request.form.get('section_id', type=int)

        errors = []
        if not title or len(title) < 2:
            errors.append('标题至少 2 个字符')
        if len(title) > 100:
            errors.append('标题不能超过 100 字符')
        if not content:
            errors.append('内容不能为空')
        if not section_id:
            errors.append('请选择板块')

        if errors:
            for e in errors:
                flash(e, 'error')
            return render_template('post/create.html', sections=sections,
                                   title=title, content=content)

        post_id = post_repo.create(title, content, session['user_id'], section_id)
        flash('发帖成功', 'success')
        return redirect(url_for('post.detail', post_id=post_id))

    return render_template('post/create.html', sections=sections)


@post_bp.route('/posts/<int:post_id>/like', methods=['POST'])
@login_required
def toggle_like(post_id):
    """切换点赞状态"""
    post = post_repo.find_by_id(post_id)
    if not post:
        flash('帖子不存在', 'error')
        return redirect(url_for('post.index'))

    user_id = session['user_id']
    if post_repo.is_liked_by(post_id, user_id):
        post_repo.remove_like(post_id, user_id)
    else:
        post_repo.add_like(post_id, user_id)

    return redirect(url_for('post.detail', post_id=post_id))


@post_bp.route('/posts/<int:post_id>')
def detail(post_id):
    post = post_repo.find_by_id_with_detail(post_id)
    if not post:
        flash('帖子不存在', 'error')
        return redirect(url_for('post.index'))

    all_comments = comment_repo.list_by_post(post_id)
    top_comments = [c for c in all_comments if c['parent_comment_id'] is None]
    replies = {}
    for c in all_comments:
        pid = c['parent_comment_id']
        if pid is not None:
            replies.setdefault(pid, []).append(c)

    # 判断当前用户是否已点赞
    is_liked = False
    # 这篇帖子已被收藏进的 folder_id 集合
    favorited_folder_ids = set()      
    folders = []
    if session.get('user_id'):
        uid = session['user_id']
        is_liked = post_repo.is_liked_by(post_id, uid)
        folders = favorite_repo.get_folders_by_user(uid)
        favorited_folder_ids = favorite_repo.get_folder_ids_containing_post(post_id, uid)

    # 整体是否已收藏(任意一个夹里有就算)
    is_favorited = len(favorited_folder_ids) > 0

    return render_template('post/detail.html',
                           post=post,
                           top_comments=top_comments,
                           replies=replies,
                           is_liked=is_liked,
                           is_favorited=is_favorited,
                           favorited_folder_ids=favorited_folder_ids,   # 新增
                           folders=folders)
    

@post_bp.route('/posts/<int:post_id>/delete', methods=['POST'])
@login_required
def delete(post_id):
    """删除帖子(仅作者本人)"""
    post = post_repo.find_by_id(post_id)
    if not post:
        flash('帖子不存在', 'error')
        return redirect(url_for('post.index'))

    # 权限校验:只有作者能删
    if post['user_id'] != session['user_id']:
        flash('你只能删除自己的帖子', 'error')
        return redirect(url_for('post.detail', post_id=post_id))

    post_repo.delete_with_count(post_id)
    flash('帖子已删除', 'success')
    return redirect(url_for('post.index'))