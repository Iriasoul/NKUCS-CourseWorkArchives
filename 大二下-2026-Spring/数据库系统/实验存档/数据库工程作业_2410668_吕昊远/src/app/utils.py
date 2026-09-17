"""通用工具函数"""
from functools import wraps
from flask import session, redirect, url_for, flash


def login_required(f):
    """路由装饰器:未登录时跳转到登录页"""
    @wraps(f)
    def decorated(*args, **kwargs):
        if 'user_id' not in session:
            flash('请先登录', 'error')
            return redirect(url_for('auth.login'))
        return f(*args, **kwargs)
    return decorated