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

    std::cout << "\n--- 2D Viewer Controls ---\n"
              << "Mouse Drag: Pan | Mouse Wheel: Zoom\n"
              << "M: Cycle display mode\n"
              << "SPACE: Pause/Resume | LEFT/RIGHT: Speed\n"
              << "[ / ]: RGB Alpha | ' / ;: Event Alpha\n"
              << ", / .: Time Window\n"
              << "ESC: Exit\n" << std::endl;
}

void Renderer::setupCallbacks() {
    glfwSetKeyCallback(m_window, [](GLFWwindow* w, int k, int s, int a, int m) { static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onKey(k, s, a, m); });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* w, int b, int a, int m) { static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onMouseButton(b, a, m); });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* w, double x, double y) { static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onCursorPosition(x, y); });
    glfwSetScrollCallback(m_window, [](GLFWwindow* w, double x, double y) { static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onScroll(x, y); });
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* w, int width, int height) { static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onFramebufferSize(width, height); });
}

void Renderer::run(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset) {
    init();
    setupCallbacks();
    loadData(all_events, all_images, sensor_width, sensor_height, t_offset);
    mainLoop();
}

void Renderer::mainLoop() {
    double last_frame_time = glfwGetTime();

    while (!glfwWindowShouldClose(m_window)) {
        double current_frame_time = glfwGetTime();
        double delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;

        glfwPollEvents();
        
        if (!m_state.is_paused) {
            m_current_time_us += delta_time * 1000000.0 * m_state.playback_speed;
        }
        
        renderScene();

        glfwSwapBuffers(m_window);
    }
}

