#include "core/Application.h"
#include "core/Audio.h"
#include "core/Input.h"
#include "core/Scripting.h"
#include "core/Prefs.h"
#include "utils/ConsoleLog.h"
#include "utils/AssetIO.h"
#include "ecs/Transforms.h"
#include "ecs/Physics3D.h"
#include <algorithm>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <memory>

static float RandomU01() {
    static std::mt19937 rng{std::random_device{}()};
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
}


Application::Application(const AppOptions& options) {
    // Single-exe игра? Если к этому бинарнику приклеен бандл — распаковываем
    // и работаем из папки рядом с exe, всегда в режиме плеера.
    AppOptions opts = options;
    {
        std::string bundleDir = AstraBundleExtract();
        if (!bundleDir.empty()) {
            std::error_code ec;
            std::filesystem::current_path(bundleDir, ec);
            opts.player = true; // сцена подберётся из game.json в распакованной папке
        }
    }
    ConsoleLog::CaptureStdio(); // всё, что пишется в cout/cerr, видно в панели Console
    m_Options = opts;
    m_PlayerMode = opts.player;
    m_Window = std::make_unique<Window>(1920, 1080,
                                        m_PlayerMode ? "Astra Player" : "Astra");

    // Проект открываем до инициализации рендера: пути ассетов относительные
    m_ProjectManager = std::make_unique<ProjectManager>();
    bool projectOpened = false;
    if (m_PlayerMode && !options.projectDir.empty())
        projectOpened = m_ProjectManager->OpenProject(options.projectDir);
    if (!projectOpened && m_PlayerMode &&
        (std::filesystem::exists("game.json") || !options.scenePath.empty())) {
        projectOpened = true; // запуск из папки билда: cwd и есть корень игры
    }
    if (!projectOpened && m_ProjectManager->HasLastProject()) {
        if (!m_ProjectManager->OpenProject(m_ProjectManager->GetLastProject())) {
            std::cerr << "Last project unavailable: " << m_ProjectManager->LastError() << "\n";
        }
    }

    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Init();

    m_Camera = std::make_unique<Camera>(16.0f / 9.0f);

    m_SceneManager = std::make_unique<SceneManager>();
    m_Scene = std::make_unique<Scene>();
    m_Scene->Init(m_Renderer.get());

    m_Serializer = std::make_unique<SceneSerializer>();

    m_GUI = std::make_unique<GUI>(m_Window->GetNativeWindow());
    m_GUI->Init();

    // Input — после ImGui: бэкенд навешивает свои колбэки, но мы полим GLFW напрямую
    Input::Get().Init(m_Window->GetNativeWindow());

    m_Ctx = { m_Camera.get(), m_SceneManager.get(), m_Scene.get(), m_Serializer.get(),
              m_Renderer.get(), m_ProjectManager.get(), &m_EditorState };
    m_Ctx.app = this;

    // Стартовая сцена: в плеере — из --scene / game.json, в редакторе — default.scene
    if (m_PlayerMode) {
        std::string scene = m_Options.scenePath;
        if (scene.empty()) {
            std::string txt = AssetIO::ReadAll("game.json");
            {
                size_t k = txt.find("\"scene\"");
                size_t q1 = k == std::string::npos ? std::string::npos : txt.find('"', txt.find(':', k) + 1);
                size_t q2 = q1 == std::string::npos ? std::string::npos : txt.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos)
                    scene = txt.substr(q1 + 1, q2 - q1 - 1);
            }
        }
        if (scene.empty()) scene = "assets/scenes/default.scene";
        std::error_code ec;
        if (!std::filesystem::exists(scene, ec)) {
            std::cerr << "[Player] scene not found: " << scene << "\n";
            glfwSetWindowShouldClose(m_Window->GetNativeWindow(), GLFW_TRUE);
        } else {
            m_Serializer->Load(m_SceneManager.get(), scene);
            std::cout << "[Player] scene: " << scene << "\n";
        }
        m_EditorState = EditorState::Play; // первый Update выполнит переход Edit->Play
    } else if (std::filesystem::exists("assets/scenes/default.scene")) {
        m_Serializer->Load(m_SceneManager.get(), "assets/scenes/default.scene");
    } else {
        SeedDemoScene();
    }
}

void Application::SeedDemoScene() {
    // Тестовые объекты
    Entity e1;
    e1.name = "Red Box";
    e1.sprite.type = SpriteType::Quad;
    e1.sprite.color = glm::vec3(1.0f, 1.0f, 1.0f);
    e1.sprite.texturePath = "assets/textures/checker.png";
    e1.transform.scale = glm::vec2(200.0f, 100.0f);
    e1.collider.type = ColliderType::Box;
    e1.collider.size = glm::vec2(100.0f, 50.0f);
    m_SceneManager->AddEntity(e1);

    Entity e2;
    e2.name = "Green Circle";
    e2.sprite.type = SpriteType::Circle;
    e2.sprite.color = glm::vec3(0.3f, 0.8f, 0.3f);
    e2.transform.position = glm::vec2(200.0f, 100.0f);
    e2.transform.scale = glm::vec2(150.0f, 150.0f);
    e2.collider.type = ColliderType::Circle;
    e2.collider.radius = 75.0f;
    m_SceneManager->AddEntity(e2);

    // Демо физики: пол, падающий мяч и триггерная зона
    Entity ground;
    ground.name = "Ground";
    ground.sprite.type = SpriteType::Quad;
    ground.sprite.color = glm::vec3(0.4f, 0.4f, 0.45f);
    ground.transform.position = glm::vec2(0.0f, -300.0f);
    ground.transform.scale = glm::vec2(1600.0f, 100.0f);
    ground.collider.type = ColliderType::Box;
    ground.collider.size = glm::vec2(800.0f, 50.0f);
    ground.rigidbody.isKinematic = true;
    m_SceneManager->AddEntity(ground);

    Entity ball;
    ball.name = "Falling Ball";
    ball.sprite.type = SpriteType::Circle;
    ball.sprite.color = glm::vec3(1.0f, 1.0f, 1.0f);
    ball.sprite.texturePath = "assets/textures/checker.png";
    ball.transform.position = glm::vec2(-300.0f, 300.0f);
    ball.transform.scale = glm::vec2(80.0f, 80.0f);
    ball.collider.type = ColliderType::Circle;
    ball.collider.radius = 40.0f;
    ball.rigidbody.useGravity = true;
    m_SceneManager->AddEntity(ball);

    Entity sensor;
    sensor.name = "Sensor Zone";
    sensor.sprite.type = SpriteType::None;
    sensor.transform.position = glm::vec2(-300.0f, -100.0f);
    sensor.collider.type = ColliderType::Box;
    sensor.collider.size = glm::vec2(150.0f, 200.0f);
    sensor.collider.isTrigger = true;
    m_SceneManager->AddEntity(sensor);

    Entity cam;
    cam.name = "Main Camera";
    cam.sprite.type = SpriteType::None;
    cam.collider.type = ColliderType::None;
    cam.rigidbody.isKinematic = true;
    cam.hasCamera = true;
    cam.camera.mainCamera = true;
    m_SceneManager->AddEntity(cam);
}

