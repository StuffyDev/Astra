#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Entity;
class SceneManager;
class Script;

// Компиляция (g++ -shared) и горячая загрузка (dlopen) C++-скриптов.
// Жизненный цикл: SetScene -> LoadForScene -> SyncInstances -> Update... -> Unload.
class Scripting {
public:
    static void SetScene(SceneManager* sm);

    // Компиляция и dlopen всех уникальных scriptPath сцены; ошибки — в Errors()
    static void LoadForScene(const std::vector<Entity>& entities);
    // Создание экземпляров для новых носителей скрипта, OnDestroy+удаление для мёртвых
    static void SyncInstances(const std::vector<Entity>& entities);
    static void Update(float dt, const std::vector<Entity>& entities);
    // Разослать событие анимации сущности (вызывает Script::OnAnimEvent)
    static void DispatchAnimEvent(uint32_t entityId, const char* name);
    static void Unload();

    static Script* InstanceFor(uint32_t entityId);
    static const std::vector<std::string>& Errors();

    // TimeScale по умолчанию: переживает выход из Play (для окна настроек движка)
    static void SetDefaultTimeScale(float scale);
    static float DefaultTimeScale();

    // Принудительная компиляция в build-scripts/<stem>.so (для Build Game)
    static bool PrecompileScript(const std::string& cppPath, std::string& outSoPath,
                                 std::string& outError);

    // Менеджер сцен: LoadScene() из скрипта; true, если есть незагруженный запрос
    static bool ConsumeSceneChange(std::string& outPath);
    // Тряска камеры: смещение на этот кадр (0,0 если не трясём)
    static glm::vec2 ShakeOffset();
    static bool QuitRequested();
    static void ClearQuit();
    static int CaptureMouseState(); // 1=захват, 2=отпустить, 0=нет изменений
};

// Символы, которые используют скрипты (экспортируются бинарником, -rdynamic)
void Log(const std::string& message);
bool DestroyEntity(uint32_t id);
void LoadScene(const std::string& scenePath);
// Тряска game-камеры: амплитуда в мировых единицах, длительность в секундах
void ShakeCamera(float amplitude, float duration);
// Выйти из игры (плеер — закрыть окно; редактор — остановить Play)
void QuitGame();
// Захват курсора (мышь внутри окна, без указателя системы)
void CaptureMouse(bool on);
bool IsMouseCaptured();
// Спавн префаба в мировой точке -> id корня (0 = ошибка)
uint32_t InstantiatePrefab(const std::string& prefabPath, const glm::vec2& worldPos);
