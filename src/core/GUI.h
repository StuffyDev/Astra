#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "core/Camera.h"
#include "core/EditorState.h"
#include "ecs/SceneManager.h"

class SceneManager;
class Scene;
class SceneSerializer;
class Renderer;
class ProjectManager;
class Application;
struct Entity;

struct EditorContext {
    Camera* camera;
    SceneManager* sceneManager;
    Scene* scene;
    SceneSerializer* serializer;
    Renderer* renderer;
    ProjectManager* projectManager;
    EditorState* state;
    Application* app = nullptr; // undo/redo
    // true — при выходе из Play не надо восстанавливать снимок (например, сменили проект)
    bool discardSnapshot = false;
};

class GUI {
public:
    GUI(GLFWwindow* window);
    ~GUI();

    void Init();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    void RenderEditorPanels(EditorContext& ctx, float deltaTime);
    void RenderPlayerFrame(EditorContext& ctx, int w, int h);

    bool IsSceneHovered() const { return m_SceneHovered; }
    bool IsSceneFocused() const { return m_SceneFocused; }
    glm::vec2 GetSceneMousePos() const { return m_SceneMousePos; }
    glm::vec2 GetSceneSize() const { return m_SceneSize; }

    bool IsAnyPopupOpen() const { return m_PopupOpen; }

private:
    GLFWwindow* m_Window;
    bool m_SceneHovered = false;
    bool m_SceneFocused = false;
    glm::vec2 m_SceneMousePos;
    glm::vec2 m_SceneSize;
    ImVec2 m_SceneImagePos = ImVec2(0, 0);
    bool m_PopupOpen = false;

    // ===== Сцены: путь, dirty-подпись, незакрытые действия =====
    std::string m_CurrentScenePath;
    uint64_t m_SceneSignature = 0;
    bool m_SceneDirty = false;
    bool m_BaselineValid = false;       // первый кадр: снять baseline, не помечая dirty
    int m_PendingAction = 0;          // 1 = New Scene, 2 = Open Scene
    std::string m_PendingScenePath;   // что открываем после подтверждения
    bool m_ShowSceneConfirm = false;

    // Browser сцен (открыть/сохранить)
    bool m_ShowSceneBrowser = false;
    bool m_SceneBrowserSave = false;
    std::string m_SceneBrowserDir = "assets/scenes";
    std::string m_SceneBrowserSelected;   // полный путь выбранного .scene
    char m_SceneBrowserNameBuf[256] = {};
    std::string m_SceneBrowserAfterOpen;  // после сохранения открыть это
    bool m_SceneBrowserAfterNew = false;  // после сохранения создать новую сцену

    // ===== Префабы =====
    bool m_ShowPrefabSaveDialog = false;
    int m_PrefabSourceIndex = -1;
    std::string m_PrefabSaveDir = "assets/prefabs";
    char m_PrefabNameBuf[256] = {};

    // Диалоги сущностей
    bool m_ShowNewEntityDialog = false;
    bool m_ShowRenameDialog = false;
    int m_RenameIndex = -1;
    char m_RenameBuffer[256] = {};
    char m_NewEntityName[256] = "New Entity";
    int m_NewEntityType = 1; // 1 = Quad, 2 = Circle

    // Инспектор: буферы полей (синхронизация по id, а не по указателю —
    // Restore после Stop пересобирает вектор сущностей)
    uint32_t m_InspectorEntityId = 0;
    char m_TexturePathBuffer[512] = {};
    char m_AnimTextureBuffer[512] = {};
    char m_UILabelBuffer[256] = {};
    char m_ScriptPathBuffer[512] = {};
    char m_AudioPathBuffer[512] = {};

    // Видимость панелей
    bool m_ShowScene = true;
    bool m_ShowGame = true;
    bool m_ShowHierarchy = true;
    bool m_ShowInspector = true;
    bool m_ShowProject = true;
    bool m_RebuildDockLayout = false;

    // Game-камера и область изображения Game-view (для UI-оверлея)
    std::unique_ptr<Camera> m_GameCamera;
    bool m_HasGameCamera = false;
    glm::vec2 m_GameSize = glm::vec2(1280.0f, 720.0f);
    ImVec2 m_GameImagePos = ImVec2(0, 0);
    ImVec2 m_GameImageSize = ImVec2(0, 0);

    // Project-панель (Unity-style браузер ассетов)
    std::string m_BrowsePath = "assets";
    bool m_ProjectGrid = true;
    bool m_ProjectPanelFocused = false;
    bool m_ProjectConsoleTab = false; // активная вкладка: false = Assets, true = Console
    char m_ProjectSearch[128] = "";
    std::string m_SelectedAsset;
    std::string m_PendingDelete;
    bool m_ShowDeleteConfirm = false;
    std::string m_RenameAssetPath;
    char m_AssetNameBuffer[256] = {};

