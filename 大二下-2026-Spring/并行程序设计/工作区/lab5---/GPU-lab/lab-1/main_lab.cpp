#include <hip/hip_runtime.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstdlib>

// Forward declaration for external kernel
__global__ void vector_add(const float* A, const float* B, float* C, int N);

#define HIP_CHECK(call) \
    do { \
        hipError_t err = call; \
        if (err != hipSuccess) { \
            std::cerr << "HIP Error: " << hipGetErrorString(err) \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            exit(1); \
        } \
    } while(0)

void print_device_info() {
    hipDeviceProp_t prop;
    HIP_CHECK(hipGetDeviceProperties(&prop, 0));
    
    std::cout << "=== Device Information ===" << std::endl;
    std::cout << "Device: " << prop.name << std::endl;
    std::cout << "Compute Units: " << prop.multiProcessorCount << std::endl;
    std::cout << "Max Threads per Block: " << prop.maxThreadsPerBlock << std::endl;
    std::cout << "Warp Size: " << prop.warpSize << std::endl;
    std::cout << "Max Blocks per CU: " << prop.maxBlocksPerMultiProcessor << std::endl;
    std::cout << "==========================" << std::endl << std::endl;
}

bool verify_result(const float* A, const float* B, const float* C, int N) {
    for (int i = 0; i < N; i++) {
        float expected = A[i] + B[i];
        if (std::fabs(C[i] - expected) > 1e-5) {
            std::cerr << "Verification failed at index " << i << std::endl;
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    // Default values
    int N = 1 << 20;  // 1M elements
    int block_size = 256;
    
    // Parse command line arguments
    if (argc >= 2) {
        N = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        block_size = std::atoi(argv[2]);
    }
    
    std::cout << "Vector size: " << N << std::endl;
    std::cout << "Block size: " << block_size << std::endl;
    
    // Print device info
    print_device_info();
    
    // Allocate host memory
    size_t size = N * sizeof(float);
    std::vector<float> h_A(N), h_B(N), h_C(N);
    
    // Initialize input vectors
    for (int i = 0; i < N; i++) {
        h_A[i] = static_cast<float>(i % 1000) / 1000.0f;
        h_B[i] = static_cast<float>((i + 500) % 1000) / 1000.0f;
    }
    
    // Allocate device memory
    float *d_A, *d_B, *d_C;
    HIP_CHECK(hipMalloc(&d_A, size));
    HIP_CHECK(hipMalloc(&d_B, size));
    HIP_CHECK(hipMalloc(&d_C, size));
    
    // Copy data to device
    HIP_CHECK(hipMemcpy(d_A, h_A.data(), size, hipMemcpyHostToDevice));
    HIP_CHECK(hipMemcpy(d_B, h_B.data(), size, hipMemcpyHostToDevice));
    
    // Calculate grid dimensions
    int grid_size = (N + block_size - 1) / block_size;
    
    std::cout << "\n=== Launch Configuration ===" << std::endl;
    std::cout << "Grid size (blocks): " << grid_size << std::endl;
    std::cout << "Block size (threads): " << block_size << std::endl;
    std::cout << "Total threads: " << grid_size * block_size << std::endl;
    std::cout << "Active threads (actual work): " << N << std::endl;
    std::cout << "Thread utilization: " 
              << (100.0 * N) / (grid_size * block_size) << "%" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // Warm-up run
    hipLaunchKernelGGL(vector_add, dim3(grid_size), dim3(block_size), 0, 0,
                       d_A, d_B, d_C, N);
    HIP_CHECK(hipDeviceSynchronize());
    
    // Timed run
    hipEvent_t start, stop;
    HIP_CHECK(hipEventCreate(&start));
    HIP_CHECK(hipEventCreate(&stop));
    
    HIP_CHECK(hipEventRecord(start));
    hipLaunchKernelGGL(vector_add, dim3(grid_size), dim3(block_size), 0, 0,
                       d_A, d_B, d_C, N);
    HIP_CHECK(hipEventRecord(stop));
    HIP_CHECK(hipEventSynchronize(stop));
    
    float ms = 0;
    HIP_CHECK(hipEventElapsedTime(&ms, start, stop));
    
    std::cout << "\nKernel execution time: " << ms << " ms" << std::endl;
    
    // Copy result back
    HIP_CHECK(hipMemcpy(h_C.data(), d_C, size, hipMemcpyDeviceToHost));
    
    // Verify
    if (verify_result(h_A.data(), h_B.data(), h_C.data(), N)) {
        std::cout << "Verification: PASSED" << std::endl;
    } else {
        std::cout << "Verification: FAILED" << std::endl;
    }
    
    // Cleanup
    HIP_CHECK(hipFree(d_A));
    HIP_CHECK(hipFree(d_B));
    HIP_CHECK(hipFree(d_C));
    HIP_CHECK(hipEventDestroy(start));
    HIP_CHECK(hipEventDestroy(stop));
    
    return 0;
}
