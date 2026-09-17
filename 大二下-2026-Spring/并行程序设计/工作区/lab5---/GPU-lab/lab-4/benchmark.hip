#include <iostream>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <hip/hip_runtime.h>

// ============================================================
// Strategy 0: Naive Atomic — every thread atomicAdd to one location
// ============================================================
__global__ void reduction_naive(const int* input, int* output, int N) {
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    if (idx >= N) return;
    atomicAdd(output, input[idx]);
}

// ============================================================
// Strategy 1: Tree Reduction with Shared Memory
// ============================================================
__global__ void reduction_tree(const int* input, int* output, int N) {
    extern __shared__ int sdata[];

    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    sdata[tid] = (idx < N) ? input[idx] : 0;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            sdata[tid] += sdata[tid + stride];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(output, sdata[0]);
    }
}

// ============================================================
// Strategy 2: Tree + Warp Shuffle (last warp uses __shfl_down)
// ============================================================
__global__ void reduction_warp_shuffle(const int* input, int* output, int N) {
    extern __shared__ int sdata[];

    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    sdata[tid] = (idx < N) ? input[idx] : 0;
    __syncthreads();

    // Tree reduction down to warp size
    for (int stride = blockDim.x / 2; stride >= warpSize; stride >>= 1) {
        if (tid < stride) {
            sdata[tid] += sdata[tid + stride];
        }
        __syncthreads();
    }

    // Warp-level reduction using shuffle (no syncthreads needed)
    if (tid < warpSize) {
        int val = sdata[tid];
        for (int offset = warpSize / 2; offset > 0; offset >>= 1) {
            val += __shfl_down(val, offset);
        }
        if (tid == 0) {
            atomicAdd(output, val);
        }
    }
}

// ============================================================
// Strategy 3: Multi-Element Per Thread + Tree + Warp Shuffle
// ============================================================
__global__ void reduction_multi_element(const int* input, int* output, int N) {
    extern __shared__ int sdata[];

    int tid = threadIdx.x;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int grid_stride = gridDim.x * blockDim.x;

    // Each thread accumulates multiple elements
    int sum = 0;
    for (int i = idx; i < N; i += grid_stride) {
        sum += input[i];
    }
    sdata[tid] = sum;
    __syncthreads();

    // Tree reduction down to warp size
    for (int stride = blockDim.x / 2; stride >= warpSize; stride >>= 1) {
        if (tid < stride) {
            sdata[tid] += sdata[tid + stride];
        }
        __syncthreads();
    }

    // Warp shuffle for final reduction
    if (tid < warpSize) {
        int val = sdata[tid];
        for (int offset = warpSize / 2; offset > 0; offset >>= 1) {
            val += __shfl_down(val, offset);
        }
        if (tid == 0) {
            atomicAdd(output, val);
        }
    }
}

// ============================================================
// Benchmark helper
// ============================================================
struct BenchResult {
    float time_ms;
    int result;
};

BenchResult run_naive(const int* d_input, int* d_output, int N, int block_size) {
    int grid = (N + block_size - 1) / block_size;
    hipMemset(d_output, 0, sizeof(int));

    // Warm up
    reduction_naive<<<grid, block_size>>>(d_input, d_output, N);
    hipDeviceSynchronize();
    hipMemset(d_output, 0, sizeof(int));

    hipEvent_t start, stop;
    hipEventCreate(&start);
    hipEventCreate(&stop);
    hipEventRecord(start);
    reduction_naive<<<grid, block_size>>>(d_input, d_output, N);
    hipEventRecord(stop);
    hipEventSynchronize(stop);

    float ms = 0;
    hipEventElapsedTime(&ms, start, stop);
    hipEventDestroy(start);
    hipEventDestroy(stop);

    int h_result;
    hipMemcpy(&h_result, d_output, sizeof(int), hipMemcpyDeviceToHost);
    return {ms, h_result};
}

BenchResult run_tree(const int* d_input, int* d_output, int N, int block_size) {
    int grid = (N + block_size - 1) / block_size;
    int smem = block_size * sizeof(int);
    hipMemset(d_output, 0, sizeof(int));

    reduction_tree<<<grid, block_size, smem>>>(d_input, d_output, N);
    hipDeviceSynchronize();
    hipMemset(d_output, 0, sizeof(int));

    hipEvent_t start, stop;
    hipEventCreate(&start);
    hipEventCreate(&stop);
    hipEventRecord(start);
    reduction_tree<<<grid, block_size, smem>>>(d_input, d_output, N);
    hipEventRecord(stop);
    hipEventSynchronize(stop);

    float ms = 0;
    hipEventElapsedTime(&ms, start, stop);
    hipEventDestroy(start);
    hipEventDestroy(stop);

    int h_result;
    hipMemcpy(&h_result, d_output, sizeof(int), hipMemcpyDeviceToHost);
    return {ms, h_result};
}

