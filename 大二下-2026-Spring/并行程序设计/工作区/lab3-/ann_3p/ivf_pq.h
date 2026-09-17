// Lab3 - IVF-PQ-SIMD 算法
// 采取“先 IVF 再 PQ”的全局编码方案: 所有 base 向量做一份全局 PQ 编码,
// 同时用 IVF 划分簇. 查询时:
//   1) 粗排: 选 nprobe 个最近簇
//   2) 在选中簇内, 用 PQ 的 ADC 距离做"二级粗排", 取 Top-rerank_k 候选
//   3) 用原始 float 向量做精排, 得到最终 Top-K
//
// 这种方案兼具 IVF 的搜索范围裁剪与 PQ 的快速距离估算, 结合 rerank 保持高召回.
// 多线程: 按 nprobe 个簇并行 dispatch, 线程局部维护粗排+精排, 最后归并.
#pragma once

#include <queue>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <omp.h>
#include <arm_neon.h>

#include "ivf.h" // 复用 IVF 的索引结构 (centroids, reordered_base, ids, starts)

// 全局 PQ 编码 (跟随 IVF 的重排顺序)
namespace ivfpq_ns {
    int PQM = 8;          // 子空间数
    const int PQK = 256;  // 每个子空间的簇数 (用 uint8_t 编码)
    uint8_t* pq_codes = nullptr;        // [base_number * PQM], 按重排后顺序存储
    uint8_t* pq_codes_soa = nullptr;    // [PQM][base_number] SoA 布局
    float* pq_centroids = nullptr;      // [PQM][PQK][sub_dim] (训练得到)
    float* pq_centroids_soa = nullptr;  // [PQM][sub_dim][PQK] (LUT构建用)
}

// 内积 (用于 PQ 训练和 LUT 构建)
static inline float ivfpq_neon_ip(const float* a, const float* b, size_t dim) {
    float32x4_t s = vdupq_n_f32(0.0f);
    size_t d = 0;
    for (; d + 3 < dim; d += 4) {
        s = vfmaq_f32(s, vld1q_f32(a + d), vld1q_f32(b + d));
    }
    float sum = vaddvq_f32(s);
    for (; d < dim; ++d) sum += a[d] * b[d];
    return sum;
}

// PQ 子空间内的 KMeans (使用内积聚类, 与 Lab2 一致)
inline void ivfpq_kmeans(float* data, size_t n, size_t dim, float* cents, int iters = 30) {
    using namespace ivfpq_ns;
    for (int i = 0; i < PQK; ++i) {
        std::memcpy(cents + i * dim, data + (rand() % n) * dim, dim * sizeof(float));
    }
    for (int it = 0; it < iters; ++it) {
        std::vector<float> next(PQK * dim, 0);
        std::vector<int> cnt(PQK, 0);
        for (size_t i = 0; i < n; ++i) {
            float best_ip = -1e30f; int best = 0;
            for (int c = 0; c < PQK; ++c) {
                float ip = ivfpq_neon_ip(data + i * dim, cents + c * dim, dim);
                if (ip > best_ip) { best_ip = ip; best = c; }
            }
            cnt[best]++;
            for (size_t d = 0; d < dim; ++d) next[best * dim + d] += data[i * dim + d];
        }
        for (int c = 0; c < PQK; ++c) {
            if (cnt[c]) for (size_t d = 0; d < dim; ++d) cents[c * dim + d] = next[c * dim + d] / cnt[c];
        }
    }
}

