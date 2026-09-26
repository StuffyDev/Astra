#include "core/Scripting.h"
#include "scripts/ScriptAPI.h"
#include "ecs/SceneManager.h"
#include "ecs/Transforms.h"
#include "ecs/Physics.h"
#include "utils/ConsoleLog.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#ifndef SCRIPT_INCLUDES
#define SCRIPT_INCLUDES "-I."
#endif

// Символ точки входа скрипта (см. макрос SCRIPT_ENTRY)
static const char* const kScriptEntrySymbol = "CreateGameScript";

namespace {

struct LibHandle {
    void* lib = nullptr;
    Script* (*factory)() = nullptr;
};

struct Instance {
    Script* script = nullptr;
    std::string path; // scriptPath на момент создания — смена пути пересоздаёт инстанс
};

SceneManager* g_Scene = nullptr;
std::unordered_map<std::string, LibHandle> g_Libs;   // cpp path -> библиотеки
std::unordered_set<std::string> g_Failed;            // не перекомпилировать каждый кадр
std::unordered_map<uint32_t, Instance> g_Instances;  // entity id -> инстанс
std::vector<std::string> g_Errors;

float g_Delta = 0.0f, g_UnscaledDelta = 0.0f, g_Elapsed = 0.0f, g_TimeScale = 1.0f;
float g_DefaultTimeScale = 1.0f;
std::string g_PendingScene;   // запрос LoadScene из скрипта, обрабатывает Application

Script* CreateInstance(const std::string& path, uint32_t ownerId) {
    auto it = g_Libs.find(path);
    if (it == g_Libs.end() || !it->second.factory) return nullptr;
    Script* s = it->second.factory();
    if (!s) return nullptr;
    s->ownerId = ownerId;
    return s;
}

Entity* FindById(uint32_t id) {
    if (!g_Scene) return nullptr;
    for (auto& e : g_Scene->GetEntities()) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

std::string ShellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

// Обёртка-translation unit: движок сам подключает API, Input, звук и т.п.,
// чтобы в скрипте не было нужды писать инклюды.
bool WriteWrapper(const std::string& cppPath, std::string& outWrapper) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("build-scripts", ec);
    outWrapper = (fs::path("build-scripts") /
                  (fs::path(cppPath).stem().string() + "_gen.cpp")).string();
    std::string absUser = fs::absolute(cppPath, ec).string();
    std::ofstream wf(outWrapper);
    if (!wf.is_open()) return false;
    wf << "// Сгенерировано движком — не редактировать\n"
          "#include \"scripts/ScriptAPI.h\"\n"
          "#include \"ecs/SceneManager.h\"\n"
          "#include \"ecs/Transforms.h\"\n"
          "#include \"ecs/Physics.h\"\n"
          "#include \"core/Input.h\"\n"
          "#include \"core/GameUI.h\"\n"
          "#include \"core/Audio.h\"\n"
          "#include \"core/EditorState.h\"\n"
          "#include <GLFW/glfw3.h>\n"
          "#include <glm/glm.hpp>\n"
          "#include <cmath>\n"
          "#include <string>\n"
          "#include <vector>\n"
          "#include <algorithm>\n"
       << "#include \"" << absUser << "\"\n";
    wf.close();
    return true;
}

bool CompileScript(const std::string& cppPath, std::string& outSoPath, std::string& outLog) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("build-scripts", ec);

    std::string wrapper;
    if (!WriteWrapper(cppPath, wrapper)) {
        outLog = "Failed to write wrapper " + wrapper;
        return false;
    }

    outSoPath = (fs::path("build-scripts") / (fs::path(cppPath).stem().string() + ".so")).string();

    std::string cmd = "g++ -std=c++17 -g -shared -fPIC -o " + ShellQuote(outSoPath) + " " +
                      ShellQuote(wrapper) + " " SCRIPT_INCLUDES + " 2>&1";
    char buf[512];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        outLog = "Failed to launch g++";
        return false;
    }
    while (fgets(buf, sizeof(buf), pipe)) outLog += buf;
    int status = pclose(pipe);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string SoPathFor(const std::string& cppPath) {
    return (std::filesystem::path("build-scripts") /
            (std::filesystem::path(cppPath).stem().string() + ".so")).string();
}

