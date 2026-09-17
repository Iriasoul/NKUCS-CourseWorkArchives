// Lab3 - Flat-SIMD 单线程基线 + 多线程并行
// 0) SIMD 单线程基线: 用于多线程加速比对比的起点 (等价于 Lab2 Flat-SIMD)
// 1) OpenMP 版本: 基于 base 向量划分并行 + 局部 top-k + 最终归并
// 2) Pthread 版本: 手动管理线程, 用于对比
#pragma once

#include <queue>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <pthread.h>
#include <omp.h>
#include <arm_neon.h>
#include "thread_pool.h"

// --------------------------- 内积辅助 ---------------------------
static inline float neon_ip(const float* a, const float* b, size_t dim) {
    float32x4_t s = vdupq_n_f32(0.0f);
    size_t d = 0;
    for (; d + 3 < dim; d += 4) {
        s = vfmaq_f32(s, vld1q_f32(a + d), vld1q_f32(b + d));
    }
    float sum = vaddvq_f32(s);
    for (; d < dim; ++d) sum += a[d] * b[d];
    return sum;
}

// --------------------------- 单线程基线 (策略 0) ---------------------------
// Flat-SIMD 单线程版本, 用作策略 1/2 多线程加速比的基线.
// 等价于 Lab2 的 Flat-SIMD 实现, 仅作为 Lab3 多线程对比的起点.
// 与策略 1 的区别: 不进入 OpenMP parallel 区域, 没有任何线程开销.
inline std::priority_queue<std::pair<float, uint32_t>>
flat_search_simd(float* base, float* query,
                 size_t base_number, size_t vecdim, size_t k)
{
    std::priority_queue<std::pair<float, uint32_t>> q;
    for (size_t i = 0; i < base_number; ++i) {
        float32x4_t sum_v = vdupq_n_f32(0.0f);
        float* cur = base + i * vecdim;
        size_t d = 0;
        for (; d + 3 < vecdim; d += 4) {
            sum_v = vfmaq_f32(sum_v, vld1q_f32(cur + d), vld1q_f32(query + d));
        }
        float ip = vaddvq_f32(sum_v);
        for (; d < vecdim; ++d) ip += cur[d] * query[d];
        float dis = 1.0f - ip;

        if (q.size() < k || dis < q.top().first) {
            q.push({dis, (uint32_t)i});
            if (q.size() > k) q.pop();
        }
    }
    return q;
}

// --------------------------- OpenMP 版本 ---------------------------
// base 向量划分并行: 每个线程负责一段 base 区间, 内部维护局部 top-k,
// 最终把所有局部队列归并为全局 top-k.
// 这里设计每个线程的局部 top-p = k (因为是精确距离, p=k 足以保证全局正确)
//
// num_threads = 0 表示沿用 OMP 默认值; >0 时显式指定本次查询使用的线程数
inline std::priority_queue<std::pair<float, uint32_t>>
flat_search_omp(float* base, float* query,
                size_t base_number, size_t vecdim, size_t k,
                int num_threads = 0)
{
    int nthr = (num_threads > 0) ? num_threads : omp_get_max_threads();
    std::vector<std::priority_queue<std::pair<float, uint32_t>>> local_q(nthr);

    #pragma omp parallel num_threads(nthr)
    {
        int tid = omp_get_thread_num();
        auto& q = local_q[tid];

        #pragma omp for schedule(static)
        for (size_t i = 0; i < base_number; ++i) {
            float32x4_t sum_v = vdupq_n_f32(0.0f);
            float* cur = base + i * vecdim;
            size_t d = 0;
            for (; d + 3 < vecdim; d += 4) {
                sum_v = vfmaq_f32(sum_v, vld1q_f32(cur + d), vld1q_f32(query + d));
            }
            float ip = vaddvq_f32(sum_v);
            for (; d < vecdim; ++d) ip += cur[d] * query[d];
            float dis = 1.0f - ip;

            if (q.size() < k || dis < q.top().first) {
                q.push({dis, (uint32_t)i});
                if (q.size() > k) q.pop();
            }
        }
    }

    // 归并 -- 仅 max_threads * k 个元素, 开销很小
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

// --------------------------- Pthread 版本 ---------------------------
struct FlatPthreadArgs {
    float* base;
    float* query;
    size_t start, end;
    size_t vecdim;
    size_t k;
    std::priority_queue<std::pair<float, uint32_t>>* result;
};

inline void* flat_pthread_worker(void* arg) {
    FlatPthreadArgs* a = (FlatPthreadArgs*)arg;
    auto& q = *a->result;
    for (size_t i = a->start; i < a->end; ++i) {
        float32x4_t sum_v = vdupq_n_f32(0.0f);
        float* cur = a->base + i * a->vecdim;
        size_t d = 0;
        for (; d + 3 < a->vecdim; d += 4) {
            sum_v = vfmaq_f32(sum_v, vld1q_f32(cur + d), vld1q_f32(a->query + d));
        }
        float ip = vaddvq_f32(sum_v);
        for (; d < a->vecdim; ++d) ip += cur[d] * a->query[d];
        float dis = 1.0f - ip;

        if (q.size() < a->k || dis < q.top().first) {
            q.push({dis, (uint32_t)i});
            if (q.size() > a->k) q.pop();
        }
    }
    return nullptr;
}

inline std::priority_queue<std::pair<float, uint32_t>>
flat_search_pthread(float* base, float* query,
                    size_t base_number, size_t vecdim, size_t k,
                    int num_threads = 4, ThreadPool* pool = nullptr)
{
    std::vector<FlatPthreadArgs> args(num_threads);
    std::vector<std::priority_queue<std::pair<float, uint32_t>>> results(num_threads);

    size_t chunk = (base_number + num_threads - 1) / num_threads;
    for (int t = 0; t < num_threads; ++t) {
        args[t].base   = base;
        args[t].query  = query;
        args[t].start  = (size_t)t * chunk;
        args[t].end    = std::min(base_number, (size_t)(t + 1) * chunk);
        args[t].vecdim = vecdim;
        args[t].k      = k;
        args[t].result = &results[t];
    }

    if (pool) {
        // 线程池路径: 复用已有线程, 无创建/销毁开销
        pool->dispatch([&](int t) { flat_pthread_worker(&args[t]); });
    } else {
        // 回退路径: 每次查询创建线程 (仅在无 pool 时使用)
        std::vector<pthread_t> tids(num_threads);
        for (int t = 0; t < num_threads; ++t)
            pthread_create(&tids[t], nullptr, flat_pthread_worker, &args[t]);
        for (int t = 0; t < num_threads; ++t)
            pthread_join(tids[t], nullptr);
    }

    std::priority_queue<std::pair<float, uint32_t>> final_q;
    for (auto& q : results) {
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
