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
}

void Scene::Render(Camera* camera, SceneManager* sceneManager, int width, int height) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    m_ViewportFB->Resize(width, height);
    m_ViewportFB->Bind();

    m_Renderer->BeginScene(camera, width, height);
    m_Renderer->RenderGrid(camera);
    m_Renderer->RenderEntities(sceneManager->GetEntities(), camera);
    m_Renderer->RenderGizmo(sceneManager->GetSelectedEntityPtr(), camera, width, height);
    m_Renderer->EndScene();

    m_ViewportFB->Unbind();
}

GLuint Scene::GetViewportTexture() const {
    return m_ViewportFB->GetTexture();
}

glm::vec2 Scene::GetViewportSize() const {
    return glm::vec2(m_ViewportFB->GetWidth(), m_ViewportFB->GetHeight());
}
