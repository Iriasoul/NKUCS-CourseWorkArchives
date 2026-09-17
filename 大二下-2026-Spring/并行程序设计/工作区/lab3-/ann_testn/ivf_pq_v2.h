// Lab3 - IVF-PQ 方法 (2): 先 IVF 再簇内 PQ
//
// 与 ivf_pq.h (方法 1) 的核心差异:
//   方法 1: 全局 PQ centroids, 全库一份编码表; 查询时 LUT 构建一次, 簇内扫描复用.
//   方法 2: 每个簇有自己独立的 PQ centroids; 查询时每访问一簇都要重建 LUT.
//
// 取舍 (对照指导书要求):
//   优势: 簇内向量分布更紧凑 (相比全库), 相同 K 下编码精度更高, 召回率预期更高
//   代价: nlist 倍的离线空间开销; 在线 LUT 重建 nprobe 次, 单 query 延迟更高
//   备注: 因 LUT 重建开销, 该方法更适合 nprobe 较小的场景
//
// 多线程设计:
//   每个线程独立处理被分配的簇 (LUT 构建 + ADC + 局部粗排), 这是天然的负载隔离
//   不需要跨簇的 barrier (每簇 LUT 是私有的, 不共享)

#pragma once

#include <queue>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <omp.h>
#include <pthread.h>
#include <arm_neon.h>
#include "thread_pool.h"

#include "ivf.h"  // 复用 IVF 索引 (centroids, reordered_base, ids, cluster_starts)

namespace ivfpq2_ns {
    int PQM = 8;
    const int PQK = 256;
    // 每簇独立的 PQ 模型. 用 vector<指针> 维护变长结构.
    // per_cluster_centroids[c]: [PQM][PQK][sub_dim] (KMeans 训练得到的原始布局)
    // per_cluster_centroids_soa[c]: [PQM][sub_dim][PQK] (LUT 构建的 SIMD 友好布局)
    // per_cluster_codes[c]: 该簇所有向量的 PQ 编码, 长度 = cluster_size * PQM (AoS)
    // per_cluster_codes_soa[c]: SoA, 长度 = PQM * cluster_size, [m * size + i]
    std::vector<float*>   per_cluster_centroids;
    std::vector<float*>   per_cluster_centroids_soa;
    std::vector<uint8_t*> per_cluster_codes;
    std::vector<uint8_t*> per_cluster_codes_soa;
}

// 内积 (与 ivf_pq.h 风格一致)
static inline float ivfpq2_neon_ip(const float* a, const float* b, size_t dim) {
    float32x4_t s = vdupq_n_f32(0.0f);
    size_t d = 0;
    for (; d + 3 < dim; d += 4) {
        s = vfmaq_f32(s, vld1q_f32(a + d), vld1q_f32(b + d));
    }
    float sum = vaddvq_f32(s);
    for (; d < dim; ++d) sum += a[d] * b[d];
    return sum;
}

// 簇内 KMeans, 内积聚类
// 注意: 输入 n 可能小于 PQK, 此时簇过小, K-Means 会退化, 需要特殊处理
inline void ivfpq2_kmeans_subspace(const float* data, size_t n, size_t dim,
                                   float* cents, int K, int iters = 20)
{
    if (n == 0) {
        std::memset(cents, 0, K * dim * sizeof(float));
        return;
    }
    // 初始化: 如果 n >= K, 随机采样 n 中 K 个; 否则用前 n 个重复填充
    for (int i = 0; i < K; ++i) {
        size_t src = (n >= (size_t)K) ? (rand() % n) : (i % n);
        std::memcpy(cents + i * dim, data + src * dim, dim * sizeof(float));
    }
    if (n < (size_t)K) return;  // 数据量不够, 不再迭代 (centroids 已退化为部分原始向量)

    for (int it = 0; it < iters; ++it) {
        std::vector<float> next(K * dim, 0);
        std::vector<int>   cnt(K, 0);
        for (size_t i = 0; i < n; ++i) {
            float best_ip = -1e30f; int best = 0;
            for (int c = 0; c < K; ++c) {
                float ip = ivfpq2_neon_ip(data + i * dim, cents + c * dim, dim);
                if (ip > best_ip) { best_ip = ip; best = c; }
            }
            cnt[best]++;
            for (size_t d = 0; d < dim; ++d) next[best * dim + d] += data[i * dim + d];
        }
        for (int c = 0; c < K; ++c) {
            if (cnt[c]) for (size_t d = 0; d < dim; ++d) cents[c * dim + d] = next[c * dim + d] / cnt[c];
        }
    }
}

