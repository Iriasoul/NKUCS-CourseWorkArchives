// hetero_split.cuh —— strategy 4：融合一·异构数据并行分流（CPU–GPU 查询流分流）
// GPU 与 CPU 各处理一部分 query，同时运行 → 合并吞吐 ≈ qps_gpu + qps_cpu。
//   GPU 分支：cuBLAS 精确 GEMM + 设备 Top-k（recall≈1.0）
//   CPU 分支：IVF(AVX2+OpenMP) 近似检索
// GPU 段异步入队后立即返回，CPU 段在主线程跑 → 二者真正并行；最后同步归并。
// 扫描 GPU 占比 f，寻找使 max(GPU时间, CPU时间) 最小的平衡点。
#pragma once
#include "cpu_index.h"      // build_ivf / cpu_ivf_search_range / free_ivf
#include "gpu_util.cuh"

// strategy 4 入口
inline void run_hetero_split(const float* base, size_t N_, const float* query, size_t M,
                             size_t d_, size_t k, const int* gt, size_t gtd,
                             int batch_B, int nthr, int nprobe) {
    const int N = (int)N_, d = (int)d_;
    const int nlist = 256;
    const double BASE = BENCH_BASELINE_US;
    std::printf("\n== [S4] 融合一(数据并行) 异构 CPU-GPU 查询分流 ==  CPU=%d线程 IVF(nprobe=%d)\n", nthr, nprobe);
    std::printf("N=%d d=%d M=%zu k=%zu batch_B=%d  baseline=%.1f us/q\n\n", N, d, M, k, batch_B, BASE);

    // CPU IVF 索引
    CpuTimer bt; bt.start(); build_ivf(base, N, d, nlist);
    std::printf("build CPU IVF: %.0f ms\n", bt.stop_ms());

    // GPU 常驻
    cublasHandle_t h; CUBLAS_CHECK(cublasCreate(&h));
    float* dB; CUDA_CHECK(cudaMalloc(&dB, (size_t)N*d*sizeof(float)));
    CUDA_CHECK(cudaMemcpy(dB, base, (size_t)N*d*sizeof(float), cudaMemcpyHostToDevice));
    float *dQ, *dS; uint32_t* dIdx;
    CUDA_CHECK(cudaMalloc(&dQ,  (size_t)batch_B*d*sizeof(float)));
    CUDA_CHECK(cudaMalloc(&dS,  (size_t)batch_B*N*sizeof(float)));
    CUDA_CHECK(cudaMalloc(&dIdx,(size_t)batch_B*k*sizeof(uint32_t)));
    // query / 结果 用锁页内存，保证 H2D/D2H 异步（否则从可分页内存拷贝会变同步、无法与 CPU 重叠）
    CUDA_CHECK(cudaHostRegister((void*)query, (size_t)M*d*sizeof(float), cudaHostRegisterDefault));
    uint32_t* hIdx; CUDA_CHECK(cudaHostAlloc(&hIdx, (size_t)M*k*sizeof(uint32_t), cudaHostAllocDefault));
    cudaStream_t st; CUDA_CHECK(cudaStreamCreate(&st));
    const float alpha = 1.f, beta = 0.f; int nt = 256; size_t sh = (size_t)nt*(sizeof(float)+sizeof(int));

    // 异步入队 GPU 处理 query[lo,hi)（单流、逐 batch，全部非阻塞）
    auto gpu_async_range = [&](size_t lo, size_t hi) {
        CUBLAS_CHECK(cublasSetStream(h, st));
        for (size_t off = lo; off < hi; off += batch_B) {
            int B = (int)std::min<size_t>(batch_B, hi - off);
            CUDA_CHECK(cudaMemcpyAsync(dQ, query+off*d, (size_t)B*d*sizeof(float), cudaMemcpyHostToDevice, st));
            CUBLAS_CHECK(cublasSgemm(h, CUBLAS_OP_T, CUBLAS_OP_N, N, B, d, &alpha, dB, d, dQ, d, &beta, dS, N));
            size_t tot = (size_t)B*N; ip_to_dist<<<(int)((tot+255)/256), 256, 0, st>>>(dS, tot);
            topk_rows<<<B, nt, sh, st>>>(dS, N, (int)k, dIdx, nullptr);
            CUDA_CHECK(cudaMemcpyAsync(hIdx+off*k, dIdx, (size_t)B*k*sizeof(uint32_t), cudaMemcpyDeviceToHost, st));
        }
    };

    std::vector<std::vector<uint32_t>> res(M);
    // 一次分流执行：GPU 段 [0,g) 异步 + CPU 段 [g,M) 同步并行
    auto run = [&](double f, bool fill)->double {
        size_t g = (size_t)(f*M + 0.5); if (g > M) g = M;
        for (auto& r : res) r.clear();
        CpuTimer t; t.start();
        if (g > 0) gpu_async_range(0, g);                            // 非阻塞入队
        if (g < M) cpu_ivf_search_range(query, d, k, nprobe, g, M, res, nthr); // CPU 与 GPU 并行
        CUDA_CHECK(cudaStreamSynchronize(st));                       // 等 GPU 段完成
        double ms = t.stop_ms();
        if (fill) for (size_t i=0;i<g;++i) res[i].assign(hIdx+i*k, hIdx+(i+1)*k);
        return ms;
    };

    auto timed = [&](double f)->std::pair<double,double> {
        run(f, true); double rec = compute_recall(res, gt, gtd, k);
        run(f, false); double s = 0; const int R = 3; for (int r=0;r<R;++r) s += run(f, false); s /= R;
        return {rec, s};
    };

    std::printf("\ngpu_frac,recall,us_per_query,qps,speedup_vs_baseline\n");
    for (double f : {1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.0}) {
        auto pr = timed(f); double ms = pr.second, rec = pr.first;
        std::printf("%.2f,%.4f,%.1f,%.0f,%.2f\n", f, rec, ms/M*1000.0, M/(ms/1000.0), BASE/(ms/M*1000.0));
    }
    std::printf("(gpu_frac=1.0 → 纯GPU；0.0 → 纯CPU；中间为异构分流)\n====================\n");

    cudaHostUnregister((void*)query);
    cudaFree(dB); cudaFree(dQ); cudaFree(dS); cudaFree(dIdx); cudaFreeHost(hIdx);
    cudaStreamDestroy(st); cublasDestroy(h); free_ivf();
}
