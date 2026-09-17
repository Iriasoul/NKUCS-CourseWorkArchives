/**
 * Reference Implementation: Vector Addition Kernel
 * 
 * This file contains the complete reference solution for the 
 * Thread Utilization Lab. Use this to verify student implementations.
 * 
 * Key Concepts Demonstrated:
 * 1. Global thread indexing: blockIdx.x * blockDim.x + threadIdx.x
 * 2. Boundary checking to handle non-divisible array sizes
 * 3. One-to-one mapping of threads to data elements
 */

#include "kernel.h"

__global__ void vector_add(const float* A, const float* B, float* C, int N) {
    // Global thread index calculation
    // - blockIdx.x: which block this thread belongs to (0 to gridDim.x-1)
    // - blockDim.x: number of threads per block
    // - threadIdx.x: thread's position within its block (0 to blockDim.x-1)
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Boundary check is essential when N is not evenly divisible by block_size
    // Some threads in the last block may have idx >= N
    // These threads should not access memory or compute
    if (idx < N) {
        C[idx] = A[idx] + B[idx];
    }
    
    // Note: Threads with idx >= N still execute (they are part of the warp)
    // but the boundary check prevents them from doing incorrect work.
    // This is an example of "thread divergence" within a warp.
}

/**
 * Analysis Questions for Students:
 * 
 * Q1: If N = 1000 and block_size = 256, how many threads are "wasted"?
 * A1: grid_size = ceil(1000/256) = 4 blocks
 *     Total threads = 4 * 256 = 1024
 *     Wasted threads = 1024 - 1000 = 24
 *     Utilization = 1000/1024 = 97.7%
 * 
 * Q2: If N = 1000 and block_size = 64, what is the thread utilization?
 * A2: grid_size = ceil(1000/64) = 16 blocks
 *     Total threads = 16 * 64 = 1024
 *     Wasted threads = 1024 - 1000 = 24
 *     Utilization = 1000/1024 = 97.7%
 * 
 * Q3: Why does AMD GPU have warp size (wavefront) of 64?
 * A3: AMD GCN architecture has SIMD width of 16. With 4 SIMDs per CU,
 *     a warp is issued over 4 cycles (16 * 4 = 64 threads).
 *     This design balances hardware complexity with parallelism.
 * 
 * Q4: What happens to performance with very small block sizes (e.g., 32)?
 * A4: On AMD GPUs with warp size 64, using block_size=32 means each
 *     block only uses half a warp. This reduces occupancy and
 *     hardware utilization, leading to performance degradation.
 */