// .so новее исходника (и обёртки, которая генерируется при компиляции) — g++ не нужен
bool SoUpToDate(const std::string& cppPath) {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string so = SoPathFor(cppPath);
    if (!fs::exists(so, ec)) return false;
    auto soTime = fs::last_write_time(so, ec);
    if (ec) return false;
    auto srcTime = fs::last_write_time(cppPath, ec);
    if (ec) return false;
    return soTime >= srcTime;
}

bool OpenScript(const std::string& soPath, const std::string& cppPath) {
    void* lib = dlopen(soPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        g_Errors.push_back(cppPath + ": dlopen: " + dlerror());
        return false;
    }
    dlerror();
    auto factory = reinterpret_cast<Script* (*)()>(dlsym(lib, kScriptEntrySymbol));
    if (!factory) {
        g_Errors.push_back(cppPath + ": нет точки входа CreateGameScript — добавьте макрос SCRIPT_ENTRY(Класс)");
        dlclose(lib);
        return false;
    }
    g_Libs[cppPath] = { lib, factory };
    return true;
}

void DestroyInstance(uint32_t id) {
    auto it = g_Instances.find(id);
    if (it == g_Instances.end()) return;
    it->second.script->OnDestroy();
    delete it->second.script;
    g_Instances.erase(it);
}

void LoadPath(const std::string& cppPath) {
    if (g_Libs.count(cppPath) || g_Failed.count(cppPath)) return;
    std::string so = SoPathFor(cppPath);
    if (!SoUpToDate(cppPath)) {
        std::string log;
        if (!CompileScript(cppPath, so, log)) {
            g_Failed.insert(cppPath);
            g_Errors.push_back(cppPath + ":\n" + log);
            std::cerr << "[Script compile error] " << cppPath << "\n" << log << "\n";
            return;
        }
    }
    OpenScript(so, cppPath);
}

} // namespace

// ==== Реализации Script/Time для скриптов (символы экспортируются бинарником) ====

Entity* Script::Owner() {
    return FindById(ownerId);
}

::SceneManager* Script::Scene() {
    return g_Scene;
}

void Script::Translate(const glm::vec2& localDelta) {
    if (Entity* e = Owner()) e->transform.position += localDelta;
}

void Script::SetWorldPosition(const glm::vec2& world) {
    if (!g_Scene) return;
    if (Entity* e = Owner())
        e->transform.position = Transforms::WorldToLocalPoint(g_Scene->GetEntities(), *e, world);
}

glm::vec2 Script::WorldPosition() const {
    if (!g_Scene) return glm::vec2(0.0f);
    const Entity* e = FindById(ownerId);
    return e ? Transforms::WorldPosition(g_Scene->GetEntities(), *e) : glm::vec2(0.0f);
}

float Script::AngleTo(const glm::vec2& worldPoint) const {
    glm::vec2 d = worldPoint - WorldPosition();
    if (glm::length(d) < 1e-6f) return 0.0f;
    return glm::degrees(std::atan2(d.y, d.x));
}

void Script::LookAt(const glm::vec2& worldPoint) {
    if (Entity* e = Owner()) e->transform.rotation = AngleTo(worldPoint) - 90.0f;
}

float Time::Delta() { return g_Delta; }
float Time::UnscaledDelta() { return g_UnscaledDelta; }
float Time::SinceStart() { return g_Elapsed; }
float Time::TimeScale() { return g_TimeScale; }
void Time::SetTimeScale(float s) { g_TimeScale = s; }

void Scripting::SetDefaultTimeScale(float s) {
    g_DefaultTimeScale = s;
    g_TimeScale = s;
}
float Scripting::DefaultTimeScale() { return g_DefaultTimeScale; }

bool Scripting::PrecompileScript(const std::string& cppPath, std::string& outSoPath,
                                 std::string& outError) {
    outError.clear();
    return CompileScript(cppPath, outSoPath, outError);
}

bool Scripting::ConsumeSceneChange(std::string& outPath) {
    if (g_PendingScene.empty()) return false;
    outPath = g_PendingScene;
    g_PendingScene.clear();
    return true;
}

// ==== Хелперы скриптов (экспортируются бинарником) ====
void Log(const std::string& message) {
    ConsoleLog::Push(message, false);
}

