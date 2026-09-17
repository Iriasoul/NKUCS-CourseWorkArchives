"""
用户系统 (注册 / 登录 / 偏好设置)
================================
- SQLite 存用户表 (user_id, username, pwd_hash, prefer_sites, prefer_terms)
- Flask-Login 管理 session
- 用户偏好用于 search_service 中的个性化排序
"""
from __future__ import annotations

import json
import os
import sqlite3
import sys
from contextlib import contextmanager
from typing import Optional

from flask_login import UserMixin
from werkzeug.security import generate_password_hash, check_password_hash

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import config


# DB
SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT UNIQUE NOT NULL,
    pwd_hash      TEXT NOT NULL,
    college       TEXT DEFAULT '',       -- 学院 (个性化关键词词库的 key)
    role          TEXT DEFAULT '',       -- 身份: undergraduate/graduate/teacher
    prefer_sites  TEXT DEFAULT '[]',     -- JSON list
    prefer_terms  TEXT DEFAULT '[]',     -- JSON list
    created_at    INTEGER DEFAULT (strftime('%s','now'))
);
"""


@contextmanager
def db_conn():
    conn = sqlite3.connect(config.DB_PATH)
    conn.row_factory = sqlite3.Row
    try:
        yield conn
        conn.commit()
    finally:
        conn.close()


def init_db():
    with db_conn() as c:
        c.executescript(SCHEMA)


# User
class User(UserMixin):
    def __init__(self, row: sqlite3.Row):
        self.id = str(row["id"])
        self.username = row["username"]
        self.college = row["college"] if "college" in row.keys() else ""
        self.role = row["role"] if "role" in row.keys() else ""
        try:
            self.prefer_sites = json.loads(row["prefer_sites"] or "[]")
        except Exception:
            self.prefer_sites = []
        try:
            self.prefer_terms = json.loads(row["prefer_terms"] or "[]")
        except Exception:
            self.prefer_terms = []

    @property
    def profile(self) -> dict:
        return {
            "college": self.college,
            "role": self.role,
            "prefer_sites": self.prefer_sites,
            "prefer_terms": self.prefer_terms,
        }

    @classmethod
    def get(cls, user_id) -> Optional["User"]:
        with db_conn() as c:
            row = c.execute("SELECT * FROM users WHERE id=?",
                            (user_id,)).fetchone()
            return cls(row) if row else None

    @classmethod
    def get_by_username(cls, username: str) -> Optional["User"]:
        with db_conn() as c:
            row = c.execute("SELECT * FROM users WHERE username=?",
                            (username,)).fetchone()
            return cls(row) if row else None


def create_user(username: str, password: str,
                college: str = "", role: str = "",
                prefer_sites: list[str] | None = None) -> Optional[User]:
    if User.get_by_username(username):
        return None
    pwd_hash = generate_password_hash(password)
    with db_conn() as c:
        c.execute(
            "INSERT INTO users(username, pwd_hash, college, role, prefer_sites) "
            "VALUES(?,?,?,?,?)",
            (username, pwd_hash, college, role, json.dumps(prefer_sites or []))
        )
    return User.get_by_username(username)


def verify_user(username: str, password: str) -> Optional[User]:
    with db_conn() as c:
        row = c.execute("SELECT * FROM users WHERE username=?",
                        (username,)).fetchone()
        if not row:
            return None
        if check_password_hash(row["pwd_hash"], password):
            return User(row)
    return None


def update_preferences(user_id: str | int,
                       prefer_sites: list[str] | None = None,
                       prefer_terms: list[str] | None = None,
                       college: str | None = None,
                       role: str | None = None) -> None:
    updates, values = [], []
    if prefer_sites is not None:
        updates.append("prefer_sites=?")
        values.append(json.dumps(prefer_sites))
    if prefer_terms is not None:
        updates.append("prefer_terms=?")
        values.append(json.dumps(prefer_terms))
    if college is not None:
        updates.append("college=?")
        values.append(college)
    if role is not None:
        updates.append("role=?")
        values.append(role)
    if not updates:
        return
    values.append(user_id)
    with db_conn() as c:
        c.execute(f"UPDATE users SET {', '.join(updates)} WHERE id=?", values)