Application::~Application() {
    Audio::Shutdown();
}

// ===== Undo/redo =====
void Application::PushUndoSnapshot(const SceneManager::SceneSnapshot& snap) {
    m_UndoStack.push_back(snap);
    if (m_UndoStack.size() > 60) m_UndoStack.pop_front();
    m_RedoStack.clear();
}

bool Application::Undo() {
    if (m_UndoStack.empty() || m_EditorState != EditorState::Edit) return false;
    m_RedoStack.push_back(m_SceneManager->TakeSnapshot());
    m_SceneManager->Restore(m_UndoStack.back());
    m_UndoStack.pop_back();
    std::cout << "[Undo] шаг назад\n";
    return true;
}

bool Application::Redo() {
    if (m_RedoStack.empty() || m_EditorState != EditorState::Edit) return false;
    m_UndoStack.push_back(m_SceneManager->TakeSnapshot());
    m_SceneManager->Restore(m_RedoStack.back());
    m_RedoStack.pop_back();
    std::cout << "[Redo] шаг вперёд\n";
    return true;
}

void Application::Run() {
    float lastTime = 0.0f;

    while (!m_Window->ShouldClose()) {
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        lastTime = currentTime;

        HandleFileDrops();
        ProcessInput(deltaTime);
        Update(deltaTime);
        Render(deltaTime);

        m_Window->SwapBuffers();
        m_Window->PollEvents();
    }
}

