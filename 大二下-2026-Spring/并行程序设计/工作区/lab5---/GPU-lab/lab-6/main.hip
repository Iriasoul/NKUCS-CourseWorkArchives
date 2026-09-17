#include <hip/hip_runtime.h>
#include <iostream>
#include <vector>
#include <cmath>
using namespace std;

__global__ void find_nearest_centroids_kernel(const float* data_x, const float* data_y, int* labels,
                                              const float* centroids_x, const float* centroids_y,
                                              int sample_size, int k) {
    int point_id = blockIdx.x * blockDim.x + threadIdx.x;
    if(point_id < sample_size) {
        int nearest_centroid_index = 0;
        float min_distance_sq = INFINITY;
        for(int centroid_index = 0; centroid_index < k; ++centroid_index) {
            float x = centroids_x[centroid_index] - data_x[point_id];
            float y = centroids_y[centroid_index] - data_y[point_id];
            float dist = x*x + y*y;
            if(dist < min_distance_sq) {
                min_distance_sq = dist;
                nearest_centroid_index = centroid_index;
            }
        }
        labels[point_id] = nearest_centroid_index;
    }
}

__global__ void recalculate_centroid_kernel(const float* data_x, const float* data_y, const int* labels,
                                            int* centroid_point_nums, float* new_centroids_x, float* new_centroids_y,
                                            int sample_size, int k) {
    __shared__ float new_centroids_x_shared[100];
    __shared__ float new_centroids_y_shared[100];
    __shared__ int centroid_point_nums_shared[100];

    if (threadIdx.x < k) {
        new_centroids_x_shared[threadIdx.x] = 0;
        new_centroids_y_shared[threadIdx.x] = 0;
        centroid_point_nums_shared[threadIdx.x] = 0;
    }
    __syncthreads();

    int point_id = blockIdx.x * blockDim.x + threadIdx.x;
    if(point_id < sample_size) {
        int cid = labels[point_id];
        atomicAdd(new_centroids_x_shared + cid, data_x[point_id]);
        atomicAdd(new_centroids_y_shared + cid, data_y[point_id]);
        atomicAdd(centroid_point_nums_shared + cid, 1);
    }

    __syncthreads();

    if(threadIdx.x < k) {
        atomicAdd(new_centroids_x + threadIdx.x, new_centroids_x_shared[threadIdx.x]);
        atomicAdd(new_centroids_y + threadIdx.x, new_centroids_y_shared[threadIdx.x]);
        atomicAdd(centroid_point_nums + threadIdx.x, centroid_point_nums_shared[threadIdx.x]);
    }
}

__global__ void finalize_recalculation_and_compare_kernel(const int* centroid_point_nums,
                                                          float* new_centroids_x, float* new_centroids_y,
                                                          const float* prev_centroids_x, const float* prev_centroids_y,
                                                          int k, int* num_converged_centroids) {

    int idx = threadIdx.x;

    if (idx == 0) {
        *num_converged_centroids = 0;
    }
    __syncthreads();

    if (idx < k) {
        int count = centroid_point_nums[idx];
        if (count > 0) {
            new_centroids_x[idx] /= count;
            new_centroids_y[idx] /= count;
        } else {
            new_centroids_x[idx] = prev_centroids_x[idx];
            new_centroids_y[idx] = prev_centroids_y[idx];
        }

        float dx = new_centroids_x[idx] - prev_centroids_x[idx];
        float dy = new_centroids_y[idx] - prev_centroids_y[idx];

        if (dx*dx + dy*dy < 1.0e-8f) {
            atomicAdd(num_converged_centroids, 1);
        }
    }
}

