// ===========================================================================
//  main.cc —— 并行程序设计 lab4 (MPI 编程)
//  在 lab2-SIMD / lab3-多线程 基础上做 MPI 多进程并行.
//
//  策略表 (在线查询所用的并行方式):
//   0  = Serial IVF baseline        单进程全量搜索, 加速比基准 T1 (建议 -np 1 运行)
//   --- A 组: MPI-IVF (对应 4.1) ---
//   1  = MPI-IVF  block/cyclic + Gather归并        核心基础版 (np=1 时应≈策略0)
//   2  = MPI-IVF  cyclic 划分 + Gather归并          循环划分对比         [待实现]
//   3  = MPI-IVF  block + 树形Reduce归并(自定义Op)  对比 Gather          [待实现]
//   4  = MPI-IVF  block + OpenMP 混合 (P×T)         簇级 OMP 动态调度    [待实现]
//   5  = MPI-IVF  批量通信 (一次性归并全部 query)   摊薄通信开销         [待实现]
//   6  = MPI-IVF  非阻塞通信/计算重叠 (流水线)      Isend/Irecv overlap  [待实现]
//   --- B 组: MPI-IVF-PQ (4.1 进阶迁移) ---
//   7  = MPI-IVF-PQ 方法一(全局PQ)  纯 MPI                                [待实现]
//   8  = MPI-IVF-PQ 方法一(全局PQ)  + OpenMP                              [待实现]
//   9  = MPI-IVF-PQ 方法二(簇内独立PQ) 纯 MPI                             [待实现]
//   10 = MPI-IVF-PQ 方法二(簇内独立PQ) + OpenMP                           [待实现]
//   --- C 组: 图索引 (对应 4.2) ---
//   11 = MPI-HNSW   多分块 + Merge (纯 MPI, 每进程一张子图)               [待实现]
//   12 = MPI-HNSW   多分块 + OpenMP                                       [待实现]
//   13 = MPI IVF+HNSW 混合 (每簇建 HNSW, 簇间 MPI 并行)                   [待实现]
//   14 = HNSW-on-HNSW (分块建子HNSW + 顶层路由HNSW决定激活哪些分块)       [待实现]
//
//  注: 切换策略 = 修改下方 `strategy` 常量后重新 make;
//      进程数 P 由 mpiexec 的 -np 决定 (改 qsub_mpi.sh).
// ===========================================================================

#include <mpi.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstddef>
#include <omp.h>

#include "flat_scan.h"     // lab2 基线 (本 lab 不直接用, 保留以与框架一致)
#include "ivf.h"
#include "mpi_common.h"
#include "mpi_ivf.h"
// 后续阶段将启用:
// #include "mpi_ivf_pq.h"
// #include "mpi_hnsw.h"

// 读二进制向量数据: 前 8 字节为 (n, d) 各 4 字节, 随后 n*d 个 T
template<typename T>
T* LoadData(const std::string& data_path, size_t& n, size_t& d) {
    std::ifstream fin(data_path, std::ios::in | std::ios::binary);
    if (!fin.is_open()) {
        std::cerr << "Failed to open " << data_path << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    n = 0; d = 0;                       // 仅读低 4 字节, 必须先清零
    fin.read((char*)&n, 4);
    fin.read((char*)&d, 4);
    T* data = new T[n * d];
    int sz = sizeof(T);
    for (size_t i = 0; i < n; ++i)
        fin.read(((char*)data + i * d * sz), d * sz);
    fin.close();
    return data;
}

int main(int argc, char** argv) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    int rank = 0, P = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    // ============ 配置区 ============
    int    strategy   = 1;                 // 0..14, 见上表
    int    nlist      = 256;               // IVF 簇数
    int    nprobe     = 16;                // IVF 探查簇数
    size_t k          = 10;                // top-k
    size_t local_topk = k;                 // 每 rank 上报候选数 (>=k 可换更高召回)
    int    rerank_k   = 500;               // (B 组 PQ 精排候选, 暂未用)
    PartitionScheme part = PART_BLOCK;     // PART_BLOCK / PART_CYCLIC
    size_t test_number = 2000;             // 测试 query 数
    int    online_omp  = 4;                // (混合策略的进程内线程数, 暂未用)
    (void)rerank_k; (void)online_omp;

    std::string data_path = "/anndata/";
    // ================================

    // ---- 数据加载 ----
    size_t qn = 0, vecdim = 0, gtn = 0, gt_d = 0, bn = 0, bvd = 0;
    float* query = LoadData<float>(data_path + "DEEP100K.query.fbin", qn, vecdim);
    float* base  = LoadData<float>(data_path + "DEEP100K.base.100k.fbin", bn, bvd);
    int*   gt    = nullptr;
    if (rank == 0)
        gt = LoadData<int>(data_path + "DEEP100K.gt.query.100k.top100.bin", gtn, gt_d);

    if (test_number > qn) test_number = qn;
    ivf_ns::nlist = nlist;

    if (rank == 0) {
        std::cout << "==== lab4 MPI ====\n"
                  << "strategy=" << strategy << "  P=" << P
                  << "  N=" << bn << "  vecdim=" << vecdim
                  << "  k=" << k << "  nprobe=" << nprobe
                  << "  part=" << (part == PART_BLOCK ? "block" : "cyclic")
                  << "  local_topk=" << local_topk << "\n";
    }

    // ---- 索引构建 (离线, 计时但不计入查询延迟) + 在线搜索 ----
    RunResult res;
    std::vector<uint32_t> shard_ids;
    double tb0 = MPI_Wtime();

    switch (strategy) {
        case 0: {   // 串行基线: 仅 rank0 全量建 IVF
            if (rank == 0) build_ivf(base, bn, vecdim, nlist);
            double tb1 = MPI_Wtime();
            res = serial_ivf_run(query, test_number, vecdim, k, nprobe, gt, gt_d, rank);
            if (rank == 0) { res.build_sec = tb1 - tb0; free_ivf(); }
            break;
        }
        case 1: {   // MPI-IVF block/cyclic + Gather
            omp_set_num_threads(1);    // 建索引也单线程, 避免多 rank 在同节点过度订阅
            mpi_ivf_build_local(base, bn, vecdim, nlist, part, rank, P, shard_ids);
            double my_build = MPI_Wtime() - tb0, max_build = my_build;
            MPI_Reduce(&my_build, &max_build, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
            res = mpi_ivf_run_gather(query, test_number, vecdim, k, nprobe,
                                     local_topk, gt, gt_d, shard_ids, MPI_COMM_WORLD);
            if (rank == 0) res.build_sec = max_build;
            free_ivf();
            break;
        }
        default:
            if (rank == 0)
                std::cerr << "[strategy " << strategy << "] 尚未实现 (后续阶段补上)\n";
    }

    // ---- 结果输出 (仅 root) ----
    if (rank == 0) {
        std::cout << "Avg Recall:  " << res.recall  << "\n"
                  << "Avg Latency: " << res.avg_latency << " us\n"
                  << "Total:       " << res.total_sec << " s   QPS="
                  << (res.total_sec > 0 ? test_number / res.total_sec : 0) << "\n"
                  << "Build:       " << res.build_sec << " s\n"
                  << "====================\n";
    }

    delete[] query;
    delete[] base;
    if (rank == 0 && gt) delete[] gt;

    MPI_Finalize();
    return 0;
}
