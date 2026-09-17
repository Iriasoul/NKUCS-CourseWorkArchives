#ifndef DRONE_TSPTW_HELD_KARP_PRUNED_HPP
#define DRONE_TSPTW_HELD_KARP_PRUNED_HPP

//  Held-Karp DP + 剪枝

#include "instance.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace tsptw {

    // 贪心启发式
    // 每步选可行未访问客户中最近的，或者选最早时限的。
    inline SolveResult greedy_build(const Instance& inst, int mode) {
        const int K = inst.k;
        std::vector<int> order;
        std::vector<char> used(K + 1, 0);
        double flight = 0.0, service = 0.0;
        int prev = 0;
        for (int step = 0; step < K; ++step) {
            int pick = -1; double bestKey = INF;
            for (int v = 1; v <= K; ++v) {
                if (used[v]) continue;
                double arrival = flight + inst.tt(prev, v) + service;
                if (arrival > inst.L[v] + EPS) continue; // 该步选 v 不可行
                double key = (mode == 0) ? inst.tt(prev, v) : inst.L[v];
                if (key < bestKey) { bestKey = key; pick = v; }
            }
            if (pick == -1) return SolveResult{}; // 启发式失败
            flight += inst.tt(prev, pick);
            order.push_back(pick);
            used[pick] = 1; prev = pick; service += inst.theta;
        }
        EvalResult r = evaluate_route(inst, order);
        SolveResult s;
        if (r.feasible) { s.feasible = true; s.total = r.total; s.route = order; }
        return s;
    }

    inline SolveResult greedy_incumbent(const Instance& inst) {
        SolveResult best;
        for (int mode = 0; mode <= 1; ++mode) {
            SolveResult s = greedy_build(inst, mode);
            if (s.feasible && s.total < best.total - EPS) best = s;
        }
        return best;
    }

    inline SolveResult held_karp_pruned(const Instance& inst) {
        SolveResult res;
        const int K = inst.k;
        const int FULL = (1 << K) - 1;
        const double th = inst.theta;

        // 预计算每个节点的最小入边 minIn[v]
        std::vector<double> minIn(K + 1, INF);
        for (int v = 0; v <= K; ++v) {
            double m = INF;
            for (int u = 0; u <= K; ++u)
                if (u != v) m = std::min(m, inst.tt(u, v));
            minIn[v] = m;
        }

        // 启发式初始 incumbent(下界剪枝阈值)
        SolveResult seed = greedy_incumbent(inst);
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

                // 扫描剩余客户 R, 同时做时间窗消减与下界累加
                bool dead = false;
                double lbFlight = 0.0;
                int rcount = 0;
                for (int v = 1; v <= K; ++v) {
                    if (mask & (1 << (v - 1))) continue;
                    if (tau + th + inst.tt(j, v) > inst.L[v] + EPS) { dead = true; break; }
                    lbFlight += minIn[v];
                    ++rcount;
                }
                if (dead) { ++res.pruned; continue; }                       // 消减

                double LB = (rcount + 1) * th + lbFlight + minIn[0];        // 可采纳下界
                if (tau + LB >= incumbent - EPS) { ++res.pruned; continue; } // 下界剪枝

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

        // 答案取两者更优
        double dpBest = INF; int dpBestJ = -1;
        for (int j = 1; j <= K; ++j) {
            double v = dp[IDX(FULL, j)];
            if (v == INF) continue;
            double tot = v + th + inst.tt(j, 0);
            if (tot < dpBest - EPS) { dpBest = tot; dpBestJ = j; }
        }

        if (dpBestJ != -1 && dpBest < incumbent - EPS) {
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
#endif // DRONE_TSPTW_HELD_KARP_PRUNED_HPP
