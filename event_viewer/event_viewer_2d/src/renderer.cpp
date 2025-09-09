#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "renderer.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

void run_renderer(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int width, int height, int64_t t_offset) {
    try {
        Renderer app(1280, 960, "2D Event Viewer");
        app.run(all_events, all_images, width, height, t_offset);
    } catch (const std::exception& e) {
        std::cerr << "A critical error occurred: " << e.what() << std::endl;
    }
}

Renderer::Renderer(int width, int height, const std::string& title) 
    : m_width(width), m_height(height), m_title(title), m_window(nullptr) {}

Renderer::~Renderer() {
    cleanup();
}

void Renderer::init() {
    if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), NULL, NULL);
    if (!m_window) { 
        glfwTerminate(); 
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);

    if (glewInit() != GLEW_OK) throw std::runtime_error("Failed to initialize GLEW");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    m_event_accum_shader = std::make_unique<Shader>("shaders/event_accum.vert", "shaders/event_accum.frag");
    m_quad_shader = std::make_unique<Shader>("shaders/quad.vert", "shaders/quad.frag");

    // (以下、コンストラクタの残りの部分と他のメソッド実装は前回の `Renderer2D` と同様)
}