// 离线: 在 IVF 重排后的 base 上做全局 PQ 编码
// (重排后 reordered_base[i] 对应原始 reordered_ids[i])
inline void build_ivf_pq(size_t base_number, size_t vecdim, int pq_m) {
    using namespace ivfpq_ns;
    using namespace ivf_ns;
    PQM = pq_m;
    size_t sub_dim = vecdim / PQM;

    pq_codes = new uint8_t[base_number * PQM];
    pq_centroids = new float[PQM * PQK * sub_dim];

    size_t train_n = std::min((size_t)20000, base_number);

    // 训练 PQ 质心
    for (int m = 0; m < PQM; ++m) {
        std::vector<float> sub(train_n * sub_dim);
        for (size_t i = 0; i < train_n; ++i) {
            std::memcpy(&sub[i * sub_dim],
                        reordered_base + i * vecdim + m * sub_dim,
                        sub_dim * sizeof(float));
        }
        ivfpq_kmeans(sub.data(), train_n, sub_dim, pq_centroids + m * PQK * sub_dim, 20);
    }

    // 编码所有向量 (并行)
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < base_number; ++i) {
        for (int m = 0; m < PQM; ++m) {
            float* sub_v = reordered_base + i * vecdim + m * sub_dim;
            float* cents = pq_centroids + m * PQK * sub_dim;
            float best_ip = -1e30f; int best = 0;
            for (int c = 0; c < PQK; ++c) {
                float ip = ivfpq_neon_ip(sub_v, cents + c * sub_dim, sub_dim);
                if (ip > best_ip) { best_ip = ip; best = c; }
            }
            pq_codes[i * PQM + m] = (uint8_t)best;
        }
    }

    // SoA 布局 (codes: [PQM][N])
    pq_codes_soa = new uint8_t[PQM * base_number];
    for (int m = 0; m < PQM; ++m) {
        for (size_t i = 0; i < base_number; ++i) {
            pq_codes_soa[m * base_number + i] = pq_codes[i * PQM + m];
        }
    }
    // SoA centroids: [PQM][sub_dim][PQK]
    pq_centroids_soa = new float[PQM * sub_dim * PQK];
    for (int m = 0; m < PQM; ++m) {
        for (int d = 0; d < (int)sub_dim; ++d) {
            for (int c = 0; c < PQK; ++c) {
                pq_centroids_soa[m * sub_dim * PQK + d * PQK + c] =
                    pq_centroids[m * PQK * sub_dim + c * sub_dim + d];
            }
        }
    }
}

inline void free_ivf_pq() {
    using namespace ivfpq_ns;
    if (pq_codes) { delete[] pq_codes; pq_codes = nullptr; }
    if (pq_codes_soa) { delete[] pq_codes_soa; pq_codes_soa = nullptr; }
    if (pq_centroids) { delete[] pq_centroids; pq_centroids = nullptr; }
    if (pq_centroids_soa) { delete[] pq_centroids_soa; pq_centroids_soa = nullptr; }
}

// ============== IVF-PQ-SIMD 单线程 baseline ==============
// 流程: nprobe 簇 -> 在簇内用 PQ ADC 粗排取 rerank_k -> 原始向量精排
inline std::priority_queue<std::pair<float, uint32_t>>
ivf_pq_search_simd(float* query, size_t vecdim,
                   size_t k, int nprobe, size_t rerank_k)
{
    using namespace ivf_ns;
    using namespace ivfpq_ns;
    size_t sub_dim = vecdim / PQM;

    // 构建 LUT (跨 centroid 4-路 SIMD 并行, 沿用 Lab2 思路)
    alignas(64) float lut[64][256]; // 上界
    for (int m = 0; m < PQM; ++m) {
        float* sub_q = query + m * sub_dim;
        float* m_soa = pq_centroids_soa + m * sub_dim * PQK;
        for (int c = 0; c < PQK; c += 4) {
            float32x4_t acc = vdupq_n_f32(0.0f);
            for (size_t d = 0; d < sub_dim; ++d) {
                acc = vfmaq_f32(acc, vdupq_n_f32(sub_q[d]),
                                vld1q_f32(m_soa + d * PQK + c));
            }
            vst1q_f32(&lut[m][c], acc);
        }
    }

    // 粗排: 选 nprobe 个簇
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivfpq_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // PQ-ADC 二级粗排
    size_t base_number = cluster_starts[nlist];  // 全库总向量数
    std::priority_queue<std::pair<float, uint32_t>> coarse_q;
    for (int c : probe_list) {
        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        for (size_t i = s; i < e; ++i) {
            float ip = 0.0f;
            for (int m = 0; m < PQM; ++m) {
                ip += lut[m][pq_codes_soa[m * base_number + i]];
            }
            float dis = 1.0f - ip;
            if (coarse_q.size() < rerank_k || dis < coarse_q.top().first) {
                coarse_q.push({dis, (uint32_t)i});
                if (coarse_q.size() > rerank_k) coarse_q.pop();
            }
        }
    }

    // 原始向量精排
    std::priority_queue<std::pair<float, uint32_t>> final_q;
    while (!coarse_q.empty()) {
        uint32_t pos = coarse_q.top().second;
        coarse_q.pop();
        float ip = ivfpq_neon_ip(query, reordered_base + pos * vecdim, vecdim);
        float dis = 1.0f - ip;
        if (final_q.size() < k || dis < final_q.top().first) {
            final_q.push({dis, reordered_ids[pos]});
            if (final_q.size() > k) final_q.pop();
        }
    }
    return final_q;
}

