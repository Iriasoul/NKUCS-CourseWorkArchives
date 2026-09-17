// =============================================================
//  一次性测试样例生成器
//
//  生成一个 TSPTW 实例(可选空间分布与时间窗松紧),用精确 DP 算出标准
//  答案,并把实例与 # OPT 一起写入文件,供 main.cpp 读取与精度度量。
//
//  用法:  ./gen <输出文件> <k> <dist> <tight> <seed>
//      dist  : uniform | clustered | linear | spiral | sparse
//      tight : tight | loose
//  示例:  ./gen tests/inst_k16.txt 16 uniform loose 0
//  缺省:  无参数时生成 tests/inst_k12.txt, k=12, uniform, loose, seed 0。
// =============================================================

#include "instance.h"
#include "held_karp.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstdint>

using namespace tsptw;

static const double PI = 3.14159265358979323846;
static const double R = 100.0;   // 基准坐标半径
static const double SPEED = 10.0;
static const double THETA = 1.0;

// 按分布把客户坐标写入 inst.x[1..k], inst.y[1..k];站点固定 (0,0)。
static void make_points(Instance& inst, const std::string& dist, std::mt19937_64& rng) {
    const int k = inst.k;
    std::uniform_real_distribution<double> U(-R, R);
    std::normal_distribution<double> N01(0.0, 1.0);

    if (dist == "uniform") {
        for (int i = 1; i <= k; ++i) { inst.x[i] = U(rng); inst.y[i] = U(rng); }
    }
    else if (dist == "clustered") {
        int ncl = std::max(2, k / 4);
        std::vector<double> cx(ncl), cy(ncl);
        for (int c = 0; c < ncl; ++c) { cx[c] = U(rng); cy[c] = U(rng); }
        std::uniform_int_distribution<int> pick(0, ncl - 1);
        for (int i = 1; i <= k; ++i) {
            int c = pick(rng);
            inst.x[i] = cx[c] + 0.08 * R * N01(rng);
            inst.y[i] = cy[c] + 0.08 * R * N01(rng);
        }
    }
    else if (dist == "linear") {
        double dx = N01(rng), dy = N01(rng);
        double nrm = std::sqrt(dx * dx + dy * dy) + 1e-12; dx /= nrm; dy /= nrm;
        std::uniform_real_distribution<double> T(-1.0, 1.0);
        for (int i = 1; i <= k; ++i) {
            double tparam = T(rng);
            inst.x[i] = tparam * R * dx + 0.03 * R * N01(rng);
            inst.y[i] = tparam * R * dy + 0.03 * R * N01(rng);
        }
    }
    else if (dist == "spiral") {
        for (int i = 1; i <= k; ++i) {
            double ts = 0.3 + (4.0 * PI - 0.3) * (i - 1) / std::max(1, k - 1);
            double rad = R * ts / (4.0 * PI);
            inst.x[i] = rad * std::cos(ts) + 0.02 * R * N01(rng);
            inst.y[i] = rad * std::sin(ts) + 0.02 * R * N01(rng);
        }
    }
    else if (dist == "sparse") {
        std::uniform_real_distribution<double> US(-3.0 * R, 3.0 * R);
        for (int i = 1; i <= k; ++i) { inst.x[i] = US(rng); inst.y[i] = US(rng); }
    }
    else {
        throw std::runtime_error("未知分布: " + dist);
    }
}

// 种子航线法定时间窗:随机一条访问顺序,沿途算到达时刻,L = 到达 + U(0,W)。
// 保证种子航线本身可行(每个 L 不小于其到达时刻)。
static void make_deadlines(Instance& inst, double W, std::mt19937_64& rng) {
    const int k = inst.k;
    std::vector<int> perm(k);
    for (int i = 0; i < k; ++i) perm[i] = i + 1;
    std::shuffle(perm.begin(), perm.end(), rng);

    std::uniform_real_distribution<double> Uw(0.0, W);
    double flight = 0.0, service = 0.0;
    int prev = 0;
    for (int pos = 0; pos < k; ++pos) {
        int c = perm[pos];
        flight += inst.dist(prev, c);          // dist() 已含 /speed
        double arrival = flight + service;
        inst.L[c] = arrival + Uw(rng);
        prev = c; service += inst.theta;
    }
}

int main(int argc, char** argv) {
    std::string outfile = "tests/inst_k12.txt";
    int k = 12;
    std::string dist = "uniform";
    std::string tight = "loose";
    unsigned long long seed = 0;

    if (argc >= 2) outfile = argv[1];
    if (argc >= 3) k = std::stoi(argv[2]);
    if (argc >= 4) dist = argv[3];
    if (argc >= 5) tight = argv[4];
    if (argc >= 6) seed = std::stoull(argv[5]);

    double W = (tight == "tight") ? 4.0 : 120.0;

    Instance inst;
    inst.k = k; inst.speed = SPEED; inst.theta = THETA;
    inst.x.assign(k + 1, 0.0); inst.y.assign(k + 1, 0.0); inst.L.assign(k + 1, INF);
    inst.x[0] = 0.0; inst.y[0] = 0.0; // 站点

    // 用 (k,dist,tight,seed) 派生一个稳定的随机种子(统一为 uint32_t)
    std::seed_seq sq{ static_cast<std::uint32_t>(k),
                      static_cast<std::uint32_t>(std::hash<std::string>{}(dist)),
                      static_cast<std::uint32_t>(std::hash<std::string>{}(tight)),
                      static_cast<std::uint32_t>(seed) };
    std::mt19937_64 rng(sq);

    try {
        make_points(inst, dist, rng);
    }
    catch (const std::exception& e) {
        std::cerr << "生成失败: " << e.what() << "\n";
        return 1;
    }
    make_deadlines(inst, W, rng);
    inst.build_matrix();

    // 精确求解得到标准答案
    SolveResult opt = held_karp(inst);
    if (!opt.feasible) {
        std::cerr << "警告:生成的实例无可行解(时窗过紧),请调整参数后重试。\n";
        return 1;
    }

    // 写出文件(含 # OPT 标准答案)
    std::ofstream fout(outfile);
    if (!fout) { std::cerr << "无法写入: " << outfile << "\n"; return 1; }
    fout << std::fixed << std::setprecision(4);
    fout << "# generated k=" << k << " dist=" << dist
         << " tight=" << tight << " seed=" << seed << "\n";
    fout << "# OPT " << opt.total << "\n";
    fout << "# ROUTE 0";
    for (int c : opt.route) fout << " " << c;
    fout << " 0\n";
    fout << std::setprecision(6);
    fout << inst.speed << " " << inst.theta << "\n";
    fout << inst.x[0] << " " << inst.y[0] << "\n";
    fout << k << "\n";
    for (int i = 1; i <= k; ++i)
        fout << i << " " << inst.x[i] << " " << inst.y[i] << " " << inst.L[i] << "\n";
    fout.close();

    std::cerr << "已生成 " << outfile << "  (k=" << k << ", " << dist
              << ", " << tight << ", seed=" << seed << ", OPT="
              << std::setprecision(4) << opt.total << ")\n";
    return 0;
}
