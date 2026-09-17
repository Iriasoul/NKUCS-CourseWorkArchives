#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif

#include "instance.h"
#include "brute_force.h"
#include "held_karp.h"
#include "held_karp_parallel.h"
#include "held_karp_pruned.h"
#include "held_karp_approx.h"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>
#include <functional>

using namespace tsptw;

int main(int argc, char** argv) {
    // 实验配置
    int strategy = 5;  // 1暴力 2DP 3DP并行 4DP剪枝 5近似
    int omp_threads = 4;  // 并行线程数
    int repeats = 5;  // 重复测量次数
    double eps = 0.10;  // 近似容差
    const char* default_file = "tests/inst_k16.txt"; // 默认实例文件


    std::string path = (argc >= 2) ? argv[1] : default_file;
    Instance inst;
    try {
        inst = load_instance(path);
    }
    catch (const std::exception& e) {
        std::cerr << "读取实例失败: " << e.what() << "\n";
        return 1;
    }

    // 选定策略
    std::function<SolveResult(const Instance&)> solver;
    std::string name;
    switch (strategy) {
    case 1: solver = brute_force; name = "暴力枚举"; break;
    case 2: solver = held_karp; name = "Held-Karp DP(串行)"; break;
    case 3: solver = held_karp_parallel; name = "Held-Karp DP(并行)"; break;
    case 4: solver = held_karp_pruned; name = "DP+剪枝"; break;
    case 5: solver = [eps](const Instance& i) { return held_karp_approx(i, eps); };
            name = "近似求解";             break;
    default:
        std::cerr << "未知编号: " << strategy << "(应为 1~5)\n";
        return 1;
    }

#ifdef _OPENMP
    if (strategy == 3) omp_set_num_threads(omp_threads);
#endif

    // 预热，再重复测量取均值
    SolveResult res = solver(inst);
    double sum_ms = 0.0;
    for (int it = 0; it < repeats; ++it) {
        auto t0 = std::chrono::high_resolution_clock::now();
        res = solver(inst);
        auto t1 = std::chrono::high_resolution_clock::now();
        sum_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    double avg_ms = sum_ms / repeats;

    // 精度比率
    // 返回解 / 标准答案 OPT
    double ratio;
    bool ratio_known = true;
    if (!res.feasible) {
        ratio = INF; ratio_known = false;
    }
    else if (inst.opt != INF) {
        ratio = res.total / inst.opt;
    }
    else if (strategy != 5) {
        ratio = 1.0; // 精确策略
    }
    else {
        ratio_known = false; // 近似但文件未含标准答案,无法度量
    }

    // 结果输出
    std::cout << std::fixed;
    std::cout << "=========== 实验结果 ===========\n";
    std::cout << "策略    : [" << strategy << "] " << name << "\n";
    std::cout << "参数    : 重复=" << repeats;
    if (strategy == 3) std::cout << ", OMP线程数=" << parallel_threads();
    if (strategy == 5) std::cout << ", eps=" << std::setprecision(3) << eps;
    std::cout << "\n";
    std::cout << "实例    : k=" << inst.k << "  文件=" << path;
    if (inst.opt != INF) std::cout << "  OPT=" << std::setprecision(4) << inst.opt;
    std::cout << "\n";
    if (res.feasible)
        std::cout << "本次解  : " << std::setprecision(4) << res.total << "\n";
    else
        std::cout << "本次解  : 无可行解\n";
    std::cout << "平均延迟: " << std::setprecision(4) << avg_ms << " ms\n";
    std::cout << "精度比率: ";
    if (ratio_known) std::cout << std::setprecision(6) << ratio
                               << "\n";
    else             std::cout << "N/A \n";
    std::cout << "====================================\n";
    return 0;
}
