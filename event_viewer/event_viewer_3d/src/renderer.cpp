// stb_image.hの実装を定義(必ず他のincludeより前に記述)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "renderer.h"
#include "cuda_processor.h"
#include "event_data.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdio> // printfのため

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- 状態管理用変数 ---
// オービットカメラ
float camera_azimuth = -135.0f;
float camera_elevation = -30.0f;
float camera_radius = 4.0f;
glm::vec3 camera_target = glm::vec3(0.0f, 0.0f, 0.0f);
// マウス入力
bool is_mouse_dragging = false;
double last_x = 0.0, last_y = 0.0;
// 輪郭表示フラグ
bool show_bounding_box = true;

// 再生制御用の変数
float playback_speed = 1.0f;
double current_time_us = 0.0;
bool is_paused = false;

// 奥行き制御用の変数
float depth_scale = 1.0f;

// 表示モードを管理するための列挙型と変数
enum class DisplayMode {
    EVENTS_AND_RGB,
    EVENTS_ONLY,
    RGB_ONLY
};
DisplayMode current_display_mode = DisplayMode::EVENTS_AND_RGB;
float image_alpha = 0.8f;

// 表示時間幅を管理する変数
double time_window_us = 2000000.0; // デフォルトは2秒

// --- GLFWコールバック関数 ---
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        show_bounding_box = !show_bounding_box;
    }

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        is_paused = !is_paused;
        std::cout << (is_paused ? "--- Paused ---\n" : "--- Resumed ---\n");
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        if (current_display_mode == DisplayMode::EVENTS_AND_RGB) {
            current_display_mode = DisplayMode::EVENTS_ONLY;
            std::cout << "--- Mode: Events Only ---\n";
        } else if (current_display_mode == DisplayMode::EVENTS_ONLY) {
            current_display_mode = DisplayMode::RGB_ONLY;
            std::cout << "--- Mode: RGB Only ---\n";
        } else {
            current_display_mode = DisplayMode::EVENTS_AND_RGB;
            std::cout << "--- Mode: Events and RGB ---\n";
        }
    }

    if (key == GLFW_KEY_RIGHT_BRACKET && (action == GLFW_PRESS || action == GLFW_REPEAT)) { // ']' key
        image_alpha += 0.05f;
        if (image_alpha > 1.0f) image_alpha = 1.0f;
        printf("Image Alpha: %.2f\n", image_alpha);
    }
    if (key == GLFW_KEY_LEFT_BRACKET && (action == GLFW_PRESS || action == GLFW_REPEAT)) { // '[' key
        image_alpha -= 0.05f;
        if (image_alpha < 0.0f) image_alpha = 0.0f;
        printf("Image Alpha: %.2f\n", image_alpha);
    }

    if (key == GLFW_KEY_PERIOD && (action == GLFW_PRESS || action == GLFW_REPEAT)) { // '.' key
        time_window_us *= 1.2;
        printf("Time Window: %.2f s\n", time_window_us / 1000000.0);
    }
    if (key == GLFW_KEY_COMMA && (action == GLFW_PRESS || action == GLFW_REPEAT)) { // ',' key
        time_window_us /= 1.2;
        if (time_window_us < 10000) time_window_us = 10000; // 10ms未満にはしない
        printf("Time Window: %.2f s\n", time_window_us / 1000000.0);
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool updated = false;
        switch (key) {
            case GLFW_KEY_RIGHT:
                playback_speed *= 1.2f;
                updated = true;
                break;
            case GLFW_KEY_LEFT:
                playback_speed /= 1.2f;
                updated = true;
                break;
            case GLFW_KEY_UP:
                depth_scale *= 1.2f;
                updated = true;
                break;
            case GLFW_KEY_DOWN:
                depth_scale /= 1.2f;
                if (depth_scale < 0.01f) depth_scale = 0.01f;
                updated = true;
                break;
        }
        if(updated){
             printf("Speed: %.2fx, Depth Scale: %.2f\n", playback_speed, depth_scale);
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            is_mouse_dragging = true;
            glfwGetCursorPos(window, &last_x, &last_y);
        } else if (action == GLFW_RELEASE) {
            is_mouse_dragging = false;
        }
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!is_mouse_dragging) return;

    float xoffset = xpos - last_x;
    float yoffset = ypos - last_y;
    last_x = xpos;
    last_y = ypos;

    float sensitivity = 0.2f;
    camera_azimuth += xoffset * sensitivity;
    camera_elevation += yoffset * sensitivity;

    if (camera_elevation > 89.0f) camera_elevation = 89.0f;
    if (camera_elevation < -89.0f) camera_elevation = -89.0f;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera_radius -= (float)yoffset * 0.2f;
    if (camera_radius < 1.0f) camera_radius = 1.0f;
    if (camera_radius > 20.0f) camera_radius = 20.0f;
}


