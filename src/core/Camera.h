#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(float aspectRatio);

    void SetPosition(const glm::vec2& pos) { m_Position = pos; }
    glm::vec2 GetPosition() const { return m_Position; }

    void SetZoom(float zoom) { m_Zoom = zoom; }
    float GetZoom() const { return m_Zoom; }

    void SetAspectRatio(float aspect) { m_AspectRatio = aspect; }
    float GetAspectRatio() const { return m_AspectRatio; }

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;
    glm::mat4 GetInverseViewProjectionMatrix() const;

    glm::vec2 ScreenToWorld(const glm::vec2& screenPos, float screenWidth, float screenHeight) const;

    void Pan(const glm::vec2& delta);
    void Zoom(float factor);

private:
    glm::vec2 m_Position;
    float m_Zoom;
    float m_AspectRatio;
    float m_ViewHeight = 1080.0f;
};