// 离线: 在 IVF 重排后的 base 上, 对每个簇独立训练 PQ
inline void build_ivf_pq_v2(size_t base_number, size_t vecdim, int pq_m) {
    using namespace ivfpq2_ns;
    using namespace ivf_ns;
    (void)base_number;  // 与方法 1 接口对齐, 但本实现通过 cluster_starts 定簇范围, 不需要全库数
    PQM = pq_m;
    size_t sub_dim = vecdim / PQM;

    per_cluster_centroids.assign(nlist, nullptr);
    per_cluster_centroids_soa.assign(nlist, nullptr);
    per_cluster_codes.assign(nlist, nullptr);
    per_cluster_codes_soa.assign(nlist, nullptr);

    // 簇间外层并行 (不同簇互相独立, 无共享状态)
    #pragma omp parallel for schedule(dynamic, 1)
    for (int c = 0; c < nlist; ++c) {
        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        size_t csize = e - s;

        per_cluster_centroids[c]     = new float[PQM * PQK * sub_dim];
        per_cluster_centroids_soa[c] = new float[PQM * sub_dim * PQK];
        if (csize > 0) {
            per_cluster_codes[c]     = new uint8_t[csize * PQM];
            per_cluster_codes_soa[c] = new uint8_t[PQM * csize];
        }

        // 训练: 对每个子空间, 用簇内向量训练 PQK 个 centroids
        for (int m = 0; m < PQM; ++m) {
            std::vector<float> sub(csize * sub_dim);
            for (size_t i = 0; i < csize; ++i) {
                std::memcpy(&sub[i * sub_dim],
                            reordered_base + (s + i) * vecdim + m * sub_dim,
                            sub_dim * sizeof(float));
            }
            ivfpq2_kmeans_subspace(sub.data(), csize, sub_dim,
                                   per_cluster_centroids[c] + m * PQK * sub_dim,
                                   PQK, 15);
        }

        // 编码: 该簇内每个向量用本簇 PQ 模型编码
        for (size_t i = 0; i < csize; ++i) {
            for (int m = 0; m < PQM; ++m) {
                float* sub_v = reordered_base + (s + i) * vecdim + m * sub_dim;
                float* cents = per_cluster_centroids[c] + m * PQK * sub_dim;
                float best_ip = -1e30f; int best = 0;
                for (int kk = 0; kk < PQK; ++kk) {
                    float ip = ivfpq2_neon_ip(sub_v, cents + kk * sub_dim, sub_dim);
                    if (ip > best_ip) { best_ip = ip; best = kk; }
                }
                per_cluster_codes[c][i * PQM + m] = (uint8_t)best;
            }
        }

        // SoA 重排 (AoS -> SoA for ADC 扫描连续访存)
        for (int m = 0; m < PQM; ++m) {
            for (size_t i = 0; i < csize; ++i) {
                per_cluster_codes_soa[c][m * csize + i] = per_cluster_codes[c][i * PQM + m];
            }
        }
        // centroids 也建 SoA: [PQM][sub_dim][PQK], LUT 构建用
        for (int m = 0; m < PQM; ++m) {
            for (int d = 0; d < (int)sub_dim; ++d) {
                for (int kk = 0; kk < PQK; ++kk) {
                    per_cluster_centroids_soa[c][m * sub_dim * PQK + d * PQK + kk] =
                        per_cluster_centroids[c][m * PQK * sub_dim + kk * sub_dim + d];
                }
            }
        }
    }
}

