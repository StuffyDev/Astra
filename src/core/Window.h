#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <glm/glm.hpp>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    bool ShouldClose() const;
    void SwapBuffers();
    void PollEvents();
    void MakeContextCurrent();

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    GLFWwindow* GetNativeWindow() const { return m_Window; }

    glm::vec2 GetMousePos() const;
    glm::vec2 GetLastMousePos() const { return m_LastMousePos; }
    void UpdateLastMousePos();

    bool IsMouseButtonDown(int button) const;
    bool IsKeyDown(int key) const;

    float GetScrollOffset() const { return m_ScrollOffset; }
    void ResetScrollOffset() { m_ScrollOffset = 0.0f; }

private:
    GLFWwindow* m_Window;
    int m_Width, m_Height;
    glm::vec2 m_LastMousePos;
    float m_ScrollOffset = 0.0f;

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