void Application::ProcessInput(float deltaTime) {
    ImGuiIO& io = ImGui::GetIO();
    const bool editing = (m_EditorState == EditorState::Edit);

    Input::Get().NewFrame();

    // Плеер: только выход по ESC, остальное — через ImGui-оверлей
    if (m_PlayerMode) {
        if (Input::Get().WasKeyPressed(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(m_Window->GetNativeWindow(), GLFW_TRUE);
        m_Window->UpdateLastMousePos();
        m_Window->ResetScrollOffset();
        return;
    }

    // ESC — выход из Play/Pause в Edit (как остановка в Unity)
    if (Input::Get().WasKeyPressed(GLFW_KEY_ESCAPE) && !io.WantCaptureKeyboard &&
        m_EditorState != EditorState::Edit) {
        m_EditorState = EditorState::Edit;
    }

    // ===== 3D-сцена: ПКМ — осмотреться и лететь (WASDQE), колесо — толчок, ЛКМ — гизмо/выбор =====
    if (m_GUI->Is3DEditor()) {
        m_Camera->SetPerspective(true);
        if (!m_FlyInit) {
            // стартуем «над» прежним 2D-видом, чтобы сцена не пропала из виду
            m_FlyPos = glm::vec3(m_Camera->GetPosition(), 0.0f) +
                       glm::vec3(0.0f, m_FlyRefDist * 0.35f, m_FlyRefDist);
            m_FlyYaw = 0.0f; m_FlyPitch = 18.0f;
            m_FlyInit = true;
        }
        const glm::vec2 vp = m_GUI->GetSceneSize();
        const bool overScene = m_GUI->IsSceneHovered() && !m_GUI->IsAnyPopupOpen() &&
                               vp.x > 1.0f && vp.y > 1.0f;
        if (overScene) {
            const bool rmb = m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT);
            if (rmb) {
                glm::vec2 d = m_Window->GetMousePos() - m_Window->GetLastMousePos();
                m_FlyYaw -= d.x * 0.22f;
                m_FlyPitch = std::clamp(m_FlyPitch + d.y * 0.22f, -89.5f, 89.5f);
            }
            // полёт — только с зажатой ПКМ (иначе W/E/R/Q — переключение инструментов)
            if (rmb) {
                float mult = m_Window->IsKeyDown(GLFW_KEY_LEFT_SHIFT) ? 4.0f
                           : (m_Window->IsKeyDown(GLFW_KEY_LEFT_CONTROL) ? 0.25f : 1.0f);
                float sp = m_FlySpeed * mult * deltaTime;
                glm::vec3 f = m_Camera->Forward(), r = m_Camera->Right();
                glm::vec3 fwd(f.x, 0.0f, f.z);
                fwd = glm::length(fwd) > 1e-4f ? glm::normalize(fwd) : glm::vec3(0, 0, -1);
                glm::vec3 rgt(r.x, 0.0f, r.z);
                rgt = glm::length(rgt) > 1e-4f ? glm::normalize(rgt) : glm::vec3(1, 0, 0);
                if (m_Window->IsKeyDown(GLFW_KEY_W)) m_FlyPos += fwd * sp;
                if (m_Window->IsKeyDown(GLFW_KEY_S)) m_FlyPos -= fwd * sp;
                if (m_Window->IsKeyDown(GLFW_KEY_D)) m_FlyPos += rgt * sp;
                if (m_Window->IsKeyDown(GLFW_KEY_A)) m_FlyPos -= rgt * sp;
                if (m_Window->IsKeyDown(GLFW_KEY_E)) m_FlyPos.y += sp;
                if (m_Window->IsKeyDown(GLFW_KEY_Q)) m_FlyPos.y -= sp;
            }
            // пан: средней кнопкой или ЛКМ в инструменте Hand
            const bool handTool = m_Scene->GetGizmoMode() == 3;
            const bool panDrag = m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE) ||
                                 (handTool && m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT));
            if (panDrag) {
                glm::vec2 d = m_Window->GetMousePos() - m_Window->GetLastMousePos();
                float upp = m_Camera->WorldPerPixelAt(SceneViewFocusPoint(), vp);
                m_FlyPos += (m_Camera->Right() * (-d.x) + m_Camera->Up() * d.y) * upp;
            }
            float scroll = m_Window->GetScrollOffset();
            if (scroll != 0.0f) {
                m_FlyPos -= m_Camera->Forward() * (scroll * m_FlyRefDist * 0.12f);
                m_FlyRefDist = std::clamp(m_FlyRefDist * (1.0f - scroll * 0.12f), 50.0f, 60000.0f);
            }
            Input::Get().FeedScroll(scroll);

            if (!io.WantCaptureKeyboard) {
                // F — навестись на выбранное; NUMPAD 1/2/3/4/5/7 — виды, 6 — перспектива
                if (m_Window->IsKeyDown(GLFW_KEY_F)) FocusOnSelection();
                struct ViewKey { int key; float yaw, pitch; };
                static const ViewKey kViews[] = {
                    { GLFW_KEY_KP_1, 0.0f, 0.0f },
                    { GLFW_KEY_KP_2, 180.0f, 0.0f },
                    { GLFW_KEY_KP_3, 90.0f, 0.0f },
                    { GLFW_KEY_KP_4, -90.0f, 0.0f },
                    { GLFW_KEY_KP_5, 0.0f, 89.5f },
                    { GLFW_KEY_KP_7, 0.0f, -89.5f },
                    { GLFW_KEY_KP_6, 40.0f, 22.0f },
                };
                for (const auto& v : kViews)
                    if (Input::Get().WasKeyPressed(v.key)) { SceneViewLook(v.yaw, v.pitch); break; }
            }
        }
        // матрицы — уже с новыми углами, иначе picking и гизмо живут прошлокадровой камерой
        m_Camera->SetFly(m_FlyPos, m_FlyYaw, m_FlyPitch);
        // hot — ровно изображение сцены: клик по вкладкам Scene/Game или компасу не трогает
        // выделение, иначе инспектор пустеет «на ровном месте»
        HandleSceneMouse3D(vp, m_Scene->GetGizmoMode(), editing,
                           overScene && m_GUI->IsSceneImageHot());
        m_Window->UpdateLastMousePos();
        m_Window->ResetScrollOffset();
        return;
    }
    m_G3DDragging = false;
    m_G3DGrab = -1;
    m_Scene->SetGizmo3D(false, glm::vec3(0.0f), 60.0f, 0, -1);
    m_FlyInit = false;
    m_Camera->SetPerspective(false);
    m_Camera->ClearOrbit();

    // Если GUI открыт попап — не обрабатываем ввод сцены
    if (m_GUI->IsAnyPopupOpen()) {
        m_Window->UpdateLastMousePos();
        m_Window->ResetScrollOffset();
        return;
    }

    // ВАЖНО: здесь нельзя гейтиться на io.WantCaptureMouse — с докспейсом курсор
    // всегда над каким-нибудь ImGui-окном, и навигация бы не работала никогда.
    // Правильный гейт — «мышь прямо над Scene-вьюпортом» (IsSceneHovered).

    // ПКМ/средней или ЛКМ в режиме Hand — пан камеры; цепляем, если старт был над Scene,
    // и продолжаем тащить, даже если курсор убежал за края вьюпорта
    const bool handTool = m_Scene->GetGizmoMode() == 3;
    const bool panHeld = m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT) ||
                         m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE) ||
                         (handTool && m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT));
    if (panHeld && m_GUI->IsSceneHovered()) m_ScenePanning = true;
    if (!panHeld) m_ScenePanning = false;
    if (m_ScenePanning) {
        glm::vec2 current = m_Window->GetMousePos();
        glm::vec2 delta = current - m_Window->GetLastMousePos();
        m_Camera->Pan(glm::vec2(-delta.x, delta.y) * m_Camera->GetZoom());
    }

    // Стрелки — пан по Scene (WASD freed: W/E/R — инструменты, Q — hand)
    if (m_GUI->IsSceneHovered() && !io.WantCaptureKeyboard && !m_ScenePanning) {
        float moveSpeed = 500.0f * deltaTime * m_Camera->GetZoom();
        if (m_Window->IsKeyDown(GLFW_KEY_UP)) m_Camera->Pan(glm::vec2(0.0f, moveSpeed));
        if (m_Window->IsKeyDown(GLFW_KEY_DOWN)) m_Camera->Pan(glm::vec2(0.0f, -moveSpeed));
        if (m_Window->IsKeyDown(GLFW_KEY_LEFT)) m_Camera->Pan(glm::vec2(-moveSpeed, 0.0f));
        if (m_Window->IsKeyDown(GLFW_KEY_RIGHT)) m_Camera->Pan(glm::vec2(moveSpeed, 0.0f));
    }

    // Зум колёсиком — только над Scene-вьюпортом, к позиции курсора
    float scroll = m_Window->GetScrollOffset();
    m_Window->ResetScrollOffset();
    Input::Get().FeedScroll(scroll);
    if (scroll != 0.0f && m_GUI->IsSceneHovered() && !m_ScenePanning) {
        float oldZoom = m_Camera->GetZoom();
        glm::vec2 anchorWorld = m_Camera->ScreenToWorld(
            m_GUI->GetSceneMousePos(), m_GUI->GetSceneSize().x, m_GUI->GetSceneSize().y);
        m_Camera->Zoom(1.0f - scroll * 0.1f);
        float newZoom = m_Camera->GetZoom();
        if (newZoom != oldZoom) {
            float f = newZoom / oldZoom;
            // держим мировую точку под курсором на месте
            m_Camera->SetPosition(anchorWorld - (anchorWorld - m_Camera->GetPosition()) * f);
        }
    }

    // F — фокус камеры на выбранном объекте
    if (m_GUI->IsSceneHovered() && !io.WantCaptureKeyboard && !m_ScenePanning &&
        m_Window->IsKeyDown(GLFW_KEY_F)) {
        Entity* sel = m_SceneManager->GetSelectedEntityPtr();
        if (sel) m_Camera->SetPosition(Transforms::WorldPosition(m_SceneManager->GetEntities(), *sel));
    }

    // ===== Выделение в Scene и Gizmo (только в Edit-режиме, Hand-режим тащит камеру) =====
    if (editing && m_GUI->IsSceneImageHot() && !m_ScenePanning && m_Scene->GetGizmoMode() < 3) {
        auto& ents = m_SceneManager->GetEntities();
        Entity* selected = m_SceneManager->GetSelectedEntityPtr();

        glm::vec2 worldMouse = m_Camera->ScreenToWorld(
            m_GUI->GetSceneMousePos(), m_GUI->GetSceneSize().x, m_GUI->GetSceneSize().y);

        const bool leftDown = m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
        const bool leftPressed = Input::Get().WasMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);

        // мировые единицы на экранный пиксель — гизмо постоянного размера
        float upp = 1080.0f * m_Camera->GetZoom() /
                    (m_GUI->GetSceneSize().y > 0.0f ? m_GUI->GetSceneSize().y : 1.0f);
        float gizmoLen = 70.0f * upp;
        float grab = 8.0f * upp;

        int mode = m_Scene->GetGizmoMode();
        glm::vec2 gpos = selected ? Transforms::WorldPosition(ents, *selected) : glm::vec2(0.0f);

        // axis: -1 = свободный драг, 0 = X, 1 = Y, 2 = дуга вращения / равномерный scale
        auto hitPart = [&]() -> int {
            if (!selected) return -999;
            if (mode == 0) {
                bool hitX = std::fabs(worldMouse.y - gpos.y) < grab &&
                            worldMouse.x >= gpos.x - grab && worldMouse.x <= gpos.x + gizmoLen + grab;
                bool hitY = std::fabs(worldMouse.x - gpos.x) < grab &&
                            worldMouse.y >= gpos.y - grab && worldMouse.y <= gpos.y + gizmoLen + grab;
                if (hitX && !hitY) return 0;
                if (hitY && !hitX) return 1;
                if (hitX && hitY) return -1; // за начало осей — свободный перенос
                glm::vec2 half = selected->transform.scale * 0.5f;
                if (std::fabs(worldMouse.x - gpos.x) <= half.x &&
                    std::fabs(worldMouse.y - gpos.y) <= half.y) return -1;
                return -999;
            } else if (mode == 1) {
                float dist = glm::length(worldMouse - gpos);
                return std::fabs(dist - gizmoLen) < grab ? 2 : -999;
            } else { // scale
                auto nearPt = [&](const glm::vec2& h) {
                    return std::fabs(worldMouse.x - h.x) < grab && std::fabs(worldMouse.y - h.y) < grab; };
                if (nearPt(gpos + glm::vec2(gizmoLen, 0.0f))) return 0;
                if (nearPt(gpos + glm::vec2(0.0f, gizmoLen))) return 1;
                if (nearPt(gpos)) return 2;
                return -999;
            }
        };

        if (leftPressed && !m_Scene->IsGizmoActive()) {
            int part = hitPart();
            if (part != -999) {
                m_Scene->SetGizmoActive(true);
                m_Scene->SetGizmoAxis(part);
                m_Scene->SetGizmoStartPos(selected->transform.position);
                m_Scene->SetDragStartMouse(worldMouse);
                m_Scene->SetDragStartAngle(std::atan2(worldMouse.y - gpos.y, worldMouse.x - gpos.x));
                m_Scene->SetDragStartRotation(selected->transform.rotation);
                m_Scene->SetDragStartScale(selected->transform.scale);
            } else if (mode == 0) {
                // клик по пустому месту: выделить верхний объект, иначе — снять выделение
                int hitIdx = -1;
                for (size_t i = ents.size(); i > 0; i--) {
                    const Entity& e = ents[i - 1];
                    if (!e.active || e.sprite.type == SpriteType::None) continue;
                    glm::vec2 p = Transforms::WorldPosition(ents, e);
                    glm::vec2 half = e.transform.scale * 0.5f;
                    if (std::fabs(worldMouse.x - p.x) <= half.x && std::fabs(worldMouse.y - p.y) <= half.y) {
                        hitIdx = static_cast<int>(i - 1);
                        break;
                    }
                }
                m_SceneManager->SetSelectedEntity(hitIdx);
            }
        } else if (m_Scene->IsGizmoActive()) {
            if (!leftDown) {
                m_Scene->SetGizmoActive(false);
                m_Scene->SetGizmoAxis(-1);
            } else if (selected) {
                const bool snap = Input::Get().IsKeyDown(GLFW_KEY_LEFT_CONTROL);
                glm::vec2 worldDelta = worldMouse - m_Scene->GetDragStartMouse();
                int axis = m_Scene->GetGizmoAxis();

                if (mode == 0) {
                    if (axis == 0) worldDelta = glm::vec2(worldDelta.x, 0.0f);
                    else if (axis == 1) worldDelta = glm::vec2(0.0f, worldDelta.y);
                    if (snap) worldDelta = glm::round(worldDelta / AstraPrefs::GridSize) * AstraPrefs::GridSize;

                    // тянем в мировых координатах, результат пересчитываем в локальные
                    glm::mat4 chain = Transforms::ParentWorldMatrix(ents, *selected);
                    glm::vec2 startWorld = Transforms::LocalToWorld(ents, chain, m_Scene->GetGizmoStartPos());
                    selected->transform.position = Transforms::WorldToLocalPoint(
                        ents, *selected, startWorld + worldDelta);
                } else if (mode == 1) {
                    float angle = std::atan2(worldMouse.y - gpos.y, worldMouse.x - gpos.x);
                    float deg = glm::degrees(angle - m_Scene->GetDragStartAngle());
                    if (snap) deg = std::round(deg / AstraPrefs::SnapDegrees) * AstraPrefs::SnapDegrees;
                    selected->transform.rotation = m_Scene->GetDragStartRotation() + deg;
                } else { // scale
                    const glm::vec2& startScale = m_Scene->GetDragStartScale();
                    glm::vec2 s = startScale;
                    if (axis == 0) s.x = startScale.x + worldDelta.x;
                    else if (axis == 1) s.y = startScale.y + worldDelta.y;
                    else s += glm::vec2(worldDelta.x + worldDelta.y) * 0.5f;
                    selected->transform.scale = glm::max(s, glm::vec2(1.0f));
                }
            }
        }
    } else {
        m_Scene->SetGizmoActive(false);
        m_Scene->SetGizmoAxis(-1);
    }

    // Инструмент Tile (T): ЛКМ — положить тайл, Shift+ЛКМ — стереть
    if (editing && m_Scene->GetGizmoMode() == 4 && m_GUI->IsSceneImageHot() && !m_GUI->IsAnyPopupOpen() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        PaintTileAtMouse();
    }

    m_Window->UpdateLastMousePos();
}

