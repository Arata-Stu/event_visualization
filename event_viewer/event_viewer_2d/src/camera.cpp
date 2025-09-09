#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
    : m_position(0.0f, 0.0f), m_zoom(1.0f) {}

glm::mat4 Camera::getViewMatrix() const {
    return glm::translate(glm::mat4(1.0f), glm::vec3(-m_position, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(float aspect_ratio) const {
    float left = -aspect_ratio * m_zoom;
    float right = aspect_ratio * m_zoom;
    float bottom = -1.0f * m_zoom;
    float top = 1.0f * m_zoom;
    return glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
}

void Camera::processMouseMovement(float xoffset, float yoffset, int screen_width, int screen_height) {
    float dx = -xoffset * (2.0f * m_zoom / screen_height);
    float dy = yoffset * (2.0f * m_zoom / screen_height);
    m_position += glm::vec2(dx, dy);
}

void Camera::processMouseScroll(float yoffset) {
    float zoom_speed = 0.1f;
    m_zoom -= yoffset * zoom_speed * m_zoom;
    if (m_zoom < 0.05f) m_zoom = 0.05f;
    if (m_zoom > 5.0f) m_zoom = 5.0f;
}