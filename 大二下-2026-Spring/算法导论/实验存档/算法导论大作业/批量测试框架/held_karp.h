#ifndef DRONE_TSPTW_HELD_KARP_HPP
#define DRONE_TSPTW_HELD_KARP_HPP


// Held-Karp 状态压缩 DP


#include "instance.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace tsptw {

    inline SolveResult held_karp(const Instance& inst) {
        SolveResult res;
        const int K = inst.k;
        const int FULL = (1 << K) - 1;

        std::vector<double> dp(static_cast<size_t>(1 << K) * K, INF);
        auto IDX = [K](int mask, int j) -> size_t {
            return static_cast<size_t>(mask) * K + (j - 1);
            };
        const double th = inst.theta;

        // 初始化:从站点直达单个客户
        for (int j = 1; j <= K; ++j) {
            double a = inst.tt(0, j);
            if (a <= inst.L[j] + EPS)
                dp[IDX(1 << (j - 1), j)] = a;
        }

        // 转移按 mask 递增前向扩展
        for (int mask = 1; mask <= FULL; ++mask) {
            for (int j = 1; j <= K; ++j) {
                if (!(mask & (1 << (j - 1)))) continue;
                double cur = dp[IDX(mask, j)];
                if (cur == INF) continue;
                ++res.evaluated; 

                for (int nj = 1; nj <= K; ++nj) {
                    if (mask & (1 << (nj - 1))) continue;
                    double arr = cur + th + inst.tt(j, nj);
                    if (arr > inst.L[nj] + EPS) continue; // 时限过滤
                    int nmask = mask | (1 << (nj - 1));
                    size_t ni = IDX(nmask, nj);
                    if (arr < dp[ni]) dp[ni] = arr;
                }
            }
        }

        // 加上 j 处服务与返航
        double best = INF; int best_j = -1;
        for (int j = 1; j <= K; ++j) {
            double v = dp[IDX(FULL, j)];
            if (v == INF) continue;
            double tot = v + th + inst.tt(j, 0);
            if (tot < best - EPS) { best = tot; best_j = j; }
        }
        if (best_j == -1) return res; // 无可行解

        // 回溯 dp 
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

        res.feasible = true;
        res.total = best;
        res.route = route;
        return res;
    }

} // namespace tsptw
#endif // DRONE_TSPTW_HELD_KARP_HPP
