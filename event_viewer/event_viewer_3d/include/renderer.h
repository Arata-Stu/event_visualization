#pragma once
#include <vector>
#include <memory>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "types.h" // EventCD, RGBFrame, Vertex, ColorConfig
#include "camera.h"
#include "shader.h"
#include "viewer_state.h"

// アプリケーション全体を管理するクラス
class Renderer {
public:
    Renderer(int width, int height, const std::string& title);
    ~Renderer();
    
    // コピーとムーブを禁止
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void run(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset, const ColorConfig& colors);

private:
    void init();
    void setupCallbacks();
    // 👇 この行を修正
    void loadData(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset, const ColorConfig& colors);
    void mainLoop();
    void renderScene();
    void cleanup();

    // Window
    GLFWwindow* m_window;
    int m_width, m_height;
    std::string m_title;
    
    // 状態管理
    Camera m_camera;
    ViewerState m_state;
    ColorConfig m_colors;
    double m_current_time_us = 0.0;
    
    // マウス入力用
    bool m_is_mouse_dragging = false;
    double m_last_x = 0.0, m_last_y = 0.0;

    // シェーダー (RAII)
    std::unique_ptr<Shader> m_point_shader;
    std::unique_ptr<Shader> m_box_shader;
    std::unique_ptr<Shader> m_image_shader;

    // OpenGLオブジェクトID
    GLuint m_point_vao = 0, m_point_vbo = 0;
    GLuint m_box_vao = 0, m_box_vbo = 0, m_box_ebo = 0;
    GLuint m_quad_vao = 0, m_quad_vbo = 0, m_quad_ebo = 0;
    std::vector<GLuint> m_image_textures;

    // データ参照
    const std::vector<RGBFrame>* m_all_images_ptr = nullptr;
    unsigned int m_point_count = 0;
    double m_base_time = 0.0;
    
    // コールバックハンドラ
    void onKey(int key, int scancode, int action, int mods);
    void onMouseButton(int button, int action, int mods);
    void onCursorPosition(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
};

void run_renderer(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int width, int height, int64_t t_offset, const ColorConfig& colors);