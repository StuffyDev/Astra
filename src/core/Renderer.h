#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
class Camera;
struct Entity;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();

    // Рендер сцены в Framebuffer
    void BeginScene(Camera* camera, int width, int height);
    void RenderGrid(Camera* camera);
    void RenderEntities(const std::vector<Entity>& entities, Camera* camera);
    void RenderGizmo(const Entity* selectedEntity, Camera* camera, int screenW, int screenH);
    void EndScene();

    // Рендер Gizmo линий
    void RenderGizmoLines(const std::vector<float>& lines, const glm::vec3& color, Camera* camera);

    void Shutdown();

private:
    std::unique_ptr<Shader> m_LineShader;
    std::unique_ptr<Shader> m_SpriteShader;
    std::unique_ptr<Shader> m_CircleShader;

    GLuint m_GridVAO = 0, m_GridVBO = 0;
    GLuint m_QuadVAO = 0, m_QuadVBO = 0, m_QuadEBO = 0;
    GLuint m_GizmoVAO = 0, m_GizmoVBO = 0;

    int m_GridCount = 0;
    int m_ScreenW = 0, m_ScreenH = 0;

    void SetupGridBuffers();
    void UpdateGrid(Camera* camera);
    void SetupQuad();
    void SetupGizmoBuffers();
};
