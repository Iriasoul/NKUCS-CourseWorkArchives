// Lab3 - IVF (Inverted File) 算法
//  - 离线: KMeans 聚类 base data, 构建倒排索引; 内存重排, 同簇向量连续存储
//  - 在线: (1) 粗排, 找到最近的 nprobe 个簇; (2) 精排, 扫描这些簇内向量
//  - 多线程: 簇级并行(精排阶段最适合并行), 每线程维护局部 top-k, 最后归并
#pragma once

#include <queue>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <omp.h>
#include <arm_neon.h>
#include "thread_pool.h"

// 全局 IVF 索引数据
namespace ivf_ns {
    int nlist = 256;                       // 簇数量 (推荐 sqrt(N) ~ 4*sqrt(N))
    float* centroids = nullptr;            // [nlist][vecdim]
    float* reordered_base = nullptr;       // 内存重排后的 base 数据
    std::vector<uint32_t> reordered_ids;   // 重排后 i 位置对应的原始 ID
    std::vector<size_t> cluster_starts;    // 每个簇在 reordered_base 中的起始 index, 长度 nlist+1
}

// 内积距离 (用于 query 与簇心 / query 与 base 的精排)
static inline float ivf_neon_ip(const float* a, const float* b, size_t dim) {
    float32x4_t s = vdupq_n_f32(0.0f);
    size_t d = 0;
    for (; d + 3 < dim; d += 4) {
        s = vfmaq_f32(s, vld1q_f32(a + d), vld1q_f32(b + d));
    }
    float sum = vaddvq_f32(s);
    for (; d < dim; ++d) sum += a[d] * b[d];
    return sum;
}

// L2 距离, 用于 KMeans 聚类训练 (聚类用 L2 更稳定, 查询时再用 ip)
static inline float ivf_l2_sq(const float* a, const float* b, size_t dim) {
    float32x4_t s = vdupq_n_f32(0.0f);
    size_t d = 0;
    for (; d + 3 < dim; d += 4) {
        float32x4_t diff = vsubq_f32(vld1q_f32(a + d), vld1q_f32(b + d));
        s = vfmaq_f32(s, diff, diff);
    }
    float sum = vaddvq_f32(s);
    for (; d < dim; ++d) { float t = a[d] - b[d]; sum += t * t; }
    return sum;
}

// 简化 KMeans (使用 L2 距离), 用于 IVF 簇心生成
inline void ivf_kmeans(float* data, size_t n, size_t dim, int nlist, float* cents,
                       int iters = 20)
{
    // 随机初始化 (注意: 实际项目应使用 KMeans++ 提升稳定性, 这里简化)
    for (int i = 0; i < nlist; ++i) {
        std::memcpy(cents + i * dim, data + (rand() % n) * dim, dim * sizeof(float));
    }

    std::vector<int> assign(n, 0);
    for (int it = 0; it < iters; ++it) {
        // E 步: 分配
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            float best_d = 1e30f; int best_c = 0;
            for (int c = 0; c < nlist; ++c) {
                float d2 = ivf_l2_sq(data + i * dim, cents + c * dim, dim);
                if (d2 < best_d) { best_d = d2; best_c = c; }
            }
            assign[i] = best_c;
        }

        // M 步: 重算中心 (单线程, 避免对 next_cents 的竞争; n_list 不大, 开销可控)
        std::vector<float> next(nlist * dim, 0.0f);
        std::vector<int> cnt(nlist, 0);
        for (size_t i = 0; i < n; ++i) {
            int c = assign[i];
            cnt[c]++;
            for (size_t d = 0; d < dim; ++d) next[c * dim + d] += data[i * dim + d];
        }
        for (int c = 0; c < nlist; ++c) {
            if (cnt[c]) for (size_t d = 0; d < dim; ++d) cents[c * dim + d] = next[c * dim + d] / cnt[c];
        }
    }
}

