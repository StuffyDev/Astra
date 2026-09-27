#pragma once
#include <deque>
#include <memory>
#include <unordered_set>
#include "core/Window.h"
#include "core/Renderer.h"
#include "core/Camera.h"
#include "core/GUI.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/ProjectManager.h"
#include "core/EditorState.h"
#include "ecs/SceneManager.h"
#include "ecs/Physics.h"

struct AppOptions {
    bool player = false;       // режим игры без редактора (--play)
    std::string projectDir;    // --project <dir>
    std::string scenePath;     // --scene <path>, иначе game.json в cwd
};

class Application {
public:
    Application(const AppOptions& options = {});
    ~Application();

    void Run();

    // Undo/redo редактора: снапшоты сцены (GUI пушит «до изменения», горячие клавиши — Ctrl+Z/Ctrl+Shift+Z)
    void PushUndoSnapshot(const SceneManager::SceneSnapshot& snap);
    bool Undo();
    bool Redo();

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<ProjectManager> m_ProjectManager;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<Camera> m_Camera;
    std::unique_ptr<GUI> m_GUI;
    std::unique_ptr<Scene> m_Scene;
    std::unique_ptr<SceneManager> m_SceneManager;
    std::unique_ptr<SceneSerializer> m_Serializer;

    EditorContext m_Ctx;

    EditorState m_EditorState = EditorState::Edit;
    EditorState m_LastEditorState = EditorState::Edit;
    SceneManager::SceneSnapshot m_PlaySnapshot;

    float m_PhysicsAccumulator = 0.0f;
    bool m_ScenePanning = false;
    bool m_OrbitInit = false;
    glm::vec3 m_OrbitFocus = glm::vec3(0.0f);
    float m_OrbitYaw = 40.0f, m_OrbitPitch = 28.0f, m_OrbitDist = 1500.0f;
    std::unordered_set<uint32_t> m_AudioStarted; // playOnAwake уже запущен (в этом Play)

    AppOptions m_Options;
    bool m_PlayerMode = false;
    std::deque<SceneManager::SceneSnapshot> m_UndoStack;
    std::deque<SceneManager::SceneSnapshot> m_RedoStack;

    void ProcessInput(float deltaTime);
    void HandleFileDrops();
    void Update(float deltaTime);
    void Render(float deltaTime);
    void ConsumePhysicsEvents();
    void PaintTileAtMouse();
    void SeedDemoScene();
};