// ============== IVF-PQ-SIMD + 多线程 ==============
// 设计要点:
// 1) 粗排 (簇选择): nlist 不大, 单线程足够;
// 2) ADC 二级粗排: 按 nprobe 个簇 dynamic 调度并行, 每线程局部 top-p;
// 3) 精排: 各线程独立 rerank 自己的局部候选, 最后归并;
// 这样整个流水线无锁, 充分利用多核.
inline std::priority_queue<std::pair<float, uint32_t>>
ivf_pq_search_omp(float* query, size_t vecdim,
                  size_t k, int nprobe, size_t rerank_k,
                  int num_threads = 0)
{
    using namespace ivf_ns;
    using namespace ivfpq_ns;
    size_t sub_dim = vecdim / PQM;

    int nthr = (num_threads > 0) ? num_threads : omp_get_max_threads();

    // 阶段 1: LUT 构建 (子空间并行 + 跨 centroid SIMD)
    alignas(64) float lut[64][256];
    #pragma omp parallel for schedule(static) num_threads(nthr)
    for (int m = 0; m < PQM; ++m) {
        float* sub_q = query + m * sub_dim;
        float* m_soa = pq_centroids_soa + m * sub_dim * PQK;
        for (int c = 0; c < PQK; c += 4) {
            float32x4_t acc = vdupq_n_f32(0.0f);
            for (size_t d = 0; d < sub_dim; ++d) {
                acc = vfmaq_f32(acc, vdupq_n_f32(sub_q[d]),
                                vld1q_f32(m_soa + d * PQK + c));
            }
            vst1q_f32(&lut[m][c], acc);
        }
    }

    // 阶段 2: 粗排选 nprobe 簇 (单线程)
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivfpq_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // 阶段 3 + 4: 并行扫描 nprobe 个簇, 线程内粗排 + 精排
    std::vector<std::vector<std::pair<float, uint32_t>>> thread_results(nthr);
    size_t base_number = cluster_starts[nlist];

    #pragma omp parallel num_threads(nthr)
    {
        int tid = omp_get_thread_num();
        std::priority_queue<std::pair<float, uint32_t>> local_coarse;

        #pragma omp for schedule(dynamic, 1) nowait
        for (int pi = 0; pi < (int)probe_list.size(); ++pi) {
            int c = probe_list[pi];
            size_t s = cluster_starts[c];
            size_t e = cluster_starts[c + 1];
            for (size_t i = s; i < e; ++i) {
                float ip = 0.0f;
                for (int m = 0; m < PQM; ++m) {
                    ip += lut[m][pq_codes_soa[m * base_number + i]];
                }
                float dis = 1.0f - ip;
                if (local_coarse.size() < rerank_k || dis < local_coarse.top().first) {
                    local_coarse.push({dis, (uint32_t)i});
                    if (local_coarse.size() > rerank_k) local_coarse.pop();
                }
            }
        }

        // 线程内精排
        std::priority_queue<std::pair<float, uint32_t>> local_fine;
        while (!local_coarse.empty()) {
            uint32_t pos = local_coarse.top().second;
            local_coarse.pop();
            float ip = ivfpq_neon_ip(query, reordered_base + pos * vecdim, vecdim);
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

    // 归并
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

// ============================================================================
// IVF-PQ-SIMD + Pthread (策略 12)
//
// 集大成版本, 复合了之前 Pthread 策略的所有技巧:
//   - LUT 构建: tid==0 单独做, 用 pthread_barrier_t 同步 (类似策略 6)
//   - 二级粗排: 簇级动态调度, 用 pthread_mutex_t 保护任务计数器 (类似策略 9)
//   - 线程内精排: 各线程独立, 无锁
//   - 全局归并: T*k 元素
//
// 流程:
//   1. 主线程做 IVF 簇粗排 (单线程, 找出 nprobe 个簇)
//   2. 创建 num_threads 个 pthread
//   3. 每个线程:
//      a. (仅 tid==0) 做 LUT 构建
//      b. barrier 等待 LUT 完成
//      c. 循环从共享计数器取簇 idx, 处理: ADC + 维护局部 coarse top-p
//      d. 线程内 rerank: 用 float 原始向量重算局部 coarse 中的候选, 得局部 top-k
//      e. 把局部 top-k 写到 thread_results[tid]
//   4. join, 主线程归并所有 thread_results
// ============================================================================
struct IVFPQPthreadArgs {
    int tid;
    float* query;
    size_t vecdim;
    size_t k;
    size_t rerank_k;
    int* probe_list;
    int nprobe;

    float (*lut)[256];                              // 共享 LUT
    pthread_barrier_t* barrier;                     // LUT 阶段同步
    int* next_task_idx;                             // 共享任务计数器
    pthread_mutex_t* mtx;                           // 保护计数器
    std::vector<std::pair<float, uint32_t>>* result;  // 局部 top-k 输出
};

inline void* ivf_pq_pthread_worker(void* arg) {
    IVFPQPthreadArgs* a = (IVFPQPthreadArgs*)arg;
    using namespace ivf_ns;
    using namespace ivfpq_ns;
    size_t sub_dim = a->vecdim / PQM;
    size_t base_number = cluster_starts[nlist];

    // ===== 阶段 1: LUT 构建 (tid==0 串行做) =====
    if (a->tid == 0) {
        for (int m = 0; m < PQM; ++m) {
            float* sub_q = a->query + m * sub_dim;
            float* m_soa = pq_centroids_soa + m * sub_dim * PQK;
            for (int c = 0; c < PQK; c += 4) {
                float32x4_t acc = vdupq_n_f32(0.0f);
                for (size_t d = 0; d < sub_dim; ++d) {
                    acc = vfmaq_f32(acc, vdupq_n_f32(sub_q[d]),
                                    vld1q_f32(m_soa + d * PQK + c));
                }
                vst1q_f32(&a->lut[m][c], acc);
            }
        }
    }
    pthread_barrier_wait(a->barrier);  // 等 LUT 完成

    // ===== 阶段 2: 簇级动态调度 + PQ-ADC 粗排 =====
    std::priority_queue<std::pair<float, uint32_t>> local_coarse;
    while (true) {
        pthread_mutex_lock(a->mtx);
        int task = (*a->next_task_idx)++;
        pthread_mutex_unlock(a->mtx);
        if (task >= a->nprobe) break;

        int c = a->probe_list[task];
        size_t s = cluster_starts[c];
        size_t e = cluster_starts[c + 1];
        for (size_t i = s; i < e; ++i) {
            float ip = 0.0f;
            for (int m = 0; m < PQM; ++m) {
                ip += a->lut[m][pq_codes_soa[m * base_number + i]];
            }
            float dis = 1.0f - ip;
            if (local_coarse.size() < a->rerank_k || dis < local_coarse.top().first) {
                local_coarse.push({dis, (uint32_t)i});  // i = 重排后位置
                if (local_coarse.size() > a->rerank_k) local_coarse.pop();
            }
        }
    }

    // ===== 阶段 3: 线程内精排 (用 float 原始向量重算) =====
    std::priority_queue<std::pair<float, uint32_t>> local_fine;
    while (!local_coarse.empty()) {
        uint32_t pos = local_coarse.top().second;  // 重排后位置
        local_coarse.pop();
        float ip = ivfpq_neon_ip(a->query, reordered_base + pos * a->vecdim, a->vecdim);
        float dis = 1.0f - ip;
        uint32_t orig_id = reordered_ids[pos];     // 映射回原始 ID
        if (local_fine.size() < a->k || dis < local_fine.top().first) {
            local_fine.push({dis, orig_id});
            if (local_fine.size() > a->k) local_fine.pop();
        }
    }

    // 输出局部 top-k
    a->result->reserve(a->k);
    while (!local_fine.empty()) {
        a->result->push_back(local_fine.top());
        local_fine.pop();
    }
    return nullptr;
}

inline std::priority_queue<std::pair<float, uint32_t>>
ivf_pq_search_pthread(float* query, size_t vecdim, size_t k,
                      int nprobe, size_t rerank_k, int num_threads = 4)
{
    using namespace ivf_ns;
    using namespace ivfpq_ns;

    // ===== 主线程: IVF 簇粗排 (找出 nprobe 个簇) =====
    std::priority_queue<std::pair<float, int>> coarse;
    for (int c = 0; c < nlist; ++c) {
        float ip = ivfpq_neon_ip(query, centroids + c * vecdim, vecdim);
        float dis = 1.0f - ip;
        if ((int)coarse.size() < nprobe || dis < coarse.top().first) {
            coarse.push({dis, c});
            if ((int)coarse.size() > nprobe) coarse.pop();
        }
    }
    std::vector<int> probe_list;
    probe_list.reserve(nprobe);
    while (!coarse.empty()) { probe_list.push_back(coarse.top().second); coarse.pop(); }

    // ===== 创建 pthread, 跑后续三阶段 =====
    alignas(64) float lut[64][256];  // 上界, 实际用 PQM x PQK
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, nullptr, num_threads);
    int next_task_idx = 0;
    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx, nullptr);

    std::vector<pthread_t> tids(num_threads);
    std::vector<IVFPQPthreadArgs> args(num_threads);
    std::vector<std::vector<std::pair<float, uint32_t>>> results(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        args[t] = {t, query, vecdim, k, rerank_k,
                   probe_list.data(), nprobe,
                   lut, &barrier, &next_task_idx, &mtx, &results[t]};
        pthread_create(&tids[t], nullptr, ivf_pq_pthread_worker, &args[t]);
    }
    for (int t = 0; t < num_threads; ++t) pthread_join(tids[t], nullptr);
    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&mtx);

    // ===== 全局归并 =====
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
