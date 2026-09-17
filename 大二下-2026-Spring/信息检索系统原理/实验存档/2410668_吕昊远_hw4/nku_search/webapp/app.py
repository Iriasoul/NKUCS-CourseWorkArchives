"""
Flask 主应用 (Web 界面)
=======================
路由:
    /                   首页 (搜索框 + 推荐)
    /search             查询结果页 (统一入口, 自动判断查询类型)
    /search?type=phrase 强制短语查询
    /search?type=wildcard 强制通配查询
    /search?type=doc    强制文档查询
    /snapshot/<doc_id>  网页快照
    /download/<doc_id>  文档下载
    /history            查询日志页 (登录后)
    /login /logout /register /profile

    /api/suggest?q=     联想接口 (JSON)
    /api/recommend      推荐接口 (JSON)
"""
from __future__ import annotations

import os
import sys
import urllib.parse
from pathlib import Path
from indexer.es_indexer import get_es

from flask import (Flask, abort, jsonify, redirect, render_template, request,
                   send_file, url_for, flash)
from flask_login import (LoginManager, current_user, login_required,
                         login_user, logout_user)

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config
from search.search_service import (smart_search, site_search, doc_search,
                                   phrase_search, wildcard_search,
                                   log_query, get_user_logs,
                                   build_user_profile)
from search.recommend import suggest, recommend_for_user
from webapp.auth import (User, create_user, init_db, update_preferences,
                         verify_user)


# 站内查询的站点下拉选项: (域名值, 显示名); 空值=全部校内
SITE_OPTIONS = [
    ("", "全部校内"),
    ("news.nankai.edu.cn", "新闻网"),
    ("www.nankai.edu.cn", "主站"),
    ("cc.nankai.edu.cn", "计算机学院"),
    ("cyber.nankai.edu.cn", "网安学院"),
    ("math.nankai.edu.cn", "数学学院"),
    ("history.nankai.edu.cn", "历史学院"),
    ("lib.nankai.edu.cn", "图书馆"),
    ("graduate.nankai.edu.cn", "研究生院"),
]

def _lookup_url(doc_id):
    try:
        es = get_es()
        r = es.search(index=config.INDEX_PAGES,
                      query={"term": {"doc_id": doc_id}},
                      size=1, _source=["url"])
        hits = r.body.get("hits", {}).get("hits", [])
        return hits[0]["_source"].get("url") if hits else None
    except Exception:
        return None

