#include "core/GUI.h"
#include "core/Camera.h"
#include "core/Scene.h"
#include "ecs/SceneManager.h"
#include "ecs/Entity.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <imgui.h>
#include <imgui_internal.h>

GUI::GUI(GLFWwindow* window) : m_Window(window) {}
GUI::~GUI() { Shutdown(); }

void GUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Unity-like style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.WindowPadding = ImVec2(8.0f, 8.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.50f, 0.15f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.50f, 0.20f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.90f, 0.50f, 0.15f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void GUI::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GUI::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GUI::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}

void GUI::RenderEditorPanels(Camera* camera, SceneManager* sceneManager, Scene* scene, float deltaTime) {
    // Main Menu Bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {}
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {}
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("GameObject")) {
            if (ImGui::MenuItem("Create Quad")) {
                Entity e;
                e.name = "Quad";
                e.sprite.type = SpriteType::Quad;
                e.sprite.color = glm::vec3(1.0f, 1.0f, 1.0f);
                e.transform.scale = glm::vec2(100.0f, 100.0f);
                sceneManager->AddEntity(e);
            }
            if (ImGui::MenuItem("Create Circle")) {
                Entity e;
                e.name = "Circle";
                e.sprite.type = SpriteType::Circle;
                e.sprite.color = glm::vec3(1.0f, 1.0f, 1.0f);
                e.transform.scale = glm::vec2(100.0f, 100.0f);
                sceneManager->AddEntity(e);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // DockSpace Root
    ImGuiWindowFlags dockFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    dockFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
              | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
              | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("DockSpaceRoot", nullptr, dockFlags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

        ImGuiID dock_main = dockspaceID;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.25f, nullptr, &dock_main);
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_bottom);
        ImGui::DockBuilderDockWindow("Scene", dock_main);
        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();

    // ===== SCENE VIEW (Framebuffer texture) =====
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene");

    m_SceneFocused = ImGui::IsWindowFocused();
    m_SceneHovered = ImGui::IsWindowHovered();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_SceneSize = glm::vec2(avail.x, avail.y);

    // Рендерим сцену в текстуру
    scene->Render(camera, sceneManager, static_cast<int>(avail.x), static_cast<int>(avail.y));

    // Показываем текстуру
    ImGui::Image((ImTextureID)(intptr_t)scene->GetViewportTexture(), avail, ImVec2(0, 1), ImVec2(1, 0));

    // Мышь в координатах сцены
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    // cursorPos уже после Image, так что считаем от windowPos + padding
    ImVec2 scenePos = ImVec2(
        windowPos.x + ImGui::GetWindowContentRegionMin().x,
        windowPos.y + ImGui::GetWindowContentRegionMin().y + ImGui::GetFrameHeight()
    );

    m_SceneMousePos = glm::vec2(
        mousePos.x - scenePos.x,
        mousePos.y - scenePos.y
    );

    ImGui::End();
    ImGui::PopStyleVar();

    // ===== HIERARCHY =====
    ImGui::Begin("Hierarchy");
    auto& entities = sceneManager->GetEntities();
    for (size_t i = 0; i < entities.size(); i++) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
        if (sceneManager->GetSelectedEntity() == static_cast<int>(i))
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(entities[i].name.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            sceneManager->SetSelectedEntity(static_cast<int>(i));
        }
        if (open) ImGui::TreePop();
    }
    ImGui::End();

    // ===== INSPECTOR =====
    ImGui::Begin("Inspector");
    Entity* selected = sceneManager->GetSelectedEntityPtr();
    if (selected) {
        ImGui::Text("Transform");
        ImGui::DragFloat2("Position", &selected->transform.position.x, 1.0f);
        ImGui::DragFloat("Rotation", &selected->transform.rotation, 1.0f);
        ImGui::DragFloat2("Scale", &selected->transform.scale.x, 1.0f, 0.1f, 10000.0f);

        ImGui::Separator();
        ImGui::Text("Sprite");
        const char* types[] = { "None", "Quad", "Circle" };
        int type = static_cast<int>(selected->sprite.type);
        if (ImGui::Combo("Type", &type, types, 3)) {
            selected->sprite.type = static_cast<SpriteType>(type);
        }
        ImGui::ColorEdit3("Color", &selected->sprite.color.r);
    } else {
        ImGui::TextDisabled("Select an object to inspect");
    }
    ImGui::End();

    // ===== PROJECT =====
    ImGui::Begin("Project");
    ImGui::Text("Assets");
    ImGui::End();
}
