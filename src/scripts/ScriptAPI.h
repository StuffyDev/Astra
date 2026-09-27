// Публичный API игровых скриптов.
// Скрипты компилируются g++ в .so при входе в Play и линкуются с бинарником движка.
// Движок сам подключает этот заголовок и базовые инклюды — в скрипте их писать не нужно.
#pragma once
#include "ecs/Entity.h"
#include <cstdint>
#include <string>
#include <random>
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

    // Событие анимации: кадр клипа пересёк отметку из инспектора (step kick, shoot, footstep)
    virtual void OnAnimEvent(const char* name) { (void)name; }

protected:
    // Реализации — на стороне движка (core/Scripting.cpp)
    Entity* Owner();
    ::SceneManager* Scene();

    // Серелиазуемые переменные (как [SerializeField] в Unity): объявить в Start(),
    // ползунок появится в Inspector; значение живёт в сцене и доступно в Update()
    void DefineVar(const char* name, float defaultValue); // если уже есть — не трогает
    float GetVar(const char* name, float fallback = 0.0f) const;
    void SetVar(const char* name, float value);

    // Удобные хелперы над Owner()
    void Translate(const glm::vec2& localDelta);      // сдвиг в локальных координатах родителя
    void SetWorldPosition(const glm::vec2& world);    // позиция в мире (с учётом цепочки)
    glm::vec2 WorldPosition() const;                  // мировая позиция с учётом родителя
    float AngleTo(const glm::vec2& worldPoint) const; // градусы направления на точку
    void LookAt(const glm::vec2& worldPoint);         // повернуть "верх" объекта на точку

    // ===== 3D (сущность с is3D): поза, скорость, силы =====
    // Реализации — в core/Scripting.cpp. Все величины в мировых единицах движка
    // (100 единиц = 1 метр по умолчанию, см. Settings ▸ Pixels per meter).
    glm::vec3 Position3D() const;                     // мировая позиция (с parent-цепочкой)
    void SetPosition3D(const glm::vec3& world);       // позиция в мире (пересчитывает в локальную)
    void Translate3D(const glm::vec3& delta);         // сдвиг pos3 (без учёта родителя)
    glm::vec3 Rotation3D() const;                     // углы в градусах (X→Y→Z)
    void SetRotation3D(const glm::vec3& degrees);
    void SetScale3D(const glm::vec3& scale);
    glm::vec3 Velocity3D() const;                     // скорость Rigidbody (3D)
    void SetVelocity3D(const glm::vec3& v);           // создаст Rigidbody (3D), если его нет
    void AddForce3D(const glm::vec3& impulse);        // импульс F*dt: velocity += impulse / mass
    void LookAt3D(const glm::vec3& worldTarget);      // развернуть -Z объекта на цель (как в Unity)
    void SetGravityEnabled3D(bool on);
    bool Is3D() const;
    // То же для ЧУЖОЙ сущности (inside Script имя перекрывает глобальную функцию,
    // поэтому нужны перегрузки-члены, а не ::AddForce3D(other, ...))
    void AddForceTo3D(Entity* target, const glm::vec3& impulse);
    void SetVelocityOf3D(Entity* target, const glm::vec3& v);
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

// ===== Хелперы =====
// Лог в панель Console движка (реализация на стороне движка)
void Log(const std::string& message);
// Уничтожить сущность по id (например, Owner()->id). Возвращает false, если её нет
bool DestroyEntity(uint32_t id);
// Тряска камеры (game-feel): амплитуда в мировых единицах, длительность в секундах
void ShakeCamera(float amplitude, float duration);
// Выход из игры: плеер закрывает окно, редактор останавливает Play
void QuitGame();
// Захват мыши (шутеры/ RTS): курсор системы скрывается, события мыши остаются
void CaptureMouse(bool on);
bool IsMouseCaptured();
// Спавн префаба в мировой точке; возвращает id корня (0 — ошибка)
uint32_t InstantiatePrefab(const std::string& prefabPath, const glm::vec2& worldPos);
// Переключение сцены во время Play/игры: движок загрузит файл и пересоздаст скрипты
void LoadScene(const std::string& scenePath);