inline void free_ivf_pq_v2() {
    using namespace ivfpq2_ns;
    for (auto* p : per_cluster_centroids)     if (p) delete[] p;
    for (auto* p : per_cluster_centroids_soa) if (p) delete[] p;
    for (auto* p : per_cluster_codes)         if (p) delete[] p;
    for (auto* p : per_cluster_codes_soa)     if (p) delete[] p;
    per_cluster_centroids.clear();
    per_cluster_centroids_soa.clear();
    per_cluster_codes.clear();
    per_cluster_codes_soa.clear();
}

// ----- 辅助: 给定簇 c, 构建该簇的 LUT (跨 centroid SIMD) -----
// LUT 写入调用方提供的 lut[m][PQK] 缓冲区
static inline void ivfpq2_build_lut_for_cluster(const float* query, size_t sub_dim,
                                                int c, float lut[][256])
{
    using namespace ivfpq2_ns;
    for (int m = 0; m < PQM; ++m) {
        const float* sub_q = query + m * sub_dim;
        const float* m_soa = per_cluster_centroids_soa[c] + m * sub_dim * PQK;
        for (int kk = 0; kk < PQK; kk += 4) {
            float32x4_t acc = vdupq_n_f32(0.0f);
            for (size_t d = 0; d < sub_dim; ++d) {
                acc = vfmaq_f32(acc, vdupq_n_f32(sub_q[d]),
                                vld1q_f32(m_soa + d * PQK + kk));
            }
            vst1q_f32(&lut[m][kk], acc);
        }
    }
}

// ============== IVF-PQ-v2 单线程 baseline (策略 13) ==============
inline std::priority_queue<std::pair<float, uint32_t>>
ivf_pq2_search_simd(float* query, size_t vecdim,
                    size_t k, int nprobe, size_t rerank_k)
{
    using namespace ivf_ns;
    using namespace ivfpq2_ns;
    size_t sub_dim = vecdim / PQM;

    // 1) IVF 粗排: 选 nprobe 个簇 (复用 ivf.h 的全局 centroids)
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivfpq2_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // 2) 每访问一簇都重建 LUT (这是方法 2 的核心代价), 然后 ADC 粗排
    alignas(64) float lut[64][256];
    std::priority_queue<std::pair<float, uint32_t>> coarse_q;
    for (int c : probe_list) {
        ivfpq2_build_lut_for_cluster(query, sub_dim, c, lut);

        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        size_t csize = e - s;
        const uint8_t* codes_soa = per_cluster_codes_soa[c];
        for (size_t i = 0; i < csize; ++i) {
            float ip = 0.0f;
            for (int m = 0; m < PQM; ++m) {
                ip += lut[m][codes_soa[m * csize + i]];
            }
            float dis = 1.0f - ip;
            // 注意: 这里存 reordered 位置 (s + i), 与方法 1 保持一致便于精排
            if (coarse_q.size() < rerank_k || dis < coarse_q.top().first) {
                coarse_q.push({dis, (uint32_t)(s + i)});
                if (coarse_q.size() > rerank_k) coarse_q.pop();
            }
        }
    }

    // 3) 原始 float 向量精排
    std::priority_queue<std::pair<float, uint32_t>> final_q;
    while (!coarse_q.empty()) {
        uint32_t pos = coarse_q.top().second;
        coarse_q.pop();
        float ip = ivfpq2_neon_ip(query, reordered_base + pos * vecdim, vecdim);
        float dis = 1.0f - ip;
        if (final_q.size() < k || dis < final_q.top().first) {
            final_q.push({dis, reordered_ids[pos]});
            if (final_q.size() > k) final_q.pop();
        }
    }
    return final_q;
}

