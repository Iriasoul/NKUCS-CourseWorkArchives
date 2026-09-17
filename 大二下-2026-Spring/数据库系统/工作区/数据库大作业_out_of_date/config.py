# config.py
# 连接串分析：
# host: 数据库服务器地址 (127.0.0.1 表示本地机器)
# user: 登录数据库的用户名 (一般为 root)
# password: 登录密码
# database: 连接的具体数据库名称
# charset: 字符集，防止中文乱码
DB_CONFIG = {
    'host': '127.0.0.1',
    'user': 'root',
    'password': '123456hh',  # 请替换为你的真实密码
    'database': 'campus_blog',         # 假设你的数据库叫这个名字
    'charset': 'utf8mb4',
    'cursorclass': None 
}