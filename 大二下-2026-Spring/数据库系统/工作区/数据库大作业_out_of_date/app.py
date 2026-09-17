from flask import Flask, render_template, request, jsonify
import pymysql
from config import DB_CONFIG

app = Flask(__name__)

# 获取数据库连接的辅助函数
def get_db_connection():
    return pymysql.connect(**DB_CONFIG, cursorclass=pymysql.cursors.DictCursor)

@app.route('/')
def index():
    return render_template('index.html')

# ==========================================
# 操作1：含有事务应用的删除操作 (删除用户及其名下所有帖子)
# ==========================================
@app.route('/delete_user', methods=['POST'])
def delete_user_with_transaction():
    user_id = request.form.get('user_id')
    connection = get_db_connection()
    try:
        with connection.cursor() as cursor:
            # 开启事务是自动的，只需在最后 commit
            # 1. 先删除该用户发布的所有帖子 (涉及 Post 表)
            cursor.execute("DELETE FROM Post WHERE user_id = %s", (user_id,))
            # 2. 再删除该用户 (涉及 User 表)
            cursor.execute("DELETE FROM User WHERE user_id = %s", (user_id,))
        connection.commit() # 事务提交
        return jsonify({"status": "success", "message": "事务执行成功，用户及其帖子已删除。"})
    except Exception as e:
        connection.rollback() # 发生异常，事务回滚
        return jsonify({"status": "error", "message": f"操作失败，已回滚事务: {str(e)}"})
    finally:
        connection.close()

# ==========================================
# 操作2：触发器控制下的添加操作 (添加用户)
# ==========================================
@app.route('/insert_user', methods=['POST'])
def insert_user():
    username = request.form.get('username')
    password = request.form.get('password')
    user_type = request.form.get('user_type')
    
    connection = get_db_connection()
    try:
        with connection.cursor() as cursor:
            # 尝试插入，如果 user_type 不合规，数据库层的触发器会抛出异常
            sql = "INSERT INTO User (username, password, user_type) VALUES (%s, %s, %s)"
            cursor.execute(sql, (username, password, user_type))
        connection.commit()
        return jsonify({"status": "success", "message": "插入成功，未违背触发器。"})
    except Exception as e:
        return jsonify({"status": "error", "message": f"插入失败（触发器拦截）: {str(e)}"})
    finally:
        connection.close()

# ==========================================
# 操作3：存储过程控制下的更新操作 (更新帖子)
# ==========================================
@app.route('/update_post', methods=['POST'])
def update_post():
    post_id = request.form.get('post_id')
    new_content = request.form.get('content')
    
    connection = get_db_connection()
    try:
        with connection.cursor() as cursor:
            # 调用我们在 SQL 中建好的存储过程
            cursor.callproc('UpdatePostContent', (post_id, new_content))
        connection.commit()
        return jsonify({"status": "success", "message": "存储过程执行成功，帖子已更新。"})
    except Exception as e:
        return jsonify({"status": "error", "message": f"存储过程报错: {str(e)}"})
    finally:
        connection.close()

# ==========================================
# 操作4：含有视图的查询操作
# ==========================================
@app.route('/query_view', methods=['GET'])
def query_view():
    connection = get_db_connection()
    try:
        with connection.cursor() as cursor:
            # 直接查询建立好的视图
            cursor.execute("SELECT * FROM PostDetailsView LIMIT 10")
            results = cursor.fetchall()
        return jsonify({"status": "success", "data": results})
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})
    finally:
        connection.close()

if __name__ == '__main__':
    app.run(debug=True)