// ===== 3D: выделение кликом и гизмо =====

namespace {

const glm::vec3& AxisVec(int a) {
    static const glm::vec3 kAxes[3] = { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };
    return kAxes[a < 0 ? 0 : (a > 2 ? 2 : a)];
}

float PointSegDistance(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    glm::vec2 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 < 1e-9f) return glm::length(p - a);
    float t = std::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    return glm::length(p - (a + ab * t));
}

} // namespace

bool Application::PlaneHit(const glm::vec3& planePoint, const glm::vec3& planeNormal,
                           const glm::vec2& mouse, const glm::vec2& vp, glm::vec3& out) const {
    glm::vec3 ro, rd;
    m_Camera->ScreenToRay(mouse, vp, ro, rd);
    float denom = glm::dot(rd, planeNormal);
    if (std::fabs(denom) < 1e-5f) return false;      // луч параллелен плоскости
    float t = glm::dot(planePoint - ro, planeNormal) / denom;
    out = ro + rd * t;
    return true;
}

int Application::HitGizmo3D(const glm::vec3& center, float len, int mode,
                            const glm::vec2& mouse, const glm::vec2& vp) const {
    const float grabPx = 12.0f;
    glm::vec2 c2;
    if (!m_Camera->WorldToScreen(center, vp, c2)) return -1;

    auto polylineHit = [&](const std::vector<glm::vec3>& pts) -> float {
        float best = 1e18f;
        bool havePrev = false;
        glm::vec2 prev(0.0f);
        for (const auto& p : pts) {
            glm::vec2 s;
            if (!m_Camera->WorldToScreen(p, vp, s)) { havePrev = false; continue; }
            best = havePrev ? std::min(best, PointSegDistance(mouse, prev, s))
                            : std::min(best, glm::length(s - mouse));
            prev = s;
            havePrev = true;
        }
        return best;
    };

    // центральный маркер — раньше осей: все оси сходятся в центре, иначе он был бы недостижим
    if (mode != 1 && glm::length(mouse - c2) < grabPx * 0.9f) return 3;

    // курсор может попасть и в «смазанную» с ребра ось, и в полноценную — берём то, что
    // лучше видно (ось — перпендикулярнее взгляду, кольцо — перпендикулярнее своей оси);
    // совсем тонкие с ребра ручки игнорируем, как в Unity
    glm::vec3 view = m_Camera->ViewDirection();
    int bestGrab = -1;
    float bestScore = -1e18f;
    for (int a = 0; a < 3; a++) {
        float d;
        if (mode == 1) {
            glm::vec3 u = AxisVec((a + 1) % 3), v = AxisVec((a + 2) % 3);
            std::vector<glm::vec3> ring;
            for (int i = 0; i <= 32; i++) {
                float t = (float)i / 32.0f * 2.0f * 3.14159265f;
                ring.push_back(center + (u * cosf(t) + v * sinf(t)) * len);
            }
            d = polylineHit(ring);
        } else {
            d = polylineHit({ center, center + AxisVec(a) * len });
        }
        if (d >= grabPx) continue;
        float align = std::fabs(glm::dot(AxisVec(a), view));   // 1 = ось смотрит в камеру
        float visible = (mode == 1) ? align : (1.0f - align);
        if (visible < 0.25f) continue;
        float score = visible - d / grabPx * 0.3f;
        if (score > bestScore) { bestScore = score; bestGrab = a; }
    }
    return bestGrab;
}

