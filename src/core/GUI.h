#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera;
class SceneManager;
class Scene;

class GUI {
public:
    GUI(GLFWwindow* window);
    ~GUI();

    void Init();
    void Shutdown();
    void BeginFrame();
    void EndFrame();

    void RenderEditorPanels(Camera* camera, SceneManager* sceneManager, Scene* scene, float deltaTime);

    bool IsSceneHovered() const { return m_SceneHovered; }
    bool IsSceneFocused() const { return m_SceneFocused; }
    glm::vec2 GetSceneMousePos() const { return m_SceneMousePos; }
    glm::vec2 GetSceneSize() const { return m_SceneSize; }

private:
    GLFWwindow* m_Window;
    bool m_SceneHovered = false;
    bool m_SceneFocused = false;
    glm::vec2 m_SceneMousePos;
    glm::vec2 m_SceneSize;
};
