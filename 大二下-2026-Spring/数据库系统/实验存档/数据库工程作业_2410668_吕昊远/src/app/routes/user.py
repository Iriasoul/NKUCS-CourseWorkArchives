"""用户相关路由:个人主页、收藏管理"""
from flask import Blueprint, render_template, request, redirect, url_for, flash, session
import pymysql
from ..repositories import favorite_repo
from ..utils import login_required

user_bp = Blueprint('user', __name__, url_prefix='/user')


@user_bp.route('/profile')
@login_required
def profile():
    """个人主页:展示自己的收藏夹"""
    folders = favorite_repo.get_folders_by_user(session['user_id'])
    return render_template('user/profile.html', folders=folders)


@user_bp.route('/folders/create', methods=['POST'])
@login_required
def create_folder():
    """新建收藏夹"""
    folder_name = request.form.get('folder_name', '').strip()
    remark = request.form.get('remark', '').strip() or None

    if not folder_name:
        flash('收藏夹名称不能为空', 'error')
        return redirect(url_for('user.profile'))

    try:
        favorite_repo.create_folder(session['user_id'], folder_name, remark)
        flash(f'收藏夹「{folder_name}」创建成功', 'success')
    except pymysql.IntegrityError:
        # unique key 冲突:同名收藏夹已存在
        flash('已有同名收藏夹', 'error')

    return redirect(url_for('user.profile'))


@user_bp.route('/folders/<int:folder_id>')
@login_required
def folder_detail(folder_id):
    """收藏夹详情:该收藏夹里所有帖子"""
    folder = favorite_repo.get_folder(folder_id)
    if not folder or folder['user_id'] != session['user_id']:
        flash('收藏夹不存在或无权访问', 'error')
        return redirect(url_for('user.profile'))
    
    posts = favorite_repo.get_posts_in_folder(folder_id)
    return render_template('user/folder_detail.html',
                           posts=posts, folder_id=folder_id)


@user_bp.route('/favorite', methods=['POST'])
@login_required
def toggle_favorite():
    """
    将帖子加入/移出收藏夹。
    表单需要传 folder_id 和 post_id。
    """
    folder_id = request.form.get('folder_id', type=int)
    post_id   = request.form.get('post_id',   type=int)
    
    folder = favorite_repo.get_folder(folder_id)
    if not folder or folder['user_id'] != session['user_id']:
        flash('无权操作该收藏夹', 'error')
        return redirect(url_for('post.detail', post_id=post_id))

    if not folder_id or not post_id:
        flash('参数错误', 'error')
        return redirect(url_for('post.index'))

    if favorite_repo.is_post_in_folder(folder_id, post_id):
        favorite_repo.remove_post_from_folder(folder_id, post_id)
        flash('已取消收藏', 'success')
    else:
        favorite_repo.add_post_to_folder(folder_id, post_id)
        flash('收藏成功', 'success')

    return redirect(url_for('post.detail', post_id=post_id))