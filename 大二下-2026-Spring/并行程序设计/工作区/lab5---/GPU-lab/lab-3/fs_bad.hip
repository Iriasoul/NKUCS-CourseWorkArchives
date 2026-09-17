#include <iostream>
#include <vector>
#include <iomanip>
#include <hip/hip_runtime.h>

#include <fstream>

#define threadsperblock 1024

__global__ void kernel(const int *input, int *histogram, int N, int num_bins){

    __shared__ int sdata[threadsperblock]; 

    int tidx = threadIdx.x;
    int whichnumbins = blockIdx.x;

    sdata[tidx] = 0;

    for(int i = tidx; i < N; i+= threadsperblock){
        if(input[i] == whichnumbins) sdata[tidx]++;
        
    }

    __syncthreads();

    for (int i = blockDim.x / 2; i > 0; i >>= 1) {
        if (tidx < i) sdata[tidx] += sdata[tidx + i];
        __syncthreads();
    }

    if (tidx == 0) histogram[whichnumbins] = sdata[0]; 
}


extern "C" void solve(const int* input, int* histogram, int N, int num_bins) {
    
    int threads = threadsperblock;  // 1024 threads per block
    int blocks = num_bins;          // One block per bin (this is the naive approach!)
    int *d_input, *d_histogram;

    hipMalloc(&d_input, N * sizeof(int));
    hipMalloc(&d_histogram, num_bins * sizeof(int));

    hipMemcpy(d_input, input, N * sizeof(int), hipMemcpyHostToDevice);

    // Warm-up
    hipMemset(d_histogram, 0, num_bins * sizeof(int));
    kernel<<<blocks, threads>>>(d_input, d_histogram, N, num_bins);
    hipDeviceSynchronize();

    // Time kernel with HIP events (averaged over REPEATS launches)
    hipEvent_t ev_s, ev_e;
    hipEventCreate(&ev_s); hipEventCreate(&ev_e);
    const int REPEATS = 5;  // naive is slow, fewer repeats
    hipEventRecord(ev_s);
    for (int r = 0; r < REPEATS; ++r) {
        hipMemsetAsync(d_histogram, 0, num_bins * sizeof(int));
        kernel<<<blocks, threads>>>(d_input, d_histogram, N, num_bins);
    }
    hipEventRecord(ev_e); hipEventSynchronize(ev_e);
    float total_ms = 0.f;
    hipEventElapsedTime(&total_ms, ev_s, ev_e);
    std::cerr << "KERNEL_MS " << (total_ms / REPEATS) << std::endl;
    hipEventDestroy(ev_s); hipEventDestroy(ev_e);

    hipMemcpy(histogram, d_histogram, num_bins * sizeof(int), hipMemcpyDeviceToHost);

    hipFree(d_input);
    hipFree(d_histogram);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }
    
    std::ifstream input_file;
    std::string filename = argv[1];
    
    input_file.open(filename);
    if (!input_file.is_open()) {
        std::cerr << "fileopen error" << filename << std::endl;
        return 1;
    }
    int N, num_bins;
    input_file >> N >> num_bins;

    std::vector<int> input(N), histogram(num_bins);
    
    for(int i = 0; i < N ; ++i) input_file >> input[i];
    
    input_file.close();

    solve(input.data(), histogram.data(), N, num_bins);

    for(int i = 0; i < num_bins ; ++i) std::cout << histogram[i] << " ";
    std::cout << std::endl;
}