// --- シェーダー読み込みヘルパー関数 ---
GLuint LoadShaders(const char* vertex_file_path, const char* fragment_file_path) {
    std::string VertexShaderCode;
    std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
    if (!VertexShaderStream.is_open()) {
        std::cerr << "Failed to open " << vertex_file_path << std::endl;
        return 0;
    }
    std::stringstream sstr;
    sstr << VertexShaderStream.rdbuf();
    VertexShaderCode = sstr.str();
    VertexShaderStream.close();

    std::string FragmentShaderCode;
    std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
    if (!FragmentShaderStream.is_open()) {
        std::cerr << "Failed to open " << fragment_file_path << std::endl;
        return 0;
    }
    std::stringstream sstr2;
    sstr2 << FragmentShaderStream.rdbuf();
    FragmentShaderCode = sstr2.str();
    FragmentShaderStream.close();

    GLint Result = GL_FALSE;
    int InfoLogLength;

    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    char const* VertexSourcePointer = VertexShaderCode.c_str();
    glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
    glCompileShader(VertexShaderID);
    glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
        std::cerr << "Vertex Shader Error: " << &VertexShaderErrorMessage[0] << std::endl;
    }

    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    char const* FragmentSourcePointer = FragmentShaderCode.c_str();
    glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
    glCompileShader(FragmentShaderID);
    glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
        std::cerr << "Fragment Shader Error: " << &FragmentShaderErrorMessage[0] << std::endl;
    }

    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
    glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
    if (InfoLogLength > 0) {
        std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
        glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
        std::cerr << "Shader Link Error: " << &ProgramErrorMessage[0] << std::endl;
    }

    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);
    return ProgramID;
}


