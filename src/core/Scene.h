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
    void Render(Camera* camera, SceneManager* sceneManager, int width, int height);

    GLuint GetViewportTexture() const;
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

private:
    Renderer* m_Renderer = nullptr;  // <-- храним указатель
    std::unique_ptr<Framebuffer> m_ViewportFB;
    bool m_GizmoHovered = false;
    bool m_GizmoActive = false;
    int m_GizmoAxis = -1;
    glm::vec2 m_GizmoStartPos;
    glm::vec2 m_DragStartMouse;
};
