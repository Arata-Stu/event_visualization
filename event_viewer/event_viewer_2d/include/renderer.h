#pragma once
#include <vector>
#include <memory>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "types.h"
#include "camera.h"
#include "shader.h"
#include "viewer_state.h"

// 頂点データ構造をfloatタイムスタンプに対応
struct EventVertex {
    float x, y;
    float timestamp;
    uint8_t polarity;
    uint8_t padding[3]; // 16バイトアライメントのためのパディング
};

class Renderer {
public:
    Renderer(int width, int height, const std::string& title);
    ~Renderer();
    
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void run(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset);

private:
    void init();
    void setupCallbacks();
    void loadData(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset);
    void mainLoop();
    void renderScene();
    void cleanup();

    GLFWwindow* m_window;
    int m_width, m_height;
    std::string m_title;

    int m_sensor_width, m_sensor_height;
    
    Camera m_camera;
    ViewerState m_state;
    double m_current_time_us = 0.0;
    
    bool m_is_mouse_dragging = false;
    double m_last_x = 0.0, m_last_y = 0.0;

    std::unique_ptr<Shader> m_event_accum_shader;
    std::unique_ptr<Shader> m_quad_shader;

    GLuint m_event_vao = 0, m_event_vbo = 0;
    GLuint m_quad_vao = 0, m_quad_vbo = 0;
    
    GLuint m_event_fbo = 0;
    GLuint m_event_texture = 0;

    std::vector<GLuint> m_image_textures;
    const std::vector<RGBFrame>* m_all_images_ptr = nullptr;
    
    size_t m_event_count = 0;
    double m_base_time = 0.0;

    // CPUカリング（パフォーマンス改善）のためにタイムスタンプのリストを保持
    std::vector<float> m_event_timestamps;
    
    void onKey(int key, int scancode, int action, int mods);
    void onMouseButton(int button, int action, int mods);
    void onCursorPosition(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
    void onFramebufferSize(int width, int height);
};

void run_renderer(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int width, int height, int64_t t_offset);