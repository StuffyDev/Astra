#include "core/Scripting.h"
#include "scripts/ScriptAPI.h"
#include "ecs/SceneManager.h"
#include "ecs/Transforms.h"
#include "ecs/Physics.h"
#include "ecs/Physics3D.h"
#include "utils/ConsoleLog.h"
#include "core/SceneSerializer.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <memory>

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
float g_ShakeAmp = 0.0f, g_ShakeLeft = 0.0f; // тряска камеры
bool g_QuitRequested = false;
int g_CaptureMouse = 0; // 0 = не меняли, 1 = захват, 2 = отпустить

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
    if (soTime < srcTime) return false;
    // ScriptAPI.h изменился (новый vtable/хуки) — старые .so перекомпилировать
    fs::path api = fs::path(ASTRA_SRC_DIR) / "scripts" / "ScriptAPI.h";
    auto apiTime = fs::last_write_time(api, ec);
    if (!ec && soTime < apiTime) return false;
    return true;
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
    // Билд с зашифрованными ассетами: исходника может не быть — используем готовый .so
    if (!std::filesystem::exists(cppPath) && std::filesystem::exists(so)) { OpenScript(so, cppPath); return; }
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

// ===== 3D-хелперы Script =====
glm::vec3 Script::Position3D() const {
    if (!g_Scene) return glm::vec3(0.0f);
    const Entity* e = FindById(ownerId);
    return e ? Transforms::WorldPos3(g_Scene->GetEntities(), *e) : glm::vec3(0.0f);
}

void Script::SetPosition3D(const glm::vec3& world) {
    if (!g_Scene) return;
    if (Entity* e = Owner()) e->pos3 = world - (Transforms::WorldPos3(g_Scene->GetEntities(), *e) - e->pos3);
}

void Script::Translate3D(const glm::vec3& delta) {
    if (Entity* e = Owner()) e->pos3 += delta;
}

glm::vec3 Script::Rotation3D() const {
    const Entity* e = FindById(ownerId);
    return e ? e->rot3 : glm::vec3(0.0f);
}

void Script::SetRotation3D(const glm::vec3& degrees) {
    if (Entity* e = Owner()) e->rot3 = degrees;
}

void Script::SetScale3D(const glm::vec3& scale) {
    if (Entity* e = Owner()) {
        e->scale3 = scale;
        e->transform.scale = glm::vec2(scale.x, scale.y);   // 2D-наследие не разъезжается
    }
}

glm::vec3 Script::Velocity3D() const {
    const Entity* e = FindById(ownerId);
    return e ? e->rb3.velocity : glm::vec3(0.0f);
}

void Script::SetVelocity3D(const glm::vec3& v) {
    if (Entity* e = Owner()) ::SetVelocity3D(e, v);
}

void Script::AddForce3D(const glm::vec3& impulse) {
    if (Entity* e = Owner()) ::AddForce3D(e, impulse);
}

void Script::LookAt3D(const glm::vec3& worldTarget) {
    Entity* e = Owner();
    if (!e) return;
    glm::vec3 d = worldTarget - Position3D();
    if (glm::length(d) < 1e-5f) return;
    e->rot3 = glm::vec3(-glm::degrees(std::asin(d.y / glm::length(d))),
                        glm::degrees(std::atan2(-d.x, -d.z)),
                        0.0f);
}

void Script::SetGravityEnabled3D(bool on) {
    if (Entity* e = Owner()) {
        if (!e->hasRigidbody3D) { e->hasRigidbody3D = true; e->rb3.drag = 0.0f; }
        e->rb3.useGravity = on;
    }
}

bool Script::Is3D() const {
    const Entity* e = FindById(ownerId);
    return e && e->is3D;
}

void Script::AddForceTo3D(Entity* target, const glm::vec3& impulse) { ::AddForce3D(target, impulse); }
void Script::SetVelocityOf3D(Entity* target, const glm::vec3& v) { ::SetVelocity3D(target, v); }

// ===== глобальные 3D-функции =====
glm::vec3 Gravity3D() { return Physics3D::Gravity; }