BenchResult run_warp_shuffle(const int* d_input, int* d_output, int N, int block_size) {
    int grid = (N + block_size - 1) / block_size;
    int smem = block_size * sizeof(int);
    hipMemset(d_output, 0, sizeof(int));

    reduction_warp_shuffle<<<grid, block_size, smem>>>(d_input, d_output, N);
    hipDeviceSynchronize();
    hipMemset(d_output, 0, sizeof(int));

    hipEvent_t start, stop;
    hipEventCreate(&start);
    hipEventCreate(&stop);
    hipEventRecord(start);
    reduction_warp_shuffle<<<grid, block_size, smem>>>(d_input, d_output, N);
    hipEventRecord(stop);
    hipEventSynchronize(stop);

    float ms = 0;
    hipEventElapsedTime(&ms, start, stop);
    hipEventDestroy(start);
    hipEventDestroy(stop);

    int h_result;
    hipMemcpy(&h_result, d_output, sizeof(int), hipMemcpyDeviceToHost);
    return {ms, h_result};
}

BenchResult run_multi_element(const int* d_input, int* d_output, int N, int block_size, int max_blocks) {
    int smem = block_size * sizeof(int);
    hipMemset(d_output, 0, sizeof(int));

    reduction_multi_element<<<max_blocks, block_size, smem>>>(d_input, d_output, N);
    hipDeviceSynchronize();
    hipMemset(d_output, 0, sizeof(int));

    hipEvent_t start, stop;
    hipEventCreate(&start);
    hipEventCreate(&stop);
    hipEventRecord(start);
    reduction_multi_element<<<max_blocks, block_size, smem>>>(d_input, d_output, N);
    hipEventRecord(stop);
    hipEventSynchronize(stop);

    float ms = 0;
    hipEventElapsedTime(&ms, start, stop);
    hipEventDestroy(start);
    hipEventDestroy(stop);

    int h_result;
    hipMemcpy(&h_result, d_output, sizeof(int), hipMemcpyDeviceToHost);
    return {ms, h_result};
}

// ============================================================
// Main — read input file, benchmark all strategies
// ============================================================
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    std::ifstream fin(argv[1]);
    if (!fin.is_open()) {
        std::cerr << "Cannot open " << argv[1] << std::endl;
        return 1;
    }

    int N;
    fin >> N;
    std::vector<int> h_input(N);
    for (int i = 0; i < N; i++) fin >> h_input[i];
    fin.close();

    // Device memory
    int *d_input, *d_output;
    hipMalloc(&d_input, (size_t)N * sizeof(int));
    hipMalloc(&d_output, sizeof(int));
    hipMemcpy(d_input, h_input.data(), (size_t)N * sizeof(int), hipMemcpyHostToDevice);

    // Device info
    hipDeviceProp_t prop;
    hipGetDeviceProperties(&prop, 0);

    int block_size = 256;
    // For multi-element strategy, limit grid size to CU count * reasonable factor
    int max_blocks = prop.multiProcessorCount * 4;

    std::cout << "=== Reduction Benchmark ===" << std::endl;
    std::cout << "Device: " << prop.name << std::endl;
    std::cout << "N = " << N << ", Block size = " << block_size << std::endl;
    std::cout << std::endl;

    printf("%-35s %12s %15s\n", "Strategy", "Time (ms)", "Result");
    printf("%-35s %12s %15s\n", "-----------------------------------", "------------", "---------------");

    auto r0 = run_naive(d_input, d_output, N, block_size);
    printf("%-35s %12.4f %15d\n", "0. Naive Atomic", r0.time_ms, r0.result);

    auto r1 = run_tree(d_input, d_output, N, block_size);
    printf("%-35s %12.4f %15d\n", "1. Tree (Shared Memory)", r1.time_ms, r1.result);

    auto r2 = run_warp_shuffle(d_input, d_output, N, block_size);
    printf("%-35s %12.4f %15d\n", "2. Tree + Warp Shuffle", r2.time_ms, r2.result);

    auto r3 = run_multi_element(d_input, d_output, N, block_size, max_blocks);
    printf("%-35s %12.4f %15d\n", "3. Multi-Element + Tree + Shuffle", r3.time_ms, r3.result);

    // Speedup summary
    std::cout << std::endl;
    std::cout << "=== Speedup vs Naive ===" << std::endl;
    printf("  Tree:          %.2fx\n", r0.time_ms / r1.time_ms);
    printf("  Warp Shuffle:  %.2fx\n", r0.time_ms / r2.time_ms);
    printf("  Multi-Element: %.2fx\n", r0.time_ms / r3.time_ms);

    // Verify all results match
    std::cout << std::endl;
    if (r0.result == r1.result && r1.result == r2.result && r2.result == r3.result) {
        std::cout << "Verification: ALL PASSED (all strategies produce same result)" << std::endl;
    } else {
        std::cout << "Verification: MISMATCH detected!" << std::endl;
    }

    hipFree(d_input);
    hipFree(d_output);
    return 0;
}
