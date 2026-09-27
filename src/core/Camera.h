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

    // Орбита в редакторе: взгляд вокруг фокуса (3D-вид)
    void SetOrbit(const glm::vec3& focus, float yawDeg, float pitchDeg, float dist) {
        m_Orbit = true; m_OrbitFocus = focus; m_OrbitYaw = yawDeg; m_OrbitPitch = pitchDeg; m_OrbitDist = dist;
    }
    void ClearOrbit() { m_Orbit = false; }
    bool IsOrbit() const { return m_Orbit; }
    glm::vec3 GetOrbitFocus() const { return m_OrbitFocus; }
    float GetOrbitYaw() const { return m_OrbitYaw; }
    float GetOrbitPitch() const { return m_OrbitPitch; }
    float GetOrbitDist() const { return m_OrbitDist; }

    void SetPerspective(bool on) { m_Perspective = on; }
    bool IsPerspective() const { return m_Perspective; }
    void SetFov(float deg) { m_Fov = deg; }
    float GetFov() const { return m_Fov; }

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;
    glm::mat4 GetInverseViewProjectionMatrix() const;

    glm::vec2 ScreenToWorld(const glm::vec2& screenPos, float screenWidth, float screenHeight) const;

    // ===== 3D-хелперы (в пикселях вьюпорта, y вниз — как в ImGui) =====
    // Мировая точка -> экран; false, если точка за камерой (w <= 0)
    bool WorldToScreen(const glm::vec3& world, const glm::vec2& vpSize, glm::vec2& outScreen) const;
    // Точка на экране -> луч в мире (начало + нормализованное направление)
    void ScreenToRay(const glm::vec2& screen, const glm::vec2& vpSize,
                     glm::vec3& origin, glm::vec3& dir) const;
    // Направление взгляда камеры
    glm::vec3 ViewDirection() const;
    // Мировых единиц на один экранный пиксель на глубине точки world
    float WorldPerPixelAt(const glm::vec3& world, const glm::vec2& vpSize) const;

    void Pan(const glm::vec2& delta);
    void Zoom(float factor);

private:
    glm::vec2 m_Position;
    float m_Zoom;
    float m_AspectRatio;
    float m_ViewHeight = 1080.0f;
    bool m_Perspective = false;
    float m_Fov = 50.0f;
    bool m_Orbit = false;
    glm::vec3 m_OrbitFocus = glm::vec3(0.0f);
    float m_OrbitYaw = 40.0f, m_OrbitPitch = 28.0f, m_OrbitDist = 1500.0f;
};
