#include <hip/hip_runtime.h>

/**
 * Vector Addition Kernel
 * 
 * Each thread computes one element: C[i] = A[i] + B[i]
 * 
 * Thread Indexing:
 *   Global thread ID = blockIdx.x * blockDim.x + threadIdx.x
 * 
 * @param A     Input vector A
 * @param B     Input vector B  
 * @param C     Output vector C
 * @param N     Vector length
 */
__global__ void vector_add(const float* A, const float* B, float* C, int N) {
    // Calculate global thread index
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Boundary check
    if (idx < N) {
        C[idx] = A[idx] + B[idx];
    }
}
