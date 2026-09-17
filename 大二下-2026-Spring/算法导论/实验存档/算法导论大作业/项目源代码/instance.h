#ifndef DRONE_TSPTW_INSTANCE_HPP
#define DRONE_TSPTW_INSTANCE_HPP

// 公共头文件
// 实例表示、I/O、距离矩阵、航线评估等

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace tsptw {

    constexpr double INF = std::numeric_limits<double>::infinity();
    constexpr double EPS = 1e-9; // 浮点比较容差

    //  一个 TSPTW 实例
    //  节点编号约定:0 = 起降站;1-k = k 位客户(订单)
    struct Instance {
        int k = 0;                 // 客户数
        double speed = 1.0;        // 航速
        double theta = 0.0;        // 每个客户点的卸餐服务时间
        double opt = INF;          // 标准答案(最优总完成时间);由生成器写入 # OPT,未知则为 INF
        std::vector<double> x, y;  // 坐标,下标 0-k(0 为站点)
        std::vector<double> L;     // 最晚送达时刻
        std::vector<std::vector<double>> t; // 飞行时间矩阵 t[i][j]

        int n() const { return k + 1; } // 节点总数(含站点)

        // 欧氏距离 / 航速 
        double dist(int i, int j) const {
            double dx = x[i] - x[j], dy = y[i] - y[j];
            return std::sqrt(dx * dx + dy * dy) / speed;
        }
        double tt(int i, int j) const { return t[i][j]; }

        // 预计算全部 t[i][j],各求解方法直接查表即可
        void build_matrix() {
            t.assign(n(), std::vector<double>(n(), 0.0));
            for (int i = 0; i < n(); ++i)
                for (int j = 0; j < n(); ++j)
                    t[i][j] = dist(i, j);
        }
    };

    //  从输入流读入实例
    //  文本格式(# 起注释,空白分隔):
    //     # OPT <value>          (可选:标准答案,由生成器写入)
    //     speed theta
    //     station_x station_y
    //     k
    //     id x y L               (重复 k 行,id 仅作标注,按读入顺序映射为 1..k)

    inline Instance load_instance(std::istream& in) {
        Instance inst;

        // 读取下一行有效内容
        auto next_line = [&](std::string& line) -> bool {
            while (std::getline(in, line)) {
                auto pos = line.find('#');
                if (pos != std::string::npos) {
                    std::istringstream cs(line.substr(pos + 1)); // 注释内容
                    std::string tag;
                    if (cs >> tag && tag == "OPT") { double v; if (cs >> v) inst.opt = v; }
                    line = line.substr(0, pos); // 去注释
                }
                if (line.find_first_not_of(" \t\r\n") != std::string::npos) return true;
            }
            return false;
            };

        std::string line;

        if (!next_line(line)) throw std::runtime_error("缺少 speed/theta 行");
        { std::istringstream ss(line); ss >> inst.speed >> inst.theta; }

        double sx, sy;
        if (!next_line(line)) throw std::runtime_error("缺少站点坐标行");
        { std::istringstream ss(line); ss >> sx >> sy; }

        if (!next_line(line)) throw std::runtime_error("缺少客户数 k");
        { std::istringstream ss(line); ss >> inst.k; }
        if (inst.k <= 0) throw std::runtime_error("客户数 k 必须为正");

        inst.x.assign(inst.n(), 0.0);
        inst.y.assign(inst.n(), 0.0);
        inst.L.assign(inst.n(), INF);
        inst.x[0] = sx; inst.y[0] = sy; // 站点坐标

        for (int c = 1; c <= inst.k; ++c) {
            if (!next_line(line)) throw std::runtime_error("客户数据行不足");
            std::istringstream ss(line);
            int id; double cx, cy, cl;
            if (!(ss >> id >> cx >> cy >> cl))
                throw std::runtime_error("客户数据行格式错误");
            inst.x[c] = cx; inst.y[c] = cy; inst.L[c] = cl;
        }

        inst.build_matrix();
        return inst;
    }

    // 从文件读入实例
    inline Instance load_instance(const std::string& path) {
        std::ifstream fin(path);
        if (!fin) throw std::runtime_error("无法打开实例文件: " + path);
        return load_instance(fin);
    }

    //  航线评估
    struct EvalResult {
        bool feasible = false;
        double total = INF; // 总完成时间
    };

    //  沿途递推到达时刻 A:到达第 m 个客户(0 起算)= 累计飞行时间 + m*theta
    //  可行即为每个客户 i 满足 A_i <= L_i
    inline EvalResult evaluate_route(const Instance& inst, const std::vector<int>& perm) {
        double flight = 0.0;   // 累计飞行时间
        double service = 0.0;  // 当前到达点之前已发生的服务时间总和(= m*theta)
        int prev = 0;          // 上一节点,从站点出发
        for (int m = 0; m < static_cast<int>(perm.size()); ++m) {
            int cur = perm[m];
            flight += inst.tt(prev, cur);
            double arrival = flight + service; // 到达 cur 的时刻
            if (arrival > inst.L[cur] + EPS) return EvalResult{}; // 违反时限则为不可行
            prev = cur;
            service += inst.theta; // 在 cur 卸餐,计入后续到达
        }
        flight += inst.tt(prev, 0); // 返回站点
        EvalResult r;
        r.feasible = true;
        r.total = flight + service; // service 此时 = k*theta
        return r;
    }

    //  统一输出结构
    struct SolveResult {
        bool feasible = false;      // 是否存在可行解
        double total = INF;         // 最优(或近似)总完成时间
        std::vector<int> route;     // 客户访问顺序
        long long evaluated = 0;    // 暴力:枚举排列数;DP:保留/扩展的状态数
        long long pruned = 0;       // 剪枝层:被消减/下界剪掉的状态数
        double incumbent0 = INF;    // 剪枝/近似层:启发式给出的初始 incumbent
    };

} // namespace tsptw
#endif // DRONE_TSPTW_INSTANCE_HPP