void Application::BeginGizmoDrag3D(Entity* e, int mode, int grab, const glm::vec2& mouse,
                                   const glm::vec2& vp) {
    m_G3DDragging = true;
    m_G3DGrab = grab;
    m_G3DMode = mode;
    m_G3DEntity = e->id;
    m_G3DCenter = e->pos3;
    m_G3DStartPos3 = e->pos3;
    m_G3DStartRot3 = e->rot3;
    m_G3DStartScale3 = e->scale3;

    glm::vec3 view = m_Camera->ViewDirection();
    if (mode == 1) {
        m_G3DPlaneNormal = AxisVec(grab);                     // кольцо в плоскости, перпендикулярной оси
    } else if (mode == 0 && grab <= 2) {
        // плоскость содержит ось и максимально «смотрит» на камеру:
        // нормаль = составляющая направления взгляда, перпендикулярная оси
        const glm::vec3& axis = AxisVec(grab);
        glm::vec3 n = view - axis * glm::dot(view, axis);
        m_G3DPlaneNormal = glm::length(n) > 1e-4f ? glm::normalize(n) : view;
    } else {
        m_G3DPlaneNormal = view;                              // экранная плоскость
    }
    if (!PlaneHit(m_G3DCenter, m_G3DPlaneNormal, mouse, vp, m_G3DStartPoint))
        m_G3DStartPoint = m_G3DCenter;
}

