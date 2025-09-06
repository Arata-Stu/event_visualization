#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera();

    glm::mat4 getViewMatrix() const;

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void processMouseScroll(float yoffset);
    
    // カメラパラメータへのアクセサ
    float getZoom() const { return m_radius; }

private:
    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;

    float m_azimuth;
    float m_elevation;
    float m_radius;
    
    void updateCameraVectors();
};