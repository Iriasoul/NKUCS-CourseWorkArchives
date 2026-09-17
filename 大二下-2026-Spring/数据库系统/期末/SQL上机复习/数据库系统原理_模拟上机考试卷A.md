# “数据库系统原理”上机模拟考试题（A 卷）

> 出题人：Prof. Claudian 🎓
> 考试要求：每个题目的 SQL 语句都必须在查询分析器中调试，运行无误后记录查询的 SQL 语句和查询结果。
> **注意：查询结果集中的列名必须采用查询需求中给出的列名。**
> 数据库使用课程提供的 `lib` 库（book / borrow / category / major / student）。

数据库模式如下：

- 图书类别 category（catid 类别编号，catname 类别名，num 藏书数目）
- 图书 book（bookid 图书编号，bookname 书名，author 作者，price 价格，catid 类别编号）
- 学生 student（stuid 学号，stuname 姓名，degree 学生类别，majorid 专业编号）
- 借书情况 borrow（stuid 学号，bookid 图书编号，borrowdate 借书日期）
- 专业 major（majorid 专业编号，majorname 专业名称，department 学院名称）

注：SQL 语句不应该和具体的数据有关。

---

## 第 1 题

请从 student 表中统计每个专业的学生人数。

结果列名：**（majorid, num）**

## 第 2 题

请找出所有书本，其价格比全部书本的平均价格要**低**，并按价格**升序**排序。结果中需同时显示平均价格，平均价格保留一位小数。

结果列名：**（bookname, author, price, avgprice）**

## 第 3 题

请统计每位借过书的学生所借阅的**不同图书**的个数。

结果列名：**（stuid, num）**

## 第 4 题

给出属于“经济”类别的书籍的信息，按价格**降序**排列。

结果列名：**（bookid, bookname, author, price）**

## 第 5 题

给出属于“金融学院”的学生的学号和姓名。

结果列名：**（stuid, stuname）**

## 第 6 题

给出图书类别编号为 “c2”，且**最早一次被借**（借阅日期最小）的图书信息。

结果列名：**（bookid, bookname, author）**

## 第 7 题

给出所有被学号为 “1200910211” 的同学借阅过的图书信息。

结果列名：**（bookid, bookname, author）**

## 第 8 题

给出每位**研究生**所借书籍的最高价和最低价。

结果列名：**（stuname, highestprice, lowestprice）**

## 第 9 题

给出借阅过“数理科学”类别书籍的所有学生信息。

结果列名：**（stuid, stuname, degree, majorid）**

## 第 10 题

给出借阅了学号为 “200910121” 的同学所借**全部图书**的**其他**学生的信息（即该同学借过的每一本书，这些学生也都借过；结果中不包含 “200910121” 本人）。**必须使用 exists 或者 not exists 关键字。**

结果列名：**（stuid, stuname）**

---

完成后把每题的 SQL 语句发给 Prof. Claudian 对答案即可。祝考试顺利！💪
