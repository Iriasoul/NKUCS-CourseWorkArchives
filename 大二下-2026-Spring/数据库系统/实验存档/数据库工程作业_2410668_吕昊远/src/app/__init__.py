"""Flask 应用工厂"""
from flask import Flask
from .config import Config


def create_app():
    app = Flask(__name__)
    app.config.from_object(Config)

    # 注册蓝图(blueprint)
    from .routes.auth import auth_bp
    from .routes.post import post_bp

    app.register_blueprint(auth_bp)
    app.register_blueprint(post_bp)   # post_bp 没有 url_prefix,所以 / 就是首页
    
    from .routes.user import user_bp
    app.register_blueprint(user_bp)

    return app