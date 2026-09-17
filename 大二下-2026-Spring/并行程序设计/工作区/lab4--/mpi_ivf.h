#pragma once
// ===========================================================================
//  mpi_ivf.h —— MPI-IVF 系列 (A 组)
//  阶段1 已实现:
//    · serial_ivf_run      策略 0: 串行 IVF 基线 (仅 rank0, 全量 base) → 加速比基准 T1
//    · mpi_ivf_run_gather  策略 1: MPI-IVF, block/cyclic 划分 + Gather 归并
//  说明: 索引的"构建"放在 main.cc 里统一做并计时, 这里的 run 函数只负责"在线搜索".
//        各 rank 在自己分片上调用已有的 ivf_search_simd, 返回的是局部下标,
//        通过 shard_ids 映射回全局原始 id 后再归并 (见 mpi_flatten_local).
// ===========================================================================

#include <mpi.h>
#include <vector>
#include <queue>
#include <cstdint>
#include <omp.h>

#include "ivf.h"
#include "mpi_common.h"

// 离线: 在本 rank 的数据分片上构建 IVF 索引, 并返回该分片的全局 id 映射表
inline void
mpi_ivf_build_local(const float* full_base, size_t N, size_t vecdim,
                    int nlist, PartitionScheme part, int rank, int P,
                    std::vector<uint32_t>& shard_ids /*out*/)
{
    shard_ids = mpi_compute_shard_ids(N, rank, P, part);
    float* local_base = mpi_materialize_local_base(full_base, vecdim, shard_ids);
    build_ivf(local_base, shard_ids.size(), vecdim, nlist);
    delete[] local_base;   // build_ivf 内部已 memcpy 到 reordered_base, 可释放
}

// ---- 策略 0: 串行 IVF 基线 (仅 rank0, 全量 base; 索引已在 main 中建好) ----
inline RunResult
serial_ivf_run(const float* queries, size_t test_number, size_t vecdim,
               size_t k, int nprobe, const int* gt, size_t gt_d, int rank)
{
    RunResult r;
    if (rank != 0) return r;        // 其余 rank 不参与
    omp_set_num_threads(1);

    double recall_sum = 0.0;
    double t0 = MPI_Wtime();
    for (size_t qi = 0; qi < test_number; ++qi) {
        // 全量索引下 reordered_ids 即原始全局 id, 无需再映射
        auto res = ivf_search_simd((float*)(queries + qi * vecdim),
                                   vecdim, k, nprobe);
        recall_sum += mpi_recall_one(res, gt + qi * gt_d, k);
    }
    double t1 = MPI_Wtime();

    r.total_sec   = t1 - t0;
    r.avg_latency = r.total_sec * 1e6 / (double)test_number;
    r.recall      = recall_sum / (double)test_number;
    return r;
}

// ---- 策略 1: MPI-IVF, block/cyclic 划分 + 逐查询 Gather 归并 ----
//   每个 rank: 在本地分片 IVF 上取 top-(local_topk), 映射回全局 id, 拍平成定长数组;
//   归并: 用两次 MPI_Gather 把 (dist[], id[]) 收到 root, root 合并出全局 top-k 并算召回.
inline RunResult
mpi_ivf_run_gather(const float* queries, size_t test_number, size_t vecdim,
                   size_t k, int nprobe, size_t local_topk,
                   const int* gt, size_t gt_d,
                   const std::vector<uint32_t>& shard_ids,
                   MPI_Comm comm)
{
    int rank, P;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &P);

    omp_set_num_threads(1);   // 纯 MPI 策略: 进程内单线程, 避免与 OMP 抢核

    std::vector<float>    sd, sid_f;       // 本 rank 拍平结果 (定长 local_topk)
    std::vector<uint32_t> sid;
    std::vector<float>    rd;              // root 收集缓冲 (P * local_topk)
    std::vector<uint32_t> rid;
    if (rank == 0) {
        rd.resize(local_topk * (size_t)P);
        rid.resize(local_topk * (size_t)P);
    }

    double recall_sum = 0.0;
    MPI_Barrier(comm);
    double t_start = MPI_Wtime();

    for (size_t qi = 0; qi < test_number; ++qi) {
        const float* q = queries + qi * vecdim;

        // 本地搜索 (top local_topk, 返回局部下标) → 映射回全局 id → 拍平
        auto local = ivf_search_simd((float*)q, vecdim, local_topk, nprobe);
        mpi_flatten_local(local, shard_ids, local_topk, sd, sid);

        // 归并: 两个定长数组各做一次 Gather 到 root
        MPI_Gather(sd.data(),  (int)local_topk, MPI_FLOAT,
                   rank == 0 ? rd.data() : nullptr,  (int)local_topk, MPI_FLOAT, 0, comm);
        MPI_Gather(sid.data(), (int)local_topk, MPI_UINT32_T,
                   rank == 0 ? rid.data() : nullptr, (int)local_topk, MPI_UINT32_T, 0, comm);

        if (rank == 0) {
            auto res = mpi_merge_topk(rd, rid, k);
            recall_sum += mpi_recall_one(res, gt + qi * gt_d, k);
        }
    }

    double t_end = MPI_Wtime();

    RunResult r;
    if (rank == 0) {
        r.total_sec   = t_end - t_start;
        r.avg_latency = r.total_sec * 1e6 / (double)test_number;
        r.recall      = recall_sum / (double)test_number;
    }
    return r;
}
