#ifndef DRONE_TSPTW_HELD_KARP_PARALLEL_HPP
#define DRONE_TSPTW_HELD_KARP_PARALLEL_HPP

//  并行 DP

#include "instance.h"
#include <vector>
#include <algorithm>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace tsptw {

    // 当前可用并行线程数(未启用 OpenMP 时为 1)
    inline int parallel_threads() {
#ifdef _OPENMP
        return omp_get_max_threads();
#else
        return 1;
#endif
    }

    // 可移植 popcount
    inline int popcount_int(unsigned x) {
        int c = 0;
        while (x) { x &= (x - 1); ++c; }
        return c;
    }

    inline SolveResult held_karp_parallel(const Instance& inst) {
        SolveResult res;
        const int K = inst.k;
        const int FULL = (1 << K) - 1;
        const double th = inst.theta;

        std::vector<double> dp(static_cast<size_t>(1 << K) * K, INF);
        auto IDX = [K](int mask, int j) -> size_t {
            return static_cast<size_t>(mask) * K + (j - 1);
            };

        // 按 |S| 分掩码
        std::vector<std::vector<int>> layer(K + 1);
        for (int mask = 1; mask <= FULL; ++mask)
            layer[popcount_int(static_cast<unsigned>(mask))].push_back(mask);

        // 第 1 层:从站点直达单个客户
        for (int j = 1; j <= K; ++j) {
            double a = inst.tt(0, j);
            if (a <= inst.L[j] + EPS) dp[IDX(1 << (j - 1), j)] = a;
        }

        // 第 2-K 层:同层各掩码相互独立,并行计算
        for (int s = 2; s <= K; ++s) {
            const std::vector<int>& masks = layer[s];
            const int M = static_cast<int>(masks.size());

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
            for (int idx = 0; idx < M; ++idx) { 
                int mask = masks[idx];
                for (int j = 1; j <= K; ++j) {
                    if (!(mask & (1 << (j - 1)))) continue;
                    int pmask = mask ^ (1 << (j - 1)); // 去掉 j 的子集
                    double best = INF;
                    for (int i = 1; i <= K; ++i) {
                        if (!(pmask & (1 << (i - 1)))) continue;
                        double di = dp[IDX(pmask, i)];
                        if (di == INF) continue;
                        double cand = di + th + inst.tt(i, j);
                        if (cand < best) best = cand;
                    }
                    if (best <= inst.L[j] + EPS) // pull 后统一判时限
                        dp[IDX(mask, j)] = best;
                }
            }
        }

        // 保留状态数
        long long cnt = 0;
        for (size_t p = 0; p < dp.size(); ++p) if (dp[p] != INF) ++cnt;
        res.evaluated = cnt;

        double best = INF; int best_j = -1;
        for (int j = 1; j <= K; ++j) {
            double v = dp[IDX(FULL, j)];
            if (v == INF) continue;
            double tot = v + th + inst.tt(j, 0);
            if (tot < best - EPS) { best = tot; best_j = j; }
        }
        if (best_j == -1) return res;

        std::vector<int> route;
        int mask = FULL, j = best_j;
        while (mask) {
            route.push_back(j);
            int pmask = mask ^ (1 << (j - 1));
            if (pmask == 0) break;
            double target = dp[IDX(mask, j)];
            int prev = -1;
            for (int i = 1; i <= K; ++i) {
                if (!(pmask & (1 << (i - 1)))) continue;
                double di = dp[IDX(pmask, i)];
                if (di == INF) continue;
                if (std::fabs(di + th + inst.tt(i, j) - target) < 1e-6) { prev = i; break; }
            }
            j = prev; mask = pmask;
        }
        std::reverse(route.begin(), route.end());

        res.feasible = true; res.total = best; res.route = route;
        return res;
    }

} // namespace tsptw
#endif // DRONE_TSPTW_HELD_KARP_PARALLEL_HPP
