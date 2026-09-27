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

    // Навигация 3D-вида (нужна компасу в углу Scene-вью)
    float SceneViewYaw() const { return m_FlyYaw; }
    float SceneViewPitch() const { return m_FlyPitch; }
    void SceneViewLook(float yaw, float pitch);   // смотреть на точку фокуса под новыми углами

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
    bool m_FlyInit = false;
    glm::vec3 m_FlyPos = glm::vec3(0.0f, 300.0f, 1400.0f);
    float m_FlyYaw = 0.0f, m_FlyPitch = 18.0f;   // pitch>0 — смотреть вниз
    float m_FlyRefDist = 1400.0f;                // расстояние до «точки интереса» (пан/виды)
    float m_FlySpeed = 900.0f;                   // мировых единиц в секунду
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

    // ===== 3D: выделение кликом + гизмо (только в 3D Mode) =====
    // hot — мышь ровно над изображением сцены (вне вкладок/компаса); отпускание кнопки ловим всегда
    void HandleSceneMouse3D(const glm::vec2& viewportSize, int mode, bool editing, bool hot);
    int HitGizmo3D(const glm::vec3& center, float len, int mode,
                   const glm::vec2& mouse, const glm::vec2& viewportSize) const;
    bool PlaneHit(const glm::vec3& planePoint, const glm::vec3& planeNormal,
                  const glm::vec2& mouse, const glm::vec2& viewportSize, glm::vec3& out) const;
    void BeginGizmoDrag3D(Entity* e, int mode, int grab, const glm::vec2& mouse,
                          const glm::vec2& viewportSize);
    void UpdateGizmoDrag3D(const glm::vec2& mouse, const glm::vec2& viewportSize);
    void PickEntity3D(const glm::vec2& mouse, const glm::vec2& viewportSize);
    glm::vec3 SceneViewFocusPoint() const;
    void FocusOnSelection();

    bool m_G3DDragging = false;
    int m_G3DGrab = -1;      // 0..2 — ось, 3 — центр (free move / uniform scale)
    int m_G3DMode = 0;       // 0 move, 1 rotate, 2 scale
    uint32_t m_G3DEntity = 0;
    glm::vec3 m_G3DCenter = glm::vec3(0.0f);
    float m_G3DLen = 60.0f;
    glm::vec3 m_G3DPlaneNormal = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 m_G3DStartPoint = glm::vec3(0.0f);
    glm::vec3 m_G3DStartPos3 = glm::vec3(0.0f);
    glm::vec3 m_G3DStartRot3 = glm::vec3(0.0f);
    glm::vec3 m_G3DStartScale3 = glm::vec3(1.0f);
};