namespace {

// Габарит 3D-сущности для луча: коллайдер, иначе коробка по мешу/масштабу
bool EntityRayBounds(const std::vector<Entity>& all, const Entity& e, Physics3D::Bounds& b) {
    if (!e.active || !e.is3D) return false;
    if (e.hasCollider3D) { b = Physics3D::WorldBounds(all, e); return true; }
    b.sphere = false;
    b.center = Transforms::WorldPos3(all, e);
    b.half = Transforms::RotatedBoxHalf(Transforms::Rotation3Mat(e.rot3), glm::vec3(0.5f) * e.scale3);
    b.radius = 0.5f * std::max({e.scale3.x, e.scale3.y, e.scale3.z});
    return true;
}

// Пересечение луча с AABB; возвращает расстояние до входа (или -1)
float RayVsAABB(const glm::vec3& o, const glm::vec3& d, const glm::vec3& mn, const glm::vec3& mx) {
    float tmin = 0.0f, tmax = 1e18f;
    for (int i = 0; i < 3; i++) {
        float oi = (&o.x)[i], di = (&d.x)[i], lo = (&mn.x)[i], hi = (&mx.x)[i];
        if (std::fabs(di) < 1e-8f) {
            if (oi < lo || oi > hi) return -1.0f;
            continue;
        }
        float t1 = (lo - oi) / di, t2 = (hi - oi) / di;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return -1.0f;
    }
    return tmin;
}

int Raycast3DImpl(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
                  RayHit3D* hits, int maxHits) {
    if (!g_Scene || maxHits <= 0) return 0;
    auto& all = g_Scene->GetEntities();
    glm::vec3 nd = glm::length(dir) > 1e-6f ? glm::normalize(dir) : glm::vec3(0.0f, 0.0f, -1.0f);

    struct Cand { float t; int idx; glm::vec3 n; };
    std::vector<Cand> found;
    for (size_t i = 0; i < all.size(); i++) {
        Physics3D::Bounds b;
        if (!EntityRayBounds(all, all[i], b)) continue;
        float t = -1.0f;
        glm::vec3 nrm(0.0f, 1.0f, 0.0f);
        if (b.sphere) {
            glm::vec3 oc = origin - b.center;
            float bq = glm::dot(oc, nd);
            float c = glm::dot(oc, oc) - b.radius * b.radius;
            float disc = bq * bq - c;
            if (disc < 0.0f) continue;
            t = -bq - std::sqrt(disc);
            if (t < 0.0f) t = 0.0f;
            nrm = glm::normalize(origin + nd * t - b.center);
        } else {
            t = RayVsAABB(origin, nd, b.center - b.half, b.center + b.half);
            if (t < 0.0f) continue;
            // нормаль — по оси, где луч вошёл в коробку
            glm::vec3 hitP = origin + nd * t;
            glm::vec3 rel = hitP - b.center;
            float best = -1.0f;
            for (int k = 0; k < 3; k++) {
                float v = std::fabs((&rel.x)[k]) / std::max((&b.half.x)[k], 1e-5f);
                if (v > best) { best = v; nrm = glm::vec3(0.0f); (&nrm.x)[k] = (&rel.x)[k] >= 0.0f ? 1.0f : -1.0f; }
            }
        }
        if (t > maxDist) continue;
        found.push_back({ t, (int)i, nrm });
    }
    std::sort(found.begin(), found.end(), [](const Cand& a, const Cand& b) { return a.t < b.t; });
    int n = 0;
    for (const auto& c : found) {
        if (n >= maxHits) break;
        const Entity& e = all[c.idx];
        hits[n].entityId = e.id;
        hits[n].name = e.name;
        hits[n].distance = c.t;
        hits[n].point = origin + nd * c.t;
        hits[n].normal = c.n;
        n++;
    }
    return n;
}

} // namespace

bool Raycast3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D& outHit) {
    return Raycast3DImpl(origin, dir, maxDist, &outHit, 1) == 1;
}

int RaycastAll3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist,
                 RayHit3D* outHits, int maxHits) {
    if (!outHits || maxHits <= 0) return 0;
    for (int i = 0; i < maxHits; i++) outHits[i] = RayHit3D();
    return Raycast3DImpl(origin, dir, maxDist, outHits, maxHits);
}

