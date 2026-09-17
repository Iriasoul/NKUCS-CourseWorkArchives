// main.cu —— 期末融合实验 唯一入口（仿 lab04/lab05：单主程序 + 全头文件）
/*
  strategy 分发表：
    S0 = Flat 暴力基线      (CPU, scalar vs AVX2)         —— 定加速比分母              [flat.h]
    S1 = CPU IVF / IVF-PQ   (AVX2 + OpenMP, 两阶段重排)     —— CPU 侧优化算法上限        [cpu_search.h]
    S2 = GPU 粗排           (cuBLAS GEMM + 设备 Top-k)      —— GPU-only 精确基线         [gpu_coarse.cuh]
    S3 = 融合一·异构级联     (GPU 近似粗筛 + CPU 精排, 双缓冲) —— 流水线重叠              [hetero_cascade.cuh]
    S4 = 融合一·异构分流     (GPU 段 ∥ CPU 段, 查询流切分)    —— 数据并行, 吞吐叠加        [hetero_split.cuh]

  编译：  build.bat            （一条 nvcc 命令，-Xcompiler /openmp /arch:AVX2 /utf-8 /O2）
  运行：  main.exe [strategy] [data_dir] [p1] [p2] [p3]
           S0/S2: p1=batch_B
           S1   : p1=threads
           S3   : p1=batch_B p2=rerank_k p3=threads
           S4   : p1=batch_B p2=threads  p3=nprobe
*/
#include "bench_common.h"
#include "flat.h"
#include "cpu_search.h"
#include "gpu_util.cuh"
#include "gpu_coarse.cuh"
#include "hetero_cascade.cuh"
#include "hetero_split.cuh"

int main(int argc, char** argv) {
    int strategy       = (argc > 1) ? atoi(argv[1]) : 4;
    std::string dir    = (argc > 2) ? argv[2] : "../lab05/ann/data/";
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += "/";

    // 数据只加载一次（口径与 lab04/lab05 一致）
    size_t nq, dq, ngt, dgt, nb, db;
    float* query = LoadData<float>(dir + ANN_QUERY_FILE, nq, dq);
    int*   gt    = LoadData<int>  (dir + ANN_GT_FILE,    ngt, dgt);
    float* base  = LoadData<float>(dir + ANN_BASE_FILE,  nb, db);
    const size_t d = db, k = 10, M = std::min<size_t>(2000, std::min(nq, ngt));

    int dev = 0; cudaDeviceProp p;
    if (cudaGetDevice(&dev) == cudaSuccess && cudaGetDeviceProperties(&p, dev) == cudaSuccess)
        std::printf("GPU: %s (%.1f GB)\n", p.name, p.totalGlobalMem / 1073741824.0);
    std::printf("strategy=%d  data=%s\n", strategy, dir.c_str());

    switch (strategy) {
        case 0: run_flat(base, nb, query, M, d, k, gt, dgt); break;
        case 1: {
            int threads = (argc > 3) ? atoi(argv[3]) : 8;
            run_cpu_ivf(base, nb, query, M, d, k, gt, dgt, threads); break;
        }
        case 2: {
            int batch_B = (argc > 3) ? atoi(argv[3]) : 512;
            run_gpu_coarse(base, nb, query, M, d, k, gt, dgt, batch_B); break;
        }
        case 3: {
            int batch_B  = (argc > 3) ? atoi(argv[3]) : 512;
            int rerank_k = (argc > 4) ? atoi(argv[4]) : 100;
            int threads  = (argc > 5) ? atoi(argv[5]) : 8;
            run_hetero_cascade(base, nb, query, M, d, k, gt, dgt, batch_B, rerank_k, threads); break;
        }
        case 4: {
            int batch_B = (argc > 3) ? atoi(argv[3]) : 512;
            int threads = (argc > 4) ? atoi(argv[4]) : 8;
            int nprobe  = (argc > 5) ? atoi(argv[5]) : 16;
            run_hetero_split(base, nb, query, M, d, k, gt, dgt, batch_B, threads, nprobe); break;
        }
        default: std::printf("unknown strategy %d (expect 0..4)\n", strategy);
    }

    delete[] query; delete[] gt; delete[] base;
    return 0;
}
