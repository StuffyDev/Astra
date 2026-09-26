// Публичный API игровых скриптов.
// Скрипты компилируются g++ в .so при входе в Play и линкуются с бинарником движка.
// Движок сам подключает этот заголовок и базовые инклюды — в скрипте их писать не нужно.
#pragma once
#include "ecs/Entity.h"
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

class SceneManager;

// Базовый класс компонента-скрипта. Наследуйтесь и переопределяйте хуки.
class Script {
public:
    uint32_t ownerId = 0;
    virtual ~Script() = default;

    virtual void Start() {}
    virtual void Update(float dt) { (void)dt; }
    virtual void OnDestroy() {}

    // Физические события (id второго участника контакта)
    virtual void OnTriggerEnter(uint32_t otherId) { (void)otherId; }
    virtual void OnTriggerExit(uint32_t otherId) { (void)otherId; }
    virtual void OnCollisionEnter(uint32_t otherId) { (void)otherId; }

protected:
    // Реализации — на стороне движка (core/Scripting.cpp)
    Entity* Owner();
    ::SceneManager* Scene();

    // Удобные хелперы над Owner()
    void Translate(const glm::vec2& localDelta);      // сдвиг в локальных координатах родителя
    void SetWorldPosition(const glm::vec2& world);    // позиция в мире (с учётом цепочки)
    glm::vec2 WorldPosition() const;                  // мировая позиция с учётом родителя
    float AngleTo(const glm::vec2& worldPoint) const; // градусы направления на точку
    void LookAt(const glm::vec2& worldPoint);         // повернуть "верх" объекта на точку
};

// Время кадра/сессии — доступно из любого места скрипта
class Time {
public:
    static float Delta();       // dt последнего кадра (с учётом timeScale)
    static float UnscaledDelta();// dt без timeScale
    static float SinceStart();  // секунды с момента нажатия Play
    static float TimeScale();
    static void SetTimeScale(float scale);
};

// Одна скрипт-фабрика на файл: движок компилирует .so и грузит символ CreateGameScript
#define SCRIPT_ENTRY(Class) \
    extern "C" Script* CreateGameScript() { return new Class(); }
