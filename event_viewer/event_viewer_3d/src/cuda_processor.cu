#include "cuda_processor.h"
#include <iostream>
#include <vector>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

// 色の設定
__device__ const unsigned char POLARITY_1_R = 255, POLARITY_1_G = 50, POLARITY_1_B = 50;
__device__ const unsigned char POLARITY_0_R = 50, POLARITY_0_G = 50, POLARITY_0_B = 255;


#define CUDA_CHECK(err) { \
    cudaError_t err_ = (err); \
    if (err_ != cudaSuccess) { \
        std::cerr << "CUDA Error in " << __FILE__ << " at line " << __LINE__ \
                  << ": " << cudaGetErrorString(err_) << std::endl; \
        exit(EXIT_FAILURE); \
    } \
}

static struct cudaGraphicsResource* vbo_resource_cu = nullptr;

void register_gl_buffer(GLuint vbo) {
    CUDA_CHECK(cudaGraphicsGLRegisterBuffer(&vbo_resource_cu, vbo, cudaGraphicsRegisterFlagsWriteDiscard));
}
void unregister_gl_buffer() {
    if (vbo_resource_cu) {
        CUDA_CHECK(cudaGraphicsUnregisterResource(vbo_resource_cu));
        vbo_resource_cu = nullptr;
    }
}
void init_cuda_for_gl() {
    CUDA_CHECK(cudaSetDevice(0));
    CUDA_CHECK(cudaFree(0));
    std::cout << "CUDA initialized for OpenGL Interop on Device 0." << std::endl;
}

// ★★★ カーネルが t_offset と base_time を受け取るように変更 ★★★
__global__ void events_to_vertices(const EventCD* d_in, Vertex* d_out, int total_events, unsigned int* d_count, int width, int height, int64_t t_offset, double base_time) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_events) return;

    unsigned int write_idx = atomicAdd(d_count, 1);
    
    EventCD event = d_in[idx];
    Vertex v;
    v.x = (static_cast<float>(event.x) / static_cast<float>(width) - 0.5f) * 2.0f; 
    v.y = (static_cast<float>(event.y) / static_cast<float>(height) - 0.5f) * -2.0f;
    
    // ★★★ 絶対時刻を計算し、そこから基準時刻を引いて相対的なZ座標を計算 ★★★
    double absolute_time = static_cast<double>(t_offset) + event.t;
    v.z = static_cast<float>(absolute_time - base_time);

    if (event.p == 1) {
        v.r = POLARITY_1_R; v.g = POLARITY_1_G; v.b = POLARITY_1_B; v.a = 255;
    } else {
        v.r = POLARITY_0_R; v.g = POLARITY_0_G; v.b = POLARITY_0_B; v.a = 255;
    }
    
    d_out[write_idx] = v;
}

// ★★★ process_all_events関数も t_offset と base_time を受け取るように修正 ★★★
unsigned int process_all_events(const std::vector<EventCD>& all_events, int width, int height, int64_t t_offset, double base_time) {
    if (all_events.empty() || !vbo_resource_cu) return 0;
    
    std::cout << "--- 全イベントのCUDA処理を開始..." << std::endl;
    EventCD* d_events = nullptr;
    size_t data_size = all_events.size() * sizeof(EventCD);
    CUDA_CHECK(cudaMalloc(&d_events, data_size));

    std::cout << "--- CPUからGPUへのデータ転送を開始 (" 
              << data_size / (1024 * 1024) << " MB)... ---" << std::endl;
    CUDA_CHECK(cudaMemcpy(d_events, all_events.data(), data_size, cudaMemcpyHostToDevice));
    std::cout << "--- データ転送完了。カーネルを実行します ---" << std::endl;

    unsigned int* d_count = nullptr;
    CUDA_CHECK(cudaMalloc(&d_count, sizeof(unsigned int)));
    CUDA_CHECK(cudaMemset(d_count, 0, sizeof(unsigned int)));

    Vertex* d_vbo_ptr = nullptr;
    CUDA_CHECK(cudaGraphicsMapResources(1, &vbo_resource_cu, 0));
    CUDA_CHECK(cudaGraphicsResourceGetMappedPointer((void**)&d_vbo_ptr, nullptr, vbo_resource_cu));

    int threads = 256;
    int blocks = (all_events.size() + threads - 1) / threads;
    
    // ★★★ カーネル呼び出し時に t_offset と base_time を渡す ★★★
    events_to_vertices<<<blocks, threads>>>(d_events, d_vbo_ptr, all_events.size(), d_count, width, height, t_offset, base_time);
    CUDA_CHECK(cudaDeviceSynchronize());

    CUDA_CHECK(cudaGraphicsUnmapResources(1, &vbo_resource_cu, 0));

    unsigned int final_count = 0;
    CUDA_CHECK(cudaMemcpy(&final_count, d_count, sizeof(unsigned int), cudaMemcpyDeviceToHost));
    
    CUDA_CHECK(cudaFree(d_count));
    CUDA_CHECK(cudaFree(d_events));

    std::cout << "--- CUDA処理完了: " << final_count << "個の頂点を生成 ---" << std::endl;
    return final_count;
}