#include "core/Camera.h"

Camera::Camera(float aspectRatio)
    : m_Position(0.0f, 0.0f), m_Zoom(1.0f), m_AspectRatio(aspectRatio) {}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::translate(glm::mat4(1.0f), glm::vec3(-m_Position, 0.0f));
}

glm::mat4 Camera::GetProjectionMatrix() const {
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
