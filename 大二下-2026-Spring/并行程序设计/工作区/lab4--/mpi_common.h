#pragma once
// ===========================================================================
//  mpi_common.h —— lab4 MPI 公共工具
//  · 任务划分: block (连续分块, 负载均衡) / cyclic (循环划分)
//  · 本地 base 物化: 把分散的全局向量收集为连续缓冲, 供已有的 build_ivf 复用
//  · 全局 id 映射: 各 rank 在自己分片上建索引, 返回的是"局部下标", 需映射回原始全局 id
//  · top-k 归并 / 单次召回计算 (root 端)
// ===========================================================================

#include <mpi.h>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <queue>
#include <utility>
#include <cfloat>
#include <set>
#include <algorithm>

// 任务划分方式
enum PartitionScheme { PART_BLOCK = 0, PART_CYCLIC = 1 };

// 运行结果 (仅 root 上的数值有意义)
struct RunResult {
    double recall      = 0.0;   // 平均召回率
    double avg_latency = 0.0;   // 平均每查询延迟 (us)
    double total_sec   = 0.0;   // 在线查询总耗时 (s)
    double build_sec   = 0.0;   // 离线索引构建耗时 (s, 各 rank 取最大)
};

// 计算本 rank 负责的"全局向量 id"列表
//   block : 连续分块, 前 (N % P) 个 rank 各多分 1 个, 实现负载均衡
//   cyclic: 循环划分, 全局 id i 分给 rank (i % P)
inline std::vector<uint32_t>
mpi_compute_shard_ids(size_t N, int rank, int P, PartitionScheme part)
{
    std::vector<uint32_t> ids;
    if (part == PART_BLOCK) {
        size_t q = N / (size_t)P, rem = N % (size_t)P;
        size_t start = (size_t)rank * q + std::min((size_t)rank, rem);
        size_t cnt   = q + ((size_t)rank < rem ? 1 : 0);
        ids.reserve(cnt);
        for (size_t i = 0; i < cnt; ++i) ids.push_back((uint32_t)(start + i));
    } else { // PART_CYCLIC
        for (size_t i = (size_t)rank; i < N; i += (size_t)P)
            ids.push_back((uint32_t)i);
    }
    return ids;
}

// 把本 rank 负责的全局向量收集为连续的本地 base 缓冲 (调用方负责 delete[])
// 之后即可直接喂给 build_ivf / build_hnsw 等"以连续 base 为输入"的现有接口
inline float*
mpi_materialize_local_base(const float* full_base, size_t vecdim,
                           const std::vector<uint32_t>& shard_ids)
{
    size_t n = shard_ids.size();
    float* local_base = new float[n * vecdim];
    for (size_t i = 0; i < n; ++i)
        std::memcpy(local_base + i * vecdim,
                    full_base + (size_t)shard_ids[i] * vecdim,
                    vecdim * sizeof(float));
    return local_base;
}

// 把一个局部 top-k 优先队列拍平成"定长 (dist,id) 数组", 不足部分用 (FLT_MAX, UINT32_MAX) 填充.
// 同时把局部下标 (t.second) 通过 shard_ids 映射回全局原始 id, 这样归并后与 gt 直接可比.
inline void
mpi_flatten_local(std::priority_queue<std::pair<float, uint32_t>>& local,
                  const std::vector<uint32_t>& shard_ids,
                  size_t local_topk,
                  std::vector<float>& out_d, std::vector<uint32_t>& out_id)
{
    out_d.assign(local_topk, FLT_MAX);
    out_id.assign(local_topk, UINT32_MAX);
    size_t idx = 0;
    while (!local.empty() && idx < local_topk) {
        auto t = local.top(); local.pop();
        out_d[idx]  = t.first;
        out_id[idx] = (t.second < shard_ids.size()) ? shard_ids[t.second]
                                                     : UINT32_MAX; // 安全兜底
        ++idx;
    }
}

// root 端: 把收集到的若干候选 (dist,id) 归并为全局 top-k (max-heap, top=当前最差)
inline std::priority_queue<std::pair<float, uint32_t>>
mpi_merge_topk(const std::vector<float>& dists,
               const std::vector<uint32_t>& ids, size_t k)
{
    std::priority_queue<std::pair<float, uint32_t>> q;
    for (size_t i = 0; i < dists.size(); ++i) {
        if (ids[i] == UINT32_MAX) continue;            // padding, 跳过
        float d = dists[i];
        if (q.size() < k || d < q.top().first) {
            q.push({d, ids[i]});
            if (q.size() > k) q.pop();
        }
    }
    return q;
}

// 计算单次查询的召回: 结果集 res 与 gt 前 k 个的交集占比 (与 lab3 口径一致)
inline double
mpi_recall_one(std::priority_queue<std::pair<float, uint32_t>> res,
               const int* gt_row, size_t k)
{
    std::set<uint32_t> gtset;
    for (size_t j = 0; j < k; ++j) gtset.insert((uint32_t)gt_row[j]);
    size_t acc = 0;
    while (!res.empty()) {
        if (gtset.count(res.top().second)) ++acc;
        res.pop();
    }
    return (double)acc / (double)k;
}