void Renderer::renderScene() {
    // === 1. イベント蓄積パス (オフスクリーンレンダリング) ===
    glBindFramebuffer(GL_FRAMEBUFFER, m_event_fbo);
    glViewport(0, 0, m_sensor_width, m_sensor_height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_event_count > 0 && m_state.display_mode != DisplayMode::RGB_ONLY) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        m_event_accum_shader->use();
        m_event_accum_shader->setFloat("u_current_time", static_cast<float>(m_current_time_us));
        m_event_accum_shader->setFloat("u_time_window", static_cast<float>(m_state.time_window_us));
        
        // CPUカリング: 描画範囲を絞り込む
        float end_time = static_cast<float>(m_current_time_us);
        float start_time = end_time - static_cast<float>(m_state.time_window_us);

        auto start_it = std::lower_bound(m_event_timestamps.begin(), m_event_timestamps.end(), start_time);
        auto end_it = std::lower_bound(start_it, m_event_timestamps.end(), end_time);

        if (start_it != m_event_timestamps.end()) {
            GLint first = std::distance(m_event_timestamps.begin(), start_it);
            GLsizei count = std::distance(start_it, end_it);
        
            if (count > 0) {
                glBindVertexArray(m_event_vao);
                glDrawArrays(GL_POINTS, first, count);
            }
        }
    }

    // === 2. コンポジションパス (画面への描画) ===
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_quad_shader->use();
    glm::mat4 projection = m_camera.getProjectionMatrix((float)m_width / m_height);
    glm::mat4 view = m_camera.getViewMatrix();
    m_quad_shader->setMat4("projection", projection);
    m_quad_shader->setMat4("view", view);

    glBindVertexArray(m_quad_vao);

    // 2a. 背景RGB画像の描画
    if (m_all_images_ptr && !m_all_images_ptr->empty() && m_state.display_mode != DisplayMode::EVENTS_ONLY) {
        double absolute_current_time = m_current_time_us + m_base_time;
        auto it = std::upper_bound(m_all_images_ptr->begin(), m_all_images_ptr->end(), absolute_current_time, 
            [](double time, const RGBFrame& frame){ return time < frame.timestamp; });
        if (it != m_all_images_ptr->begin()) {
            --it;
            size_t image_idx = std::distance(m_all_images_ptr->begin(), it);
            if(image_idx < m_image_textures.size()) {
                m_quad_shader->setFloat("u_alpha", m_state.rgb_alpha);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_image_textures[image_idx]);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }

    // 2b. イベント蓄積画像の描画
    if (m_state.display_mode != DisplayMode::RGB_ONLY) {
        m_quad_shader->setFloat("u_alpha", m_state.event_alpha);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_event_texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
}


void Renderer::cleanup() {
    glDeleteVertexArrays(1, &m_event_vao);
    glDeleteBuffers(1, &m_event_vbo);
    glDeleteVertexArrays(1, &m_quad_vao);
    glDeleteBuffers(1, &m_quad_vbo);
    
    glDeleteFramebuffers(1, &m_event_fbo);
    glDeleteTextures(1, &m_event_texture);

    if (!m_image_textures.empty()) {
        glDeleteTextures(m_image_textures.size(), m_image_textures.data());
    }

    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Renderer::loadData(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset) {
    m_sensor_width = sensor_width;
    m_sensor_height = sensor_height;

    // イベントデータ
    if (!all_events.empty()) {
        m_base_time = static_cast<double>(t_offset) + all_events.front().t;
        m_current_time_us = 0.0; // 再生時間はベースタイムからの相対時間に

        m_event_timestamps.resize(all_events.size());
        std::vector<EventVertex> vertices(all_events.size());
        for(size_t i = 0; i < all_events.size(); ++i) {
            vertices[i].x = (static_cast<float>(all_events[i].x) / sensor_width) * 2.0f - 1.0f;
            vertices[i].y = (static_cast<float>(all_events[i].y) / sensor_height) * -2.0f + 1.0f;
            vertices[i].timestamp = static_cast<float>((static_cast<double>(t_offset) + all_events[i].t) - m_base_time);
            vertices[i].polarity = all_events[i].pol;
            m_event_timestamps[i] = vertices[i].timestamp;
        }
        m_event_count = vertices.size();

        glGenBuffers(1, &m_event_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_event_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(EventVertex), vertices.data(), GL_STATIC_DRAW);

        glGenVertexArrays(1, &m_event_vao);
        glBindVertexArray(m_event_vao);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(EventVertex), (void*)offsetof(EventVertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(EventVertex), (void*)offsetof(EventVertex, timestamp));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(EventVertex), (void*)offsetof(EventVertex, polarity));
    }

    // 表示用クアッド
    float quad_vertices[] = { -1.0f,  1.0f, 0.0f, 1.0f,  1.0f,  1.0f, 1.0f, 1.0f,  1.0f, -1.0f, 1.0f, 0.0f,
                              -1.0f,  1.0f, 0.0f, 1.0f,  1.0f, -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);
    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // FBOとイベント蓄積用テクスチャの生成
    glGenFramebuffers(1, &m_event_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_event_fbo);
    glGenTextures(1, &m_event_texture);
    glBindTexture(GL_TEXTURE_2D, m_event_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sensor_width, sensor_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_event_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Framebuffer is not complete!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 画像テクスチャ
    m_all_images_ptr = &all_images;
    if (!all_images.empty()) {
        stbi_set_flip_vertically_on_load(true);
        for (const auto& frame : all_images) {
            int w, h, ch;
            unsigned char* data = stbi_load(frame.image_path.c_str(), &w, &h, &ch, 0);
            if (data) {
                GLuint texID;
                glGenTextures(1, &texID);
                glBindTexture(GL_TEXTURE_2D, texID);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                m_image_textures.push_back(texID);
                stbi_image_free(data);
            }
        }
    }
    glBindVertexArray(0);
}

// --- コールバックハンドラの実装 ---
void Renderer::onKey(int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(m_window, true);
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        m_state.is_paused = !m_state.is_paused;
        std::cout << (m_state.is_paused ? "--- Paused ---\n" : "--- Resumed ---\n");
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        m_state.display_mode = static_cast<DisplayMode>((static_cast<int>(m_state.display_mode) + 1) % 3);
        const char* modes[] = {"Events and RGB", "Events Only", "RGB Only"};
        printf("--- Mode: %s ---\n", modes[static_cast<int>(m_state.display_mode)]);
    }
    
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool updated = false;
        switch(key) {
            case GLFW_KEY_RIGHT_BRACKET: m_state.rgb_alpha = std::min(1.0f, m_state.rgb_alpha + 0.05f); updated = true; break;
            case GLFW_KEY_LEFT_BRACKET:  m_state.rgb_alpha = std::max(0.0f, m_state.rgb_alpha - 0.05f); updated = true; break;
            case GLFW_KEY_APOSTROPHE:    m_state.event_alpha = std::min(1.0f, m_state.event_alpha + 0.05f); updated = true; break;
            case GLFW_KEY_SEMICOLON:     m_state.event_alpha = std::max(0.0f, m_state.event_alpha - 0.05f); updated = true; break;
            case GLFW_KEY_PERIOD: m_state.time_window_us *= 1.2; updated = true; break;
            case GLFW_KEY_COMMA: m_state.time_window_us = std::max(1000.0, m_state.time_window_us / 1.2); updated = true; break;
            case GLFW_KEY_RIGHT: m_state.playback_speed *= 1.2f; updated = true; break;
            case GLFW_KEY_LEFT: m_state.playback_speed /= 1.2f; updated = true; break;
        }

        if (updated) {
            printf("Speed: %.2fx, RGB-α: %.2f, Evt-α: %.2f, Win: %.2fms\n", 
                m_state.playback_speed, m_state.rgb_alpha, m_state.event_alpha, m_state.time_window_us / 1e3);
        }
    }
}

void Renderer::onMouseButton(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        m_is_mouse_dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(m_window, &m_last_x, &m_last_y);
    }
}

void Renderer::onCursorPosition(double xpos, double ypos) {
    if (!m_is_mouse_dragging) return;
    float xoffset = static_cast<float>(xpos - m_last_x);
    float yoffset = static_cast<float>(ypos - m_last_y);
    m_last_x = xpos;
    m_last_y = ypos;
    m_camera.processMouseMovement(xoffset, yoffset, m_width, m_height);
}

void Renderer::onScroll(double xoffset, double yoffset) {
    m_camera.processMouseScroll(static_cast<float>(yoffset));
}

void Renderer::onFramebufferSize(int width, int height) {
    m_width = width;
    m_height = height;
    glViewport(0, 0, width, height);
}