void Application::UpdateGizmoDrag3D(const glm::vec2& mouse, const glm::vec2& vp) {
    Entity* e = nullptr;
    for (auto& it : m_SceneManager->GetEntities())
        if (it.id == m_G3DEntity) { e = &it; break; }
    if (!e) { m_G3DDragging = false; m_G3DGrab = -1; return; }

    glm::vec3 hit;
    if (!PlaneHit(m_G3DCenter, m_G3DPlaneNormal, mouse, vp, hit)) return;
    const bool snap = Input::Get().IsKeyDown(GLFW_KEY_LEFT_CONTROL);

    if (m_G3DMode == 0) {
        glm::vec3 delta = hit - m_G3DStartPoint;
        if (m_G3DGrab <= 2) {
            const glm::vec3& axis = AxisVec(m_G3DGrab);
            float along = glm::dot(delta, axis);
            if (snap) along = std::round(along / AstraPrefs::GridSize) * AstraPrefs::GridSize;
            e->pos3 = m_G3DStartPos3 + axis * along;
        } else {
            e->pos3 = m_G3DStartPos3 + delta;
            if (snap) e->pos3 = glm::round(e->pos3 / AstraPrefs::GridSize) * AstraPrefs::GridSize;
        }
    } else if (m_G3DMode == 1) {
        const glm::vec3& axis = AxisVec(m_G3DGrab);
        glm::vec3 p0 = m_G3DStartPoint - m_G3DCenter, p1 = hit - m_G3DCenter;
        if (glm::length(p0) < 1e-4f || glm::length(p1) < 1e-4f) return;
        float deg = glm::degrees(std::atan2(glm::dot(glm::cross(p0, p1), axis), glm::dot(p0, p1)));
        if (snap) deg = std::round(deg / AstraPrefs::SnapDegrees) * AstraPrefs::SnapDegrees;
        e->rot3[m_G3DGrab] = m_G3DStartRot3[m_G3DGrab] + deg;
    } else {
        glm::vec3 rel = hit - m_G3DCenter, rel0 = m_G3DStartPoint - m_G3DCenter;
        if (m_G3DGrab <= 2) {
            float a = glm::dot(rel, AxisVec(m_G3DGrab));
            float a0 = glm::dot(rel0, AxisVec(m_G3DGrab));
            if (std::fabs(a0) < 1e-4f) return;
            e->scale3[m_G3DGrab] = std::max(0.1f, m_G3DStartScale3[m_G3DGrab] * std::clamp(a / a0, 0.01f, 100.0f));
        } else {
            float d0 = glm::length(rel0);
            if (d0 < 1e-4f) return;
            float k = std::clamp(glm::length(rel) / d0, 0.01f, 100.0f);
            e->scale3 = glm::max(m_G3DStartScale3 * k, glm::vec3(0.1f));
        }
    }
}

void Application::PickEntity3D(const glm::vec2& mouse, const glm::vec2& vp) {
    glm::vec3 ro, rd;
    m_Camera->ScreenToRay(mouse, vp, ro, rd);
    auto& ents = m_SceneManager->GetEntities();
    int best = -1;
    float bestT = 1e18f;
    for (size_t i = 0; i < ents.size(); i++) {
        const Entity& e = ents[i];
        if (!e.active || !e.is3D) continue;
        glm::vec3 centerLocal, half;
        if (!m_Renderer->GetMeshBounds(e.mesh.type, e.mesh.meshPath, centerLocal, half)) continue;
        glm::mat4 inv = glm::inverse(Transforms::Model3D(e));
        glm::vec4 o4 = inv * glm::vec4(ro, 1.0f);
        glm::vec4 d4 = inv * glm::vec4(rd, 0.0f);
        float speed = glm::length(glm::vec3(d4));            // масштаб луча в локальных координатах
        if (speed < 1e-8f) continue;
        glm::vec3 o(o4 / o4.w);
        glm::vec3 d(glm::vec3(d4) / speed);
        float t = 0.0f;
        if (!Transforms::RayHitsBox(o - centerLocal, d, half, t)) continue;
        float worldT = t / speed;                             // расстояние в мировых единицах
        if (worldT > 0.0f && worldT < bestT) { bestT = worldT; best = (int)i; }
    }
    m_SceneManager->SetSelectedEntity(best);
}

glm::vec3 Application::SceneViewFocusPoint() const {
    Entity* sel = m_SceneManager->GetSelectedEntityPtr();
    if (sel) return sel->is3D ? sel->pos3 : glm::vec3(sel->transform.position, 0.0f);
    return m_FlyPos + m_Camera->Forward() * m_FlyRefDist;
}

void Application::SceneViewLook(float yaw, float pitch) {
    glm::vec3 focus = SceneViewFocusPoint();
    m_FlyYaw = yaw;
    m_FlyPitch = pitch;
    m_FlyPos = focus - Camera::ForwardOf(yaw, pitch) * m_FlyRefDist;
}

void Application::FocusOnSelection() {
    Entity* sel = m_SceneManager->GetSelectedEntityPtr();
    if (!sel) return;
    glm::vec3 target = sel->is3D ? sel->pos3 : glm::vec3(sel->transform.position, 0.0f);
    float size = sel->is3D ? std::max({sel->scale3.x, sel->scale3.y, sel->scale3.z})
                           : std::max(sel->transform.scale.x, sel->transform.scale.y);
    m_FlyRefDist = std::clamp(size * 3.0f, 150.0f, 40000.0f);
    m_FlyPos = target - m_Camera->Forward() * m_FlyRefDist;
}

void Application::HandleSceneMouse3D(const glm::vec2& vp, int rawMode, bool editing, bool hot) {
    Entity* sel = m_SceneManager->GetSelectedEntityPtr();
    const bool sel3D = sel && sel->is3D;
    const int mode = (rawMode >= 0 && rawMode <= 2) ? rawMode : -1;   // Hand/Tile — без гизмо
    const glm::vec3 center = sel3D ? sel->pos3 : glm::vec3(0.0f);
    const float len = 70.0f * m_Camera->WorldPerPixelAt(center, vp);

    m_Scene->SetGizmo3D(sel3D && mode >= 0, center, len, mode < 0 ? 0 : mode,
                        m_G3DDragging ? m_G3DGrab : -1);

    // отпускание ловим ВСЕГДА, пока hot==false драг просто замирает: иначе он залипал
    // и объект уезжал за курсором по всему интерфейсу, включая инспектор
    const bool down = m_Window->IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    if (m_G3DDragging && !down) { m_G3DDragging = false; m_G3DGrab = -1; }

    if (!editing || !hot || mode < 0 || vp.x <= 1.0f || vp.y <= 1.0f) return;

    const glm::vec2 mouse = m_GUI->GetSceneMousePos();
    if (Input::Get().WasMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (!m_G3DDragging && sel3D) {
            int grab = HitGizmo3D(center, len, mode, mouse, vp);
            if (grab >= 0) { BeginGizmoDrag3D(sel, mode, grab, mouse, vp); return; }
        }
        PickEntity3D(mouse, vp);
        return;
    }
    if (m_G3DDragging) UpdateGizmoDrag3D(mouse, vp);
}

