#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera();

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect_ratio) const;

    void processMouseMovement(float xoffset, float yoffset, int screen_width, int screen_height);
    void processMouseScroll(float yoffset);
    
private:
    glm::vec2 m_position;
    float m_zoom;
};