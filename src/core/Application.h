#pragma once
#include <memory>
#include "core/Window.h"
#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/GUI.h"
#include "core/Scene.h"
#include "ecs/SceneManager.h"

class Application {
public:
    Application();
    ~Application();

    void Run();

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<Camera> m_Camera;
    std::unique_ptr<GUI> m_GUI;
    std::unique_ptr<Scene> m_Scene;
    std::unique_ptr<SceneManager> m_SceneManager;

    void ProcessInput(float deltaTime);
    void Update(float deltaTime);
    void Render(float deltaTime);
};