// IVF 离线构建
inline void build_ivf(float* base, size_t base_number, size_t vecdim, int nlist)
{
    using namespace ivf_ns;
    ivf_ns::nlist = nlist;
    centroids = new float[nlist * vecdim];

    // 训练数据可以采样 (n较大时降低开销)
    size_t train_n = std::min((size_t)20000, base_number);
    std::vector<float> train_data(train_n * vecdim);
    for (size_t i = 0; i < train_n; ++i)
        std::memcpy(&train_data[i * vecdim], base + i * vecdim, vecdim * sizeof(float));

    ivf_kmeans(train_data.data(), train_n, vecdim, nlist, centroids, 15);

    // 分配每个 base 向量到最近的簇
    std::vector<int> assign(base_number);
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < base_number; ++i) {
        float best_d = 1e30f; int best_c = 0;
        for (int c = 0; c < nlist; ++c) {
            float d2 = ivf_l2_sq(base + i * vecdim, centroids + c * vecdim, vecdim);
            if (d2 < best_d) { best_d = d2; best_c = c; }
        }
        assign[i] = best_c;
    }

    // 构建 cluster_starts (累积偏移)
    cluster_starts.assign(nlist + 1, 0);
    for (size_t i = 0; i < base_number; ++i) cluster_starts[assign[i] + 1]++;
    for (int c = 1; c <= nlist; ++c) cluster_starts[c] += cluster_starts[c - 1];

    // 内存重排: 同簇向量连续存储
    reordered_base = new float[base_number * vecdim];
    reordered_ids.assign(base_number, 0);
    std::vector<size_t> ptr(cluster_starts.begin(), cluster_starts.begin() + nlist);
    for (size_t i = 0; i < base_number; ++i) {
        int c = assign[i];
        size_t pos = ptr[c]++;
        std::memcpy(reordered_base + pos * vecdim, base + i * vecdim, vecdim * sizeof(float));
        reordered_ids[pos] = (uint32_t)i;
    }
}

inline void free_ivf() {
    using namespace ivf_ns;
    if (centroids) { delete[] centroids; centroids = nullptr; }
    if (reordered_base) { delete[] reordered_base; reordered_base = nullptr; }
    reordered_ids.clear();
    cluster_starts.clear();
}

