#include "core/Input.h"

Input& Input::Get() {
    static Input instance;
    return instance;
}

void Input::Init(GLFWwindow* window) {
    m_Window = window;
    m_Initialized = true;
    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);
    m_MousePos = m_MousePosPrev = glm::vec2((float)x, (float)y);
}

bool Input::KeyDown(int key) const {
    if (!m_Window) return false;
    bool held = glfwGetKey(m_Window, key) == GLFW_PRESS;
    // ленивая регистрация: если NewFrame ещё не вёл ключ, не выдумываем фантомное нажатие
    if (m_Key.find(key) == m_Key.end() && m_KeyPrev.find(key) == m_KeyPrev.end())
        m_KeyPrev[key] = held;
    m_Key[key] = held;
    return held;
}

bool Input::IsKeyDown(int key) const { return KeyDown(key); }

bool Input::WasKeyPressed(int key) const {
    return KeyDown(key) && !(m_KeyPrev.count(key) && m_KeyPrev.at(key));
}

bool Input::WasKeyReleased(int key) const {
    return !KeyDown(key) && (m_KeyPrev.count(key) && m_KeyPrev.at(key));
}

bool Input::IsMouseButtonDown(int button) const {
    if (!m_Window) return false;
    bool held = glfwGetMouseButton(m_Window, button) == GLFW_PRESS;
    if (m_Mouse.find(button) == m_Mouse.end() && m_MousePrev.find(button) == m_MousePrev.end())
        m_MousePrev[button] = held;
    m_Mouse[button] = held;
    return held;
}

bool Input::WasMouseButtonPressed(int button) const {
    return IsMouseButtonDown(button) && !(m_MousePrev.count(button) && m_MousePrev.at(button));
}

bool Input::WasMouseButtonReleased(int button) const {
    return !IsMouseButtonDown(button) && (m_MousePrev.count(button) && m_MousePrev.at(button));
}

void Input::BindKey(const std::string& action, int key) {
    m_Actions[action].keys.push_back(key);
}

void Input::BindMouseButton(const std::string& action, int button) {
    m_Actions[action].buttons.push_back(button);
}

bool Input::IsActionHeld(const std::string& action) const {
    auto it = m_Actions.find(action);
    if (it == m_Actions.end()) return false;
    for (int k : it->second.keys) if (KeyDown(k)) return true;
    for (int b : it->second.buttons) if (IsMouseButtonDown(b)) return true;
    return false;
}

bool Input::IsActionPressed(const std::string& action) const {
    auto it = m_Actions.find(action);
    if (it == m_Actions.end()) return false;
    for (int k : it->second.keys) if (WasKeyPressed(k)) return true;
    for (int b : it->second.buttons) if (WasMouseButtonPressed(b)) return true;
    return false;
}

bool Input::IsActionReleased(const std::string& action) const {
    auto it = m_Actions.find(action);
    if (it == m_Actions.end()) return false;
    for (int k : it->second.keys) if (WasKeyReleased(k)) return true;
    for (int b : it->second.buttons) if (WasMouseButtonReleased(b)) return true;
    return false;
}

void Input::BindAxis(const std::string& axis, const std::string& negativeAction,
                     const std::string& positiveAction) {
    m_Axes[axis] = { negativeAction, positiveAction };
}

float Input::GetAxis(const std::string& axis) const {
    auto it = m_Axes.find(axis);
    if (it == m_Axes.end()) return 0.0f;
    float value = 0.0f;
    if (IsActionHeld(it->second.first)) value -= 1.0f;
    if (IsActionHeld(it->second.second)) value += 1.0f;
    return value;
}

void Input::NewFrame() {
    if (!m_Initialized || !m_Window) return;

    // фиксируем предыдущее состояние для edge-проверок
    for (auto& kv : m_Key) m_KeyPrev[kv.first] = kv.second;
    for (auto& kv : m_Mouse) m_MousePrev[kv.first] = kv.second;
    for (const auto& a : m_Actions) {
        for (int k : a.second.keys) m_KeyPrev[k] = m_Key.count(k) ? m_Key.at(k) : false;
        for (int b : a.second.buttons)
            m_MousePrev[b] = m_Mouse.count(b) ? m_Mouse.at(b) : false;
    }

    // обновляем состояние всех отслеживаемых клавиш/кнопок
    for (auto& kv : m_Key)
        kv.second = glfwGetKey(m_Window, kv.first) == GLFW_PRESS;
    for (auto& kv : m_Mouse)
        kv.second = glfwGetMouseButton(m_Window, kv.first) == GLFW_PRESS;

    m_MousePosPrev = m_MousePos;
    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);
    m_MousePos = glm::vec2((float)x, (float)y);

    m_ScrollDelta = 0.0f;
}
