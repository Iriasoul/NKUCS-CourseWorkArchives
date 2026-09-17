"""
终端管理脚本:集中执行数据库管理操作。
运行: python admin.py
"""
from app.repositories import post_repo, user_repo, section_repo


def pause():
    input('\n按回车继续...')


def view_overview():
    """视图查询"""
    print('\n=== 帖子总览 ===')
    rows = post_repo.list_from_view()
    print(f"{'ID':<4}{'标题':<24}{'作者':<12}{'板块':<12}{'赞':<5}{'评论':<5}")
    print('-' * 65)
    for r in rows:
        print(f"{r['post_id']:<4}{r['title'][:22]:<24}{r['author']:<12}"
              f"{r['section_name']:<12}{r['like_count']:<5}{r['comment_count']:<5}")


def list_posts():
    """列出所有帖子,方便选 ID"""
    rows = post_repo.list_with_stats(limit=100)
    print(f"\n{'ID':<4}{'标题':<28}{'作者':<12}")
    print('-' * 48)
    for r in rows:
        print(f"{r['post_id']:<4}{r['title'][:26]:<28}{r['author']:<12}")


def delete_post():
    """事务删除帖子"""
    list_posts()
    pid = input('\n输入要删除的帖子 ID: ').strip()
    if not pid.isdigit():
        print('无效输入')
        return

    post = post_repo.find_by_id(int(pid))
    if not post:
        print('帖子不存在')
        return

    # 删除前查询
    # author = user_repo.find_by_id(post['user_id'])
    # print(f"\n删除前: 作者 {author['username']} 的 post_count = {author['post_count']}")

    ok = post_repo.delete_with_count(int(pid))
    if ok:
        # author_after = user_repo.find_by_id(post['user_id'])
        # print(f"删除后: 作者 {author['username']} 的 post_count = {author_after['post_count']}")
        print('帖子已删除')
    else:
        print('删除失败')


def list_users():
    """列出所有用户"""
    rows = user_repo.list_all_brief()
    print(f"\n{'ID':<4}{'用户名':<16}{'类型':<8}{'状态':<8}{'发帖数':<6}")
    print('-' * 44)
    for r in rows:
        utype = '学生' if r['user_type'] == 1 else '老师'
        status = '正常' if r['status'] == 1 else '已注销'
        print(f"{r['user_id']:<4}{r['username']:<16}{utype:<8}{status:<8}{r['post_count']:<6}")


def deactivate_user():
    """存储过程注销用户"""
    list_users()
    uid = input('\n输入要注销的用户 ID: ').strip()
    rid = user_repo.get_system_account_id()
    if not uid.isdigit():
        print('无效输入')
        return

    try:
        user_repo.deactivate_user(int(uid), int(rid))
        print('注销成功,帖子已转移')
        list_users()
    except Exception as e:
        print(f'! 操作被拒绝: {e}')


MENU = """
========== 校园论坛 · 数据库管理终端 ==========
  1. 查看帖子总览        
  2. 删除帖子            
  3. 注销用户            
  4. 查看所有用户
  0. 退出
==============================================="""


def main():
    actions = {
        '1': view_overview,
        '2': delete_post,
        '3': deactivate_user,
        '4': list_users,
    }
    while True:
        print(MENU)
        choice = input('请选择操作: ').strip()
        if choice == '0':
            print('再见')
            break
        action = actions.get(choice)
        if action:
            action()
            pause()
        else:
            print('无效选项')


if __name__ == '__main__':
    main()