// =============================================================
//  批量测评驱动(机器可读)
//  用法:  ./bench <实例文件> [reps] [threads] [eps_list] [brute_max_k]
//      reps        每个计时点重复次数(取 min 与 mean),默认 5
//      threads     并行线程数(strategy=3),默认 4
//      eps_list    近似容差列表,逗号分隔,默认 0,0.02,0.05,0.10,0.20
//      brute_max_k 暴力仅在 k<=此值时运行,默认 12
//
//  输出(stdout,CSV,无表头):
//      method,eps,cost,ratio,evaluated,pruned,ms_min,ms_mean,feasible,k
//  其中 method ∈ {brute, dp, dp_par, dp_prune, approx};
//       eps 仅 approx 有意义(其余为 0);ratio = cost/OPT(精确=1)。
// =============================================================

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
#include <string>
#include <vector>
#include <sstream>
#include <functional>

using namespace tsptw;

struct Timing { double ms_min; double ms_mean; SolveResult res; };

static Timing run_timed(const std::function<SolveResult(const Instance&)>& f,
                        const Instance& inst, int reps) {
    SolveResult r = f(inst); // 预热
    double mn = INF, sum = 0.0;
    for (int i = 0; i < reps; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        r = f(inst);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < mn) mn = ms;
        sum += ms;
    }
    return Timing{ mn, sum / reps, r };
}

static void emit(const std::string& method, double eps, const Timing& tm,
                 double opt, int k) {
    const SolveResult& r = tm.res;
    double ratio = (r.feasible && opt != INF) ? r.total / opt : -1.0;
    std::cout << std::fixed << std::setprecision(6)
              << method << "," << eps << ","
              << (r.feasible ? r.total : -1.0) << "," << ratio << ","
              << r.evaluated << "," << r.pruned << ","
              << tm.ms_min << "," << tm.ms_mean << ","
              << (r.feasible ? 1 : 0) << "," << k << "\n";
}

static std::vector<double> parse_eps(const std::string& s) {
    std::vector<double> v; std::stringstream ss(s); std::string tok;
    while (std::getline(ss, tok, ',')) if (!tok.empty()) v.push_back(std::stod(tok));
    return v;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "用法: " << argv[0]
        << " <实例文件> [reps] [threads] [eps_list] [brute_max_k]\n"; return 1; }
    std::string path = argv[1];
    int reps        = (argc >= 3) ? std::stoi(argv[2]) : 5;
    int threads     = (argc >= 4) ? std::stoi(argv[3]) : 4;
    std::string epl = (argc >= 5) ? argv[4] : "0,0.02,0.05,0.10,0.20";
    int brute_max_k = (argc >= 6) ? std::stoi(argv[5]) : 12;
    std::vector<double> epss = parse_eps(epl);

    Instance inst;
    try { inst = load_instance(path); }
    catch (const std::exception& e) { std::cerr << "读取失败: " << e.what() << "\n"; return 1; }

    const double opt = inst.opt;
    const int k = inst.k;

#ifdef _OPENMP
    omp_set_num_threads(threads);
#endif

    if (k <= brute_max_k)
        emit("brute", 0.0, run_timed(brute_force, inst, reps), opt, k);
    emit("dp",       0.0, run_timed(held_karp,          inst, reps), opt, k);
    emit("dp_par",   0.0, run_timed(held_karp_parallel, inst, reps), opt, k);
    emit("dp_prune", 0.0, run_timed(held_karp_pruned,   inst, reps), opt, k);
    for (double e : epss)
        emit("approx", e, run_timed([e](const Instance& i){ return held_karp_approx(i, e); },
                                    inst, reps), opt, k);
    return 0;
}
