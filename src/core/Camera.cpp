#include "core/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(float aspectRatio)
    : m_Position(0.0f, 0.0f), m_Zoom(1.0f), m_AspectRatio(aspectRatio) {}

glm::mat4 Camera::GetViewMatrix() const {
    if (m_Orbit && m_Perspective) {
        float yaw = glm::radians(m_OrbitYaw), pitch = glm::radians(m_OrbitPitch);
        glm::vec3 dir(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
        glm::vec3 eye = m_OrbitFocus + dir * m_OrbitDist;
        return glm::lookAt(eye, m_OrbitFocus, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    return glm::translate(glm::mat4(1.0f), glm::vec3(-m_Position, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix() const {
    if (m_Perspective) {
        // юниты мировые: 1080*zoom — «высота» вида на глубине 1080 (fov от неё)
        float aspect = m_AspectRatio > 0.0f ? m_AspectRatio : 1.0f;
        float tanHalf = std::tan(glm::radians(m_Fov) * 0.5f);
        float nearP = 1.0f, farP = 20000.0f;
        glm::mat4 proj(0.0f);
        proj[0][0] = 1.0f / (aspect * tanHalf);
        proj[1][1] = 1.0f / tanHalf;
        proj[2][2] = -(farP + nearP) / (farP - nearP);
        proj[2][3] = -1.0f;
        proj[3][2] = -(2.0f * farP * nearP) / (farP - nearP);
        return proj;
    }
    float viewHeight = m_ViewHeight * m_Zoom;
    float viewWidth = viewHeight * m_AspectRatio;
    return glm::ortho(-viewWidth * 0.5f, viewWidth * 0.5f,
                      -viewHeight * 0.5f, viewHeight * 0.5f, -1.0f, 1.0f);
}

glm::mat4 Camera::GetViewProjectionMatrix() const {
    return GetProjectionMatrix() * GetViewMatrix();
}

glm::mat4 Camera::GetInverseViewProjectionMatrix() const {
    return glm::inverse(GetViewProjectionMatrix());
}

glm::vec2 Camera::ScreenToWorld(const glm::vec2& screenPos, float screenWidth, float screenHeight) const {
    float x = (screenPos.x / screenWidth) * 2.0f - 1.0f;
    float y = 1.0f - (screenPos.y / screenHeight) * 2.0f;
    glm::vec4 world = GetInverseViewProjectionMatrix() * glm::vec4(x, y, 0.0f, 1.0f);
    return glm::vec2(world.x, world.y);
}

void Camera::Pan(const glm::vec2& delta) {
    m_Position += delta;
}

void Camera::Zoom(float factor) {
    m_Zoom *= factor;
    if (m_Zoom < 0.05f) m_Zoom = 0.05f;
    if (m_Zoom > 20.0f) m_Zoom = 20.0f;
}
