// Игровая input-система: опрос клавиатуры/мыши + именованные экшены и оси.
// Библиотека независима от редактора: рантайм/скрипты (Lua или C++) будут дёргать её же API.
#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Input {
public:
    static Input& Get();

    void Init(GLFWwindow* window);
    // Вызывать один раз в начале кадра (после обработки событий окна)
    void NewFrame();
    // Редактор делит scroll-колбэк с окном — пусть передаёт колесо сюда
    void FeedScroll(float scroll) { m_ScrollDelta += scroll; }

    // Событийный ввод по GLFW-кодам (GLFW_KEY_*, GLFW_MOUSE_BUTTON_*)
    bool IsKeyDown(int key) const;
    bool WasKeyPressed(int key) const;
    bool WasKeyReleased(int key) const;
    bool IsMouseButtonDown(int button) const;
    bool WasMouseButtonPressed(int button) const;
    bool WasMouseButtonReleased(int button) const;
    glm::vec2 MousePosition() const { return m_MousePos; }
    glm::vec2 MouseDelta() const { return m_MousePos - m_MousePosPrev; }
    float ScrollDelta() const { return m_ScrollDelta; }

    // Именованные экшены: срабатывают, если совпало любое связывание
    void BindKey(const std::string& action, int key);
    void BindMouseButton(const std::string& action, int button);
    bool IsActionHeld(const std::string& action) const;
    bool IsActionPressed(const std::string& action) const;
    bool IsActionReleased(const std::string& action) const;

    // Ось из двух экшенов, например BindAxis("MoveX", "Left", "Right") -> [-1..1]
    void BindAxis(const std::string& axis, const std::string& negativeAction,
                  const std::string& positiveAction);
    float GetAxis(const std::string& axis) const;

private:
    Input() = default;

    struct Binding {
        std::vector<int> keys;
        std::vector<int> buttons;
    };

    bool KeyDown(int key) const; // текущее состояние (для известных кодов опрашивает GLFW)

    GLFWwindow* m_Window = nullptr;
    // mutable: const-опросы лениво регистрируют ключи для edge-трекинга
    mutable std::unordered_map<int, bool> m_Key;        // код -> зажата
    mutable std::unordered_map<int, bool> m_KeyPrev;    // код -> была зажата в прошлом кадре
    mutable std::unordered_map<int, bool> m_Mouse;
    mutable std::unordered_map<int, bool> m_MousePrev;
    std::unordered_map<std::string, Binding> m_Actions;
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_Axes;
    glm::vec2 m_MousePos;
    glm::vec2 m_MousePosPrev;
    float m_ScrollDelta = 0.0f;
    bool m_Initialized = false;
};
