#pragma once
#include <cstdint>
#include <string>
#include <vector>

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
};

// Символы, которые используют скрипты (экспортируются бинарником, -rdynamic)
void Log(const std::string& message);
bool DestroyEntity(uint32_t id);
void LoadScene(const std::string& scenePath);
