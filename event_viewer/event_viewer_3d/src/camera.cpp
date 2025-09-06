#include "camera.h"

Camera::Camera()
    : m_target(0.0f, 0.0f, 0.0f),
      m_up(0.0f, 1.0f, 0.0f),
      m_azimuth(-135.0f),
      m_elevation(-30.0f),
      m_radius(4.0f) {
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    float az = glm::radians(m_azimuth);
    float el = glm::radians(m_elevation);

    m_position.x = m_radius * cos(el) * cos(az);
    m_position.y = m_radius * sin(el);
    m_position.z = m_radius * cos(el) * sin(az);
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_target, m_up);
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    float sensitivity = 0.2f;
    m_azimuth += xoffset * sensitivity;
    m_elevation += yoffset * sensitivity;

    if (constrainPitch) {
        if (m_elevation > 89.0f) m_elevation = 89.0f;
        if (m_elevation < -89.0f) m_elevation = -89.0f;
    }
    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
    m_radius -= yoffset * 0.2f;
    if (m_radius < 1.0f) m_radius = 1.0f;
    if (m_radius > 20.0f) m_radius = 20.0f;
    updateCameraVectors();
}