void solve(const float* data_x, const float* data_y, int* labels,
           float* initial_centroid_x, float* initial_centroid_y,
           float* final_centroid_x, float* final_centroid_y,
           int sample_size, int k, int max_iterations) {

    int threadsPerBlock = 256;
    int blocksPerGrid = (sample_size + threadsPerBlock - 1) / threadsPerBlock;

    int* centroid_point_nums;
    hipMalloc((void**)&centroid_point_nums, k * sizeof(int));

    int* num_converged_centroids;
    hipMalloc((void**)&num_converged_centroids, sizeof(int));

    float* prev_centroid_x = final_centroid_x;
    float* prev_centroid_y = final_centroid_y;
    float* new_centroid_x = initial_centroid_x;
    float* new_centroid_y = initial_centroid_y;

    for(int iter = 0; iter < max_iterations; iter++) {
        swap(new_centroid_x, prev_centroid_x);
        swap(new_centroid_y, prev_centroid_y);

        hipLaunchKernelGGL(find_nearest_centroids_kernel,
                           dim3(blocksPerGrid), dim3(threadsPerBlock), 0, 0,
                           data_x, data_y, labels,
                           prev_centroid_x, prev_centroid_y,
                           sample_size, k);

        hipMemset(centroid_point_nums, 0, k*sizeof(int));
        hipMemset(new_centroid_x, 0, k*sizeof(float));
        hipMemset(new_centroid_y, 0, k*sizeof(float));

        hipLaunchKernelGGL(recalculate_centroid_kernel,
                           dim3(blocksPerGrid), dim3(threadsPerBlock), 0, 0,
                           data_x, data_y, labels,
                           centroid_point_nums, new_centroid_x, new_centroid_y,
                           sample_size, k);

        hipLaunchKernelGGL(finalize_recalculation_and_compare_kernel,
                           dim3(1), dim3(128), 0, 0,
                           centroid_point_nums, new_centroid_x, new_centroid_y,
                           prev_centroid_x, prev_centroid_y,
                           k, num_converged_centroids);

        int num_done;
        hipMemcpy(&num_done, num_converged_centroids, sizeof(int), hipMemcpyDeviceToHost);
        hipDeviceSynchronize();

        if(num_done == k) break;
    }

    if(new_centroid_x != final_centroid_x) {
        hipMemcpy(final_centroid_x, new_centroid_x, k*sizeof(float), hipMemcpyDeviceToDevice);
        hipMemcpy(final_centroid_y, new_centroid_y, k*sizeof(float), hipMemcpyDeviceToDevice);
    }

    hipFree(centroid_point_nums);
    hipFree(num_converged_centroids);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int sample_size, k, max_iter;
    cin >> sample_size >> k >> max_iter;

    vector<float> h_data_x(sample_size), h_data_y(sample_size);
    vector<float> h_init_cx(k), h_init_cy(k);

    for(int i=0; i<sample_size; i++) cin >> h_data_x[i];
    for(int i=0; i<sample_size; i++) cin >> h_data_y[i];
    for(int i=0; i<k; i++) cin >> h_init_cx[i];
    for(int i=0; i<k; i++) cin >> h_init_cy[i];

    float *d_data_x, *d_data_y, *d_init_cx, *d_init_cy, *d_final_cx, *d_final_cy;
    int* d_labels;

    hipMalloc(&d_data_x, sample_size*sizeof(float));
    hipMalloc(&d_data_y, sample_size*sizeof(float));
    hipMalloc(&d_labels, sample_size*sizeof(int));

    hipMalloc(&d_init_cx, k*sizeof(float));
    hipMalloc(&d_init_cy, k*sizeof(float));
    hipMalloc(&d_final_cx, k*sizeof(float));
    hipMalloc(&d_final_cy, k*sizeof(float));

    hipMemcpy(d_data_x, h_data_x.data(), sample_size*sizeof(float), hipMemcpyHostToDevice);
    hipMemcpy(d_data_y, h_data_y.data(), sample_size*sizeof(float), hipMemcpyHostToDevice);

    hipMemcpy(d_init_cx, h_init_cx.data(), k*sizeof(float), hipMemcpyHostToDevice);
    hipMemcpy(d_init_cy, h_init_cy.data(), k*sizeof(float), hipMemcpyHostToDevice);

    hipMemcpy(d_final_cx, d_init_cx, k*sizeof(float), hipMemcpyDeviceToDevice);
    hipMemcpy(d_final_cy, d_init_cy, k*sizeof(float), hipMemcpyDeviceToDevice);

    solve(d_data_x, d_data_y, d_labels,
          d_init_cx, d_init_cy,
          d_final_cx, d_final_cy,
          sample_size, k, max_iter);

    vector<float> outx(k), outy(k);
    hipMemcpy(outx.data(), d_final_cx, k*sizeof(float), hipMemcpyDeviceToHost);
    hipMemcpy(outy.data(), d_final_cy, k*sizeof(float), hipMemcpyDeviceToHost);

    for(int i=0; i<k; i++) cout << outx[i] << " ";
    cout << "\n";
    for(int i=0; i<k; i++) cout << outy[i] << " ";
    cout << "\n";

    hipFree(d_data_x);
    hipFree(d_data_y);
    hipFree(d_labels);
    hipFree(d_init_cx);
    hipFree(d_init_cy);
    hipFree(d_final_cx);
    hipFree(d_final_cy);

    return 0;
}