bool DestroyEntity(uint32_t id) {
    if (!g_Scene) return false;
    bool ok = g_Scene->RemoveEntityById(id);
    if (ok) ConsoleLog::Push("[Script] DestroyEntity(" + std::to_string(id) + ")", false);
    return ok;
}

void Script::DefineVar(const char* name, float defaultValue) {
    Entity* e = Owner();
    if (!e || !name) return;
    if (!e->vars.count(name)) e->vars[name] = defaultValue;
}

float Script::GetVar(const char* name, float fallback) const {
    const Entity* e = FindById(ownerId);
    if (!e || !name) return fallback;
    auto it = e->vars.find(name);
    return it != e->vars.end() ? it->second : fallback;
}

void Script::SetVar(const char* name, float value) {
    Entity* e = FindById(ownerId);
    if (!e || !name) return;
    e->vars[name] = value;
}

void LoadScene(const std::string& scenePath) {
    g_PendingScene = scenePath;
}

void Scripting::SetScene(SceneManager* sm) {
    g_Scene = sm;
}

void Scripting::LoadForScene(const std::vector<Entity>& entities) {
    g_Errors.clear();
    g_Failed.clear();
    std::unordered_set<std::string> paths;
    for (const auto& e : entities) {
        if (!e.scriptPath.empty()) paths.insert(e.scriptPath);
    }
    for (const auto& p : paths) LoadPath(p);
}

void Scripting::SyncInstances(const std::vector<Entity>& entities) {
    // Протухшие: сущность удалена, скрипт снят или путь сменился
    std::vector<uint32_t> stale;
    for (auto& [id, inst] : g_Instances) {
        Entity* e = FindById(id);
        if (!e || e->scriptPath != inst.path) stale.push_back(id);
    }
    for (uint32_t id : stale) DestroyInstance(id);

    for (const auto& e : entities) {
        if (e.scriptPath.empty()) continue;
        if (g_Instances.count(e.id)) continue;
        if (!g_Libs.count(e.scriptPath)) {
            // путь мог появиться уже в Play (например, инстансом префаба)
            LoadPath(e.scriptPath);
        }
        Script* s = CreateInstance(e.scriptPath, e.id);
        if (!s) continue;
        g_Instances[e.id] = { s, e.scriptPath };
        s->Start();
    }
}

void Scripting::Update(float dt, const std::vector<Entity>& entities) {
    (void)entities;
    g_UnscaledDelta = dt;
    g_Delta = dt * g_TimeScale;
    g_Elapsed += g_Delta;
    for (auto& [id, inst] : g_Instances) {
        Entity* e = FindById(id);
        if (!e || !e->active) continue;
        inst.script->Update(g_Delta);
    }

    if (Physics::ConsumeEvents()) {
        for (const auto& ev : Physics::GetEvents()) {
            for (int side = 0; side < 2; side++) {
                uint32_t selfId = side == 0 ? ev.entityA : ev.entityB;
                uint32_t otherId = side == 0 ? ev.entityB : ev.entityA;
                auto it = g_Instances.find(selfId);
                if (it == g_Instances.end() || !it->second.script) continue;
                Script* s = it->second.script;
                switch (ev.type) {
                    case PhysicsEventType::TriggerEnter: s->OnTriggerEnter(otherId); break;
                    case PhysicsEventType::TriggerExit: s->OnTriggerExit(otherId); break;
                    case PhysicsEventType::Collision: s->OnCollisionEnter(otherId); break;
                }
            }
        }
    }
}

void Scripting::Unload() {
    std::vector<uint32_t> ids;
    ids.reserve(g_Instances.size());
    for (auto& [id, inst] : g_Instances) ids.push_back(id);
    for (uint32_t id : ids) DestroyInstance(id);

    for (auto& [path, lib] : g_Libs) {
        if (lib.lib) dlclose(lib.lib);
    }
    g_Libs.clear();
    g_Elapsed = 0.0f;
    g_TimeScale = g_DefaultTimeScale;
}

Script* Scripting::InstanceFor(uint32_t entityId) {
    auto it = g_Instances.find(entityId);
    return it != g_Instances.end() ? it->second.script : nullptr;
}

const std::vector<std::string>& Scripting::Errors() {
    return g_Errors;
}
