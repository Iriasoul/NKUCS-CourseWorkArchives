"""配置加载:从 .env 读取数据库连接信息"""
import os
import pymysql
from dotenv import load_dotenv

load_dotenv()


class Config:
    DB_HOST = os.getenv('DB_HOST', 'localhost')
    DB_PORT = int(os.getenv('DB_PORT', 3306))
    DB_USER = os.getenv('DB_USER')
    DB_PASSWORD = os.getenv('DB_PASSWORD')
    DB_NAME = os.getenv('DB_NAME')
    SECRET_KEY = os.getenv('SECRET_KEY', 'dev-secret-key-change-in-production')


    @classmethod
    def db_kwargs(cls):
        """返回 pymysql.connect() 需要的关键字参数字典"""
        return {
            'host': cls.DB_HOST,
            'port': cls.DB_PORT,
            'user': cls.DB_USER,
            'password': cls.DB_PASSWORD,
            'database': cls.DB_NAME,
            'charset': 'utf8mb4',
            'cursorclass': pymysql.cursors.DictCursor,
        }