// --- メインレンダリング関数 ---
void run_renderer(const std::vector<EventCD>& all_events, const std::vector<RGBFrame>& all_images, int width, int height, int64_t t_offset) {
    if (!glfwInit()) { return; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Event Viewer", NULL, NULL);
    if (!window) { glfwTerminate(); return; }
    glfwMakeContextCurrent(window);
    glViewport(0, 0, 1280, 720);

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (glewInit() != GLEW_OK) { return; }
    init_cuda_for_gl();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    std::cout << "\n--- Viewer Controls ---\n"
              << "Mouse Drag: Orbit camera\n"
              << "Mouse Wheel: Zoom in/out\n"
              << "-----------------------\n"
              << "B key: Toggle bounding box\n"
              << "M key: Cycle display mode (All / Events / RGB)\n"
              << "SPACE key: Pause / Resume\n"
              << "RIGHT/LEFT arrow: Speed up / Slow down\n"
              << "UP/DOWN arrow: Stretch / Squeeze depth\n"
              << "+/- key: Adjust RGB frame alpha\n"
              << "PageUp/PageDown: Change time window duration\n"
              << "-----------------------\n"
              << "ESC key: Exit\n"
              << "-----------------------\n" << std::endl;

    // --- シェーダープログラムの読み込み ---
    GLuint pointShaderProgram = LoadShaders("shaders/simple.vert", "shaders/simple.frag");
    GLuint boxShaderProgram = LoadShaders("shaders/box.vert", "shaders/box.frag");
    GLuint imageShaderProgram = LoadShaders("shaders/image.vert", "shaders/image.frag");
    
    // --- イベントデータの準備 ---
    unsigned int point_count = 0;
    GLuint pointVao = 0, pointVbo = 0;
    double base_time = 0;

    if (!all_events.empty()) {
        base_time = static_cast<double>(t_offset) + all_events[0].t;
        
        glGenBuffers(1, &pointVbo);
        glBindBuffer(GL_ARRAY_BUFFER, pointVbo);
        glBufferData(GL_ARRAY_BUFFER, all_events.size() * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        register_gl_buffer(pointVbo);
        point_count = process_all_events(all_events, width, height, t_offset, base_time);
        
        glGenVertexArrays(1, &pointVao);
        glBindVertexArray(pointVao);
        glBindBuffer(GL_ARRAY_BUFFER, pointVbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, r));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    
    // --- バウンディングボックスの準備 ---
    float box_vertices[] = {
        -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, 1.0f, -1.0f,  1.0f, 1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
    };
    unsigned int box_indices[] = {
        0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7
    };
    GLuint boxVao, boxVbo, boxEbo;
    glGenVertexArrays(1, &boxVao);
    glGenBuffers(1, &boxVbo);
    glGenBuffers(1, &boxEbo);
    glBindVertexArray(boxVao);
    glBindBuffer(GL_ARRAY_BUFFER, boxVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boxEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(box_indices), box_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // --- 画像描画用のクアッド(板ポリゴン)を準備 ---
    float quad_vertices[] = {
        // positions      // texture Coords
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f
    };
    unsigned int quad_indices[] = {  
        0, 1, 2, 0, 2, 3
    };
    GLuint quadVao, quadVbo, quadEbo;
    glGenVertexArrays(1, &quadVao);
    glGenBuffers(1, &quadVbo);
    glGenBuffers(1, &quadEbo);
    glBindVertexArray(quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    // --- 画像ファイルを読み込み、OpenGLテクスチャに変換 ---
    std::vector<GLuint> image_texture_ids;
    if (!all_images.empty()) {
        stbi_set_flip_vertically_on_load(true); 
        for (const auto& frame : all_images) {
            int img_width, img_height, img_channels;
            unsigned char *data = stbi_load(frame.image_path.c_str(), &img_width, &img_height, &img_channels, 0);
            if (data) {
                GLuint textureID;
                glGenTextures(1, &textureID);
                glBindTexture(GL_TEXTURE_2D, textureID);
                
                GLenum format = (img_channels == 4) ? GL_RGBA : GL_RGB;
                glTexImage2D(GL_TEXTURE_2D, 0, format, img_width, img_height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                
                image_texture_ids.push_back(textureID);
                stbi_image_free(data);
            } else {
                std::cerr << "Warning: Failed to load texture: " << frame.image_path << std::endl;
            }
        }
    }

    // --- 描画ループの開始 ---
    if (!all_events.empty()) {
        current_time_us = base_time;
    }
    double last_frame_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        double current_frame_time = glfwGetTime();
        double delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;

        if (!is_paused) {
            current_time_us += delta_time * 1000000.0 * playback_speed;
        }

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- 行列の計算 ---
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -camera_radius));
        view = glm::rotate(view, glm::radians(camera_elevation), glm::vec3(1.0f, 0.0f, 0.0f));
        view = glm::rotate(view, glm::radians(camera_azimuth), glm::vec3(0.0f, 1.0f, 0.0f));
        view = glm::translate(view, -camera_target);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, depth_scale));

        // --- イベントの描画 ---
        if (point_count > 0 && (current_display_mode == DisplayMode::EVENTS_ONLY || current_display_mode == DisplayMode::EVENTS_AND_RGB)) {
            glUseProgram(pointShaderProgram);
            glUniformMatrix4fv(glGetUniformLocation(pointShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(pointShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(pointShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
            
            float relative_u_time = static_cast<float>(current_time_us - base_time);
            glUniform1f(glGetUniformLocation(pointShaderProgram, "u_time"), relative_u_time);
            glUniform1f(glGetUniformLocation(pointShaderProgram, "u_max_age"), (float)time_window_us);

            glDisable(GL_BLEND);
            glBindVertexArray(pointVao);
            glDrawArrays(GL_POINTS, 0, point_count);
            glBindVertexArray(0);
        }

        // --- 画像フレームの描画 ---
        if (!all_images.empty() && (current_display_mode == DisplayMode::RGB_ONLY || current_display_mode == DisplayMode::EVENTS_AND_RGB)) {
            glUseProgram(imageShaderProgram);
            glUniformMatrix4fv(glGetUniformLocation(imageShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(imageShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniform1f(glGetUniformLocation(imageShaderProgram, "u_alpha"), image_alpha);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            glBindVertexArray(quadVao);
            for (size_t i = 0; i < all_images.size(); ++i) {
                if (i >= image_texture_ids.size()) continue;

                double image_age = current_time_us - all_images[i].timestamp;
                
                if (image_age > 0 && image_age < time_window_us) {
                    float normalized_age = image_age / time_window_us;
                    float display_z = 1.0f - 2.0f * normalized_age;
                    
                    glm::mat4 image_translate_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, display_z));
                    glm::mat4 final_image_model = model * image_translate_model;
                    
                    glUniformMatrix4fv(glGetUniformLocation(imageShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(final_image_model));

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, image_texture_ids[i]);
                    
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }
            glBindVertexArray(0);
            glDisable(GL_BLEND);
        }
        
        // --- バウンディングボックスの描画 ---
        if (show_bounding_box) {
            glUseProgram(boxShaderProgram);
            glUniformMatrix4fv(glGetUniformLocation(boxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(boxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(boxShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glBindVertexArray(boxVao);
            glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        
        glfwSwapBuffers(window);
    }

    // --- 後処理 ---
    glDeleteVertexArrays(1, &quadVao);
    glDeleteBuffers(1, &quadVbo);
    glDeleteBuffers(1, &quadEbo);
    if (!image_texture_ids.empty()) {
        glDeleteTextures(image_texture_ids.size(), image_texture_ids.data());
    }
    glDeleteProgram(imageShaderProgram);

    if (pointVao != 0) {
        unregister_gl_buffer();
        glDeleteBuffers(1, &pointVbo);
        glDeleteVertexArrays(1, &pointVao);
    }
    glDeleteBuffers(1, &boxVbo);
    glDeleteBuffers(1, &boxEbo);
    glDeleteVertexArrays(1, &boxVao);
    glDeleteProgram(pointShaderProgram);
    glDeleteProgram(boxShaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
}