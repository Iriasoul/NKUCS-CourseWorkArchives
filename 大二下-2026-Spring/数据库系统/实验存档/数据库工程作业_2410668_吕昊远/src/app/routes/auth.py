"""认证相关路由:注册、登录、登出"""
from flask import Blueprint, render_template, request, redirect, url_for, flash, session
from werkzeug.security import generate_password_hash, check_password_hash
from ..repositories import user_repo

auth_bp = Blueprint('auth', __name__, url_prefix='/auth')


@auth_bp.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        password = request.form.get('password', '')
        user_type = request.form.get('user_type', type=int)
        college = request.form.get('college', '').strip() or None

        # 服务端校验
        errors = []
        if not username or len(username) < 3 or len(username) > 50:
            errors.append('用户名长度必须在 3-50 字符之间')
        if not password or len(password) < 6:
            errors.append('密码至少 6 位')
        if user_type not in (1, 2):
            errors.append('请选择用户类型')
        if user_repo.exists_by_username(username):
            errors.append('用户名已被占用')

        if errors:
            for e in errors:
                flash(e, 'error')
            return render_template('auth/register.html',
                                   username=username, college=college)

        # 写入数据库
        password_hash = generate_password_hash(password)
        user_repo.create(username, password_hash, user_type, college)

        flash('注册成功,请登录', 'success')
        return redirect(url_for('auth.login'))

    # GET 请求:显示注册页
    return render_template('auth/register.html')


@auth_bp.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username', '').strip()
        password = request.form.get('password', '')

        user = user_repo.find_by_username(username)

        # 无论是用户不存在还是密码错误,统一返回同样的错误信息
        # 工程上一般出于安全考虑，防止攻击者通过错误信息差异判断用户名是否存在
        if not user or not check_password_hash(user['password_hash'], password):
            flash('用户名或密码错误', 'error')
            return render_template('auth/login.html', username=username)

        if user['status'] != 1:
            flash('账号已被禁用', 'error')
            return render_template('auth/login.html', username=username)

        # 登录成功:把关键信息写入 session
        session['user_id'] = user['user_id']
        session['username'] = user['username']
        session['user_type'] = user['user_type']

        flash(f'欢迎回来,{user["username"]}', 'success')
        return redirect(url_for('post.index'))

    return render_template('auth/login.html')


@auth_bp.route('/logout')
def logout():
    session.clear()
    flash('已登出', 'success')
    return redirect(url_for('post.index'))