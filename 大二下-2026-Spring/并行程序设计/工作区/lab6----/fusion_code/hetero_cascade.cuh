// hetero_cascade.cuh —— strategy 3：融合一·单机异构 CPU–GPU 协同级联（新方法二：异构流水线）
//
//   L0 粗筛(GPU) : cuBLAS GEMM(FAST_16F 近似内积) + 设备侧 Top-rerank_k → 候选 id
//   L2 精排(CPU) : OpenMP + AVX2 对候选做精确 FP32 内积 → 最终 Top-k（恢复粗筛近似的精度损失）
//   重叠机制     : 2 条 CUDA stream + 双缓冲，GPU 算 batch b+1 时 CPU 精排 batch b
//
// 三种执行方式对照：
//   [A] GPU-only-exact : FP32 精确 GEMM + 设备 Top-k，纯 GPU ——上限参照
//   [B] hetero-serial  : GPU 近似粗筛 + CPU 精排，串行（不重叠）
//   [C] hetero-overlap : 同 B 但双缓冲重叠 —— 量化重叠效率
#pragma once
#include "cpu_index.h"      // cpu_rerank_batch (OpenMP + AVX2)
#include "gpu_util.cuh"
#include <functional>

// strategy 3 入口
inline void run_hetero_cascade(const float* base, size_t N_, const float* query, size_t M,
                               size_t d_, size_t k, const int* gt, size_t gtd,
                               int batch_B, int rerank_k, int nthr) {
    const int N = (int)N_, d = (int)d_;
    const double BASE = BENCH_BASELINE_US;
    std::printf("\n== [S3] 融合一 异构 CPU-GPU 协同级联 ==\n");
    std::printf("N=%d d=%d M=%zu k=%zu batch_B=%d rerank_k=%d cpu_threads=%d  baseline=%.1f us/q\n\n",
                N, d, M, k, batch_B, rerank_k, nthr, BASE);

    cublasHandle_t h; CUBLAS_CHECK(cublasCreate(&h));
    float* dB; CUDA_CHECK(cudaMalloc(&dB, (size_t)N*d*sizeof(float)));
    CUDA_CHECK(cudaMemcpy(dB, base, (size_t)N*d*sizeof(float), cudaMemcpyHostToDevice));

    const float alpha = 1.f, beta = 0.f;
    int nt = 256; size_t sh = (size_t)nt * (sizeof(float) + sizeof(int));

    // [A] GPU-only-exact
    auto run_gpu_exact = [&](bool timed, std::vector<std::vector<uint32_t>>* res)->float {
        float *dQ, *dS; uint32_t* dIdx;
        CUDA_CHECK(cudaMalloc(&dQ,  (size_t)batch_B*d*sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dS,  (size_t)batch_B*N*sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dIdx,(size_t)batch_B*k*sizeof(uint32_t)));
        CUBLAS_CHECK(cublasSetStream(h, 0));
        GpuTimer gt; if (timed) gt.start();
        for (size_t off = 0; off < M; off += batch_B) {
            int B = (int)std::min<size_t>(batch_B, M - off);
            CUDA_CHECK(cudaMemcpy(dQ, query+off*d, (size_t)B*d*sizeof(float), cudaMemcpyHostToDevice));
            CUBLAS_CHECK(cublasSgemm(h, CUBLAS_OP_T, CUBLAS_OP_N, N, B, d, &alpha, dB, d, dQ, d, &beta, dS, N));
            size_t tot = (size_t)B*N; ip_to_dist<<<(int)((tot+255)/256), 256>>>(dS, tot);
            topk_rows<<<B, nt, sh>>>(dS, N, (int)k, dIdx, nullptr);
            std::vector<uint32_t> hIdx((size_t)B*k);
            CUDA_CHECK(cudaMemcpy(hIdx.data(), dIdx, (size_t)B*k*sizeof(uint32_t), cudaMemcpyDeviceToHost));
            if (res) for (int i=0;i<B;++i) (*res)[off+i].assign(hIdx.begin()+(size_t)i*k, hIdx.begin()+(size_t)(i+1)*k);
        }
        float ms = timed ? gt.stop_ms() : 0.f;
        cudaFree(dQ); cudaFree(dS); cudaFree(dIdx); return ms;
    };

    // GPU 近似粗筛（FAST_16F GEMM + 设备 Top-rerank_k），候选 id 异步回传 host
    auto gpu_coarse_batch = [&](cudaStream_t st, float* dQ, float* dS, uint32_t* dIdx,
                                uint32_t* hIdx, size_t off, int B) {
        CUDA_CHECK(cudaMemcpyAsync(dQ, query+off*d, (size_t)B*d*sizeof(float), cudaMemcpyHostToDevice, st));
        CUBLAS_CHECK(cublasSetStream(h, st));
        CUBLAS_CHECK(cublasGemmEx(h, CUBLAS_OP_T, CUBLAS_OP_N, N, B, d, &alpha,
                     dB, CUDA_R_32F, d, dQ, CUDA_R_32F, d, &beta, dS, CUDA_R_32F, N,
                     CUBLAS_COMPUTE_32F_FAST_16F, CUBLAS_GEMM_DEFAULT));
        size_t tot = (size_t)B*N; ip_to_dist<<<(int)((tot+255)/256), 256, 0, st>>>(dS, tot);
        topk_rows<<<B, nt, sh, st>>>(dS, N, rerank_k, dIdx, nullptr);
        CUDA_CHECK(cudaMemcpyAsync(hIdx, dIdx, (size_t)B*rerank_k*sizeof(uint32_t), cudaMemcpyDeviceToHost, st));
    };

    int nbatch = (int)((M + batch_B - 1) / batch_B);

    // [B] hetero-serial
    auto run_hetero_serial = [&](bool timed, std::vector<std::vector<uint32_t>>* res)->float {
        float *dQ, *dS; uint32_t* dIdx; uint32_t* hIdx;
        CUDA_CHECK(cudaMalloc(&dQ,  (size_t)batch_B*d*sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dS,  (size_t)batch_B*N*sizeof(float)));
        CUDA_CHECK(cudaMalloc(&dIdx,(size_t)batch_B*rerank_k*sizeof(uint32_t)));
        CUDA_CHECK(cudaHostAlloc(&hIdx,(size_t)batch_B*rerank_k*sizeof(uint32_t), cudaHostAllocDefault));
        std::vector<std::vector<uint32_t>> tmp(M);
        CpuTimer t; if (timed) t.start();
        for (size_t off = 0; off < M; off += batch_B) {
            int B = (int)std::min<size_t>(batch_B, M - off);
            gpu_coarse_batch(0, dQ, dS, dIdx, hIdx, off, B);
            CUDA_CHECK(cudaStreamSynchronize(0));
            cpu_rerank_batch(base, query, d, k, B, rerank_k, hIdx, tmp, off, nthr);
        }
        double ms = timed ? t.stop_ms() : 0.0; if (res) *res = tmp;
        cudaFree(dQ); cudaFree(dS); cudaFree(dIdx); cudaFreeHost(hIdx); return (float)ms;
    };

    // [C] hetero-overlap：双缓冲，GPU(b+1) 与 CPU(b) 重叠
    auto run_hetero_overlap = [&](bool timed, std::vector<std::vector<uint32_t>>* res)->float {
        cudaStream_t st[2]; CUDA_CHECK(cudaStreamCreate(&st[0])); CUDA_CHECK(cudaStreamCreate(&st[1]));
        float *dQ[2], *dS[2]; uint32_t* dIdx[2]; uint32_t* hIdx[2];
        for (int s=0;s<2;++s) {
            CUDA_CHECK(cudaMalloc(&dQ[s],  (size_t)batch_B*d*sizeof(float)));
            CUDA_CHECK(cudaMalloc(&dS[s],  (size_t)batch_B*N*sizeof(float)));
            CUDA_CHECK(cudaMalloc(&dIdx[s],(size_t)batch_B*rerank_k*sizeof(uint32_t)));
            CUDA_CHECK(cudaHostAlloc(&hIdx[s],(size_t)batch_B*rerank_k*sizeof(uint32_t), cudaHostAllocDefault));
        }
        std::vector<std::vector<uint32_t>> tmp(M);
        auto boff = [&](int b){ return (size_t)b*batch_B; };
        auto bsz  = [&](int b){ return (int)std::min<size_t>(batch_B, M - boff(b)); };
        CpuTimer t; if (timed) t.start();
        if (nbatch > 0) gpu_coarse_batch(st[0], dQ[0], dS[0], dIdx[0], hIdx[0], boff(0), bsz(0));
        for (int b=0;b<nbatch;++b) {
            int cur = b%2;
            if (b+1 < nbatch) { int nx = (b+1)%2;
                gpu_coarse_batch(st[nx], dQ[nx], dS[nx], dIdx[nx], hIdx[nx], boff(b+1), bsz(b+1)); }
            CUDA_CHECK(cudaStreamSynchronize(st[cur]));
            cpu_rerank_batch(base, query, d, k, bsz(b), rerank_k, hIdx[cur], tmp, boff(b), nthr);
        }
        double ms = timed ? t.stop_ms() : 0.0; if (res) *res = tmp;
        for (int s=0;s<2;++s) { cudaFree(dQ[s]); cudaFree(dS[s]); cudaFree(dIdx[s]); cudaFreeHost(hIdx[s]); }
        cudaStreamDestroy(st[0]); cudaStreamDestroy(st[1]); return (float)ms;
    };

    auto avg = [&](std::function<float(bool,std::vector<std::vector<uint32_t>>*)> f)->double {
        f(true, nullptr); double s = 0; const int R = 3; for (int r=0;r<R;++r) s += f(true, nullptr); return s/R; };

    std::vector<std::vector<uint32_t>> rA(M), rB(M), rC(M);
    run_gpu_exact(false, &rA);      double recA = compute_recall(rA, gt, gtd, k);
    run_hetero_serial(false, &rB);  double recB = compute_recall(rB, gt, gtd, k);
    run_hetero_overlap(false, &rC); double recC = compute_recall(rC, gt, gtd, k);

    double msA = avg([&](bool t, std::vector<std::vector<uint32_t>>* r){ return run_gpu_exact(t, r); });
    double msB = avg([&](bool t, std::vector<std::vector<uint32_t>>* r){ return run_hetero_serial(t, r); });
    double msC = avg([&](bool t, std::vector<std::vector<uint32_t>>* r){ return run_hetero_overlap(t, r); });

    auto usq = [&](double ms){ return ms/M*1000.0; };
    std::printf("method,recall,us_per_query,speedup,qps\n");
    std::printf("A_GPU-only-exact,%.4f,%.1f,%.2f,%.0f\n", recA, usq(msA), BASE/usq(msA), M/(msA/1000.0));
    std::printf("B_hetero-serial ,%.4f,%.1f,%.2f,%.0f\n", recB, usq(msB), BASE/usq(msB), M/(msB/1000.0));
    std::printf("C_hetero-overlap,%.4f,%.1f,%.2f,%.0f\n", recC, usq(msC), BASE/usq(msC), M/(msC/1000.0));
    std::printf("\n重叠效率(serial/overlap) = %.2fx   (>1 表示重叠省下时间)\n", msB/msC);
    std::printf("====================\n");

    cudaFree(dB); cublasDestroy(h);
}
