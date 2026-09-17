--  校园论坛系统

DROP PROCEDURE IF EXISTS sp_deactivate_user;
DROP TRIGGER   IF EXISTS trg_comment_same_post;
DROP VIEW      IF EXISTS v_post_overview;

DROP TABLE IF EXISTS favorite_post;
DROP TABLE IF EXISTS favorite_folder;
DROP TABLE IF EXISTS post_like;
DROP TABLE IF EXISTS comment;
DROP TABLE IF EXISTS post;
DROP TABLE IF EXISTS section;
DROP TABLE IF EXISTS user;


-- 建表

-- 1. 用户表
CREATE TABLE user (
    user_id       INT AUTO_INCREMENT PRIMARY KEY,
    username      VARCHAR(50)  NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    status        TINYINT      NOT NULL DEFAULT 1 COMMENT '1=正常, 0=已注销/禁用',
    user_type     TINYINT      NOT NULL COMMENT '1=学生, 2=老师',
    college       VARCHAR(50)  NULL,
    post_count    INT          NOT NULL DEFAULT 0 COMMENT '发帖数(冗余字段,需与post表保持一致)',
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2 板块表
CREATE TABLE section (
    section_id   INT AUTO_INCREMENT PRIMARY KEY,
    section_name VARCHAR(50)  NOT NULL UNIQUE,
    description  VARCHAR(255) NULL,
    created_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3 帖子表
CREATE TABLE post (
    post_id      INT AUTO_INCREMENT PRIMARY KEY,
    title        VARCHAR(100) NOT NULL,
    content      TEXT         NOT NULL,
    publish_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    user_id      INT          NOT NULL,
    section_id   INT          NOT NULL,
    FOREIGN KEY (user_id)    REFERENCES user(user_id)       ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY (section_id) REFERENCES section(section_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    INDEX idx_section      (section_id),
    INDEX idx_user         (user_id),
    INDEX idx_publish_time (publish_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 4 评论表(单表, post_id冗余存储, parent_comment_id自引用实现层级)
CREATE TABLE comment (
    comment_id        INT AUTO_INCREMENT PRIMARY KEY,
    content           TEXT     NOT NULL,
    publish_time      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    user_id           INT      NOT NULL,
    post_id           INT      NOT NULL COMMENT '所属帖子,即使是回复的评论也存帖子id,方便查询',
    parent_comment_id INT      NULL     COMMENT 'NULL表示直接评论帖子, 非空表示回复某条评论',
    FOREIGN KEY (user_id)           REFERENCES user(user_id)       ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY (post_id)           REFERENCES post(post_id)       ON DELETE CASCADE  ON UPDATE CASCADE,
    FOREIGN KEY (parent_comment_id) REFERENCES comment(comment_id) ON DELETE CASCADE  ON UPDATE CASCADE,
    INDEX idx_post   (post_id),
    INDEX idx_parent (parent_comment_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 5 点赞表(联合主键保证每人每帖只能赞一次)
CREATE TABLE post_like (
    user_id   INT      NOT NULL,
    post_id   INT      NOT NULL,
    like_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, post_id),
    FOREIGN KEY (user_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    FOREIGN KEY (post_id) REFERENCES post(post_id) ON DELETE CASCADE  ON UPDATE CASCADE,
    INDEX idx_post (post_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 6 收藏夹表
CREATE TABLE favorite_folder (
    folder_id   INT AUTO_INCREMENT PRIMARY KEY,
    user_id     INT          NOT NULL,
    folder_name VARCHAR(50)  NOT NULL,
    remark      VARCHAR(100) NULL,
    create_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(user_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    UNIQUE KEY uk_user_folder (user_id, folder_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 7 帖子-收藏夹关系表
CREATE TABLE favorite_post (
    folder_id     INT      NOT NULL,
    post_id       INT      NOT NULL,
    favorite_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (folder_id, post_id),
    FOREIGN KEY (folder_id) REFERENCES favorite_folder(folder_id) ON DELETE CASCADE ON UPDATE CASCADE,
    FOREIGN KEY (post_id)   REFERENCES post(post_id)              ON DELETE CASCADE ON UPDATE CASCADE,
    INDEX idx_post (post_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


-- 这里有一些基础的测试数据

-- 用户密码占位,无法用于真实登录
INSERT INTO user (username, password_hash, user_type, college) VALUES
('alice',   'placeholder_hash', 1, '计算机学院'),
('bob',     'placeholder_hash', 1, '计算机学院'),
('阿多', 'placeholder_hash', 1, '数学学院'),
('小Q', 'placeholder_hash', 2, '计算机学院');

-- 系统账号: 接收被注销用户转移过来的帖子
INSERT INTO user (username, password_hash, user_type, status, college) VALUES
('已注销用户', 'no_login', 1, 1, NULL);

-- 板块
INSERT INTO section (section_name, description) VALUES
('学习交流', '课程学习、作业讨论、考试答疑'),
('校园生活', '食堂、宿舍、社团活动'),
('失物招领', '丢东西、捡东西都来这里'),
('跳蚤市场', '二手物品交易');

-- 帖子
INSERT INTO post (title, content, user_id, section_id) VALUES
('数据库期末怎么复习？',   '求各位大佬支招,SQL 语句记不住啊', 1, 1),
('三食堂二楼麻辣香锅推荐', '今天发现的宝藏窗口,强烈推荐!',     2, 2),
('不小心丢了172张校园卡',         '在图书馆 3 楼打水间丢的,有捡到的麻烦联系', 3, 3),
('出六手自行车',           '九成九新,毕业了带不走',               1, 4),
('关于第三章的几个疑问',   '同学们,这章重点请看这几个习题',     4, 1);

-- 评论(对帖子的直接评论)
INSERT INTO comment (content, user_id, post_id, parent_comment_id) VALUES
('多刷历年题,看课件',    2, 1, NULL),
('我也想知道!',          3, 1, NULL),
('窗口在哪?我们不记得有这家?',            1, 2, NULL);

-- 评论(回复评论)
INSERT INTO comment (content, user_id, post_id, parent_comment_id) VALUES
('谢谢!历年题在哪找?',    1, 1, 1),
('二楼窗户外面那家',        2, 2, 3);

-- 点赞
INSERT INTO post_like (user_id, post_id) VALUES
(1, 2), (1, 5),
(2, 1), (2, 5),
(3, 1), (3, 2),
(4, 1);

-- 收藏夹
INSERT INTO favorite_folder (user_id, folder_name, remark) VALUES
(1, '默认收藏夹', '随手收藏'),
(1, '学习资料',   '复习用'),
(2, '默认收藏夹', NULL);

-- 收藏帖子
INSERT INTO favorite_post (folder_id, post_id) VALUES
(1, 2),
(2, 1),
(2, 5),
(3, 1);

-- 发帖计数(让 user.post_count 与 post 表真实行数一致)
UPDATE user u
SET post_count = (
    SELECT COUNT(*) FROM post p WHERE p.user_id = u.user_id
);


-- 视图

CREATE VIEW v_post_overview AS
SELECT
    p.post_id,
    p.title,
    p.publish_time,
    u.user_id   AS author_id,
    u.username  AS author,
    s.section_id,
    s.section_name,
    COUNT(DISTINCT pl.user_id)    AS like_count,
    COUNT(DISTINCT c.comment_id)  AS comment_count
FROM post p
JOIN user u    ON p.user_id = u.user_id
JOIN section s ON p.section_id = s.section_id
LEFT JOIN post_like pl ON p.post_id = pl.post_id
LEFT JOIN comment   c  ON p.post_id = c.post_id
GROUP BY p.post_id, p.title, p.publish_time,
         u.user_id, u.username, s.section_id, s.section_name;


-- 触发器

-- 插入评论前校验: 回复的父评论必须与本评论属于同一帖子
DELIMITER //

CREATE TRIGGER trg_comment_same_post
BEFORE INSERT ON comment
FOR EACH ROW
BEGIN
    DECLARE parent_post_id INT;

    IF NEW.parent_comment_id IS NOT NULL THEN
        SELECT post_id INTO parent_post_id
        FROM comment
        WHERE comment_id = NEW.parent_comment_id;

        IF parent_post_id <> NEW.post_id THEN
            SIGNAL SQLSTATE '45000'
                SET MESSAGE_TEXT = '回复的评论必须属于同一帖子';
        END IF;
    END IF;
END //

DELIMITER ;


-- 存储过程

-- 注销用户: 帖子转移给系统账号 + 维护双方计数 + 用户状态改为注销

DELIMITER //

CREATE PROCEDURE sp_deactivate_user(
    IN p_user_id      INT,
    IN p_recipient_id INT
)
BEGIN
    -- 所有 DECLARE 必须置于 BEGIN 块最前
    DECLARE v_post_count INT DEFAULT 0;
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    IF p_user_id = p_recipient_id THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = '不能将帖子转移给被注销用户自己';
    END IF;

    START TRANSACTION;

    SELECT COUNT(*) INTO v_post_count FROM post WHERE user_id = p_user_id;

    UPDATE post SET user_id = p_recipient_id WHERE user_id = p_user_id;

    UPDATE user SET post_count = post_count + v_post_count
        WHERE user_id = p_recipient_id;

    UPDATE user SET post_count = 0, status = 0
        WHERE user_id = p_user_id;

    COMMIT;
END //

DELIMITER ;

