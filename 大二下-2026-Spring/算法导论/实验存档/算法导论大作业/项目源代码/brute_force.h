#ifndef DRONE_TSPTW_BRUTE_FORCE_HPP
#define DRONE_TSPTW_BRUTE_FORCE_HPP

// 暴力枚举算法

#include "instance.h"
#include <vector>
#include <numeric>
#include <algorithm>

namespace tsptw {

    // 枚举全部 k! 个排列,对每个递推到达时刻，检查所有时限，计算总完成
    // 时间取可行解中的最小值。时间复杂度 O(k! · k)
    inline SolveResult brute_force(const Instance& inst) {
        SolveResult best;
        std::vector<int> perm(inst.k);
        std::iota(perm.begin(), perm.end(), 1); // 字典序最小,保证遍历全部

        do {
            ++best.evaluated;
            EvalResult r = evaluate_route(inst, perm);
            if (r.feasible && r.total < best.total - EPS) {
                best.feasible = true;
                best.total = r.total;
                best.route = perm;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));

        return best;
    }

} // namespace tsptw
#endif // DRONE_TSPTW_BRUTE_FORCE_HPP
