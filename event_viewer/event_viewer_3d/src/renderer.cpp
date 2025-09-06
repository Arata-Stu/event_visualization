#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "renderer.h"
#include "cuda_processor.h" 
#include <iostream>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

// グローバルスコープにあった関数は、このラッパー関数に置き換わる
void run_renderer(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int width, int height, int64_t t_offset) {
    try {
        Renderer app(1280, 720, "Event Viewer");
        app.run(all_events, all_images, width, height, t_offset);
    } catch (const std::exception& e) {
        std::cerr << "A critical error occurred: " << e.what() << std::endl;
    }
}

// --- Renderer クラス実装 ---

Renderer::Renderer(int width, int height, const std::string& title) 
    : m_width(width), m_height(height), m_title(title), m_window(nullptr) {}

Renderer::~Renderer() {
    cleanup();
}

void Renderer::init() {
    if (!glfwInit()) { throw std::runtime_error("Failed to initialize GLFW"); }
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

    if (glewInit() != GLEW_OK) { throw std::runtime_error("Failed to initialize GLEW"); }
    init_cuda_for_gl();

    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    m_point_shader = std::make_unique<GLShader>("shaders/simple.vert", "shaders/simple.frag");
    m_box_shader = std::make_unique<GLShader>("shaders/box.vert", "shaders/box.frag");
    m_image_shader = std::make_unique<GLShader>("shaders/image.vert", "shaders/image.frag");

    std::cout << "\n--- Viewer Controls ---\n"
              << "Mouse Drag: Orbit camera | Mouse Wheel: Zoom\n"
              << "B: Toggle bounding box | M: Cycle display mode\n"
              << "SPACE: Pause/Resume | LEFT/RIGHT: Speed | UP/DOWN: Depth\n"
              << "[ / ]: Image Alpha | , / .: Time Window\n"
              << "ESC: Exit\n" << std::endl;
}

void Renderer::setupCallbacks() {
    // C++のラムダ式を使い、thisポインタをキャプチャせずにコールバックを設定する
    glfwSetKeyCallback(m_window, [](GLFWwindow* w, int k, int s, int a, int m) { 
        static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onKey(k, s, a, m); 
    });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* w, int b, int a, int m) { 
        static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onMouseButton(b, a, m); 
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* w, double x, double y) { 
        static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onCursorPosition(x, y); 
    });
    glfwSetScrollCallback(m_window, [](GLFWwindow* w, double x, double y) { 
        static_cast<Renderer*>(glfwGetWindowUserPointer(w))->onScroll(x, y); 
    });
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

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        renderScene();

        glfwSwapBuffers(m_window);
    }
}