// ===== 3D-лучи и спавн =====
struct RayHit3D {
    uint32_t entityId = 0;
    std::string name;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
};

// Первый пересечение луча с 3D-телом сцены (коллайдер или габарит меша).
// dir не обязан быть нормализован; maxDist — в мировых единицах.
bool Raycast3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D& outHit);
// То же, но список всех попаданий (от ближнего к дальнему)
int RaycastAll3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
                 RayHit3D* outHits, int maxHits);
// Гравитация 3D-мира (единиц/с²), читается из настроек движка
glm::vec3 Gravity3D();

// Инстанцировать префаб в 3D-точку (возвращает id корня new-инстанса)
uint32_t InstantiatePrefab3D(const std::string& prefabPath, const glm::vec3& pos);

// Импulse приложен к скорости (F*dt-подобный «толчок», как AddForce в Unity)
inline void AddForce(Entity* e, const glm::vec2& impulse) {
    if (!e) return;
    float m = e->rigidbody.mass > 0.01f ? e->rigidbody.mass : 0.01f;
    e->rigidbody.velocity += impulse / m;
}

// То же для 3D-тела: Rigidbody (3D) создаётся автоматически, если его ещё нет
inline void AddForce3D(Entity* e, const glm::vec3& impulse) {
    if (!e) return;
    if (!e->hasRigidbody3D) { e->hasRigidbody3D = true; e->rb3.useGravity = true; }
    float m = e->rb3.mass > 0.01f ? e->rb3.mass : 0.01f;
    e->rb3.velocity += impulse / m;
}

// Мгновенная скорость 3D-тела (тоже создаёт Rigidbody при необходимости)
inline void SetVelocity3D(Entity* e, const glm::vec3& v) {
    if (!e) return;
    if (!e->hasRigidbody3D) { e->hasRigidbody3D = true; e->rb3.useGravity = false; }
    e->rb3.velocity = v;
}

// Шаг к точке по прямой (для полёта/патруля в 3D)
inline glm::vec3 MoveTowards3D(const glm::vec3& from, const glm::vec3& to, float maxDelta) {
    glm::vec3 d = to - from;
    float len = glm::length(d);
    if (len <= maxDelta || len < 1e-6f) return to;
    return from + d * (maxDelta / len);
}

// Спрайт-анимация: PlayAnimation(e) с fromStart=true — перемотка на первый кадр
inline void PlayAnimation(Entity* e, bool fromStart = false) {
    if (!e) return;
    if (fromStart) e->animTime = 0.0f;
    e->animation.active = true;
}
inline void StopAnimation(Entity* e) { if (e) e->animation.active = false; }
inline bool IsAnimating(const Entity* e) { return e && e->animation.active; }
// Именованный клип: true, если клип найден и запущен
inline bool PlayClip(Entity* e, const char* clipName, bool fromStart = true) {
    if (!e || !clipName) return false;
    auto& clips = e->animation.clips;
    for (size_t i = 0; i < clips.size(); i++) {
        if (clips[i].name == clipName) {
            e->animation.activeClip = (int)i;
            e->animation.active = true;
            if (fromStart) e->animTime = 0.0f;
            return true;
        }
    }
    return false;
}
// Выплюнуть пачку частиц из эмиттера сущности (взрыв/искры)
void EmitParticles(Entity* e, int count);

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Radians(float deg) { return deg * 3.14159265358979f / 180.0f; }
inline float Degrees(float rad) { return rad * 180.0f / 3.14159265358979f; }

// Случайные числа (свой движок на .so, не зависит от порядка загрузки)
inline float RandomRange(float a, float b) {
    static std::mt19937 rng(std::random_device{}());
    return std::uniform_real_distribution<float>(a, b)(rng);
}
inline int RandomInt(int a, int bInclusive) {
    static std::mt19937 rng(std::random_device{}());
    return std::uniform_int_distribution<int>(a, bInclusive)(rng);
}
