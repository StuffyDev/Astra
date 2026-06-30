#include "core/Window.h"
#include <iostream>

Window::Window(int width, int height, const std::string& title)
    : m_Width(width), m_Height(height), m_LastMousePos(0.0f, 0.0f) {

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    m_Window = glfwCreateWindow(mode->width, mode->height, title.c_str(), monitor, nullptr);

    if (!m_Window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return;
    }

    MakeContextCurrent();
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetScrollCallback(m_Window, ScrollCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
    }

    glViewport(0, 0, mode->width, mode->height);
    m_Width = mode->width;
    m_Height = mode->height;

    double mx, my;
    glfwGetCursorPos(m_Window, &mx, &my);
    m_LastMousePos = glm::vec2(static_cast<float>(mx), static_cast<float>(my));
}

Window::~Window() {
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void Window::SwapBuffers() {
    glfwSwapBuffers(m_Window);
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::MakeContextCurrent() {
    glfwMakeContextCurrent(m_Window);
}

glm::vec2 Window::GetMousePos() const {
    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);
    return glm::vec2(static_cast<float>(x), static_cast<float>(y));
}

void Window::UpdateLastMousePos() {
    m_LastMousePos = GetMousePos();
}

bool Window::IsMouseButtonDown(int button) const {
    return glfwGetMouseButton(m_Window, button) == GLFW_PRESS;
}

bool Window::IsKeyDown(int key) const {
    return glfwGetKey(m_Window, key) == GLFW_PRESS;
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_Width = width;
        self->m_Height = height;
    }
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_ScrollOffset = static_cast<float>(yoffset);
    }
}