void Renderer::renderScene() {
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)m_width / (float)m_height, 0.1f, 100.0f);
    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, m_state.depth_scale));

    // イベント描画
    if (m_point_count > 0 && m_state.display_mode != DisplayMode::RGB_ONLY) {
        m_point_shader->use();
        m_point_shader->setMat4("projection", projection);
        m_point_shader->setMat4("view", view);
        m_point_shader->setMat4("model", model);
        m_point_shader->setFloat("u_time", static_cast<float>(m_current_time_us - m_base_time));
        m_point_shader->setFloat("u_max_age", (float)m_state.time_window_us);

        glDisable(GL_BLEND);
        glBindVertexArray(m_point_vao);
        glDrawArrays(GL_POINTS, 0, m_point_count);
    }

    // 画像フレーム描画
    if (m_all_images_ptr && !m_all_images_ptr->empty() && m_state.display_mode != DisplayMode::EVENTS_ONLY) {
        m_image_shader->use();
        m_image_shader->setMat4("projection", projection);
        m_image_shader->setMat4("view", view);
        m_image_shader->setFloat("u_alpha", m_state.image_alpha);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(m_quad_vao);

        for (size_t i = 0; i < m_all_images_ptr->size(); ++i) {
            if (i >= m_image_textures.size()) continue;

            double image_age = m_current_time_us - (*m_all_images_ptr)[i].timestamp;
            if (image_age > 0 && image_age < m_state.time_window_us) {
                float normalized_age = static_cast<float>(image_age / m_state.time_window_us);
                float display_z = 1.0f - 2.0f * normalized_age;
                
                glm::mat4 image_model = model * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, display_z));
                m_image_shader->setMat4("model", image_model);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_image_textures[i]);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }
        glDisable(GL_BLEND);
    }

    // バウンディングボックス描画
    if (m_state.show_bounding_box) {
        m_box_shader->use();
        m_box_shader->setMat4("projection", projection);
        m_box_shader->setMat4("view", view);
        m_box_shader->setMat4("model", model);
        glBindVertexArray(m_box_vao);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

void Renderer::cleanup() {
    if (m_point_vao != 0) unregister_gl_buffer();
    
    glDeleteVertexArrays(1, &m_point_vao);
    glDeleteBuffers(1, &m_point_vbo);
    glDeleteVertexArrays(1, &m_box_vao);
    glDeleteBuffers(1, &m_box_vbo);
    glDeleteBuffers(1, &m_box_ebo);
    glDeleteVertexArrays(1, &m_quad_vao);
    glDeleteBuffers(1, &m_quad_vbo);
    glDeleteBuffers(1, &m_quad_ebo);
    if (!m_image_textures.empty()) {
        glDeleteTextures(m_image_textures.size(), m_image_textures.data());
    }

    m_point_shader.reset();
    m_box_shader.reset();
    m_image_shader.reset();

    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

void Renderer::loadData(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int sensor_width, int sensor_height, int64_t t_offset) {
    // イベントデータ
    if (!all_events.empty()) {
        m_base_time = static_cast<double>(t_offset) + all_events[0].t;
        m_current_time_us = m_base_time;

        glGenBuffers(1, &m_point_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_point_vbo);
        glBufferData(GL_ARRAY_BUFFER, all_events.size() * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
        register_gl_buffer(m_point_vbo);
        m_point_count = process_all_events(all_events, sensor_width, sensor_height, t_offset, m_base_time);
        
        glGenVertexArrays(1, &m_point_vao);
        glBindVertexArray(m_point_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_point_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    }

    // バウンディングボックス
    float box_vertices[] = { -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1 };
    unsigned int box_indices[] = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7 };
    glGenVertexArrays(1, &m_box_vao);
    glGenBuffers(1, &m_box_vbo);
    glGenBuffers(1, &m_box_ebo);
    glBindVertexArray(m_box_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_box_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_box_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(box_indices), box_indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // 画像用クアッド
    float quad_vertices[] = { -1, 1, 0, 0, 1,  -1, -1, 0, 0, 0,  1, -1, 0, 1, 0,  1, 1, 0, 1, 1 };
    unsigned int quad_indices[] = { 0, 1, 2, 0, 2, 3 };
    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);
    glGenBuffers(1, &m_quad_ebo);
    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quad_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

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
                GLenum format = (ch == 4) ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
    if (key == GLFW_KEY_B && action == GLFW_PRESS) m_state.show_bounding_box = !m_state.show_bounding_box;
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        m_state.is_paused = !m_state.is_paused;
        std::cout << (m_state.is_paused ? "--- Paused ---\n" : "--- Resumed ---\n");
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        if (m_state.display_mode == DisplayMode::EVENTS_AND_RGB) {
            m_state.display_mode = DisplayMode::EVENTS_ONLY;
            std::cout << "--- Mode: Events Only ---\n";
        } else if (m_state.display_mode == DisplayMode::EVENTS_ONLY) {
            m_state.display_mode = DisplayMode::RGB_ONLY;
            std::cout << "--- Mode: RGB Only ---\n";
        } else {
            m_state.display_mode = DisplayMode::EVENTS_AND_RGB;
            std::cout << "--- Mode: Events and RGB ---\n";
        }
    }
    
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool updated = false;
        switch(key) {
            case GLFW_KEY_RIGHT_BRACKET: m_state.image_alpha = std::min(1.0f, m_state.image_alpha + 0.05f); updated = true; break;
            case GLFW_KEY_LEFT_BRACKET: m_state.image_alpha = std::max(0.0f, m_state.image_alpha - 0.05f); updated = true; break;
            case GLFW_KEY_PERIOD: m_state.time_window_us *= 1.2; updated = true; break;
            case GLFW_KEY_COMMA: m_state.time_window_us = std::max(10000.0, m_state.time_window_us / 1.2); updated = true; break;
            case GLFW_KEY_RIGHT: m_state.playback_speed *= 1.2f; updated = true; break;
            case GLFW_KEY_LEFT: m_state.playback_speed /= 1.2f; updated = true; break;
            case GLFW_KEY_UP: m_state.depth_scale *= 1.2f; updated = true; break;
            case GLFW_KEY_DOWN: m_state.depth_scale = std::max(0.01f, m_state.depth_scale / 1.2f); updated = true; break;
        }

        if (updated) {
            printf("Speed: %.2fx, Depth: %.2f, Alpha: %.2f, Window: %.2fs\n", 
                m_state.playback_speed, m_state.depth_scale, m_state.image_alpha, m_state.time_window_us / 1e6);
        }
    }
}

void Renderer::onMouseButton(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            m_is_mouse_dragging = true;
            glfwGetCursorPos(m_window, &m_last_x, &m_last_y);
        } else if (action == GLFW_RELEASE) {
            m_is_mouse_dragging = false;
        }
    }
}

void Renderer::onCursorPosition(double xpos, double ypos) {
    if (!m_is_mouse_dragging) return;
    float xoffset = static_cast<float>(xpos - m_last_x);
    float yoffset = static_cast<float>(ypos - m_last_y);
    m_last_x = xpos;
    m_last_y = ypos;
    m_camera.processMouseMovement(xoffset, yoffset);
}

void Renderer::onScroll(double xoffset, double yoffset) {
    m_camera.processMouseScroll(static_cast<float>(yoffset));
}