// ============== IVF-PQ-v2 + OpenMP (策略 14) ==============
// 设计: 簇级 dynamic 并行. 每个线程独立处理一个簇 (LUT 构建 + ADC + 局部粗排).
// 比方法 1 更"自然"地多线程化: 因为 LUT 是每簇私有的, 不需要跨簇共享, 没有 barrier.
inline std::priority_queue<std::pair<float, uint32_t>>
ivf_pq2_search_omp(float* query, size_t vecdim,
                   size_t k, int nprobe, size_t rerank_k,
                   int num_threads = 0)
{
    using namespace ivf_ns;
    using namespace ivfpq2_ns;
    size_t sub_dim = vecdim / PQM;

    int nthr = (num_threads > 0) ? num_threads : omp_get_max_threads();

    // 粗排: 单线程 (nlist 不大)
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivfpq2_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // 并行: 簇级 dynamic, 每线程独立局部粗排 + 局部精排
    std::vector<std::vector<std::pair<float, uint32_t>>> thread_results(nthr);

    #pragma omp parallel num_threads(nthr)
    {
        int tid = omp_get_thread_num();
        alignas(64) float lut[64][256];  // 线程私有 LUT
        std::priority_queue<std::pair<float, uint32_t>> local_coarse;

        #pragma omp for schedule(dynamic, 1) nowait
        for (int pi = 0; pi < (int)probe_list.size(); ++pi) {
            int c = probe_list[pi];
            ivfpq2_build_lut_for_cluster(query, sub_dim, c, lut);

            size_t s = cluster_starts[c];
            size_t e = cluster_starts[c + 1];
            size_t csize = e - s;
            const uint8_t* codes_soa = per_cluster_codes_soa[c];
            for (size_t i = 0; i < csize; ++i) {
                float ip = 0.0f;
                for (int m = 0; m < PQM; ++m) {
                    ip += lut[m][codes_soa[m * csize + i]];
                }
                float dis = 1.0f - ip;
                if (local_coarse.size() < rerank_k || dis < local_coarse.top().first) {
                    local_coarse.push({dis, (uint32_t)(s + i)});
                    if (local_coarse.size() > rerank_k) local_coarse.pop();
                }
            }
        }

        // 线程内精排
        std::priority_queue<std::pair<float, uint32_t>> local_fine;
        while (!local_coarse.empty()) {
            uint32_t pos = local_coarse.top().second;
            local_coarse.pop();
            float ip = ivfpq2_neon_ip(query, reordered_base + pos * vecdim, vecdim);
            float dis = 1.0f - ip;
            uint32_t orig_id = reordered_ids[pos];
            if (local_fine.size() < k || dis < local_fine.top().first) {
                local_fine.push({dis, orig_id});
                if (local_fine.size() > k) local_fine.pop();
            }
        }

        thread_results[tid].reserve(k);
        while (!local_fine.empty()) {
            thread_results[tid].push_back(local_fine.top());
            local_fine.pop();
        }
    }

    // 全局归并
    std::priority_queue<std::pair<float, uint32_t>> final_q;
    for (const auto& tr : thread_results) {
        for (const auto& r : tr) {
            if (final_q.size() < k || r.first < final_q.top().first) {
                final_q.push(r);
                if (final_q.size() > k) final_q.pop();
            }
        }
    }
    return final_q;
}

// ============== IVF-PQ-v2 + Pthread (策略 15) ==============
// 簇级动态调度 (mutex + 计数器), 每线程私有 LUT, 不需要 barrier.
// 这是方法 2 相对方法 1 的另一个简洁性体现: 没有"先 LUT 后 ADC"的两阶段同步.
struct IVFPQ2PthreadArgs {
    int tid;
    float* query;
    size_t vecdim;
    size_t k;
    size_t rerank_k;
    int* probe_list;
    int nprobe;
    int* next_task_idx;
    pthread_mutex_t* mtx;
    std::vector<std::pair<float, uint32_t>>* result;
};

