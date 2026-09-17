#ifndef DRONE_TSPTW_HELD_KARP_APPROX_HPP
#define DRONE_TSPTW_HELD_KARP_APPROX_HPP

// (1+ε) 近似算法

#include "instance.h"
#include "held_karp_pruned.h" // 复用 greedy_incumbent
#include <vector>
#include <algorithm>
#include <cmath>

namespace tsptw {

    // 2-opt 局部搜索
    // 把启发式初值收紧为更好的 incumbent
    inline SolveResult improve_2opt(const Instance& inst, SolveResult cur) {
        if (!cur.feasible) return cur;
        std::vector<int> route = cur.route;
        double best = cur.total;
        bool improved = true; int passes = 0;
        while (improved && passes < 60) {
            improved = false; ++passes;
            int k = static_cast<int>(route.size());
            for (int i = 0; i < k - 1; ++i)
                for (int j = i + 1; j < k; ++j) {
                    std::vector<int> cand = route;
                    std::reverse(cand.begin() + i, cand.begin() + j + 1);
                    EvalResult e = evaluate_route(inst, cand);
                    if (e.feasible && e.total < best - EPS) {
                        route = cand; best = e.total; improved = true;
                    }
                }
        }
        SolveResult r; r.feasible = true; r.total = best; r.route = route;
        return r;
    }

    // 容错系数eps, eps=0 时与精确等价
    inline SolveResult held_karp_approx(const Instance& inst, double eps) {
        SolveResult res;
        const int K = inst.k;
        const int FULL = (1 << K) - 1;
        const double th = inst.theta;
        const double factor = 1.0 + eps;

        // 每个节点的最小入边
        std::vector<double> minIn(K + 1, INF);
        for (int v = 0; v <= K; ++v) {
            double m = INF;
            for (int u = 0; u <= K; ++u)
                if (u != v) m = std::min(m, inst.tt(u, v));
            minIn[v] = m;
        }

        // 贪心 + 2-opt 收紧后的 incumbent
        SolveResult seed = improve_2opt(inst, greedy_incumbent(inst));
        const double incumbent = seed.feasible ? seed.total : INF;
        res.incumbent0 = incumbent;

        std::vector<double> dp(static_cast<size_t>(1 << K) * K, INF);
        auto IDX = [K](int mask, int j) -> size_t {
            return static_cast<size_t>(mask) * K + (j - 1);
            };

        for (int j = 1; j <= K; ++j) {
            double a = inst.tt(0, j);
            if (a <= inst.L[j] + EPS) dp[IDX(1 << (j - 1), j)] = a;
        }

        for (int mask = 1; mask <= FULL; ++mask) {
            for (int j = 1; j <= K; ++j) {
                if (!(mask & (1 << (j - 1)))) continue;
                double tau = dp[IDX(mask, j)];
                if (tau == INF) continue;
                if (mask == FULL) { ++res.evaluated; continue; }

                bool dead = false;
                double lbFlight = 0.0; int rcount = 0;
                for (int v = 1; v <= K; ++v) {
                    if (mask & (1 << (v - 1))) continue;
                    if (tau + th + inst.tt(j, v) > inst.L[v] + EPS) { dead = true; break; } // 精确:可行性
                    lbFlight += minIn[v]; ++rcount;
                }
                if (dead) { ++res.pruned; continue; }

                double LB = (rcount + 1) * th + lbFlight + minIn[0];

                // 改为乘 (1+ε) 
                if (factor * (tau + LB) >= incumbent - EPS) { ++res.pruned; continue; }

                ++res.evaluated;
                for (int nj = 1; nj <= K; ++nj) {
                    if (mask & (1 << (nj - 1))) continue;
                    double arr = tau + th + inst.tt(j, nj);
                    if (arr > inst.L[nj] + EPS) continue;
                    size_t ni = IDX(mask | (1 << (nj - 1)), nj);
                    if (arr < dp[ni]) dp[ni] = arr;
                }
            }
        }

        double dpBest = INF; int dpBestJ = -1;
        for (int j = 1; j <= K; ++j) {
            double v = dp[IDX(FULL, j)];
            if (v == INF) continue;
            double tot = v + th + inst.tt(j, 0);
            if (tot < dpBest - EPS) { dpBest = tot; dpBestJ = j; }
        }

        if (dpBestJ != -1 && (!seed.feasible || dpBest < incumbent - EPS)) {
            std::vector<int> route;
            int mask = FULL, j = dpBestJ;
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
            res.feasible = true; res.total = dpBest; res.route = route;
        }
        else if (seed.feasible) {
            res.feasible = true; res.total = seed.total; res.route = seed.route;
        }
        return res;
    }

} // namespace tsptw
#endif // DRONE_TSPTW_HELD_KARP_APPROX_HPP