// ============== IVF-SIMD 单线程 baseline ==============
inline std::priority_queue<std::pair<float, uint32_t>>
ivf_search_simd(float* query, size_t vecdim, size_t k, int nprobe)
{
    using namespace ivf_ns;
    // ----- 粗排: 计算 query 到所有簇中心的内积距离, 选 nprobe 个 -----
    std::priority_queue<std::pair<float, int>> coarse; // max-heap of <dis, cluster_id>
    for (int c = 0; c < nlist; ++c) {
        float ip = ivf_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // ----- 精排: 扫描选中的簇内所有点 -----
    std::priority_queue<std::pair<float, uint32_t>> q;
    for (int c : probe_list) {
        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        for (size_t i = s; i < e; ++i) {
            float ip = ivf_neon_ip(query, reordered_base + i * vecdim, vecdim);
            float dis = 1.0f - ip;
            if (q.size() < k || dis < q.top().first) {
                q.push({dis, reordered_ids[i]});
                if (q.size() > k) q.pop();
            }
        }
    }
    return q;
}

// ============== IVF-SIMD + 多线程 ==============
// 粗排部分计算量较小 (nlist 通常 ~256), 多线程收益有限, 保持单线程;
// 精排部分: 对 nprobe 个簇做并行扫描, 每线程维护局部 top-k, 最后归并.
// 由于簇大小不均, 使用 dynamic schedule 实现负载均衡.
inline std::priority_queue<std::pair<float, uint32_t>>
ivf_search_omp(float* query, size_t vecdim, size_t k, int nprobe,
               int num_threads = 0)
{
    using namespace ivf_ns;
    // 粗排 (与单线程版相同)
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivf_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // 精排 (并行)
    int nthr = (num_threads > 0) ? num_threads : omp_get_max_threads();
    std::vector<std::priority_queue<std::pair<float, uint32_t>>> local_q(nthr);

    #pragma omp parallel num_threads(nthr)
    {
        int tid = omp_get_thread_num();
        auto& q = local_q[tid];

        // 按簇并行 (dynamic 调度处理簇大小不均)
        #pragma omp for schedule(dynamic, 1) nowait
        for (int pi = 0; pi < (int)probe_list.size(); ++pi) {
            int c = probe_list[pi];
            size_t s = cluster_starts[c];
            size_t e = cluster_starts[c + 1];
            for (size_t i = s; i < e; ++i) {
                float ip = ivf_neon_ip(query, reordered_base + i * vecdim, vecdim);
                float dis = 1.0f - ip;
                if (q.size() < k || dis < q.top().first) {
                    q.push({dis, reordered_ids[i]});
                    if (q.size() > k) q.pop();
                }
            }
        }
    }

    // 归并
    std::priority_queue<std::pair<float, uint32_t>> final_q;
    for (auto& q : local_q) {
        while (!q.empty()) {
            auto t = q.top(); q.pop();
            if (final_q.size() < k || t.first < final_q.top().first) {
                final_q.push(t);
                if (final_q.size() > k) final_q.pop();
            }
        }
    }
    return final_q;
}

// ============================================================================
// IVF-SIMD + Pthread (策略 9)
//
// 关键挑战: 重现 OpenMP 的 schedule(dynamic, 1) 行为.
//   OpenMP 一行 `schedule(dynamic, 1)` 就让线程从共享队列动态取任务,
//   pthread 要我们用 pthread_mutex_t + 共享计数器手动实现.
//
// 设计:
//   - 一个共享原子计数器 next_task_idx, 初始为 0
//   - 每个线程在循环中:
//       1. 加锁, 取出当前 next_task_idx 的值, 然后 ++
//       2. 解锁
//       3. 处理这个簇 (扫描成员, 维护局部 top-k)
//       4. 回到第 1 步, 直到 idx >= nprobe
//   - 注: 这里用 mutex 不是 atomic, 因为我们想让代码更清晰,
//         同时 mutex 的开销在这个粒度 (簇数十~百次) 也完全可接受
// ============================================================================
struct IVFPthreadArgs {
    float* query;
    size_t vecdim;
    size_t k;
    int* probe_list;
    int nprobe;
    int* next_task_idx;       // 共享计数器
    pthread_mutex_t* mtx;     // 保护计数器
    std::priority_queue<std::pair<float, uint32_t>>* result;  // 线程局部 top-k
};

inline void* ivf_pthread_worker(void* arg) {
    IVFPthreadArgs* a = (IVFPthreadArgs*)arg;
    using namespace ivf_ns;
    auto& q = *a->result;

    while (true) {
        // ===== 从共享任务队列取一个任务 (动态调度的核心) =====
        pthread_mutex_lock(a->mtx);
        int task = (*a->next_task_idx)++;
        pthread_mutex_unlock(a->mtx);

        if (task >= a->nprobe) break;   // 所有任务都被领走了

        // ===== 处理这一个簇 =====
        int c = a->probe_list[task];
        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        for (size_t i = s; i < e; ++i) {
            float ip = ivf_neon_ip(a->query, reordered_base + i * a->vecdim, a->vecdim);
            float dis = 1.0f - ip;
            if (q.size() < a->k || dis < q.top().first) {
                q.push({dis, reordered_ids[i]});
                if (q.size() > a->k) q.pop();
            }
        }
    }
    return nullptr;
}

inline std::priority_queue<std::pair<float, uint32_t>>
ivf_search_pthread(float* query, size_t vecdim, size_t k, int nprobe,
                   int num_threads = 4, ThreadPool* pool = nullptr)
{
    using namespace ivf_ns;

    // ===== 粗排: 不并行 =====
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivf_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // ===== 精排: pthread 手动动态调度 =====
    int next_task_idx = 0;
    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx, nullptr);

    std::vector<IVFPthreadArgs> args(num_threads);
    std::vector<std::priority_queue<std::pair<float, uint32_t>>> local_q(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        args[t] = {query, vecdim, k, probe_list.data(), nprobe,
                   &next_task_idx, &mtx, &local_q[t]};
    }

    if (pool) {
        pool->dispatch([&](int t) { ivf_pthread_worker(&args[t]); });
    } else {
        std::vector<pthread_t> tids(num_threads);
        for (int t = 0; t < num_threads; ++t)
            pthread_create(&tids[t], nullptr, ivf_pthread_worker, &args[t]);
        for (int t = 0; t < num_threads; ++t)
            pthread_join(tids[t], nullptr);
    }
    pthread_mutex_destroy(&mtx);

    // ===== 归并 =====
    std::priority_queue<std::pair<float, uint32_t>> final_q;
    for (auto& q : local_q) {
        while (!q.empty()) {
            auto t = q.top(); q.pop();
            if (final_q.size() < k || t.first < final_q.top().first) {
                final_q.push(t);
                if (final_q.size() > k) final_q.pop();
            }
        }
    }
    return final_q;
}