uint32_t InstantiatePrefab3D(const std::string& prefabPath, const glm::vec3& pos) {
    if (!g_Scene) return 0;
    std::vector<Entity> protos;
    if (!SceneSerializer::LoadEntities(prefabPath, protos)) return 0;
    for (auto& p : protos) {
        p.is3D = true;
        p.pos3 += pos;
        p.transform.position += glm::vec2(pos.x, pos.y);
    }
    int idx = g_Scene->InstantiateProtos(protos, glm::vec2(0.0f));
    if (idx < 0) return 0;
    return g_Scene->GetEntities()[(size_t)idx].id;
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

glm::vec2 Scripting::ShakeOffset() {
    if (g_ShakeLeft <= 0.0f) return glm::vec2(0.0f);
    float k = g_ShakeAmp * (g_ShakeLeft); // линейное затухание
    static uint32_t seed = 0x9E3779B9u;
    seed = seed * 1664525u + 1013904223u;
    float a = ((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    seed = seed * 1664525u + 1013904223u;
    float b = ((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    return glm::vec2(a, b) * k;
}

bool Scripting::QuitRequested() { return g_QuitRequested; }
void Scripting::ClearQuit() { g_QuitRequested = false; }
int Scripting::CaptureMouseState() { int s = g_CaptureMouse; g_CaptureMouse = 0; return s; }

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

void QuitGame() { g_QuitRequested = true; }
void CaptureMouse(bool on) { g_CaptureMouse = on ? 1 : 2; }
bool IsMouseCaptured() { return g_CaptureMouse == 1; }

// Спавн инстанса префаба в мировой точке; возвращает id корня (0 — ошибка)
uint32_t InstantiatePrefab(const std::string& prefabPath, const glm::vec2& worldPos) {
    if (!g_Scene) return 0;
    std::vector<Entity> protos;
    if (!SceneSerializer::LoadEntities(prefabPath, protos) || protos.empty()) return 0;
    int idx = g_Scene->InstantiateProtos(protos, worldPos - protos[0].transform.position);
    if (idx < 0) return 0;
    return g_Scene->GetEntities()[static_cast<size_t>(idx)].id;
}

void ShakeCamera(float amplitude, float duration) {
    g_ShakeAmp = std::max(g_ShakeAmp, amplitude);
    g_ShakeLeft = std::max(g_ShakeLeft, duration);
}

void EmitParticles(Entity* e, int count) {
    if (!e || !g_Scene) return;
    ParticleEmitter& em = e->emitter;
    em.active = true;
    glm::vec2 origin = Transforms::WorldPosition(g_Scene->GetEntities(), *e);
    static std::mt19937 rng{std::random_device{}()};
    auto u01 = [&]() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng); };
    for (int i = 0; i < count && (int)e->particles.size() < em.maxCount; i++) {
        float ang = glm::radians(em.angleMin + u01() * (em.angleMax - em.angleMin));
        float spd = em.speedMin + u01() * (em.speedMax - em.speedMin);
        Particle pt;
        pt.position = origin;
        pt.velocity = glm::vec2(std::cos(ang), std::sin(ang)) * spd;
        pt.life = em.lifeMin + u01() * (em.lifeMax - em.lifeMin);
        pt.size = em.sizeMin + u01() * (em.sizeMax - em.sizeMin);
        e->particles.push_back(pt);
    }
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

void Scripting::DispatchAnimEvent(uint32_t entityId, const char* name) {
    auto it = g_Instances.find(entityId);
    if (it != g_Instances.end() && it->second.script) it->second.script->OnAnimEvent(name);
}

void Scripting::Update(float dt, const std::vector<Entity>& entities) {
    if (g_ShakeLeft > 0.0f) {
        g_ShakeLeft -= dt;
        if (g_ShakeLeft <= 0.0f) { g_ShakeLeft = 0.0f; g_ShakeAmp = 0.0f; }
    }
    (void)entities;
    g_UnscaledDelta = dt;
    g_Delta = dt * g_TimeScale;
    g_Elapsed += g_Delta;
    for (auto& [id, inst] : g_Instances) {
        Entity* e = FindById(id);
        if (!e || !e->active) continue;
        inst.script->Update(g_Delta);
    }

    auto dispatch = [](const std::vector<PhysicsEvent>& events) {
        for (const auto& ev : events) {
            for (int side = 0; side < 2; side++) {
                uint32_t selfId = side == 0 ? ev.entityA : ev.entityB;
                uint32_t otherId = side == 0 ? ev.entityB : ev.entityA;
                auto it = g_Instances.find(selfId);
                if (it == g_Instances.end() || !it->second.script) continue;
                Script* sc = it->second.script;
                switch (ev.type) {
                    case PhysicsEventType::TriggerEnter: sc->OnTriggerEnter(otherId); break;
                    case PhysicsEventType::TriggerExit: sc->OnTriggerExit(otherId); break;
                    case PhysicsEventType::Collision: sc->OnCollisionEnter(otherId); break;
                }
            }
        }
    };
    if (Physics::ConsumeEvents()) dispatch(Physics::GetEvents());
    if (Physics3D::ConsumeEvents()) dispatch(Physics3D::GetEvents());
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