inline void* ivf_pq2_pthread_worker(void* arg) {
    IVFPQ2PthreadArgs* a = (IVFPQ2PthreadArgs*)arg;
    using namespace ivf_ns;
    using namespace ivfpq2_ns;
    size_t sub_dim = a->vecdim / PQM;

    alignas(64) float lut[64][256];  // 线程私有
    std::priority_queue<std::pair<float, uint32_t>> local_coarse;

    while (true) {
        pthread_mutex_lock(a->mtx);
        int task = (*a->next_task_idx)++;
        pthread_mutex_unlock(a->mtx);
        if (task >= a->nprobe) break;

        int c = a->probe_list[task];
        ivfpq2_build_lut_for_cluster(a->query, sub_dim, c, lut);

        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        size_t csize = e - s;
        const uint8_t* codes_soa = per_cluster_codes_soa[c];
        for (size_t i = 0; i < csize; ++i) {
            float ip = 0.0f;
            for (int m = 0; m < PQM; ++m) {
                ip += lut[m][codes_soa[m * csize + i]];
            }
            float dis = 1.0f - ip;
            if (local_coarse.size() < a->rerank_k || dis < local_coarse.top().first) {
                local_coarse.push({dis, (uint32_t)(s + i)});
                if (local_coarse.size() > a->rerank_k) local_coarse.pop();
            }
        }
    }

    // 线程内精排
    std::priority_queue<std::pair<float, uint32_t>> local_fine;
    while (!local_coarse.empty()) {
        uint32_t pos = local_coarse.top().second;
        local_coarse.pop();
        float ip = ivfpq2_neon_ip(a->query, reordered_base + pos * a->vecdim, a->vecdim);
        float dis = 1.0f - ip;
        uint32_t orig_id = reordered_ids[pos];
        if (local_fine.size() < a->k || dis < local_fine.top().first) {
            local_fine.push({dis, orig_id});
            if (local_fine.size() > a->k) local_fine.pop();
        }
    }

    a->result->reserve(a->k);
    while (!local_fine.empty()) {
        a->result->push_back(local_fine.top());
        local_fine.pop();
    }
    return nullptr;
}

inline std::priority_queue<std::pair<float, uint32_t>>
ivf_pq2_search_pthread(float* query, size_t vecdim, size_t k,
                       int nprobe, size_t rerank_k, int num_threads = 4,
                       ThreadPool* pool = nullptr)
{
    using namespace ivf_ns;
    using namespace ivfpq2_ns;

    // 粗排 (主线程)
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivfpq2_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    int next_task_idx = 0;
    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx, nullptr);

    std::vector<IVFPQ2PthreadArgs> args(num_threads);
    std::vector<std::vector<std::pair<float, uint32_t>>> results(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        args[t] = {t, query, vecdim, k, rerank_k,
                   probe_list.data(), nprobe,
                   &next_task_idx, &mtx, &results[t]};
    }

    if (pool) {
        pool->dispatch([&](int t) { ivf_pq2_pthread_worker(&args[t]); });
    } else {
        std::vector<pthread_t> tids(num_threads);
        for (int t = 0; t < num_threads; ++t)
            pthread_create(&tids[t], nullptr, ivf_pq2_pthread_worker, &args[t]);
        for (int t = 0; t < num_threads; ++t)
            pthread_join(tids[t], nullptr);
    }
    pthread_mutex_destroy(&mtx);

    // 全局归并
    std::priority_queue<std::pair<float, uint32_t>> final_q;
    for (const auto& tr : results) {
        for (const auto& r : tr) {
            if (final_q.size() < k || r.first < final_q.top().first) {
                final_q.push(r);
                if (final_q.size() > k) final_q.pop();
            }
        }
    }
    return final_q;
}
