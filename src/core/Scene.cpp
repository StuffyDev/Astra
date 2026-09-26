#include "core/Scene.h"
#include "core/Camera.h"
#include "core/Renderer.h"
#include "ecs/SceneManager.h"
#include "ecs/Entity.h"

Scene::Scene() = default;
Scene::~Scene() = default;

void Scene::Init(Renderer* renderer) {
    m_Renderer = renderer;
    m_ViewportFB = std::make_unique<Framebuffer>(1280, 720);
    m_GameFB = std::make_unique<Framebuffer>(1280, 720);
}

void Scene::Render(Camera* camera, SceneManager* sceneManager, int width, int height,
                   const Camera* gameCamera) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    m_ViewportFB->Resize(width, height);
    m_ViewportFB->Bind();

    m_Renderer->BeginScene(camera, width, height);
    m_Renderer->RenderGrid(camera);
    m_Renderer->RenderEntities(sceneManager->GetEntities(), camera);
    m_Renderer->RenderColliders(sceneManager->GetEntities(), camera);
    m_Renderer->RenderGizmo(sceneManager->GetSelectedEntityPtr(), sceneManager->GetEntities(),
                             camera, width, height, m_GizmoMode, m_GizmoAxis);

    if (gameCamera) {
        // Рамка того, что видит game-камера (как в Unity)
        float viewHeight = 1080.0f * gameCamera->GetZoom();
        float viewWidth = viewHeight * gameCamera->GetAspectRatio();
        glm::vec2 c = gameCamera->GetPosition();
        float hw = viewWidth * 0.5f, hh = viewHeight * 0.5f;
        std::vector<float> rect = {
            c.x - hw, c.y - hh,  c.x + hw, c.y - hh,
            c.x + hw, c.y - hh,  c.x + hw, c.y + hh,
            c.x + hw, c.y + hh,  c.x - hw, c.y + hh,
            c.x - hw, c.y + hh,  c.x - hw, c.y - hh
        };
        m_Renderer->RenderGizmoLines(rect, glm::vec3(1.0f, 0.6f, 0.1f), camera);
    }

    m_Renderer->EndScene();

    m_ViewportFB->Unbind();
}

void Scene::RenderGameView(Camera* gameCamera, SceneManager* sceneManager, int width, int height) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    m_GameFB->Resize(width, height);
    m_GameFB->Bind();

    m_Renderer->BeginScene(gameCamera, width, height);
    m_Renderer->RenderEntities(sceneManager->GetEntities(), gameCamera);
    m_Renderer->EndScene();

    m_GameFB->Unbind();
}

GLuint Scene::GetViewportTexture() const {
    return m_ViewportFB->GetTexture();
}

glm::vec2 Scene::GetViewportSize() const {
    return glm::vec2(m_ViewportFB->GetWidth(), m_ViewportFB->GetHeight());
}