void Application::HandleFileDrops() {
    if (m_PlayerMode) return;
    auto files = m_Window->ConsumeDroppedFiles();
    if (files.empty()) return;

    namespace fs = std::filesystem;
    for (const auto& f : files) {
        fs::path src(f);
        std::string ext = src.extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(c));

        fs::path destDir;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
            destDir = "assets/textures";
        else if (ext == ".scene")
            destDir = "assets/scenes";
        else if (ext == ".cpp")
            destDir = "assets/scripts";
        else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
            destDir = "assets/audio";
        else
            continue; // остальное в проект не тащим

        std::error_code ec;
        fs::create_directories(destDir, ec);
        fs::path dest = destDir / src.filename();
        int counter = 1;
        while (fs::exists(dest)) {
            dest = destDir / (src.stem().string() + "_" + std::to_string(counter++) +
                              src.extension().string());
        }
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Failed to import " << src << ": " << ec.message() << "\n";
        } else {
            std::cout << "[Import] " << dest.string() << "\n";
        }
    }
}


void Application::PaintTileAtMouse() {
    int sel = m_SceneManager->GetSelectedEntity();
    if (sel < 0) return;
    auto& ents = m_SceneManager->GetEntities();
    Entity& e = ents[static_cast<size_t>(sel)];
    if (!e.hasTilemap) return;
    Tilemap& tm = e.tilemap;
    if (tm.cells.size() < (size_t)tm.width * tm.height) tm.cells.assign((size_t)tm.width * tm.height, -1);
    glm::vec2 world = m_Camera->ScreenToWorld(m_GUI->GetSceneMousePos(), m_GUI->GetSceneSize().x, m_GUI->GetSceneSize().y);
    int c = (int)std::floor((world.x - e.transform.position.x) / std::max(tm.tileW, 1));
    int r = (int)std::floor((e.transform.position.y - world.y) / std::max(tm.tileH, 1));
    if (c < 0 || r < 0 || c >= tm.width || r >= tm.height) return;
    bool erase = ImGui::GetIO().KeyShift;
    tm.cells[(size_t)r * tm.width + c] = erase ? -1 : m_GUI->GetCurrentTile();
}

// Текущий абсолютный кадр анимации (та же формула, что в Renderer::AnimationRect)
static int AnimFrameAt(const Entity& e, float t) {
    const SpriteAnimation& a = e.animation;
    if (a.cols < 1 || a.rows < 1) return -1;
    int total = a.cols * a.rows;
    int first = 0, last = total - 1;
    float fps = a.fps;
    bool loop = a.loop;
    if (!a.clips.empty()) {
        const AnimClip& c = a.clips[std::clamp(a.activeClip, 0, (int)a.clips.size() - 1)];
        first = std::clamp(c.first, 0, total - 1);
        last = std::clamp(c.last, first, total - 1);
        fps = c.fps;
        loop = c.loop;
    }
    int count = last - first + 1;
    long frame = (long)std::floor(std::max(t, 0.0f) * std::max(fps, 0.01f));
    if (loop) frame = first + frame % count;
    else frame = std::min(first + frame, (long)last);
    return (int)frame;
}

