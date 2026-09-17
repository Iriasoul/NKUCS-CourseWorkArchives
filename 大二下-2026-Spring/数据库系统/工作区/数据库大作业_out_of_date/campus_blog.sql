-- 1. 用户表 [cite: 53-63]
CREATE TABLE User (
    user_id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(100) NOT NULL,
    status TINYINT DEFAULT 1,
    user_type TINYINT NOT NULL,
    college VARCHAR(50)
);

-- 2. 板块表 [cite: 68-76]
CREATE TABLE Section (
    section_id INT PRIMARY KEY AUTO_INCREMENT,
    section_name VARCHAR(50) NOT NULL UNIQUE,
    description TEXT
);

-- 3. 帖子表 [cite: 80-96]
CREATE TABLE Post (
    post_id INT PRIMARY KEY AUTO_INCREMENT,
    title VARCHAR(100) NOT NULL,
    content TEXT NOT NULL,
    publish_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    user_id INT NOT NULL,
    section_id INT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES User (user_id),
    FOREIGN KEY (section_id) REFERENCES Section (section_id)
);

-- 4. 评论超类表 [cite: 101-114]
CREATE TABLE Comment (
    comment_id INT PRIMARY KEY AUTO_INCREMENT,
    content TEXT NOT NULL,
    publish_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    user_id INT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES User (user_id)
);

-- 5. 帖子评论子类 [cite: 119-127]
CREATE TABLE CommentOfPost (
    comment_id INT PRIMARY KEY,
    post_id INT NOT NULL,
    FOREIGN KEY (comment_id) REFERENCES Comment (comment_id),
    FOREIGN KEY (post_id) REFERENCES Post (post_id)
);

-- 6. 评论追加评论子类 [cite: 128-142]
CREATE TABLE CommentOfComment (
    comment_id INT PRIMARY KEY,
    parent_comment_id INT NOT NULL,
    FOREIGN KEY (comment_id) REFERENCES Comment (comment_id),
    FOREIGN KEY (parent_comment_id) REFERENCES Comment (comment_id)
);