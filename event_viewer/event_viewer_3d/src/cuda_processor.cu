#include "cuda_processor.h"
#include <iostream>
#include <vector>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>


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

__global__ void events_to_vertices(const EventCD* d_in, Vertex* d_out, int total_events, unsigned int* d_count, int width, int height, int64_t t_offset, double base_time, float3 color_on, float3 color_off) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_events) return;

    unsigned int write_idx = atomicAdd(d_count, 1);

    EventCD event = d_in[idx];
    Vertex v;
    // ... 座標計算は同じ ...
    v.x = (static_cast<float>(event.x) / static_cast<float>(width) - 0.5f) * 2.0f;
    v.y = (static_cast<float>(event.y) / static_cast<float>(height) - 0.5f) * -2.0f;
    double absolute_time = static_cast<double>(t_offset) + event.t;
    v.z = static_cast<float>(absolute_time - base_time);

    // ★★★ 色を設定から適用 ★★★
    // 0.0-1.0のfloatを0-255のuint8_tに変換
    if (event.pol == 1) {
        v.r = static_cast<uint8_t>(color_on.x * 255.0f);
        v.g = static_cast<uint8_t>(color_on.y * 255.0f);
        v.b = static_cast<uint8_t>(color_on.z * 255.0f);
    } else {
        v.r = static_cast<uint8_t>(color_off.x * 255.0f);
        v.g = static_cast<uint8_t>(color_off.y * 255.0f);
        v.b = static_cast<uint8_t>(color_off.z * 255.0f);
    }
    v.a = 255;
    
    d_out[write_idx] = v;
}
// ★★★ process_all_events関数も t_offset と base_time を受け取るように修正 ★★★
unsigned int process_all_events(const std::vector<EventCD>& all_events, int width, int height, int64_t t_offset, double base_time, const ColorConfig& colors) {
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
    
    // ★★★ glm::vec3 から float3 に変換 ★★★
    float3 color_on = make_float3(colors.event_on.r, colors.event_on.g, colors.event_on.b);
    float3 color_off = make_float3(colors.event_off.r, colors.event_off.g, colors.event_off.b);

    // ★★★ カーネル呼び出し時に色情報を渡す ★★★
    events_to_vertices<<<blocks, threads>>>(d_events, d_vbo_ptr, all_events.size(), d_count, width, height, t_offset, base_time, color_on, color_off);
    CUDA_CHECK(cudaDeviceSynchronize());

    CUDA_CHECK(cudaGraphicsUnmapResources(1, &vbo_resource_cu, 0));

    unsigned int final_count = 0;
    CUDA_CHECK(cudaMemcpy(&final_count, d_count, sizeof(unsigned int), cudaMemcpyDeviceToHost));
    
    CUDA_CHECK(cudaFree(d_count));
    CUDA_CHECK(cudaFree(d_events));

    std::cout << "--- CUDA処理完了: " << final_count << "個の頂点を生成 ---" << std::endl;
    return final_count;
}