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

    // Полётная редакторская камера (стиль Unity): положение + yaw/pitch (в градусах).
    // forward = (-cos(p)sin(y), -sin(p), -cos(p)cos(y)); pitch>0 = смотреть вниз.
    void SetFly(const glm::vec3& eye, float yawDeg, float pitchDeg) {
        m_Fly = true; m_Eye = eye; m_FlyYaw = yawDeg; m_FlyPitch = pitchDeg;
    }
    glm::vec3 GetEyePosition() const { return m_Eye; }
    float GetFlyYaw() const { return m_FlyYaw; }
    float GetFlyPitch() const { return m_FlyPitch; }

    // Орбита как частный случай: глаз в focus - forward*dist (взгляд всегда на focus)
    void SetOrbit(const glm::vec3& focus, float yawDeg, float pitchDeg, float dist) {
        glm::vec3 f = ForwardOf(yawDeg, pitchDeg);
        SetFly(focus - f * dist, yawDeg, pitchDeg);
    }
    void ClearOrbit() { m_Fly = false; }
    bool IsOrbit() const { return m_Fly; }

    // Базис камеры в мировых координатах
    static glm::vec3 ForwardOf(float yawDeg, float pitchDeg);
    glm::vec3 Forward() const { return ForwardOf(m_FlyYaw, m_FlyPitch); }
    glm::vec3 Right() const;
    glm::vec3 Up() const;

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
    // 3D-камера редактора: глаз + углы обзора (градусы)
    bool m_Fly = false;
    glm::vec3 m_Eye = glm::vec3(0.0f, 0.0f, 1000.0f);
    float m_FlyYaw = 0.0f, m_FlyPitch = 0.0f;
};