def create_app() -> Flask:
    app = Flask(__name__, template_folder="templates", static_folder="static")
    app.config["SECRET_KEY"] = config.SECRET_KEY
    app.config["MAX_CONTENT_LENGTH"] = 64 * 1024 * 1024

    # Login manager
    login_manager = LoginManager(app)
    login_manager.login_view = "login"

    @login_manager.user_loader
    def load_user(user_id):
        return User.get(user_id)

    init_db()

    # 工具 
    def _user_profile():
        if current_user.is_authenticated:
            base = current_user.profile
            return build_user_profile(current_user.id, base)
        return None

    # 首页
    @app.route("/")
    def index():
        uid = current_user.id if current_user.is_authenticated else None
        recs = recommend_for_user(uid, size=8)
        return render_template("index.html", recommendations=recs,
                               site_options=SITE_OPTIONS)

    # 查询结果
    @app.route("/search")
    def search():
        q = request.args.get("q", "").strip()
        qtype = request.args.get("type", "auto")
        site = request.args.get("site") or None
        ext = request.args.get("ext") or None

        if not q:
            return redirect(url_for("index"))

        # 写日志 (登录用户才记)
        if current_user.is_authenticated:
            log_query(current_user.id, q)

        profile = _user_profile()
        size = 20

        # 分发
        if qtype == "doc":
            data = {"type": "doc", "hits": [], "docs":
                    doc_search(q, ext=ext, size=size)}
        elif qtype == "phrase":
            data = {"type": "phrase",
                    "hits": phrase_search(q, user_profile=profile, site=site,
                                          size=size),
                    "docs": []}
        elif qtype == "wildcard":
            data = {"type": "wildcard",
                    "hits": wildcard_search(q, user_profile=profile, site=site,
                                            size=size),
                    "docs": []}
        elif qtype == "site":
            data = {"type": "site",
                    "hits": site_search(q, user_profile=profile, site=site,
                                        size=size),
                    "docs": []}
        else:
            data = smart_search(q, user_profile=profile, size=size, site=site)

        return render_template("results.html", q=q, qtype=qtype, data=data,
                               site=site, ext=ext, site_options=SITE_OPTIONS)

    # 网页快照
    @app.route("/snapshot/<doc_id>")
    def snapshot(doc_id):
        path = os.path.join(config.SNAPSHOT_DIR, f"{doc_id}.html")
        # 懒快照: 没缓存就按 doc_id 查到原始 URL 现抓一份存下
        if not os.path.exists(path):
            url = _lookup_url(doc_id)
            if url:
                try:
                    import requests
                    r = requests.get(url, timeout=10,
                                     headers={"User-Agent": "Mozilla/5.0 NKU-SE"})
                    r.encoding = r.apparent_encoding or r.encoding or "utf-8"
                    with open(path, "w", encoding="utf-8") as f:
                        f.write(r.text)
                except Exception:
                    pass
        if not os.path.exists(path):
            return render_template("snapshot.html", doc_id=doc_id, exists=False)
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            html = f.read()
        import datetime
        archived_at = datetime.datetime.fromtimestamp(
            os.path.getmtime(path)).strftime("%Y-%m-%d %H:%M")
        return render_template("snapshot.html", doc_id=doc_id, exists=True,
                               snapshot_html=html, archived_at=archived_at)

    # 文档下载
    @app.route("/download/<doc_id>")
    def download(doc_id):
        # 简单扫描 docs 目录
        for fn in os.listdir(config.DOCS_DIR):
            if fn.startswith(doc_id + "."):
                full = os.path.join(config.DOCS_DIR, fn)
                return send_file(full, as_attachment=True, download_name=fn)
        abort(404)

    # 查询日志
    @app.route("/history")
    @login_required
    def history():
        logs = get_user_logs(current_user.id, size=50)
        return render_template("history.html", logs=logs)

    # 登录 / 注册
    @app.route("/login", methods=["GET", "POST"])
    def login():
        if request.method == "POST":
            u = request.form.get("username", "").strip()
            p = request.form.get("password", "")
            user = verify_user(u, p)
            if user:
                login_user(user)
                return redirect(url_for("index"))
            flash("用户名或密码错误", "error")
        return render_template("login.html")

    @app.route("/logout")
    @login_required
    def logout():
        logout_user()
        return redirect(url_for("index"))

    @app.route("/register", methods=["GET", "POST"])
    def register():
        if request.method == "POST":
            u = request.form.get("username", "").strip()
            p = request.form.get("password", "")
            college = request.form.get("college", "")
            role = request.form.get("role", "")
            prefer = request.form.getlist("prefer_sites")
            if not u or not p:
                flash("用户名和密码必填", "error")
            else:
                user = create_user(u, p, college=college, role=role,
                                   prefer_sites=prefer)
                if user:
                    login_user(user)
                    return redirect(url_for("index"))
                flash("用户名已存在", "error")
        sites = [
            "news.nankai.edu.cn", "cc.nankai.edu.cn",
            "cyber.nankai.edu.cn", "math.nankai.edu.cn",
            "history.nankai.edu.cn", "lib.nankai.edu.cn",
            "graduate.nankai.edu.cn", "www.nankai.edu.cn",
        ]
        return render_template("register.html", sites=sites,
                               colleges=list(config.COLLEGE_KEYWORDS.keys()),
                               roles=config.ROLE_CHOICES)

    @app.route("/profile", methods=["GET", "POST"])
    @login_required
    def profile():
        if request.method == "POST":
            sites = request.form.getlist("prefer_sites")
            terms_raw = request.form.get("prefer_terms", "")
            terms = [t.strip() for t in terms_raw.split(",") if t.strip()]
            college = request.form.get("college", "")
            role = request.form.get("role", "")
            update_preferences(current_user.id, prefer_sites=sites,
                               prefer_terms=terms, college=college, role=role)
            flash("已保存", "success")
            return redirect(url_for("profile"))
        sites_all = [
            "news.nankai.edu.cn", "cc.nankai.edu.cn",
            "cyber.nankai.edu.cn", "math.nankai.edu.cn",
            "history.nankai.edu.cn", "lib.nankai.edu.cn",
            "graduate.nankai.edu.cn", "www.nankai.edu.cn",
        ]
        return render_template("profile.html", sites_all=sites_all,
                               colleges=list(config.COLLEGE_KEYWORDS.keys()),
                               roles=config.ROLE_CHOICES)

    # API: 联想
    @app.route("/api/suggest")
    def api_suggest():
        q = request.args.get("q", "").strip()
        uid = current_user.id if current_user.is_authenticated else None
        return jsonify(suggest(q, size=8, user_id=uid))

    # API: 推荐
    @app.route("/api/recommend")
    def api_recommend():
        uid = current_user.id if current_user.is_authenticated else None
        return jsonify(recommend_for_user(uid, size=8))

    return app


# 入口
if __name__ == "__main__":
    app = create_app()
    app.run(host="0.0.0.0", port=5000, debug=True)
