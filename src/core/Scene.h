#pragma once
#include "utils/Framebuffer.h"
#include <memory>
#include <glm/glm.hpp>

class Camera;
class SceneManager;
class Renderer;  // forward declaration

class Scene {
public:
    Scene();
    ~Scene();

    void Init(Renderer* renderer);  // <-- принимаем renderer
    void Render(Camera* camera, SceneManager* sceneManager, int width, int height,
                const Camera* gameCamera = nullptr);
    void RenderGameView(Camera* gameCamera, SceneManager* sceneManager, int width, int height);

    GLuint GetViewportTexture() const;
    GLuint GetGameTexture() const { return m_GameFB ? m_GameFB->GetTexture() : 0; }
    glm::vec2 GetViewportSize() const;

    void SetGizmoHovered(bool hovered) { m_GizmoHovered = hovered; }
    void SetGizmoActive(bool active) { m_GizmoActive = active; }
    bool IsGizmoHovered() const { return m_GizmoHovered; }
    bool IsGizmoActive() const { return m_GizmoActive; }

    int GetGizmoAxis() const { return m_GizmoAxis; }
    void SetGizmoAxis(int axis) { m_GizmoAxis = axis; }

    glm::vec2 GetGizmoStartPos() const { return m_GizmoStartPos; }
    void SetGizmoStartPos(const glm::vec2& pos) { m_GizmoStartPos = pos; }

    glm::vec2 GetDragStartMouse() const { return m_DragStartMouse; }
    void SetDragStartMouse(const glm::vec2& pos) { m_DragStartMouse = pos; }

    // 0 = Move, 1 = Rotate, 2 = Scale
    int GetGizmoMode() const { return m_GizmoMode; }
    void SetGizmoMode(int mode) { m_GizmoMode = mode; }

    float GetDragStartAngle() const { return m_DragStartAngle; }
    void SetDragStartAngle(float a) { m_DragStartAngle = a; }
    float GetDragStartRotation() const { return m_DragStartRotation; }
    void SetDragStartRotation(float r) { m_DragStartRotation = r; }

    glm::vec2 GetDragStartScale() const { return m_DragStartScale; }
    void SetDragStartScale(const glm::vec2& s) { m_DragStartScale = s; }

private:
    Renderer* m_Renderer = nullptr;  // <-- храним указатель
    std::unique_ptr<Framebuffer> m_ViewportFB;
    std::unique_ptr<Framebuffer> m_GameFB;
    bool m_GizmoHovered = false;
    bool m_GizmoActive = false;
    int m_GizmoAxis = -1;
    glm::vec2 m_GizmoStartPos;
    glm::vec2 m_DragStartMouse;
    int m_GizmoMode = 0;
    float m_DragStartAngle = 0.0f;
    float m_DragStartRotation = 0.0f;
    glm::vec2 m_DragStartScale = glm::vec2(1.0f);
};