    // Проекты
    bool m_ShowNewProjectDialog = false;
    bool m_ShowOpenProjectDialog = false;
    bool m_ShowProjectManagerWindow = false;
    bool m_ShowSettings = false;
    char m_NewProjectName[256] = "NewProject";
    char m_NewProjectLocation[512] = "";
    char m_OpenProjectPath[512] = "";
    std::string m_ProjectError;

    // Выбор папки: 0 = закрыт, 1 = Location нового проекта, 2 = путь открываемого проекта
    // 3 = выбор файла для импорта (m_FolderPickerPickFile)
    int m_FolderPickerTarget = 0;
    std::string m_FolderPickerPath;
    bool m_FolderPickerPickFile = false;

    // Консоль
    bool m_ConsoleFollow = true;
    float m_FpsEma = 60.0f;

    // Undo: снимок «до пачки правок»; пуш при изменении относительно baseline
    SceneManager::SceneSnapshot m_UndoBaseline;
    uint64_t m_UndoBaselineSig = 0;
    bool m_UndoBaselineValid = false;
    bool m_WasChangedVsBaseline = false;

    // Встроенный редактор кода (IDE-lite)
    bool m_ShowCodeWindow = false;
    bool m_CodeWindowFocused = false;
    bool m_CodeWindowOpenRequest = false;
    bool m_CodeDirty = false;
    std::string m_CodePath;
    std::string m_CodeText;

    // Билд игры
    bool m_ShowBuildDialog = false;
    char m_BuildDirBuf[512] = "build_release";
    int m_BuildScene = 0;              // индекс в списке сцен; 0 = текущая
    std::vector<std::string> m_BuildSceneList;
    std::string m_BuildStatus;
    bool m_BuildRunning = false;
    int m_BuildMode = 0;            // 0 = лаунчер+либка, 1 = один exe, 2 = папка
    bool m_BuildCopyEngineLib = true;

    // Шрифты для runtime UI
    ImFont* m_FontMedium = nullptr;
    ImFont* m_FontLarge = nullptr;

    void RenderHierarchy(EditorContext& ctx);
    void RenderEntityNode(EditorContext& ctx, size_t index,
                          const std::unordered_map<uint32_t, std::vector<size_t>>& children, int depth);
    void RenderInspector(EditorContext& ctx);
    void RenderProject(EditorContext& ctx);
    void RenderSceneBrowser(EditorContext& ctx);
    void RenderSceneConfirm(EditorContext& ctx);
    void RenderPrefabSaveDialog(EditorContext& ctx);
    void RenderNewEntityDialog(SceneManager* sceneManager);
    void RenderRenameDialog(SceneManager* sceneManager);
    void RenderAssetDialogs(EditorContext& ctx);
    void RenderProjectDialogs(EditorContext& ctx);
    void RenderFolderPicker(EditorContext& ctx);
    void RenderSettings(EditorContext& ctx);
    void RenderGameUIOverlay(EditorContext& ctx);
    void HandleHotkeys(EditorContext& ctx);
    void UpdateWindowTitle(EditorContext& ctx);
    void UpdateGameCamera(SceneManager* sceneManager);
    void ApplyProject(EditorContext& ctx);

    //Workflows сцен
    void RefreshSceneDirty(EditorContext& ctx);
    void MarkSceneSaved(EditorContext& ctx);
    void SaveSceneNow(EditorContext& ctx);
    void OpenSceneSaveAs(EditorContext& ctx);
    bool LoadSceneAsset(EditorContext& ctx, const std::string& path);
    void RequestNewScene(EditorContext& ctx);
    void RequestOpenScene(EditorContext& ctx, const std::string& path);
    void DoNewScene(EditorContext& ctx);

    // Префабы
    int InstantiatePrefab(EditorContext& ctx, const std::string& path, const glm::vec2& worldPos);
    void RevertToPrefab(EditorContext& ctx, size_t index);

    // Редактор кода и импорт
    void RenderCodeWindow(EditorContext& ctx);
    void OpenCodeFile(const std::string& path);
    bool SaveCodeFile();
    void ImportFileToAssets(EditorContext& ctx, const std::string& srcPath);
    void OpenExternally(const std::string& path);

    // Билд игры
    void RenderBuildDialog(EditorContext& ctx);
    bool BuildGame(const std::string& destDir, const std::string& scenePath);

    // Буфер обмена для сущностей (Ctrl+C / Ctrl+V в редакторе)
    std::vector<Entity> m_Clipboard;
    bool m_PendingRestart = false;
};

// Сборка игры: mode 0 — маленький лаунчер, линкованный с libastra_engine.so (как в Godot:
// движок не дублируется, либка кладётся рядом или берётся по пути сборки);
// mode 1 — один exe с приклеенным бандлом; mode 2 — папка (astra + assets + build-scripts + game.json).
bool AstraBuildGame(const std::string& exeSrc, const std::string& scenePath,
                    const std::string& destDir, int mode, bool copyEngineLib, std::string& status);

// Ищет бандл в конце собственного исполняемого файла; если есть — распаковывает
// в каталог рядом с exe и возвращает его (иначе пустую строку)
std::string AstraBundleExtract();
