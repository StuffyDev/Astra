#include "core/Application.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

Application::Application() {
    m_Window = std::make_unique<Window>(1920, 1080, "Engine2D");
    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Init();

    m_Camera = std::make_unique<Camera>(16.0f / 9.0f);

    m_SceneManager = std::make_unique<SceneManager>();
    m_Scene = std::make_unique<Scene>();
    m_Scene->Init(m_Renderer.get());

    m_GUI = std::make_unique<GUI>(m_Window->GetNativeWindow());
    m_GUI->Init();

    // Тестовые объекты
    Entity e1;
    e1.name = "Quad";
    e1.sprite.type = SpriteType::Quad;
    e1.sprite.color = glm::vec3(0.8f, 0.3f, 0.3f);
    e1.transform.scale = glm::vec2(200.0f, 100.0f);
    m_SceneManager->AddEntity(e1);

    Entity e2;
    e2.name = "Circle";
    e2.sprite.type = SpriteType::Circle;
    e2.sprite.color = glm::vec3(0.3f, 0.8f, 0.3f);
    e2.transform.position = glm::vec2(200.0f, 100.0f);
    e2.transform.scale = glm::vec2(150.0f, 150.0f);
    m_SceneManager->AddEntity(e2);
}

Application::~Application() = default;

void Application::Run() {
    float lastTime = 0.0f;

    while (!m_Window->ShouldClose()) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        lastTime = currentTime;

        ProcessInput(deltaTime);
        Update(deltaTime);
        Render(deltaTime);

        m_Window->SwapBuffers();
        m_Window->PollEvents();
    }
}

void Application::ProcessInput(float deltaTime) {
    ImGuiIO& io = ImGui::GetIO();

    // Камера только если Scene в фокусе и не клик по GUI
    if (!m_GUI->IsSceneHovered() && !m_GUI->IsSceneFocused()) {
        m_Window->UpdateLastMousePos();
        m_Window->ResetScrollOffset();
        return;
    }

    if (io.WantCaptureMouse) {
        m_Window->UpdateLastMousePos();
        m_Window->ResetScrollOffset();
        return;
    }

    float moveSpeed = 500.0f * deltaTime * m_Camera->GetZoom();

    if (m_Window->IsKeyDown(GLFW_KEY_W)) m_Camera->Pan(glm::vec2(0.0f, moveSpeed));
    if (m_Window->IsKeyDown(GLFW_KEY_S)) m_Camera->Pan(glm::vec2(0.0f, -moveSpeed));
    if (m_Window->IsKeyDown(GLFW_KEY_A)) m_Camera->Pan(glm::vec2(-moveSpeed, 0.0f));
    if (m_Window->IsKeyDown(GLFW_KEY_D)) m_Camera->Pan(glm::vec2(moveSpeed, 0.0f));

    float scroll = m_Window->GetScrollOffset();
    if (scroll != 0.0f) {
        m_Camera->Zoom(1.0f - scroll * 0.1f);
        m_Window->ResetScrollOffset();
    }

    // Pan средней кнопкой
    if (m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
        glm::vec2 current = m_Window->GetMousePos();
        glm::vec2 delta = current - m_Window->GetLastMousePos();
        m_Camera->Pan(glm::vec2(-delta.x, delta.y) * m_Camera->GetZoom() * 1.5f);
    }

    // Gizmo drag
    if (m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) && m_GUI->IsSceneHovered()) {
        Entity* selected = m_SceneManager->GetSelectedEntityPtr();
        if (selected) {
            // Простой Gizmo: тянем за центр объекта
            // Пока что просто драг объекта если кликнули рядом
            glm::vec2 worldMouse = m_Camera->ScreenToWorld(
                m_GUI->GetSceneMousePos(),
                m_GUI->GetSceneSize().x,
                m_GUI->GetSceneSize().y
            );

            if (!m_Scene->IsGizmoActive()) {
                // Проверяем попадание в объект
                glm::vec2 half = selected->transform.scale * 0.5f;
                if (worldMouse.x >= selected->transform.position.x - half.x &&
                    worldMouse.x <= selected->transform.position.x + half.x &&
                    worldMouse.y >= selected->transform.position.y - half.y &&
                    worldMouse.y <= selected->transform.position.y + half.y) {
                    m_Scene->SetGizmoActive(true);
                    m_Scene->SetGizmoStartPos(selected->transform.position);
                    m_Scene->SetDragStartMouse(worldMouse);
                }
            } else {
                glm::vec2 dragDelta = worldMouse - m_Scene->GetDragStartMouse();
                selected->transform.position = m_Scene->GetGizmoStartPos() + dragDelta;
            }
        }
    } else {
        m_Scene->SetGizmoActive(false);
    }

    m_Window->UpdateLastMousePos();
}

void Application::Update(float deltaTime) {
    (void)deltaTime;
}

void Application::Render(float deltaTime) {
    int w = m_Window->GetWidth();
    int h = m_Window->GetHeight();

    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_GUI->BeginFrame();
    m_GUI->RenderEditorPanels(m_Camera.get(), m_SceneManager.get(), m_Scene.get(), deltaTime);
    m_GUI->EndFrame();
}