void Application::Update(float deltaTime) {
    Audio::NewFrame();

    // Запрос выхода из скрипта: QuitGame()
    if (Scripting::QuitRequested()) {
        Scripting::ClearQuit();
        if (m_PlayerMode) glfwSetWindowShouldClose(m_Window->GetNativeWindow(), GLFW_TRUE);
        else m_EditorState = EditorState::Edit;
    }
    // Захват мыши: CaptureMouse(true/false)
    if (int cap = Scripting::CaptureMouseState()) {
        glfwSetInputMode(m_Window->GetNativeWindow(), GLFW_CURSOR,
                         cap == 1 ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    // Спрайт-анимации: кадры крутятся и в Edit (превью), и в Play; Pause — стоп
    if (m_EditorState != EditorState::Pause) {
        for (auto& e : m_SceneManager->GetEntities()) {
            if (!(e.animation.active && e.animation.cols >= 1 && e.animation.rows >= 1)) continue;
            int prevFrame = AnimFrameAt(e, e.animTime);
            e.animTime += deltaTime;
            int curFrame = AnimFrameAt(e, e.animTime);
            if (prevFrame < 0 || curFrame < 0 || !e.animation.events.size()) continue;
            for (const AnimEvent& ev : e.animation.events) {
                if (ev.clip != -1 && ev.clip != e.animation.activeClip) continue;
                bool fired = false;
                if (curFrame >= prevFrame) fired = (ev.frame > prevFrame && ev.frame <= curFrame);
                else fired = (ev.frame > prevFrame || ev.frame <= curFrame); // цикл: пересечение нуля
                if (fired) Scripting::DispatchAnimEvent(e.id, ev.name.c_str());
            }
        }
    }

    // Частицы: спавн по rate, интеграция, смерть; Pause — стоп (в Edit — превью)
    if (m_EditorState != EditorState::Pause) {
        auto& ents = m_SceneManager->GetEntities();
        for (auto& e : ents) {
            ParticleEmitter& em = e.emitter;
            if (!em.active || !e.active) continue;
            glm::vec2 origin = Transforms::WorldPosition(ents, e);
            if (em.loop) {
                em.emitAcc += em.rate * deltaTime;
                int spawn = (int)em.emitAcc;
                em.emitAcc -= spawn;
                for (int i = 0; i < spawn && (int)e.particles.size() < em.maxCount; i++) {
                    float ang = glm::radians(em.angleMin + RandomU01() * (em.angleMax - em.angleMin));
                    float spd = em.speedMin + RandomU01() * (em.speedMax - em.speedMin);
                    Particle pt;
                    pt.position = origin;
                    pt.velocity = glm::vec2(std::cos(ang), std::sin(ang)) * spd;
                    pt.life = em.lifeMin + RandomU01() * (em.lifeMax - em.lifeMin);
                    pt.size = em.sizeMin + RandomU01() * (em.sizeMax - em.sizeMin);
                    e.particles.push_back(pt);
                }
            }
            for (auto& pt : e.particles) {
                pt.age += deltaTime;
                pt.velocity.y -= em.gravity * deltaTime;
                pt.position += pt.velocity * deltaTime;
            }
            e.particles.erase(std::remove_if(e.particles.begin(), e.particles.end(),
                              [](const Particle& q) { return q.age >= q.life; }),
                              e.particles.end());
        }
    }

    // Обработка переходов Edit/Play/Pause
    if (m_EditorState != m_LastEditorState) {
        if (m_EditorState == EditorState::Play && m_LastEditorState == EditorState::Edit) {
            m_PlaySnapshot = m_SceneManager->TakeSnapshot();
            Scripting::SetScene(m_SceneManager.get());
            Scripting::LoadForScene(m_SceneManager->GetEntities());
            Scripting::SyncInstances(m_SceneManager->GetEntities());
            m_AudioStarted.clear();
            for (auto& e : m_SceneManager->GetEntities()) {
                // анимации: playOnAwake как у звука; старт с нуля
                e.animation.active = e.animation.playOnAwake;
                e.animTime = 0.0f;
                e.emitter.active = e.emitter.playOnAwake;
                e.emitter.emitAcc = 0.0f;
                e.particles.clear();
            }
            for (const auto& e : m_SceneManager->GetEntities()) {
                if (e.active && !e.audio.path.empty() && e.audio.playOnAwake) {
                    uint32_t vid = e.audio.loop
                        ? Audio::PlayLooped(e.audio.path, e.audio.volume, e.audio.pitch, e.audio.group)
                        : Audio::PlayOneShot(e.audio.path, e.audio.volume, e.audio.pitch, e.audio.group);
                    if (vid) m_AudioStarted.insert(e.id);
                }
            }
        }
        if (m_EditorState == EditorState::Edit && m_LastEditorState != EditorState::Edit) {
            Scripting::Unload();
            Audio::StopAll();
            m_AudioStarted.clear();
            if (m_Ctx.discardSnapshot) {
                m_Ctx.discardSnapshot = false;
            } else {
                m_SceneManager->Restore(m_PlaySnapshot);
            }
            Physics::ClearState();
            Physics3D::ClearState();
            m_PhysicsAccumulator = 0.0f;
        }
        m_LastEditorState = m_EditorState;
    }

    // Симуляция идёт только в Play
    if (m_EditorState != EditorState::Play) {
        m_PhysicsAccumulator = 0.0f;
        return;
    }

    // Фиксированный шаг физики, как в Unity: максимум 5 подшагов за кадр,
    // иначе симуляция не успевает за реальным временем и уходит в спираль
    m_PhysicsAccumulator += deltaTime;
    const float fixedDt = Physics::FixedDeltaTime;
    int steps = 0;
    while (m_PhysicsAccumulator >= fixedDt && steps < 5) {
        Physics::Step(m_SceneManager->GetEntities(), fixedDt);
        Physics3D::Step(m_SceneManager->GetEntities(), fixedDt);
        ConsumePhysicsEvents();
        m_PhysicsAccumulator -= fixedDt;
        steps++;
    }
    if (steps == 5) m_PhysicsAccumulator = 0.0f;

    // Скрипты: Update раз в кадр, как Unity MonoBehaviour (не в фиксированном шаге)
    Scripting::SyncInstances(m_SceneManager->GetEntities());
    Scripting::Update(deltaTime, m_SceneManager->GetEntities());

    // Менеджер сцен: LoadScene() из скрипта — переключаем уровень
    std::string nextScene;
    if (Scripting::ConsumeSceneChange(nextScene)) {
        std::error_code ec;
        if (std::filesystem::exists(nextScene, ec)) {
            Scripting::Unload();               // старые инстансы/библиотеки долой
            m_Serializer->Load(m_SceneManager.get(), nextScene);
            Scripting::LoadForScene(m_SceneManager->GetEntities());
            Scripting::SyncInstances(m_SceneManager->GetEntities());
            std::cout << "[Scene] loaded: " << nextScene << "\n";
        } else {
            std::cerr << "[Scene] not found: " << nextScene << "\n";
        }
    }

    // playOnAwake для сущностей, появившихся уже в Play (инстансы префабов)
    for (const auto& e : m_SceneManager->GetEntities()) {
        if (!e.active || e.audio.path.empty() || !e.audio.playOnAwake) continue;
        if (m_AudioStarted.count(e.id)) continue;
        uint32_t vid = e.audio.loop
            ? Audio::PlayLooped(e.audio.path, e.audio.volume, e.audio.pitch, e.audio.group)
            : Audio::PlayOneShot(e.audio.path, e.audio.volume, e.audio.pitch, e.audio.group);
        if (vid) m_AudioStarted.insert(e.id);
    }
}

void Application::ConsumePhysicsEvents() {
    const auto& entities = m_SceneManager->GetEntities();
    auto nameOf = [&](uint32_t id) -> std::string {
        for (const auto& e : entities) {
            if (e.id == id) return e.name;
        }
        return "(destroyed)";
    };

    for (const auto& ev : Physics::GetEvents()) {
        switch (ev.type) {
            case PhysicsEventType::TriggerEnter:
                std::cout << "[TriggerEnter] " << nameOf(ev.entityA) << " <-> " << nameOf(ev.entityB) << "\n";
                break;
            case PhysicsEventType::TriggerExit:
                std::cout << "[TriggerExit ] " << nameOf(ev.entityA) << " <-> " << nameOf(ev.entityB) << "\n";
                break;
            case PhysicsEventType::Collision:
                std::cout << "[Collision  ] " << nameOf(ev.entityA) << " <-> " << nameOf(ev.entityB) << "\n";
                break;
        }
    }
}

void Application::Render(float deltaTime) {
    int w = m_Window->GetWidth();
    int h = m_Window->GetHeight();

    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_GUI->BeginFrame();
    if (m_PlayerMode) {
        m_GUI->RenderPlayerFrame(m_Ctx, w, h);
    } else {
        m_GUI->RenderEditorPanels(m_Ctx, deltaTime);
    }
    m_GUI->EndFrame();
}
