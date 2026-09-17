#### 登录MySQL：
    mysql -u root -p --default-character-set=utf8mb4
**（这里要指定编码）**


#### 建库：
    DROP DATABASE IF EXISTS campus_forum;
    CREATE DATABASE campus_forum CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    USE campus_forum;
    SET NAMES utf8mb4;
    SOURCE ./database.sql;