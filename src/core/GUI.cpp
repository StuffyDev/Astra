#include "core/GUI.h"
#include "core/Application.h" // ctx.app->Undo/Redo (Application.h сам включает GUI.h — цикла нет)
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/Renderer.h"
#include "core/Prefs.h"
#include "core/ProjectManager.h"
#include "core/GameUI.h"
#include "core/Audio.h"
#include "core/Input.h"
#include "core/Scripting.h"
#include "utils/ConsoleLog.h"
#include "utils/AssetIO.h"
#include "ecs/SceneManager.h"
#include "ecs/Entity.h"
#include "ecs/Transforms.h"
#include "ecs/Physics.h"
#include "scripts/ScriptAPI.h"
#include <imgui.h>
#include "misc/cpp/imgui_stdlib.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace fs = std::filesystem;

static fs::path GuiHomeDir() {
    if (const char* home = std::getenv("HOME")) return home;
    return fs::current_path();
}

static std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(c));
    return s;
}

static std::string ExtLower(const fs::path& p) {
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    return ext;
}

static bool IsImageExt(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}

static bool IsAudioExt(const std::string& ext) {
    return ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac";
}

static bool IsCodeExt(const std::string& ext) {
    return ext == ".cpp" || ext == ".h" || ext == ".txt" || ext == ".json" ||
           ext == ".vert" || ext == ".frag" || ext == ".scene" || ext == ".prefab" || ext == ".md";
}

// Скан DefineVar("name", default) из исходника скрипта — чтобы ползунки были видно
// и в Edit-режиме, до первого запуска Start() (как [SerializeField] в Unity)
static void DiscoverScriptVars(const std::string& path,
                               std::vector<std::pair<std::string, float>>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return;
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string src = ss.str();
    const std::string key = "DefineVar(";
    size_t pos = 0;
    while ((pos = src.find(key, pos)) != std::string::npos) {
        size_t q1 = src.find('"', pos);
        size_t q2 = (q1 == std::string::npos) ? std::string::npos : src.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string name = src.substr(q1 + 1, q2 - q1 - 1);
        float def = 0.0f;
        size_t comma = src.find(',', q2);
        if (comma != std::string::npos) def = std::strtof(src.c_str() + comma + 1, nullptr);
        if (!name.empty()) out.emplace_back(std::move(name), def);
        pos = q2 + 1;
    }
}

// ===== Системные векторные иконки (зашиты в код, без ассетов) =====
enum SysIcon { SysIcon_File, SysIcon_Folder, SysIcon_Image, SysIcon_Script,
               SysIcon_Shader, SysIcon_Scene, SysIcon_Prefab, SysIcon_Audio };

static SysIcon IconForExt(const std::string& ext, bool isDir) {
    if (isDir) return SysIcon_Folder;
    if (IsImageExt(ext)) return SysIcon_Image;
    if (ext == ".cpp" || ext == ".h") return SysIcon_Script;
    if (ext == ".vert" || ext == ".frag") return SysIcon_Shader;
    if (ext == ".scene") return SysIcon_Scene;
    if (ext == ".prefab") return SysIcon_Prefab;
    if (IsAudioExt(ext)) return SysIcon_Audio;
    return SysIcon_File;
}

static void DrawSysIcon(ImDrawList* dl, ImVec2 c, float s, SysIcon kind) {
    const float h = s * 0.5f;
    auto col = [](int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); };
    ImVec2 a(c.x - h, c.y - h), b(c.x + h, c.y + h);
    switch (kind) {
        case SysIcon_Folder: {
            dl->AddRectFilled(ImVec2(a.x, a.y + s * 0.18f), b, col(224, 164, 64), 3.0f);
            dl->AddRectFilled(ImVec2(a.x, a.y + s * 0.06f), ImVec2(c.x + s * 0.1f, a.y + s * 0.28f),
                              col(224, 164, 64), 2.0f);
            dl->AddRect(ImVec2(a.x, a.y + s * 0.18f), b, col(140, 96, 24), 3.0f);
            dl->AddLine(ImVec2(a.x + 2, c.y), ImVec2(b.x - 2, c.y), col(255, 214, 140), 1.0f);
        } break;
        case SysIcon_Image: {
            dl->AddRectFilled(a, b, col(235, 235, 240), 3.0f);
            dl->AddRect(a, b, col(120, 120, 135), 3.0f);
            dl->AddCircleFilled(ImVec2(c.x - s * 0.25f, c.y - s * 0.22f), s * 0.12f, col(240, 200, 80));
            const ImVec2 tri[3] = { ImVec2(a.x + 2, b.y - 2), ImVec2(c.x + s * 0.1f, c.y + s * 0.05f),
                                    ImVec2(b.x - 2, b.y - 2) };
            dl->AddConvexPolyFilled(tri, 3, col(90, 170, 110));
        } break;
        case SysIcon_Script: {
            dl->AddRectFilled(a, b, col(235, 235, 240), 3.0f);
            dl->AddRect(a, b, col(120, 120, 135), 3.0f);
            ImU32 ink = col(70, 130, 210);
            dl->AddLine(ImVec2(c.x - s * 0.12f, c.y - s * 0.28f), ImVec2(c.x - s * 0.34f, c.y), ink, 2.0f);
            dl->AddLine(ImVec2(c.x - s * 0.34f, c.y), ImVec2(c.x - s * 0.12f, c.y + s * 0.28f), ink, 2.0f);
            dl->AddLine(ImVec2(c.x + s * 0.12f, c.y - s * 0.28f), ImVec2(c.x + s * 0.34f, c.y), ink, 2.0f);
            dl->AddLine(ImVec2(c.x + s * 0.34f, c.y), ImVec2(c.x + s * 0.12f, c.y + s * 0.28f), ink, 2.0f);
        } break;
        case SysIcon_Shader: {
            dl->AddRectFilled(a, b, col(235, 235, 240), 3.0f);
            dl->AddRect(a, b, col(120, 120, 135), 3.0f);
            dl->PathArcTo(ImVec2(c.x, c.y + s * 0.15f), s * 0.32f, 3.14159f, 0.0f, 16);
            dl->PathFillConvex(col(150, 95, 205));
            dl->AddCircle(ImVec2(c.x, c.y + s * 0.15f), s * 0.32f, col(90, 50, 140), 16, 1.0f);
            dl->AddLine(ImVec2(c.x - s * 0.32f, c.y + s * 0.15f), ImVec2(c.x + s * 0.32f, c.y + s * 0.15f),
                        col(90, 50, 140), 1.0f);
        } break;
        case SysIcon_Scene: {
            const ImVec2 top[4] = { ImVec2(c.x, a.y), b, ImVec2(c.x, c.y), a };
            dl->AddConvexPolyFilled(top, 4, col(90, 180, 190));
            const ImVec2 left[4] = { a, ImVec2(c.x, c.y), ImVec2(c.x, b.y), ImVec2(a.x, c.y + h * 0.5f) };
            dl->AddConvexPolyFilled(left, 4, col(50, 130, 140));
            const ImVec2 right[4] = { ImVec2(c.x, c.y), ImVec2(b.x, c.y + h * 0.5f), ImVec2(c.x, b.y), ImVec2(c.x, c.y) };
            dl->AddConvexPolyFilled(right, 4, col(70, 155, 165));
            dl->AddPolyline(&top[0], 4, col(30, 90, 100), ImDrawFlags_Closed, 1.0f);
            dl->AddLine(ImVec2(c.x, c.y), ImVec2(c.x, b.y), col(30, 90, 100), 1.0f);
        } break;
        case SysIcon_Prefab: {
            const ImVec2 dia[4] = { ImVec2(c.x, a.y), ImVec2(b.x, c.y), ImVec2(c.x, b.y), ImVec2(a.x, c.y) };
            dl->AddConvexPolyFilled(dia, 4, col(120, 100, 220));
            dl->AddPolyline(dia, 4, col(60, 45, 140), ImDrawFlags_Closed, 1.0f);
            dl->AddLine(ImVec2(c.x - s * 0.14f, c.y), ImVec2(c.x + s * 0.14f, c.y), col(255, 255, 255), 2.0f);
            dl->AddLine(ImVec2(c.x, c.y - s * 0.14f), ImVec2(c.x, c.y + s * 0.14f), col(255, 255, 255), 2.0f);
        } break;
        case SysIcon_Audio: {
            dl->AddRectFilled(ImVec2(a.x + s * 0.12f, c.y - s * 0.12f), ImVec2(a.x + s * 0.3f, c.y + s * 0.12f),
                              col(70, 70, 85));
            const ImVec2 tri[3] = { ImVec2(a.x + s * 0.3f, c.y - s * 0.12f), ImVec2(a.x + s * 0.5f, c.y - s * 0.3f),
                                    ImVec2(a.x + s * 0.5f, c.y + s * 0.3f) };
            dl->AddConvexPolyFilled(tri, 3, col(70, 70, 85));
            dl->PathArcTo(ImVec2(a.x + s * 0.5f, c.y), s * 0.28f, -0.9f, 0.9f, 10);
            dl->PathStroke(col(225, 110, 110), 0, 2.0f);
            dl->PathArcTo(ImVec2(a.x + s * 0.5f, c.y), s * 0.45f, -0.8f, 0.8f, 10);
            dl->PathStroke(col(225, 110, 110), 0, 2.0f);
        } break;
        default: {
            dl->AddRectFilled(ImVec2(a.x + s * 0.1f, a.y), ImVec2(b.x - s * 0.25f, b.y), col(235, 235, 240), 2.0f);
            dl->AddRect(ImVec2(a.x + s * 0.1f, a.y), ImVec2(b.x - s * 0.25f, b.y), col(120, 120, 135), 2.0f);
            dl->AddTriangleFilled(ImVec2(b.x - s * 0.25f, c.y), ImVec2(b.x - s * 0.25f, b.y),
                                  ImVec2(b.x - s * 0.05f, b.y - s * 0.25f), col(200, 200, 210));
        }
    }
}

static const char* AssetIcon(const std::string& ext) {
    if (IsImageExt(ext)) return "[img]";
    if (ext == ".vert" || ext == ".frag") return "[shd]";
    if (ext == ".scene") return "[scn]";
    if (ext == ".prefab") return "[prf]";
    if (ext == ".cpp" || ext == ".h") return "[src]";
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") return "[snd]";
    return "[---]";
}

// Уникальный путь внутри папки: name.ext, name_1.ext, ...
static fs::path UniquePath(const fs::path& dir, const std::string& name, const std::string& ext) {
    fs::path p = dir / (name + ext);
    int i = 1;
    while (fs::exists(p)) p = dir / (name + "_" + std::to_string(i++) + ext);
    return p;
}

// ===== dirty-подпись сцены: FNV-1a по всем значимым полям =====
static void FnvUpdate(uint64_t& h, const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
}

static void FnvStr(uint64_t& h, const std::string& s) {
    FnvUpdate(h, s.data(), s.size());
    h ^= 0xFF; // разделитель строк
}

static uint64_t ComputeSignatureFor(const std::vector<Entity>& ents) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const Entity& e : ents) {
        FnvUpdate(h, &e.id, sizeof(e.id));
        FnvUpdate(h, &e.parentId, sizeof(e.parentId));
        FnvUpdate(h, &e.active, sizeof(e.active));
        FnvStr(h, e.name);
        FnvStr(h, e.prefabSource);
        FnvUpdate(h, &e.transform, sizeof(e.transform));
        FnvUpdate(h, &e.is3D, sizeof(e.is3D));
        if (e.is3D) {
            FnvUpdate(h, &e.pos3, sizeof(e.pos3));
            FnvUpdate(h, &e.rot3, sizeof(e.rot3));
            FnvUpdate(h, &e.scale3, sizeof(e.scale3));
            FnvUpdate(h, &e.mesh.type, sizeof(int));
            FnvStr(h, e.mesh.meshPath);
            FnvStr(h, e.mesh.texturePath);
            FnvUpdate(h, &e.mesh.color, sizeof(e.mesh.color));
        }
        FnvUpdate(h, &e.hasRigidbody, sizeof(e.hasRigidbody));
        FnvUpdate(h, &e.hasCollider, sizeof(e.hasCollider));
        FnvUpdate(h, &e.hasAudio, sizeof(e.hasAudio));
        FnvUpdate(h, &e.hasScript, sizeof(e.hasScript));
        FnvUpdate(h, &e.hasParticles, sizeof(e.hasParticles));
        FnvUpdate(h, &e.sprite.type, sizeof(e.sprite.type));
        FnvUpdate(h, &e.sprite.color, sizeof(e.sprite.color));
        FnvStr(h, e.sprite.texturePath);
        FnvStr(h, e.sprite.shaderPath);
        FnvUpdate(h, &e.sprite.sortingOrder, sizeof(e.sprite.sortingOrder));
        FnvUpdate(h, &e.sprite.materialParams, sizeof(e.sprite.materialParams));
        FnvUpdate(h, &e.sprite.materialColor, sizeof(e.sprite.materialColor));
        FnvUpdate(h, &e.animation.active, sizeof(e.animation.active));
        FnvStr(h, e.animation.texturePath);
        FnvUpdate(h, &e.animation.cols, sizeof(e.animation.cols));
        FnvUpdate(h, &e.animation.rows, sizeof(e.animation.rows));
        FnvUpdate(h, &e.animation.fps, sizeof(e.animation.fps));
        FnvUpdate(h, &e.animation.loop, sizeof(e.animation.loop));
        FnvUpdate(h, &e.animation.playOnAwake, sizeof(e.animation.playOnAwake));
        FnvStr(h, e.scriptPath);
        for (const auto& [varName, varValue] : e.vars) {
            FnvStr(h, varName);
            FnvUpdate(h, &varValue, sizeof(varValue));
        }
        FnvStr(h, e.audio.path);
        FnvUpdate(h, &e.audio.volume, sizeof(e.audio.volume));
        FnvUpdate(h, &e.audio.pitch, sizeof(e.audio.pitch));
        FnvUpdate(h, &e.audio.loop, sizeof(e.audio.loop));
        FnvUpdate(h, &e.audio.playOnAwake, sizeof(e.audio.playOnAwake));
        FnvUpdate(h, &e.audio.group, sizeof(e.audio.group));
        for (const AnimEvent& ev : e.animation.events) {
            FnvUpdate(h, &ev.clip, sizeof(int) * 2);
            FnvStr(h, ev.name);
        }
        FnvUpdate(h, &e.hasTilemap, sizeof(e.hasTilemap));
        if (e.hasTilemap) {
            FnvStr(h, e.tilemap.texturePath);
            FnvUpdate(h, &e.tilemap.tileW, sizeof(int) * 5);
            FnvUpdate(h, e.tilemap.cells.data(), e.tilemap.cells.size() * sizeof(int));
            FnvUpdate(h, &e.tilemap.color, sizeof(e.tilemap.color));
            FnvUpdate(h, &e.tilemap.sortingOrder, sizeof(e.tilemap.sortingOrder));
        }
        FnvUpdate(h, &e.emitter.active, sizeof(e.emitter.active));
        FnvStr(h, e.emitter.texturePath);
        FnvUpdate(h, &e.emitter.maxCount, sizeof(int) + sizeof(float));
        FnvUpdate(h, &e.emitter.lifeMin, sizeof(float) * 8);
        FnvUpdate(h, &e.emitter.colorStart, sizeof(e.emitter.colorStart));
        FnvUpdate(h, &e.emitter.colorEnd, sizeof(e.emitter.colorEnd));
        for (const auto& c : e.animation.clips) {
            FnvStr(h, c.name);
            FnvUpdate(h, &c.first, sizeof(int) * 2);
            FnvUpdate(h, &c.fps, sizeof(float));
        }
        FnvUpdate(h, &e.animation.activeClip, sizeof(e.animation.activeClip));
        FnvUpdate(h, &e.rigidbody, sizeof(e.rigidbody));
        FnvUpdate(h, &e.collider, sizeof(e.collider));
        FnvUpdate(h, &e.hasCamera, sizeof(e.hasCamera));
        if (e.hasCamera) FnvUpdate(h, &e.camera, sizeof(e.camera));
        FnvUpdate(h, &e.hasUI, sizeof(e.hasUI));
        if (e.hasUI) {
            FnvUpdate(h, &e.ui.kind, sizeof(e.ui.kind));
            FnvStr(h, e.ui.label);
            FnvUpdate(h, &e.ui.minValue, sizeof(e.ui.minValue));
            FnvUpdate(h, &e.ui.maxValue, sizeof(e.ui.maxValue));
            FnvUpdate(h, &e.ui.value, sizeof(e.ui.value));
            FnvUpdate(h, &e.ui.interactable, sizeof(e.ui.interactable));
            FnvUpdate(h, &e.ui.textColor, 3 * sizeof(float));
            FnvUpdate(h, &e.ui.bgColor, 3 * sizeof(float));
            FnvUpdate(h, &e.ui.fontScale, sizeof(e.ui.fontScale));
        }
    }
    return h;
}

static uint64_t ComputeSceneSignature(SceneManager* sm) {
    return ComputeSignatureFor(sm->GetEntities());
}

// Хлебные крошки от корня rootDir (например "assets") до path
static void RenderBreadcrumb(std::string& path, const char* rootDir) {
    if (ImGui::SmallButton("Assets")) path = rootDir;
    if (!fs::exists(rootDir)) return;

    std::string rel;
    std::error_code ec;
    rel = fs::path(path).lexically_relative(rootDir).string();
    if (rel == "." || ec) rel.clear();

    std::string acc = rootDir;
    size_t pos = 0;
    while (pos < rel.size()) {
        size_t sep = rel.find('/', pos);
        if (sep == std::string::npos) sep = rel.size();
        std::string seg = rel.substr(pos, sep - pos);
        acc += "/" + seg;
        ImGui::SameLine();
        if (ImGui::SmallButton(seg.c_str())) path = acc;
        pos = sep + 1;
        if (pos < rel.size() + 1 && sep < rel.size()) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
        }
    }
}

GUI::GUI(GLFWwindow* window) : m_Window(window) {
    m_GameCamera = std::make_unique<Camera>(16.0f / 9.0f);
}

GUI::~GUI() { Shutdown(); }

static fs::path AstraConfigPath() {
    return GuiHomeDir() / ".astra" / "config.ini";
}

void GUI::LoadUserSettings() {
    std::ifstream f(AstraConfigPath());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto num = [&]() { return std::atof(v.c_str()); };
        if (k == "light_theme") AstraPrefs::LightTheme = num() != 0;
        else if (k == "clear") sscanf(v.c_str(), "%f %f %f", &AstraPrefs::ClearColor.r, &AstraPrefs::ClearColor.g, &AstraPrefs::ClearColor.b);
        else if (k == "show_grid") AstraPrefs::ShowGrid = num() != 0;
        else if (k == "show_colliders") AstraPrefs::ShowColliders = num() != 0;
        else if (k == "grid_size") AstraPrefs::GridSize = (float)num();
        else if (k == "snap_deg") AstraPrefs::SnapDegrees = (float)num();
        else if (k == "gravity") sscanf(v.c_str(), "%f %f", &Physics::Gravity.x, &Physics::Gravity.y);
        else if (k == "ppm") Physics::PixelsPerMeter = std::max(1.0f, (float)num());
        else if (k == "master_volume") Audio::SetMasterVolume((float)num());
        else if (k == "muted") Audio::SetMuted(num() != 0);
        else if (k == "time_scale") Scripting::SetDefaultTimeScale((float)num());
        else if (k == "light_dir") sscanf(v.c_str(), "%f %f %f", &AstraPrefs::LightDir.x, &AstraPrefs::LightDir.y, &AstraPrefs::LightDir.z);
        else if (k == "light_color") sscanf(v.c_str(), "%f %f %f", &AstraPrefs::LightColor.x, &AstraPrefs::LightColor.y, &AstraPrefs::LightColor.z);
        else if (k == "ambient") AstraPrefs::Ambient = (float)num();
    }
}

void GUI::SaveUserSettings() {
    std::error_code ec;
    fs::path cfg = AstraConfigPath();
    fs::create_directories(cfg.parent_path(), ec);
    std::ofstream f(cfg);
    if (!f.is_open()) return;
    f << "light_theme=" << (AstraPrefs::LightTheme ? 1 : 0) << "\n"
      << "clear=" << AstraPrefs::ClearColor.r << " " << AstraPrefs::ClearColor.g << " " << AstraPrefs::ClearColor.b << "\n"
      << "show_grid=" << (AstraPrefs::ShowGrid ? 1 : 0) << "\n"
      << "show_colliders=" << (AstraPrefs::ShowColliders ? 1 : 0) << "\n"
      << "grid_size=" << AstraPrefs::GridSize << "\n"
      << "snap_deg=" << AstraPrefs::SnapDegrees << "\n"
      << "ppm=" << Physics::PixelsPerMeter << "\n"
      << "gravity=" << Physics::Gravity.x << " " << Physics::Gravity.y << "\n"
      << "master_volume=" << Audio::MasterVolume() << "\n"
      << "muted=" << (Audio::IsMuted() ? 1 : 0) << "\n"
      << "time_scale=" << Scripting::DefaultTimeScale() << "\n"
      << "light_dir=" << AstraPrefs::LightDir.x << " " << AstraPrefs::LightDir.y << " " << AstraPrefs::LightDir.z << "\n"
      << "light_color=" << AstraPrefs::LightColor.x << " " << AstraPrefs::LightColor.y << " " << AstraPrefs::LightColor.z << "\n"
      << "ambient=" << AstraPrefs::Ambient << "\n";
}

void GUI::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    const bool light = AstraPrefs::LightTheme;
    if (light) ImGui::StyleColorsLight(); else ImGui::StyleColorsDark();

    // Мягкий «macOS»: скругления, воздух, почти без рамок
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 7.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 7.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(9.0f, 5.0f);
    style.ItemSpacing = ImVec2(9.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 14.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    const ImVec4 accent = light ? ImVec4(0.90f, 0.48f, 0.10f, 1.00f)
                                : ImVec4(0.97f, 0.60f, 0.16f, 1.00f);
    ImVec4* c = style.Colors;

    if (light) {
        // "Astra Paper": светлый мягкий серый, белые поля ввода, синий-accent у тегов не нужен
        c[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.48f, 0.53f, 1.00f);
        c[ImGuiCol_WindowBg] = ImVec4(0.925f, 0.925f, 0.945f, 1.00f);
        c[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_PopupBg] = ImVec4(0.97f, 0.97f, 0.985f, 0.99f);
        c[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.93f, 0.93f, 0.96f, 1.00f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.88f, 0.88f, 0.93f, 1.00f);
        c[ImGuiCol_TitleBg] = ImVec4(0.88f, 0.88f, 0.91f, 1.00f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.93f, 0.93f, 0.96f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.86f, 0.86f, 0.89f, 1.00f);
        c[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
        c[ImGuiCol_ScrollbarBg] = ImVec4(0.925f, 0.925f, 0.945f, 0.60f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.70f, 0.70f, 0.75f, 0.70f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.58f, 0.58f, 0.66f, 0.85f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.48f, 0.48f, 0.58f, 1.00f);
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = ImVec4(0.75f, 0.36f, 0.05f, 1.00f);
        c[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.86f, 0.92f, 1.00f);
        c[ImGuiCol_ButtonActive] = accent;
        c[ImGuiCol_Header] = ImVec4(0.83f, 0.83f, 0.89f, 0.90f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.75f, 0.85f, 1.00f);
        c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
        c[ImGuiCol_SeparatorHovered] = accent;
        c[ImGuiCol_SeparatorActive] = accent;
        c[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.08f);
        c[ImGuiCol_ResizeGripHovered] = accent;
        c[ImGuiCol_ResizeGripActive] = accent;
        c[ImGuiCol_Tab] = ImVec4(0.855f, 0.855f, 0.90f, 1.00f);
        c[ImGuiCol_TabHovered] = ImVec4(0.75f, 0.75f, 0.84f, 1.00f);
        c[ImGuiCol_TabActive] = ImVec4(0.98f, 0.98f, 1.00f, 1.00f);
        c[ImGuiCol_TabUnfocused] = ImVec4(0.88f, 0.88f, 0.92f, 1.00f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.94f, 0.94f, 0.97f, 1.00f);
        c[ImGuiCol_DockingPreview] = ImVec4(0.90f, 0.48f, 0.10f, 0.55f);
        c[ImGuiCol_DockingEmptyBg] = ImVec4(0.88f, 0.88f, 0.91f, 1.00f);
        c[ImGuiCol_TextSelectedBg] = ImVec4(0.90f, 0.48f, 0.10f, 0.28f);
        c[ImGuiCol_DragDropTarget] = accent;
        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.35f, 0.35f, 0.42f, 0.35f);
    } else {
        // "Astra Slate": тёмный сине-серый, оранжевый акцент
        c[ImGuiCol_Text] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
        c[ImGuiCol_WindowBg] = ImVec4(0.126f, 0.126f, 0.137f, 1.00f);
        c[ImGuiCol_ChildBg] = ImVec4(0.118f, 0.118f, 0.128f, 1.00f);
        c[ImGuiCol_PopupBg] = ImVec4(0.140f, 0.140f, 0.155f, 0.98f);
        c[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.29f, 0.45f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg] = ImVec4(0.165f, 0.165f, 0.185f, 1.00f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
        c[ImGuiCol_TitleBg] = ImVec4(0.094f, 0.094f, 0.105f, 1.00f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.155f, 0.155f, 0.175f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
        c[ImGuiCol_MenuBarBg] = ImVec4(0.105f, 0.105f, 0.118f, 1.00f);
        c[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.34f, 0.34f, 0.38f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.42f, 0.42f, 0.47f, 1.00f);
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = ImVec4(0.85f, 0.53f, 0.15f, 1.00f);
        c[ImGuiCol_SliderGrabActive] = accent;
        c[ImGuiCol_Button] = ImVec4(0.205f, 0.205f, 0.235f, 1.00f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.29f, 0.33f, 1.00f);
        c[ImGuiCol_ButtonActive] = ImVec4(0.97f, 0.60f, 0.16f, 0.85f);
        c[ImGuiCol_Header] = ImVec4(0.195f, 0.195f, 0.225f, 1.00f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.36f, 0.41f, 1.00f);
        c[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(0.97f, 0.60f, 0.16f, 0.78f);
        c[ImGuiCol_SeparatorActive] = ImVec4(0.97f, 0.60f, 0.16f, 1.00f);
        c[ImGuiCol_ResizeGrip] = ImVec4(0.25f, 0.25f, 0.29f, 0.40f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.97f, 0.60f, 0.16f, 0.67f);
        c[ImGuiCol_ResizeGripActive] = ImVec4(0.97f, 0.60f, 0.16f, 0.95f);
        c[ImGuiCol_Tab] = ImVec4(0.135f, 0.135f, 0.150f, 1.00f);
        c[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.26f, 0.30f, 1.00f);
        c[ImGuiCol_TabActive] = ImVec4(0.205f, 0.205f, 0.235f, 1.00f);
        c[ImGuiCol_TabUnfocused] = ImVec4(0.115f, 0.115f, 0.128f, 1.00f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.155f, 0.155f, 0.175f, 1.00f);
        c[ImGuiCol_DockingPreview] = ImVec4(0.97f, 0.60f, 0.16f, 0.70f);
        c[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
        c[ImGuiCol_TextSelectedBg] = ImVec4(0.97f, 0.60f, 0.16f, 0.25f);
        c[ImGuiCol_DragDropTarget] = accent;
        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
    }
}

void GUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    LoadUserSettings();
    ApplyTheme();

    // Шрифты с кириллицей: базовый + средний/крупный для runtime UI
    {
        const char* candidates[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        };
        const char* ttf = nullptr;
        for (const char* c : candidates) {
            if (fs::exists(c)) { ttf = c; break; }
        }
        if (ttf) {
            ImFontAtlas* atlas = io.Fonts;
            const ImWchar* ranges = atlas->GetGlyphRangesCyrillic(); // включает латиницу
            atlas->AddFontFromFileTTF(ttf, 13.0f, nullptr, ranges);
            m_FontMedium = atlas->AddFontFromFileTTF(ttf, 17.0f, nullptr, ranges);
            m_FontLarge = atlas->AddFontFromFileTTF(ttf, 24.0f, nullptr, ranges);
        }
    }

    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

void GUI::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void GUI::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GUI::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::UpdateGameCamera(SceneManager* sceneManager) {
    auto& entities = sceneManager->GetEntities();
    Entity* mainCam = nullptr;
    Entity* anyCam = nullptr;
    for (auto& e : entities) {
        if (!e.active || !e.hasCamera) continue;
        if (!anyCam) anyCam = &e;
        if (e.camera.mainCamera) { mainCam = &e; break; }
    }

    Entity* chosen = mainCam ? mainCam : anyCam;
    m_HasGameCamera = chosen != nullptr;
    if (chosen) {
        // мировая позиция с учётом parent-цепочки: камера-ребёнок следует за родителем
        glm::vec2 pos = Transforms::WorldPosition(entities, *chosen) + chosen->camera.offset;
        if (chosen->camera.followTargetId != 0) {
            for (auto& e : entities) {
                if (e.id != chosen->camera.followTargetId || !e.active) continue;
                glm::vec2 target = Transforms::WorldPosition(entities, e) + chosen->camera.followOffset;
                float damp = std::max(chosen->camera.followDamping, 0.001f);
                float k = std::min(1.0f, ImGui::GetIO().DeltaTime / damp);
                pos += (target - pos) * k;
                break;
            }
        }
        if (chosen->camera.useBounds) {
            const glm::vec4& bd = chosen->camera.bounds;
            float hh = 1080.0f * chosen->camera.zoom * 0.5f;
            float hw = hh * (m_GameSize.x / std::max(m_GameSize.y, 1.0f));
            float minX = bd.x + hw, maxX = bd.x + bd.z - hw;
            float minY = bd.y + hh, maxY = bd.y + bd.w - hh;
            pos.x = (minX > maxX) ? (bd.x + bd.z * 0.5f) : std::clamp(pos.x, minX, maxX);
            pos.y = (minY > maxY) ? (bd.y + bd.w * 0.5f) : std::clamp(pos.y, minY, maxY);
        }
        pos += Scripting::ShakeOffset();
        m_GameCamera->SetPosition(pos);
        m_GameCamera->SetZoom(chosen->camera.zoom);
        m_GameCamera->SetPerspective(chosen->camera.perspective);
        m_GameCamera->SetFov(chosen->camera.fov);
        m_GameCamera->SetAspectRatio(m_GameSize.x / m_GameSize.y);
    }
}

void GUI::ApplyProject(EditorContext& ctx) {
    ctx.renderer->ClearProjectCaches();
    ctx.sceneManager->Clear();
    if (*ctx.state != EditorState::Edit) {
        *ctx.state = EditorState::Edit;
        ctx.discardSnapshot = true;
    }
    m_BrowsePath = "assets";
    m_SelectedAsset.clear();
    if (fs::exists("assets/scenes/default.scene")) {
        ctx.serializer->Load(ctx.sceneManager, "assets/scenes/default.scene");
        m_CurrentScenePath = "assets/scenes/default.scene";
    } else {
        m_CurrentScenePath.clear();
    }
    MarkSceneSaved(ctx);
}

// ===== Workflows сцен =====
void GUI::MarkSceneSaved(EditorContext& ctx) {
    m_SceneSignature = ComputeSceneSignature(ctx.sceneManager);
    m_SceneDirty = false;
    // undo: сохранённое состояние — новая точка отсчёта
    m_UndoBaseline = ctx.sceneManager->TakeSnapshot();
    m_UndoBaselineSig = m_SceneSignature;
    m_UndoBaselineValid = true;
    m_WasChangedVsBaseline = false;
}

void GUI::RefreshSceneDirty(EditorContext& ctx) {
    // Во время Play трансформы меняет физика — не считаем это правками
    if (*ctx.state != EditorState::Edit) return;
    if (!m_BaselineValid) {
        // стартовая сцена загружена Application'ом до GUI — первый снимок не считается правкой
        m_BaselineValid = true;
        MarkSceneSaved(ctx);
        return;
    }
    uint64_t sig = ComputeSceneSignature(ctx.sceneManager);
    m_SceneDirty = sig != m_SceneSignature;

    // Undo: один снимок «до правок» на пачку изменений (первый кадр отличия от baseline)
    if (!m_UndoBaselineValid) {
        m_UndoBaseline = ctx.sceneManager->TakeSnapshot();
        m_UndoBaselineSig = sig;
        m_UndoBaselineValid = true;
        m_WasChangedVsBaseline = false;
    }
    bool changed = sig != m_UndoBaselineSig;
    if (changed && !m_WasChangedVsBaseline && ctx.app)
        ctx.app->PushUndoSnapshot(m_UndoBaseline);
    m_WasChangedVsBaseline = changed;
}

void GUI::SaveSceneNow(EditorContext& ctx) {
    if (m_CurrentScenePath.empty()) {
        OpenSceneSaveAs(ctx);
        return;
    }
    std::error_code ec;
    fs::create_directories(fs::path(m_CurrentScenePath).parent_path(), ec);
    ctx.serializer->Save(ctx.sceneManager, m_CurrentScenePath);
    MarkSceneSaved(ctx);
}

void GUI::OpenSceneSaveAs(EditorContext& ctx) {
    m_ShowSceneBrowser = true;
    m_SceneBrowserSave = true;
    m_SceneBrowserSelected.clear();
    m_SceneBrowserAfterOpen.clear();
    m_SceneBrowserAfterNew = false;
    if (m_CurrentScenePath.empty()) {
        m_SceneBrowserDir = "assets/scenes";
        if (!m_SceneBrowserNameBuf[0]) strcpy(m_SceneBrowserNameBuf, "new_scene");
    } else {
        m_SceneBrowserDir = fs::path(m_CurrentScenePath).parent_path().string();
        strcpy(m_SceneBrowserNameBuf, fs::path(m_CurrentScenePath).filename().string().c_str());
    }
}

bool GUI::LoadSceneAsset(EditorContext& ctx, const std::string& path) {
    if (!ctx.serializer->Load(ctx.sceneManager, path)) return false;
    m_CurrentScenePath = path;
    MarkSceneSaved(ctx);
    return true;
}

void GUI::DoNewScene(EditorContext& ctx) {
    ctx.sceneManager->Clear();
    m_CurrentScenePath.clear();
    MarkSceneSaved(ctx);
}

void GUI::RequestNewScene(EditorContext& ctx) {
    if (m_SceneDirty) {
        m_PendingAction = 1;
        m_PendingScenePath.clear();
        m_ShowSceneConfirm = true;
    } else {
        DoNewScene(ctx);
    }
}

void GUI::RequestOpenScene(EditorContext& ctx, const std::string& path) {
    if (m_SceneDirty && path != m_CurrentScenePath) {
        m_PendingAction = 2;
        m_PendingScenePath = path;
        m_ShowSceneConfirm = true;
    } else {
        LoadSceneAsset(ctx, path);
    }
}

// ===== Префабы =====
int GUI::InstantiatePrefab(EditorContext& ctx, const std::string& path, const glm::vec2& worldPos) {
    std::vector<Entity> protos;
    if (!ctx.serializer->LoadEntities(path, protos) || protos.empty()) return -1;
    int root = ctx.sceneManager->InstantiateProtos(protos, worldPos - protos[0].transform.position);
    if (root >= 0) ctx.sceneManager->GetEntities()[root].prefabSource = path;
    return root;
}

void GUI::RevertToPrefab(EditorContext& ctx, size_t index) {
    auto& ents = ctx.sceneManager->GetEntities();
    if (index >= ents.size() || ents[index].prefabSource.empty()) return;
    const std::string prefab = ents[index].prefabSource;
    const uint32_t parentId = ents[index].parentId;
    const glm::vec2 worldPos = Transforms::WorldPosition(ents, ents[index]);

    std::vector<Entity> protos;
    if (!ctx.serializer->LoadEntities(prefab, protos) || protos.empty()) return;

    ctx.sceneManager->DeleteSubtree(index);
    int newRoot = ctx.sceneManager->InstantiateProtos(protos, worldPos - protos[0].transform.position);
    if (newRoot < 0) return;
    auto& after = ctx.sceneManager->GetEntities();
    after[newRoot].prefabSource = prefab;
    if (parentId != 0) {
        for (size_t i = 0; i < after.size(); i++) {
            if (after[i].id == parentId) {
                ctx.sceneManager->SetParent(static_cast<size_t>(newRoot), i);
                break;
            }
        }
    }
}

void GUI::HandleHotkeys(EditorContext& ctx) {
    ImGuiIO& io = ImGui::GetIO();
    // Пока фокус во встроенном редакторе кода — все клавиши (Ctrl+S, ^C/^V/^X/^Z, Undo) его
    if (m_ShowCodeWindow && m_CodeWindowFocused) return;
    if (m_PopupOpen || io.WantCaptureKeyboard || m_ShowProjectManagerWindow || m_FolderPickerTarget) return;

    Input& in = Input::Get();
    const bool ctrl = in.IsKeyDown(GLFW_KEY_LEFT_CONTROL) || in.IsKeyDown(GLFW_KEY_RIGHT_CONTROL);
    const bool shift = in.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || in.IsKeyDown(GLFW_KEY_RIGHT_SHIFT);
    SceneManager* sm = ctx.sceneManager;
    const int sel = sm->GetSelectedEntity();

    if (ctrl && in.WasKeyPressed(GLFW_KEY_N)) { RequestNewScene(ctx); return; }
    if (ctrl && in.WasKeyPressed(GLFW_KEY_O)) {
        m_ShowSceneBrowser = true;
        m_SceneBrowserSave = false;
        m_SceneBrowserSelected.clear();
        m_SceneBrowserAfterOpen.clear();
        m_SceneBrowserAfterNew = false;
        m_SceneBrowserDir = m_CurrentScenePath.empty() ? "assets/scenes"
                       : fs::path(m_CurrentScenePath).parent_path().string();
        return;
    }
    if (ctrl && in.WasKeyPressed(GLFW_KEY_S)) {
        if (shift) OpenSceneSaveAs(ctx); else SaveSceneNow(ctx);
        return;
    }
    if (ctrl && in.WasKeyPressed(GLFW_KEY_D)) {
        if (sel >= 0) sm->DuplicateSubtree(static_cast<size_t>(sel));
        return;
    }
    if (ctrl && (in.WasKeyPressed(GLFW_KEY_Z) || in.WasKeyPressed(GLFW_KEY_Y))) {
        // Ctrl+Z — назад, Ctrl+Shift+Z / Ctrl+Y — вперёд
        if (ctx.app) {
            bool ok = (in.WasKeyPressed(GLFW_KEY_Y) || shift) ? ctx.app->Redo() : ctx.app->Undo();
            if (ok) {
                m_UndoBaseline = sm->TakeSnapshot();
                m_UndoBaselineSig = ComputeSceneSignature(sm);
                m_WasChangedVsBaseline = false;
            }
        }
        return;
    }
    if (ctrl && in.WasKeyPressed(GLFW_KEY_C)) {
        if (!m_ProjectPanelFocused && sel >= 0) {
            m_Clipboard = sm->GetSubtree(static_cast<size_t>(sel));
            std::cout << "[Copy] " << m_Clipboard.size() << " сущностей в буфере\n";
        }
        return;
    }
    if (ctrl && in.WasKeyPressed(GLFW_KEY_V)) {
        PasteClipboard(sm);
        return;
    }
    if (ctrl && (in.WasKeyPressed(GLFW_KEY_UP) || in.WasKeyPressed(GLFW_KEY_DOWN))) {
        // перестановка среди сиблингов (как Ctrl+PageUp/PageDown в Unity)
        if (sel >= 0) {
            auto& ents = sm->GetEntities();
            const Entity& cur = ents[static_cast<size_t>(sel)];
            int mode = in.WasKeyPressed(GLFW_KEY_UP) ? -1 : 1;
            uint32_t refId = 0;
            if (mode < 0) {
                for (int i = sel - 1; i >= 0; i--)
                    if (ents[i].parentId == cur.parentId) { refId = ents[i].id; break; }
            } else {
                for (size_t i = sel + 1; i < ents.size(); i++)
                    if (ents[i].parentId == cur.parentId) { refId = ents[i].id; break; }
            }
            if (refId) sm->MoveEntity(cur.id, refId, mode);
        }
        return;
    }
    if (!ctrl && (in.WasKeyPressed(GLFW_KEY_DELETE) || in.WasKeyPressed(GLFW_KEY_BACKSPACE))) {
        if (!m_ProjectPanelFocused && sel >= 0) sm->DeleteSubtree(static_cast<size_t>(sel));
        return;
    }
    if (in.WasKeyPressed(GLFW_KEY_F2)) {
        if (m_ProjectPanelFocused && !m_SelectedAsset.empty() && m_SelectedAsset != "assets") {
            m_RenameAssetPath = m_SelectedAsset;
            snprintf(m_AssetNameBuffer, sizeof(m_AssetNameBuffer), "%s",
                     fs::path(m_SelectedAsset).filename().string().c_str());
        } else if (!m_ProjectPanelFocused && sel >= 0) {
            m_ShowRenameDialog = true;
            m_RenameIndex = sel;
            strcpy(m_RenameBuffer, sm->GetEntities()[sel].name.c_str());
        }
        return;
    }
    if (ctrl && shift && in.WasKeyPressed(GLFW_KEY_B)) { m_ShowBuildDialog = !m_ShowBuildDialog; return; }
    if (ctrl && in.WasKeyPressed(GLFW_KEY_P)) {
        EditorState& st = *ctx.state;
        st = (st == EditorState::Edit) ? EditorState::Play : EditorState::Edit;
        return;
    }
    if (!ctrl && !shift) {
        if (in.WasKeyPressed(GLFW_KEY_W)) ctx.scene->SetGizmoMode(0);
        if (in.WasKeyPressed(GLFW_KEY_E)) ctx.scene->SetGizmoMode(1);
        if (in.WasKeyPressed(GLFW_KEY_R)) ctx.scene->SetGizmoMode(2);
        if (in.WasKeyPressed(GLFW_KEY_Q)) ctx.scene->SetGizmoMode(3);
        if (in.WasKeyPressed(GLFW_KEY_T)) ctx.scene->SetGizmoMode(4);
    }
}

void GUI::PasteClipboard(SceneManager* sm) {
    if (m_Clipboard.empty()) return;
    std::vector<Entity> protos = m_Clipboard;
    std::unordered_map<uint32_t, uint32_t> remap;
    uint32_t t = 1;
    for (auto& e : protos) remap[e.id] = t++;
    for (auto& e : protos) {
        uint32_t oldParent = e.parentId;
        e.id = remap[e.id];
        e.parentId = remap.count(oldParent) ? remap[oldParent] : 0;
    }
    int idx = sm->InstantiateProtos(protos, glm::vec2(40.0f, -40.0f));
    if (idx >= 0) sm->SetSelectedEntity(idx);
}

void GUI::UpdateWindowTitle(EditorContext& ctx) {
    static std::string last;
    std::string sceneName = m_CurrentScenePath.empty()
        ? "Untitled"
        : fs::path(m_CurrentScenePath).stem().string();
    std::string title = "Astra — " + ctx.projectManager->CurrentProjectName() +
                        " | " + sceneName + (m_SceneDirty ? "*" : "");
    if (title != last) {
        glfwSetWindowTitle(m_Window, title.c_str());
        last = title;
    }
}

void GUI::RenderEditorPanels(EditorContext& ctx, float deltaTime) {
    m_PopupOpen = false;
    SceneManager* sceneManager = ctx.sceneManager;
    Scene* scene = ctx.scene;
    Camera* camera = ctx.camera;
    EditorState& state = *ctx.state;

    // Restart: кадр в Edit (Application сделает cleanup) — затем снова Play
    if (m_PendingRestart && state == EditorState::Edit) {
        m_PendingRestart = false;
        state = EditorState::Play;
    }

    if (m_ThemeDirty) { ApplyTheme(); m_ThemeDirty = false; }
    static float fpsEma = 60.0f;
    fpsEma = fpsEma * 0.95f + (1.0f / std::max(deltaTime, 1e-5f)) * 0.05f;
    m_FpsEma = fpsEma;

    HandleHotkeys(ctx);
    UpdateGameCamera(sceneManager);
    RefreshSceneDirty(ctx);
    UpdateWindowTitle(ctx);
    (void)deltaTime;

    // ===== MENU BAR + TOOLBAR =====
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) RequestNewScene(ctx);
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                m_ShowSceneBrowser = true;
                m_SceneBrowserSave = false;
                m_SceneBrowserSelected.clear();
                m_SceneBrowserAfterOpen.clear();
                m_SceneBrowserAfterNew = false;
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) SaveSceneNow(ctx);
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) OpenSceneSaveAs(ctx);
            ImGui::Separator();
            if (ImGui::MenuItem("New Project...")) {
                m_ShowNewProjectDialog = true;
                m_ProjectError.clear();
                snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s",
                         (GuiHomeDir() / "projects").string().c_str());
            }
            if (ImGui::MenuItem("Open Project...")) {
                m_ShowOpenProjectDialog = true;
                m_ProjectError.clear();
            }
            if (ImGui::MenuItem("Projects Manager")) {
                m_ShowProjectManagerWindow = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Settings...", "Ctrl+Shift+B")) {
                m_ShowBuildDialog = true;
                m_BuildStatus.clear();
                m_BuildRunning = false;
                if (!m_BuildDirBuf[0]) strcpy(m_BuildDirBuf, "build_release");
            }
            if (ImGui::BeginMenu("Open Recent")) {
                const auto& recent = ctx.projectManager->GetRecent();
                if (recent.empty()) ImGui::TextDisabled("(empty)");
                for (const auto& path : recent) {
                    std::string label = fs::path(path).filename().string() + "  (" + path + ")";
                    if (ImGui::MenuItem(label.c_str())) {
                        if (ctx.projectManager->OpenProject(path)) ApplyProject(ctx);
                        else m_ProjectError = ctx.projectManager->LastError();
                        m_ShowOpenProjectDialog = true;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Settings...")) m_ShowSettings = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("GameObject")) {
            if (ImGui::MenuItem("Create Empty")) {
                Entity e;
                e.name = "Empty";
                sceneManager->AddEntity(e);
            }
            if (ImGui::MenuItem("Create Quad")) {
                m_ShowNewEntityDialog = true;
                m_NewEntityType = 1;
                strcpy(m_NewEntityName, "Quad");
            }
            if (ImGui::MenuItem("Create Circle")) {
                m_ShowNewEntityDialog = true;
                m_NewEntityType = 2;
                strcpy(m_NewEntityName, "Circle");
            }
            if (ImGui::MenuItem("Create Camera")) {
                bool anyMain = false;
                for (const auto& e : sceneManager->GetEntities()) {
                    if (e.hasCamera && e.camera.mainCamera) { anyMain = true; break; }
                }
                Entity cam;
                cam.name = "Main Camera";
                cam.sprite.type = SpriteType::None;
                cam.collider.type = ColliderType::None;
                cam.rigidbody.isKinematic = true;
                cam.hasCamera = true;
                cam.camera.mainCamera = !anyMain;
                sceneManager->AddEntity(cam);
            }
            if (ImGui::BeginMenu("Create 3D")) {
                auto add3D = [&](const char* name, int meshType) {
                    Entity e;
                    e.name = name;
                    e.is3D = true;
                    e.sprite.type = SpriteType::None;
                    e.collider.type = ColliderType::None;
                    e.pos3 = glm::vec3(camera->GetPosition(), 0.0f) + glm::vec3(0, 0, 0);
                    e.scale3 = glm::vec3(100.0f);
                    e.mesh.type = meshType;
                    e.mesh.color = glm::vec3(0.85f, 0.85f, 0.9f);
                    sceneManager->AddEntity(e);
                };
                if (ImGui::MenuItem("Cube")) add3D("Cube", 0);
                if (ImGui::MenuItem("Plane")) add3D("Plane", 1);
                if (ImGui::MenuItem("Sphere")) add3D("Sphere", 2);
                if (ImGui::MenuItem("OBJ Model")) add3D("Model", 3);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Create UI")) {
                auto addUIEntity = [&](const char* name, UIKind kind, glm::vec2 size) {
                    Entity e;
                    e.name = name;
                    e.sprite.type = SpriteType::None;
                    e.collider.type = ColliderType::None;
                    e.rigidbody.isKinematic = true;
                    e.hasUI = true;
                    e.ui.kind = kind;
                    e.ui.label = (kind == UIKind::Text) ? "Hello" : name;
                    if (kind == UIKind::Slider || kind == UIKind::ProgressBar) { e.ui.minValue = 0.0f; e.ui.maxValue = 1.0f; e.ui.value = 0.5f; }
                    if (kind == UIKind::Checkbox) { e.ui.minValue = 0.0f; e.ui.maxValue = 1.0f; e.ui.value = 0.0f; }
                    e.transform.position = camera->GetPosition(); // мировые координаты центра вида
                    e.transform.scale = size;
                    sceneManager->AddEntity(e);
                };
                if (ImGui::MenuItem("Button")) addUIEntity("Button", UIKind::Button, glm::vec2(140.0f, 30.0f));
                if (ImGui::MenuItem("Text")) addUIEntity("Text", UIKind::Text, glm::vec2(100.0f, 20.0f));
                if (ImGui::MenuItem("Slider")) addUIEntity("Slider", UIKind::Slider, glm::vec2(180.0f, 0.0f));
                if (ImGui::MenuItem("Checkbox")) addUIEntity("Checkbox", UIKind::Checkbox, glm::vec2(140.0f, 0.0f));
                if (ImGui::MenuItem("Progress Bar")) addUIEntity("Progress Bar", UIKind::ProgressBar, glm::vec2(180.0f, 20.0f));
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Scene", nullptr, m_ShowScene)) { m_ShowScene = !m_ShowScene; m_RebuildDockLayout = true; }
            if (ImGui::MenuItem("Game", nullptr, m_ShowGame)) { m_ShowGame = !m_ShowGame; m_RebuildDockLayout = true; }
            if (ImGui::MenuItem("Hierarchy", nullptr, m_ShowHierarchy)) { m_ShowHierarchy = !m_ShowHierarchy; m_RebuildDockLayout = true; }
            if (ImGui::MenuItem("Inspector", nullptr, m_ShowInspector)) { m_ShowInspector = !m_ShowInspector; m_RebuildDockLayout = true; }
            if (ImGui::MenuItem("Project", nullptr, m_ShowProject)) { m_ShowProject = !m_ShowProject; m_RebuildDockLayout = true; }
            if (ImGui::MenuItem("Script Editor", nullptr, m_ShowCodeWindow)) m_ShowCodeWindow = !m_ShowCodeWindow;
            if (ImGui::MenuItem("Build Settings", nullptr, m_ShowBuildDialog)) m_ShowBuildDialog = !m_ShowBuildDialog;
            if (ImGui::MenuItem("3D Mode", nullptr, m_3DEditor)) m_3DEditor = !m_3DEditor;
            ImGui::EndMenu();
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", ctx.projectManager->CurrentProjectName().c_str());
        ImGui::SameLine();
        {
            std::string sceneName = m_CurrentScenePath.empty()
                ? "Untitled" : fs::path(m_CurrentScenePath).filename().string();
            ImGui::TextColored(m_SceneDirty ? ImVec4(0.95f, 0.7f, 0.2f, 1.0f) : ImVec4(0.75f, 0.75f, 0.75f, 1.0f),
                               "%s%s", sceneName.c_str(), m_SceneDirty ? " *" : "");
        }
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

        ImGui::BeginDisabled(state != EditorState::Edit);
        if (ImGui::Button("Play")) state = EditorState::Play;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state != EditorState::Play);
        if (ImGui::Button("Pause")) state = EditorState::Pause;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state == EditorState::Edit);
        if (ImGui::Button("Stop")) state = EditorState::Edit;
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Restart")) { m_PendingRestart = true; state = EditorState::Edit; }

        if (state != EditorState::Edit) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(state == EditorState::Play ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                               state == EditorState::Play ? "PLAYING" : "PAUSED");
        }

        // Ошибки компиляции скриптов: без тултипа, бегающего за курсором — клик кидает в Console
        if (!Scripting::Errors().empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.18f, 0.15f, 1.0f));
            std::string errBtn = "Ошибки: " + std::to_string(Scripting::Errors().size()) + "###scriptErr";
            if (ImGui::Button(errBtn.c_str())) {
                m_ProjectConsoleTab = true;
                m_ShowProject = true;
            }
            ImGui::PopStyleColor(2);
        }

        // Инструменты сцены: W/E/R/Q
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        static const char* toolNames[5] = { "Move", "Rotate", "Scale", "Hand", "Tile" };
        static const char* toolKeys[5] = { "W", "E", "R", "Q", "T" };
        int gmode = scene->GetGizmoMode();
        for (int i = 0; i < 5; i++) {
            ImGui::SameLine();
            char label[32];
            snprintf(label, sizeof(label), "%s (%s)", toolNames[i], toolKeys[i]);
            if (gmode == i) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.90f, 0.50f, 0.15f, 0.55f));
            if (ImGui::Button(label)) scene->SetGizmoMode(i);
            if (gmode == i) ImGui::PopStyleColor();
        }

        ImGui::EndMainMenuBar();
    }

    // ===== DOCKSPACE =====
    ImGuiWindowFlags dockFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    dockFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
              | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
              | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("DockSpaceRoot", nullptr, dockFlags);
    ImGui::PopStyleVar(2);

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    static bool firstTime = true;
    if (firstTime || m_RebuildDockLayout) {
        firstTime = false;
        m_RebuildDockLayout = false;
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

        ImGuiID dock_main = dockspaceID;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.17f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.20f, nullptr, &dock_main);
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.25f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Project", dock_bottom);
        ImGui::DockBuilderDockWindow("Scene", dock_main);
        ImGui::DockBuilderDockWindow("Game", dock_main); // рядом со Scene в табах
        ImGui::DockBuilderDockWindow("Script", dock_main); // IDE — третьей вкладкой к Scene/Game
        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();

    // ===== SCENE VIEW =====
    if (m_ShowScene) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool sceneVisible = ImGui::Begin("Scene");

        if (sceneVisible) {
            m_SceneFocused = ImGui::IsWindowFocused();
            m_SceneHovered = ImGui::IsWindowHovered();

            ImVec2 avail = ImGui::GetContentRegionAvail();
            m_SceneSize = glm::vec2(avail.x, avail.y);

            scene->SetEditor3D(m_3DEditor);
            scene->Render(camera, sceneManager, static_cast<int>(avail.x), static_cast<int>(avail.y),
                          m_HasGameCamera ? m_GameCamera.get() : nullptr);
            // мышь гейтим строго по rects изображения: любые "уточняющие" слагаемые
            // (frame height и пр.) сдвигают хит-зоны gizmo относительно отрисованных осей
            m_SceneImagePos = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)(intptr_t)scene->GetViewportTexture(), avail, ImVec2(0, 1), ImVec2(1, 0));

            ImVec2 mousePos = ImGui::GetMousePos();
            m_SceneMousePos = glm::vec2(mousePos.x - m_SceneImagePos.x, mousePos.y - m_SceneImagePos.y);

            // Предпросмотр runtime UI прямо в Scene: вкладка Game может быть скрыта за Scene
            {
                auto& ents = sceneManager->GetEntities();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                glm::mat4 vp = camera->GetViewProjectionMatrix();
                auto w2s = [&](const glm::vec2& wp) {
                    glm::vec4 clip = vp * glm::vec4(wp, 0.0f, 1.0f);
                    return ImVec2(m_SceneImagePos.x + (clip.x * 0.5f + 0.5f) * m_SceneSize.x,
                                  m_SceneImagePos.y + (1.0f - (clip.y * 0.5f + 0.5f)) * m_SceneSize.y);
                };
                for (const auto& e : ents) {
                    if (!e.hasUI || !e.active) continue;
                    glm::vec2 wp = Transforms::WorldPosition(ents, e);
                    glm::vec2 half = e.transform.scale * 0.5f;
                    half.y = std::max(half.y, 12.0f);
                    half.x = std::max(half.x, 24.0f);
                    ImVec2 a = w2s(wp - half), b = w2s(wp + half);
                    dl->AddRectFilled(a, b, IM_COL32(90, 140, 200, 50));
                    dl->AddRect(a, b, IM_COL32(120, 180, 255, 170), 3.0f);
                    ImVec2 t = w2s(wp);
                    dl->AddText(ImVec2(t.x - 30.0f, t.y - 6.0f), IM_COL32(220, 230, 255, 210),
                                e.ui.label.c_str());
                }
            }

            // ПКМ-клик без смещения (>6px = пан) — контекстное меню в точке мира
            {
                static ImVec2 rmbDown;
                static bool rmbHeld = false, rmbDragged = false;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    rmbDown = ImGui::GetMousePos();
                    rmbHeld = true;
                    rmbDragged = false;
                }
                if (rmbHeld && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    ImVec2 mp = ImGui::GetMousePos();
                    if (std::fabs(mp.x - rmbDown.x) + std::fabs(mp.y - rmbDown.y) > 6.0f) rmbDragged = true;
                }
                if (rmbHeld && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    rmbHeld = false;
                    if (!rmbDragged && ImGui::IsWindowHovered()) {
                        m_SceneCtxRequest = true;
                        m_SceneCtxWorld = camera->ScreenToWorld(m_SceneMousePos, m_SceneSize.x, m_SceneSize.y);
                    }
                }
            }
            if (m_SceneCtxRequest) { ImGui::OpenPopup("SceneCtx"); m_SceneCtxRequest = false; }
            if (ImGui::BeginPopup("SceneCtx")) {
                m_PopupOpen = true;
                if (ImGui::MenuItem("Create Empty here")) {
                    Entity e; e.name = "Empty"; e.sprite.type = SpriteType::None;
                    e.collider.type = ColliderType::None;
                    e.transform.position = m_SceneCtxWorld;
                    sceneManager->AddEntity(e);
                }
                if (ImGui::MenuItem("Create Quad here")) {
                    Entity e; e.name = "Quad"; e.sprite.type = SpriteType::Quad;
                    e.sprite.color = glm::vec3(1.0f);
                    e.collider.type = ColliderType::Box; e.collider.size = glm::vec2(50.0f, 50.0f);
                    e.transform.scale = glm::vec2(100.0f, 100.0f);
                    e.transform.position = m_SceneCtxWorld;
                    sceneManager->AddEntity(e);
                }
                if (!m_Clipboard.empty() && ImGui::MenuItem("Paste here", "Ctrl+V")) {
                    PasteClipboard(sceneManager);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Deselect")) sceneManager->SetSelectedEntity(-1);
                if (ImGui::MenuItem("Focus selection", "F")) {
                    int s = sceneManager->GetSelectedEntity();
                    if (s >= 0) camera->SetPosition(Transforms::WorldPosition(
                        sceneManager->GetEntities(), sceneManager->GetEntities()[s]));
                }
                ImGui::EndPopup();
            }

            // Дроп ассета прямо в Scene: картинка — на объект под курсором (или создать спрайт),
            // префаб — инстанцируется в точку дропа
            if (ImGui::BeginDragDropTarget()) {
                glm::vec2 wp = camera->ScreenToWorld(m_SceneMousePos, m_SceneSize.x, m_SceneSize.y);
                auto& ents = sceneManager->GetEntities();
                int hitIdx = -1;
                for (size_t i = ents.size(); i > 0; i--) {
                    const Entity& e = ents[i - 1];
                    if (!e.active || e.sprite.type == SpriteType::None) continue;
                    glm::vec2 p = Transforms::WorldPosition(ents, e);
                    glm::vec2 half = e.transform.scale * 0.5f;
                    if (std::fabs(wp.x - p.x) <= half.x && std::fabs(wp.y - p.y) <= half.y) {
                        hitIdx = static_cast<int>(i - 1);
                        break;
                    }
                }
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    std::string path((const char*)p->Data);
                    std::string ext = ExtLower(path);
                    bool isAudio = ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac";
                    if (IsImageExt(ext) || isAudio) {
                        if (hitIdx >= 0) {
                            if (isAudio) ents[hitIdx].audio.path = path;
                            else ents[hitIdx].sprite.texturePath = path;
                            sceneManager->SetSelectedEntity(hitIdx);
                        } else if (!isAudio) {
                            Entity e;
                            e.name = fs::path(path).filename().string();
                            e.sprite.type = SpriteType::Quad;
                            e.sprite.color = glm::vec3(1.0f);
                            e.transform.scale = glm::vec2(100.0f, 100.0f);
                            e.transform.position = wp;
                            e.sprite.texturePath = path;
                            sceneManager->AddEntity(e);
                        }
                    }
                }
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("PREFAB_PATH")) {
                    InstantiatePrefab(ctx, std::string((const char*)p->Data), wp);
                }
                ImGui::EndDragDropTarget();
            }
        } else {
            m_SceneHovered = false;
            m_SceneFocused = false;
        }

        ImGui::End();
        ImGui::PopStyleVar();
    } else {
        m_SceneHovered = false;
        m_SceneFocused = false;
    }

    // ===== GAME VIEW =====
    if (m_ShowGame) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        bool gameVisible = ImGui::Begin("Game");
        m_GameImageSize = ImVec2(0, 0); // пока не убедились, что вью реально рисуется

        if (gameVisible) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            m_GameSize = glm::vec2(avail.x, avail.y);

            if (m_HasGameCamera) {
                scene->RenderGameView(m_GameCamera.get(), sceneManager,
                                      static_cast<int>(avail.x), static_cast<int>(avail.y));
                m_GameImagePos = ImGui::GetCursorScreenPos();
                m_GameImageSize = avail;
                ImGui::Image((ImTextureID)(intptr_t)scene->GetGameTexture(), avail, ImVec2(0, 1), ImVec2(1, 0));
                char fpsBuf[32];
                snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", m_FpsEma);
                ImGui::GetForegroundDrawList()->AddText(
                    ImVec2(m_GameImagePos.x + 8.0f, m_GameImagePos.y + 6.0f),
                    IM_COL32(255, 255, 130, 255), fpsBuf);
            } else {
                ImGui::TextDisabled("No Camera in scene. Use GameObject > Create Camera.");
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    } else {
        m_GameImageSize = ImVec2(0, 0);
    }

    // ===== HIERARCHY =====
    if (m_ShowHierarchy) RenderHierarchy(ctx);

    // ===== INSPECTOR =====
    if (m_ShowInspector) RenderInspector(ctx);

    // ===== PROJECT / ASSET BROWSER =====
    if (m_ShowProject) RenderProject(ctx);

    // ===== DIALOGS =====
    RenderNewEntityDialog(sceneManager);
    RenderRenameDialog(sceneManager);
    RenderSceneBrowser(ctx);
    RenderSceneConfirm(ctx);
    RenderPrefabSaveDialog(ctx);
    RenderAssetDialogs(ctx);
    RenderProjectDialogs(ctx);
    RenderSettings(ctx);
    RenderBuildDialog(ctx);
    RenderCodeWindow(ctx);
    // FolderPicker — вложенная модалка: обязана вызываться ПОСЛЕ родительских модалок,
    // иначе не поднимается поверх и невидимая блокирует ввод («редактор висит»)
    RenderFolderPicker(ctx);

    // ===== RUNTIME UI OVERLAY (Play/Pause) =====
    RenderGameUIOverlay(ctx);
}

void GUI::RenderHierarchy(EditorContext& ctx) {
    ImGui::Begin("Hierarchy");
    SceneManager* sceneManager = ctx.sceneManager;
    auto& entities = sceneManager->GetEntities();

    // groupId -> индексы детей; ключ 0 — корни
    std::unordered_set<uint32_t> validIds;
    for (const auto& e : entities) validIds.insert(e.id);
    std::unordered_map<uint32_t, std::vector<size_t>> children;
    for (size_t i = 0; i < entities.size(); i++) {
        uint32_t key = 0;
        if (entities[i].parentId != 0 && validIds.count(entities[i].parentId))
            key = entities[i].parentId;
        children[key].push_back(i);
    }

    for (size_t root : children[0])
        RenderEntityNode(ctx, root, children, 0);

    // Дроп в пустую область — отцепить, создать объект из ассета или инстанцировать префаб
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
            sceneManager->SetParent(*(const size_t*)p->Data, SIZE_MAX);
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            fs::path path((const char*)p->Data);
            std::string ext = ExtLower(path);
            if (IsImageExt(ext)) {
                Entity e;
                e.name = path.filename().string();
                e.sprite.type = SpriteType::Quad;
                e.sprite.color = glm::vec3(1.0f);
                e.transform.scale = glm::vec2(100.0f, 100.0f);
                e.transform.position = ctx.camera->GetPosition();
                e.sprite.texturePath = path.string();
                sceneManager->AddEntity(e);
            }
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("PREFAB_PATH")) {
            InstantiatePrefab(ctx, std::string((const char*)p->Data), ctx.camera->GetPosition());
        }
        ImGui::EndDragDropTarget();
    }

    // ПКМ по пустому месту: создать/вставить/снять выделение
    if (ImGui::BeginPopupContextWindow("##hierEmptyCtx",
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        m_PopupOpen = true;
        if (ImGui::MenuItem("Create Empty")) {
            Entity e; e.name = "Empty"; e.sprite.type = SpriteType::None;
            e.collider.type = ColliderType::None;
            e.transform.position = ctx.camera->GetPosition();
            sceneManager->AddEntity(e);
        }
        if (!m_Clipboard.empty() && ImGui::MenuItem("Paste", "Ctrl+V")) PasteClipboard(sceneManager);
        if (ImGui::MenuItem("Deselect")) sceneManager->SetSelectedEntity(-1);
        ImGui::EndPopup();
    }

    ImGui::End();
}

void GUI::RenderEntityNode(EditorContext& ctx, size_t index,
                           const std::unordered_map<uint32_t, std::vector<size_t>>& children, int depth) {
    if (depth > 32) return;
    SceneManager* sceneManager = ctx.sceneManager;
    auto& entities = sceneManager->GetEntities();
    if (index >= entities.size()) return;
    Entity& entity = entities[index];

    auto it = children.find(entity.id);
    bool hasChildren = it != children.end() && !it->second.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                             | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    if (sceneManager->GetSelectedEntity() == static_cast<int>(index))
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(static_cast<int>(index));

    std::string label = entity.name;
    if (!entity.prefabSource.empty()) label += " [prf]";
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        sceneManager->SetSelectedEntity(static_cast<int>(index));

    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("ENTITY_INDEX", &index, sizeof(size_t));
        ImGui::Text("%s", entity.name.c_str());
        ImGui::EndDragDropSource();
    }

    // Дроп на узел: середина — ребёнком; верх/низ вставки — сиблингом перед/после.
    // Картинка -> текстура; префаб -> ребёнком этого узла
    if (ImGui::BeginDragDropTarget()) {
        ImVec2 rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectMax();
        float h = rmax.y - rmin.y;
        float t = h > 1.0f ? (ImGui::GetMousePos().y - rmin.y) / h : 0.5f;
        const int dropMode = (t < 0.33f) ? -1 : (t > 0.67f ? 1 : 0);

        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
            size_t srcIdx = *(const size_t*)p->Data;
            auto& ents = sceneManager->GetEntities();
            if (srcIdx < ents.size()) {
                if (dropMode == 0) sceneManager->SetParent(srcIdx, index);
                else sceneManager->MoveEntity(ents[srcIdx].id, entity.id, dropMode);
            }
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            fs::path path((const char*)p->Data);
            std::string ext = ExtLower(path);
            if (IsImageExt(ext)) entity.sprite.texturePath = path.string();
            else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
                entity.audio.path = path.string();
        }
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("PREFAB_PATH")) {
            int newRoot = InstantiatePrefab(ctx, std::string((const char*)p->Data),
                                           Transforms::WorldPosition(entities, entity));
            if (newRoot >= 0) sceneManager->SetParent(static_cast<size_t>(newRoot), index);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("HierarchyContext");
        sceneManager->SetSelectedEntity(static_cast<int>(index));
    }

    if (ImGui::BeginPopup("HierarchyContext")) {
        m_PopupOpen = true;
        // Перестановка среди сиблингов (B выше A) — сосед в списке детей того же родителя
        {
            uint32_t pid = entity.parentId;
            auto cit = (pid == 0) ? children.end() : children.find(pid);
            std::vector<size_t> sibs;
            if (cit != children.end()) sibs = cit->second;
            else { for (size_t i = 0; i < entities.size(); i++) if (entities[i].parentId == 0) sibs.push_back(i); }
            int pos = -1;
            for (size_t k = 0; k < sibs.size(); k++) if (sibs[k] == index) { pos = static_cast<int>(k); break; }
            if (pos > 0 && ImGui::MenuItem("Move Up", "Ctrl+Up")) {
                sceneManager->MoveEntity(entity.id, entities[sibs[pos - 1]].id, -1);
            }
            if (pos >= 0 && pos + 1 < static_cast<int>(sibs.size()) && ImGui::MenuItem("Move Down", "Ctrl+Down")) {
                sceneManager->MoveEntity(entity.id, entities[sibs[pos + 1]].id, +1);
            }
            if (pos > 0 || pos + 1 < static_cast<int>(sibs.size())) ImGui::Separator();
        }
        if (ImGui::MenuItem("Rename", "F2")) {
            m_ShowRenameDialog = true;
            m_RenameIndex = static_cast<int>(index);
            strcpy(m_RenameBuffer, entity.name.c_str());
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            sceneManager->DuplicateSubtree(index);
        }
        if (entity.parentId != 0 && ImGui::MenuItem("Detach from Parent")) {
            sceneManager->SetParent(index, SIZE_MAX);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save as Prefab")) {
            m_ShowPrefabSaveDialog = true;
            m_PrefabSourceIndex = static_cast<int>(index);
            m_PrefabSaveDir = "assets/prefabs";
            snprintf(m_PrefabNameBuf, sizeof(m_PrefabNameBuf), "%s", entity.name.c_str());
        }
        if (!entity.prefabSource.empty() && ImGui::MenuItem("Revert to Prefab")) {
            RevertToPrefab(ctx, index);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del")) {
            sceneManager->DeleteSubtree(index);
        }
        ImGui::EndPopup();
    }

    if (open) {
        if (hasChildren) {
            for (size_t child : it->second)
                RenderEntityNode(ctx, child, children, depth + 1);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void GUI::RenderInspector(EditorContext& ctx) {
    ImGui::Begin("Inspector");
    SceneManager* sceneManager = ctx.sceneManager;
    Entity* selected = sceneManager->GetSelectedEntityPtr();
    if (selected) {
        // --- Префаб-заголовок ---
        if (!selected->prefabSource.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "[prf] %s",
                               fs::path(selected->prefabSource).filename().string().c_str());
            if (ImGui::SmallButton("Revert to Prefab")) {
                int si = sceneManager->GetSelectedEntity();
                if (si >= 0) RevertToPrefab(ctx, static_cast<size_t>(si));
            }
            ImGui::Separator();
        }

        // --- Parent (нельзя выбрать себя, своих детей и потомков) ---
        auto& ents = sceneManager->GetEntities();
        int selIdx = sceneManager->GetSelectedEntity();
        std::vector<size_t> candidates;
        candidates.push_back(SIZE_MAX); // None
        for (size_t i = 0; i < ents.size(); i++) {
            if (static_cast<int>(i) == selIdx) continue;
            if (ents[i].id == selected->id) continue;
            if (Transforms::IsDescendantOf(ents, ents[i].id, selected->id)) continue;
            candidates.push_back(i);
        }
        int currentItem = 0;
        for (size_t k = 0; k < candidates.size(); k++) {
            if (candidates[k] < ents.size() && ents[candidates[k]].id == selected->parentId)
                currentItem = static_cast<int>(k);
        }
        const Entity* parentEnt = Transforms::FindById(ents, selected->parentId);
        std::string preview = parentEnt ? parentEnt->name : "None";
        if (ImGui::BeginCombo("Parent", preview.c_str())) {
            for (size_t k = 0; k < candidates.size(); k++) {
                std::string lbl = candidates[k] == SIZE_MAX ? "None" : ents[candidates[k]].name;
                if (ImGui::Selectable(lbl.c_str(), static_cast<int>(k) == currentItem)) {
                    if (selIdx >= 0) sceneManager->SetParent(static_cast<size_t>(selIdx), candidates[k]);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Transform");
        if (ImGui::Checkbox("3D Object", &selected->is3D)) {
            if (selected->is3D) {
                selected->pos3 = glm::vec3(selected->transform.position, 0.0f);
                selected->scale3 = glm::vec3(selected->transform.scale, 100.0f);
            }
        }
        if (selected->is3D) {
            ImGui::DragFloat3("Position 3", &selected->pos3.x, 1.0f);
            ImGui::DragFloat3("Rotation 3", &selected->rot3.x, 1.0f);
            ImGui::DragFloat3("Scale 3", &selected->scale3.x, 1.0f, 0.1f);
            const char* meshTypes[] = { "Cube", "Plane", "Sphere", "OBJ" };
            int mt = selected->mesh.type;
            if (ImGui::Combo("Mesh", &mt, meshTypes, 4)) selected->mesh.type = mt;
            if (selected->mesh.type == 3) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160.0f);
                ImGui::InputText("##objpath", &selected->mesh.meshPath);
                ImGui::SameLine();
                ImGui::TextDisabled("assets/models/*.obj");
            }
            ImGui::InputText("Mesh Texture", &selected->mesh.texturePath);
            ImGui::ColorEdit3("Mesh Color", &selected->mesh.color.x);
        } else {
            ImGui::DragFloat2("Position", &selected->transform.position.x, 1.0f);
            ImGui::DragFloat("Rotation", &selected->transform.rotation, 1.0f);
            ImGui::DragFloat2("Scale", &selected->transform.scale.x, 1.0f, 0.1f, 10000.0f);
        }

        ImGui::Separator();
        ImGui::Text("Sprite");
        const char* types[] = { "None", "Quad", "Circle" };
        int type = static_cast<int>(selected->sprite.type);
        if (ImGui::Combo("Type", &type, types, 3)) {
            selected->sprite.type = static_cast<SpriteType>(type);
        }
        ImGui::ColorEdit3("Color", &selected->sprite.color.r);
        ImGui::DragInt("Sorting Order", &selected->sprite.sortingOrder, 1.0f);

        if (selected->id != m_InspectorEntityId) {
            m_InspectorEntityId = selected->id;
            snprintf(m_TexturePathBuffer, sizeof(m_TexturePathBuffer),
                     "%s", selected->sprite.texturePath.c_str());
            snprintf(m_AnimTextureBuffer, sizeof(m_AnimTextureBuffer),
                     "%s", selected->animation.texturePath.c_str());
            snprintf(m_TilemapTexBuffer, sizeof(m_TilemapTexBuffer),
                     "%s", selected->tilemap.texturePath.c_str());
            snprintf(m_ParticleTexBuffer, sizeof(m_ParticleTexBuffer),
                     "%s", selected->emitter.texturePath.c_str());
            snprintf(m_UILabelBuffer, sizeof(m_UILabelBuffer), "%s", selected->ui.label.c_str());
            snprintf(m_ScriptPathBuffer, sizeof(m_ScriptPathBuffer), "%s", selected->scriptPath.c_str());
            snprintf(m_AudioPathBuffer, sizeof(m_AudioPathBuffer), "%s", selected->audio.path.c_str());
        }
        ImGui::InputText("Texture Path", m_TexturePathBuffer, sizeof(m_TexturePathBuffer));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            selected->sprite.texturePath = m_TexturePathBuffer;
        } else if (!ImGui::IsItemActive() && selected->sprite.texturePath != m_TexturePathBuffer) {
            // значение поменялось извне (Project-панель, загрузка сцены)
            snprintf(m_TexturePathBuffer, sizeof(m_TexturePathBuffer),
                     "%s", selected->sprite.texturePath.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##tex")) {
            selected->sprite.texturePath.clear();
            m_TexturePathBuffer[0] = '\0';
        }

        auto CompHeader = [&](const char* label, bool& present) {
            ImGui::Separator();
            ImGui::Text("%s", label);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24.0f);
            if (ImGui::SmallButton(("x##" + std::string(label)).c_str())) present = false;
        };

        // --- Спрайт-анимация ---
        bool animPresent = selected->animation.active || !selected->animation.texturePath.empty()
                        || !selected->animation.clips.empty();
        if (animPresent) {
        ImGui::Separator();
        ImGui::Text("Animation (sprite sheet)");
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24.0f);
        if (ImGui::SmallButton("x##anim")) {
            selected->animation.active = false;
            selected->animation.texturePath.clear();
            selected->animation.clips.clear();
        }
        ImGui::Checkbox("Active", &selected->animation.active);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("В Edit кадры крутятся как превью; в Play — по Play On Awake.");
        ImGui::InputText("Sheet Path", m_AnimTextureBuffer, sizeof(m_AnimTextureBuffer));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            selected->animation.texturePath = m_AnimTextureBuffer;
        } else if (!ImGui::IsItemActive() && selected->animation.texturePath != m_AnimTextureBuffer) {
            snprintf(m_AnimTextureBuffer, sizeof(m_AnimTextureBuffer),
                     "%s", selected->animation.texturePath.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Use Sprite##animtex")) {
            selected->animation.texturePath = selected->sprite.texturePath;
            snprintf(m_AnimTextureBuffer, sizeof(m_AnimTextureBuffer), "%s",
                     selected->animation.texturePath.c_str());
        }
        ImGui::DragInt("Cols", &selected->animation.cols, 1.0f, 1, 64);
        ImGui::DragInt("Rows", &selected->animation.rows, 1.0f, 1, 64);
        ImGui::DragFloat("FPS", &selected->animation.fps, 0.5f, 0.5f, 60.0f, "%.1f");
        ImGui::Checkbox("Loop", &selected->animation.loop);
        ImGui::Checkbox("Play On Awake", &selected->animation.playOnAwake);
        int totalFrames = std::max(selected->animation.cols, 1) * std::max(selected->animation.rows, 1);
        int curFrame = selected->animation.active
            ? static_cast<int>(static_cast<long>(selected->animTime * selected->animation.fps) % totalFrames) : 0;
        ImGui::TextDisabled("Кадр %d/%d (сетка слева→вправо, сверху вниз)", curFrame + 1, totalFrames);
        }

        // --- Material: живые параметры шейдера ---
        ImGui::Separator();
        ImGui::Text("Material (u_Params / u_PColor)");
        const char* paramNames[] = { "u_Params X", "u_Params Y", "u_Params Z", "u_Params W" };
        for (int i = 0; i < 4; i++)
            ImGui::SliderFloat(paramNames[i], &selected->sprite.materialParams[i], 0.0f, 1.0f, "%.2f");
        ImGui::ColorEdit4("u_PColor", &selected->sprite.materialColor.r);
        ImGui::TextDisabled("Доступны в пользовательских .frag (u_Params, u_PColor).\n"
                            "Пример: vec2 offset = u_Params.xy; float s = u_Params.z; fragColor *= u_PColor;");

        // --- Клипы анимации (нормальное меню) ---
        if (selected->animation.active || !selected->animation.clips.empty()) {
            ImGui::Indent();
            if (ImGui::CollapsingHeader("Clips", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& clips = selected->animation.clips;
                int totalFrames = std::max(selected->animation.cols, 1) * std::max(selected->animation.rows, 1);
                for (size_t i = 0; i < clips.size(); i++) {
                    AnimClip& c = clips[i];
                    ImGui::PushID((int)i);
                    ImGui::SetNextItemWidth(90.0f);
                    ImGui::InputText("##clipName", &c.name);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::DragInt("##first", &c.first, 1.0f, 0, totalFrames - 1)) c.first = std::clamp(c.first, 0, totalFrames - 1);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(50.0f);
                    if (ImGui::DragInt("##last", &c.last, 1.0f, 0, totalFrames - 1)) c.last = std::clamp(c.last, 0, totalFrames - 1);
                    if (c.last < c.first) c.last = c.first;
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(55.0f);
                    ImGui::DragFloat("##fps", &c.fps, 0.5f, 0.5f, 60.0f, "%.0f");
                    ImGui::SameLine();
                    ImGui::Checkbox("##loop", &c.loop);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Play")) {
                        selected->animation.activeClip = (int)i;
                        selected->animation.active = true;
                        selected->animTime = 0.0f;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        clips.erase(clips.begin() + (long)i);
                        if (selected->animation.activeClip >= (int)clips.size())
                            selected->animation.activeClip = std::max(0, (int)clips.size() - 1);
                        ImGui::PopID();
                        break;
                    }
                    if (selected->animation.activeClip == (int)i) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.97f, 0.6f, 0.16f, 1.0f), "<");
                    }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("+ Add Clip")) {
                    AnimClip c;
                    c.name = "clip" + std::to_string(clips.size() + 1);
                    c.first = 0;
                    c.last = totalFrames - 1;
                    c.fps = selected->animation.fps;
                    clips.push_back(c);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("PlayClip(e, \"run\") из скриптов");
            }
            if (ImGui::CollapsingHeader("Events")) {
                auto& evs = selected->animation.events;
                int totalFrames = std::max(selected->animation.cols, 1) * std::max(selected->animation.rows, 1);
                for (size_t i = 0; i < evs.size(); i++) {
                    AnimEvent& ev = evs[i];
                    ImGui::PushID((int)i);
                    ImGui::SetNextItemWidth(64.0f);
                    ImGui::DragInt("##evclip", &ev.clip, 1.0f, -1, std::max(0, (int)selected->animation.clips.size() - 1),
                                   ev.clip == -1 ? "any" : "clip %d");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(56.0f);
                    if (ImGui::DragInt("##evframe", &ev.frame, 1.0f, 0, std::max(0, totalFrames - 1)))
                        ev.frame = std::clamp(ev.frame, 0, std::max(0, totalFrames - 1));
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(110.0f);
                    ImGui::InputText("##evname", &ev.name);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) { evs.erase(evs.begin() + (long)i); ImGui::PopID(); break; }
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("+ Add Event")) {
                    AnimEvent ev; ev.name = "event";
                    evs.push_back(ev);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("кадр → OnAnimEvent(name) в скрипте");
            }
            ImGui::Unindent();
        }

        // --- Tilemap ---
        if (selected->hasTilemap) {
        CompHeader("Tilemap", selected->hasTilemap);
        {
            Tilemap& tm = selected->tilemap;
            ImGui::InputText("Atlas Path", m_TilemapTexBuffer, sizeof(m_TilemapTexBuffer));
            if (ImGui::IsItemDeactivatedAfterEdit()) tm.texturePath = m_TilemapTexBuffer;
            else if (!ImGui::IsItemActive() && tm.texturePath != m_TilemapTexBuffer)
                snprintf(m_TilemapTexBuffer, sizeof(m_TilemapTexBuffer), "%s", tm.texturePath.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Use Sprite##atlas")) {
                tm.texturePath = selected->sprite.texturePath;
                snprintf(m_TilemapTexBuffer, sizeof(m_TilemapTexBuffer), "%s", tm.texturePath.c_str());
            }
            ImGui::DragInt("Tile Size", &tm.tileW, 1.0f, 4, 1024);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::DragInt("##th", &tm.tileH, 1.0f, 4, 1024);
            ImGui::SameLine();
            ImGui::DragInt("Atlas Cols", &tm.atlasCols, 1.0f, 1, 64);
            int oldW = tm.width, oldH = tm.height;
            ImGui::DragInt("Grid W x H", &tm.width, 1.0f, 1, 512);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::DragInt("##gh", &tm.height, 1.0f, 1, 512);
            if (tm.width != oldW || tm.height != oldH) tm.cells.assign((size_t)tm.width * tm.height, -1);
            if ((int)tm.cells.size() < tm.width * tm.height) tm.cells.resize((size_t)tm.width * tm.height, -1);
            ImGui::ColorEdit4("Tint", &tm.color.r);
            ImGui::DragInt("Sorting Order##tile", &tm.sortingOrder, 1.0f);
            ImGui::Text("Current tile: %d   ", m_CurrentTile);
            ImGui::SameLine();
            if (ImGui::Button("Pick Tile")) ImGui::OpenPopup("TilePicker");
            ImGui::SameLine();
            if (ImGui::Button("Fill floor")) {
                for (int r = tm.height / 2; r < tm.height; r++)
                    for (int c = 0; c < tm.width; c++) tm.cells[(size_t)r * tm.width + c] = m_CurrentTile;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) std::fill(tm.cells.begin(), tm.cells.end(), -1);
            ImGui::TextDisabled("Инструмент Tile (T): ЛКМ — положить, Shift+ЛКМ — стереть.");

            if (ImGui::BeginPopup("TilePicker", ImGuiWindowFlags_AlwaysAutoResize)) {
                GLuint atex = tm.texturePath.empty() ? 0 : ctx.renderer->GetTexture(tm.texturePath);
                glm::ivec2 asize = ctx.renderer->GetTextureSize(tm.texturePath);
                if (!atex || asize.x <= 0 || asize.y <= 0) {
                    ImGui::TextDisabled("Загрузи атлас (Atlas Path)");
                } else {
                    int colsTotal = std::max(1, asize.x / std::max(tm.tileW, 1));
                    int rowsTotal = std::max(1, asize.y / std::max(tm.tileH, 1));
                    const float cell = 44.0f;
                    for (int r = 0; r < rowsTotal; r++) {
                        for (int c = 0; c < colsTotal; c++) {
                            int idx = r * tm.atlasCols + c;
                            if (c >= colsTotal) continue;
                            float u0 = c / (float)colsTotal, u1 = (c + 1) / (float)colsTotal;
                            float v0 = 1.0f - r / (float)rowsTotal, v1 = 1.0f - (r + 1) / (float)rowsTotal;
                            ImGui::PushID(idx);
                            if (m_CurrentTile == idx) ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.97f, 0.6f, 0.16f, 1));
                            if (ImGui::ImageButton("##t", (ImTextureID)(intptr_t)atex, ImVec2(cell, cell),
                                                   ImVec2(u0, v0), ImVec2(u1, v1))) {
                                m_CurrentTile = idx;
                                ImGui::CloseCurrentPopup();
                            }
                            if (m_CurrentTile == idx) ImGui::PopStyleColor();
                            ImGui::PopID();
                            if ((c + 1) % std::min(colsTotal, tm.atlasCols) == 0) ImGui::NewLine();
                            else ImGui::SameLine();
                        }
                    }
                    ImGui::TextDisabled("index = row*AtlasCols+col");
                }
                ImGui::EndPopup();
            }
        }
        }

        // --- Particle Emitter ---
        if (selected->hasParticles) {
        CompHeader("Particle Emitter", selected->hasParticles);
        ImGui::Checkbox("Active##part", &selected->emitter.active);
        ParticleEmitter& em = selected->emitter;
        ImGui::InputText("Particle Tex", m_ParticleTexBuffer, sizeof(m_ParticleTexBuffer));
        if (ImGui::IsItemDeactivatedAfterEdit()) em.texturePath = m_ParticleTexBuffer;
        else if (!ImGui::IsItemActive() && em.texturePath != m_ParticleTexBuffer)
            snprintf(m_ParticleTexBuffer, sizeof(m_ParticleTexBuffer), "%s", em.texturePath.c_str());
        ImGui::DragInt("Max / Rate", &em.maxCount, 1.0f, 1, 5000);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("##rate", &em.rate, 1.0f, 0.1f, 2000.0f, "%.0f/s");
        ImGui::DragFloat2("Life min/max", &em.lifeMin, 0.05f, 0.05f, 10.0f);
        ImGui::DragFloat2("Speed min/max", &em.speedMin, 1.0f, 0.0f, 2000.0f);
        ImGui::DragFloat2("Angle min/max", &em.angleMin, 1.0f, -360.0f, 360.0f);
        ImGui::DragFloat("Gravity", &em.gravity, 10.0f, -2000.0f, 2000.0f);
        ImGui::DragFloat2("Size min/max", &em.sizeMin, 0.5f, 1.0f, 512.0f);
        ImGui::ColorEdit4("Color Start", &em.colorStart.r);
        ImGui::ColorEdit4("Color End", &em.colorEnd.r);
        ImGui::Checkbox("Loop##part", &em.loop);
        ImGui::SameLine();
        ImGui::Checkbox("Play On Awake##part", &em.playOnAwake);
        if (ImGui::Button("Burst")) {
            glm::vec2 origin = Transforms::WorldPosition(sceneManager->GetEntities(), *selected);
            int n = std::min(em.maxCount / 2 + 1, em.maxCount - (int)selected->particles.size());
            for (int i = 0; i < n; i++) {
                float ang = glm::radians(em.angleMin + (em.angleMax - em.angleMin) * (float)rand() / RAND_MAX);
                float spd = em.speedMin + (em.speedMax - em.speedMin) * (float)rand() / RAND_MAX;
                Particle pt;
                pt.position = origin;
                pt.velocity = glm::vec2(std::cos(ang), std::sin(ang)) * spd;
                pt.life = em.lifeMin + (em.lifeMax - em.lifeMin) * (float)rand() / RAND_MAX;
                pt.size = em.sizeMin + (em.sizeMax - em.sizeMin) * (float)rand() / RAND_MAX;
                selected->particles.push_back(pt);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%zu live", selected->particles.size());
        }

        // --- Пользовательский шейдер ---
        ImGui::Text("Custom Shader");
        std::vector<std::string> shaderBases;
        std::error_code ec;
        if (fs::is_directory("assets/shaders", ec)) {
            for (const auto& entry : fs::directory_iterator("assets/shaders", ec)) {
                if (entry.is_directory()) continue;
                std::string ext = ExtLower(entry.path());
                // Достаточно одного .frag: вертекс с EngineQuadVert даёт движок
                if (ext == ".frag")
                    shaderBases.push_back("assets/shaders/" + entry.path().stem().string());
            }
        }
        std::sort(shaderBases.begin(), shaderBases.end());
        std::string shaderPreview = selected->sprite.shaderPath.empty()
            ? "(engine default)" : fs::path(selected->sprite.shaderPath).filename().string();
        if (ImGui::BeginCombo("##shader", shaderPreview.c_str())) {
            if (ImGui::Selectable("(engine default)", selected->sprite.shaderPath.empty()))
                selected->sprite.shaderPath.clear();
            for (const auto& base : shaderBases) {
                if (ImGui::Selectable(fs::path(base).filename().string().c_str(),
                                      selected->sprite.shaderPath == base))
                    selected->sprite.shaderPath = base;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            ctx.renderer->ClearProjectCaches(); // пересобирает и шейдеры, и текстуры
        }
        ImGui::TextDisabled("API: достаточно только .frag (вертекс даёт движок).\nХелперы: v_UV, EngineUV, EngineCircleMask, EngineRoundedBox, EngineRing,\nEngineRotate, EngineNoise, EngineFbm, EngineSwirl, EnginePalette, EngineRainbow,\nEnginePulse, EngineGrid, EngineVignette");

        if (selected->hasRigidbody) {
        CompHeader("Rigidbody", selected->hasRigidbody);
        ImGui::Checkbox("Is Kinematic", &selected->rigidbody.isKinematic);
        ImGui::DragFloat2("Velocity", &selected->rigidbody.velocity.x, 1.0f);
        ImGui::DragFloat("Mass", &selected->rigidbody.mass, 0.1f, 0.01f, 1000.0f);
        ImGui::DragFloat("Drag", &selected->rigidbody.drag, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Use Gravity", &selected->rigidbody.useGravity);
        }

        if (selected->hasCollider) {
        CompHeader("Collider", selected->hasCollider);
        const char* colliderTypes[] = { "None", "Box", "Circle" };
        int ct = static_cast<int>(selected->collider.type);
        if (ImGui::Combo("Collider Type", &ct, colliderTypes, 3)) {
            selected->collider.type = static_cast<ColliderType>(ct);
        }
        ImGui::Checkbox("Is Trigger", &selected->collider.isTrigger);
        if (selected->collider.type == ColliderType::Box) {
            ImGui::DragFloat2("Size", &selected->collider.size.x, 1.0f, 0.1f, 10000.0f);
        } else if (selected->collider.type == ColliderType::Circle) {
            ImGui::DragFloat("Radius", &selected->collider.radius, 1.0f, 0.1f, 10000.0f);
        }
        }

        if (selected->hasCamera) {
            CompHeader("Camera", selected->hasCamera);
            ImGui::Checkbox("Main Camera", &selected->camera.mainCamera);
            ImGui::Checkbox("Perspective (3D)", &selected->camera.perspective);
            if (selected->camera.perspective)
                ImGui::DragFloat("Field of View", &selected->camera.fov, 0.5f, 20.0f, 120.0f, "%.0f°");
            else
                ImGui::DragFloat("Zoom", &selected->camera.zoom, 0.01f, 0.1f, 20.0f);
            ImGui::DragFloat2("Viewport Offset", &selected->camera.offset.x, 1.0f);
            // Follow: цель = сущность из сцены (не сама камера)
            {
                auto& ents = sceneManager->GetEntities();
                std::vector<std::string> followNames; followNames.push_back("(нет)");
                int curIdx = 0, i = 1;
                for (auto& e : ents) {
                    if (e.id == selected->id) continue;
                    followNames.push_back(e.name);
                    if (e.id == selected->camera.followTargetId) curIdx = i;
                    i++;
                }
                ImGui::SetNextItemWidth(170.0f);
                if (ImGui::BeginCombo("Follow Target", followNames[std::clamp(curIdx, 0, (int)followNames.size() - 1)].c_str())) {
                    for (int fi = 0; fi < (int)followNames.size(); fi++) {
                        if (ImGui::Selectable(followNames[fi].c_str(), fi == curIdx)) {
                            if (fi == 0) selected->camera.followTargetId = 0;
                            else {
                                int cnt = 0;
                                for (auto& e : ents) {
                                    if (e.id == selected->id) continue;
                                    if (++cnt == fi) { selected->camera.followTargetId = e.id; break; }
                                }
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
                if (selected->camera.followTargetId != 0) {
                    ImGui::DragFloat("Follow Damping", &selected->camera.followDamping, 0.01f, 0.01f, 2.0f, "%.2f s");
                    ImGui::DragFloat2("Follow Offset", &selected->camera.followOffset.x, 1.0f);
                }
                ImGui::Checkbox("Level Bounds", &selected->camera.useBounds);
                if (selected->camera.useBounds) {
                    ImGui::DragFloat2("Bounds min", &selected->camera.bounds.x, 1.0f);
                    ImGui::DragFloat2("Bounds size", &selected->camera.bounds.z, 1.0f, 1.0f);
                }
            }
        }

        // --- UI Element ---
        if (selected->hasUI) {
        CompHeader("UI Element", selected->hasUI);
        {
            const char* kinds[] = { "Button", "Text", "Slider", "Checkbox", "Progress Bar" };
            int k = static_cast<int>(selected->ui.kind);
            if (ImGui::Combo("Kind", &k, kinds, 5)) selected->ui.kind = static_cast<UIKind>(k);

            ImGui::InputText("Label", m_UILabelBuffer, sizeof(m_UILabelBuffer));
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                selected->ui.label = m_UILabelBuffer;
            } else if (!ImGui::IsItemActive() && selected->ui.label != m_UILabelBuffer) {
                snprintf(m_UILabelBuffer, sizeof(m_UILabelBuffer), "%s", selected->ui.label.c_str());
            }

            if (selected->ui.kind == UIKind::Checkbox) {
                bool checked = selected->ui.value >= 0.5f;
                if (ImGui::Checkbox("Checked", &checked)) selected->ui.value = checked ? 1.0f : 0.0f;
            } else if (selected->ui.kind == UIKind::Slider || selected->ui.kind == UIKind::ProgressBar) {
                ImGui::DragFloat("Min", &selected->ui.minValue, 0.01f);
                ImGui::DragFloat("Max", &selected->ui.maxValue, 0.01f, selected->ui.minValue + 0.001f);
                ImGui::DragFloat("Value", &selected->ui.value, 0.01f,
                                 selected->ui.minValue, selected->ui.maxValue);
            }
            ImGui::Checkbox("Interactable", &selected->ui.interactable);

            ImGui::SeparatorText("Style");
            ImGui::ColorEdit4("Text Color", &selected->ui.textColor.r);
            ImGui::ColorEdit4("Bg Color", &selected->ui.bgColor.r);
            const char* fontNames[] = { "Default", "Medium", "Large" };
            int fs = selected->ui.fontScale;
            if (ImGui::Combo("Font", &fs, fontNames, 3)) selected->ui.fontScale = fs;

            ImGui::TextDisabled("Position/Size — мировые единицы (совпадают с gizmo в Scene);\n"
                                "в Game-view экранные пиксели = масштаб камеры Game-view.");
        }
        }

        // --- Audio Source ---
        if (selected->hasAudio) {
        CompHeader("Audio Source", selected->hasAudio);
        ImGui::InputText("Clip Path", m_AudioPathBuffer, sizeof(m_AudioPathBuffer));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            selected->audio.path = m_AudioPathBuffer;
        } else if (!ImGui::IsItemActive() && selected->audio.path != m_AudioPathBuffer) {
            snprintf(m_AudioPathBuffer, sizeof(m_AudioPathBuffer), "%s", selected->audio.path.c_str());
        }
        ImGui::DragFloat("Volume", &selected->audio.volume, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Pitch", &selected->audio.pitch, 0.01f, 0.1f, 3.0f);
        {
            const char* groups[] = { "SFX", "Music" };
            int g = selected->audio.group == 1 ? 1 : 0;
            if (ImGui::Combo("Group", &g, groups, 2)) selected->audio.group = g;
        }
        ImGui::Checkbox("Loop##audio", &selected->audio.loop);
        ImGui::Checkbox("Play On Awake##audio", &selected->audio.playOnAwake);
        if (!selected->audio.path.empty()) {
            if (ImGui::Button("Preview")) Audio::PlayOneShot(selected->audio.path, selected->audio.volume, selected->audio.pitch);
            ImGui::SameLine();
            if (ImGui::Button("Stop Preview")) Audio::StopAll();
        }
        }

        // --- Script ---
        if (selected->hasScript || !selected->scriptPath.empty()) {
        if (selected->hasScript) CompHeader("Script (C++)", selected->hasScript);
        else { ImGui::Separator(); ImGui::Text("Script (C++)"); }
        std::vector<std::string> scriptPaths;
        if (fs::is_directory("assets/scripts", ec)) {
            for (const auto& entry : fs::directory_iterator("assets/scripts", ec)) {
                if (entry.is_directory()) continue;
                if (ExtLower(entry.path()) == ".cpp")
                    scriptPaths.push_back("assets/scripts/" + entry.path().filename().string());
            }
        }
        std::sort(scriptPaths.begin(), scriptPaths.end());
        std::string scriptPreview = selected->scriptPath.empty()
            ? "(none)" : fs::path(selected->scriptPath).filename().string();
        if (ImGui::BeginCombo("##script", scriptPreview.c_str())) {
            if (ImGui::Selectable("(none)", selected->scriptPath.empty()))
                selected->scriptPath.clear();
            for (const auto& sp : scriptPaths) {
                if (ImGui::Selectable(fs::path(sp).filename().string().c_str(),
                                      selected->scriptPath == sp))
                    selected->scriptPath = sp;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##script")) selected->scriptPath.clear();
        if (!selected->scriptPath.empty() && !fs::exists(selected->scriptPath, ec))
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Файл не найден: %s", selected->scriptPath.c_str());
        ImGui::TextDisabled("Компилируется при входе в Play; API: Script, Owner(), Scene(), SCRIPT_ENTRY(Класс)");

        // --- Переменные скрипта: скан DefineVar из исходника (работает в Edit, как Unity) ---
        {
            std::vector<std::pair<std::string, float>> discovered;
            if (!selected->scriptPath.empty()) DiscoverScriptVars(selected->scriptPath, discovered);
            std::map<std::string, float> merged;
            for (auto& [n, d] : discovered) merged[n] = d;
            for (auto& [n, v] : selected->vars) merged[n] = v; // значение из сцены важнее дефолта
            if (!merged.empty()) {
                ImGui::Separator();
                ImGui::Text("Script Variables");
                int varIdx = 0;
                for (auto& [name, def] : merged) {
                    if (!selected->vars.count(name)) selected->vars[name] = def; // материализуем дефолт
                    float& val = selected->vars[name];
                    std::string label = name + "##svar" + std::to_string(varIdx++);
                    ImGui::DragFloat(label.c_str(), &val, 0.01f);
                }
                ImGui::TextDisabled("Из DefineVar(\"имя\", дефолт) в скрипте; хранятся в сцене.");
            }
        }
        }

        // --- Add Component (как в Unity) ---
        ImGui::Separator();
        if (ImGui::Button("+ Add Component", ImVec2(160, 0))) ImGui::OpenPopup("AddComponentPopup");
        if (ImGui::BeginPopup("AddComponentPopup")) {
            m_PopupOpen = true;
            if (!selected->hasRigidbody && ImGui::MenuItem("Rigidbody")) selected->hasRigidbody = true;
            if (!selected->hasCollider && ImGui::MenuItem("Collider")) {
                selected->hasCollider = true;
                if (selected->collider.type == ColliderType::None)
                    selected->collider.type = (selected->sprite.type == SpriteType::Circle) ? ColliderType::Circle : ColliderType::Box;
            }
            if (!selected->hasAudio && ImGui::MenuItem("Audio Source")) selected->hasAudio = true;
            if (!selected->hasScript && selected->scriptPath.empty() && ImGui::MenuItem("Script (C++)")) selected->hasScript = true;
            if (!selected->hasParticles && ImGui::MenuItem("Particle Emitter")) selected->hasParticles = true;
            if (!selected->hasTilemap && ImGui::MenuItem("Tilemap")) {
                selected->hasTilemap = true;
                selected->tilemap.cells.assign((size_t)std::max(selected->tilemap.width, 1) *
                                               std::max(selected->tilemap.height, 1), -1);
            }
            if (!selected->hasUI && ImGui::MenuItem("UI Element")) selected->hasUI = true;
            if (!selected->hasCamera && ImGui::MenuItem("Camera")) selected->hasCamera = true;
            if (!animPresent && ImGui::MenuItem("Sprite Animation")) {
                selected->animation.active = true;
                selected->animation.texturePath = selected->sprite.texturePath;
                snprintf(m_AnimTextureBuffer, sizeof(m_AnimTextureBuffer), "%s", selected->animation.texturePath.c_str());
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextDisabled("Select an object to inspect");
    }
    ImGui::End();
}

// ===== PROJECT PANEL: Unity-подобный браузер ассетов =====
void GUI::RenderProject(EditorContext& ctx) {
    ImGui::Begin("Project");
    SceneManager* sceneManager = ctx.sceneManager;

    // --- Вкладки Assets / Console ---
    {
        int errCount = ConsoleLog::ErrorCount();
        char consoleLabel[64];
        if (errCount > 0) snprintf(consoleLabel, sizeof(consoleLabel), "Console (%d)###consoletab", errCount);
        else snprintf(consoleLabel, sizeof(consoleLabel), "Console###consoletab");

        if (ImGui::BeginTabBar("##projectTabs")) {
            if (ImGui::BeginTabItem("Assets###assetstab")) { m_ProjectConsoleTab = false; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(consoleLabel)) { m_ProjectConsoleTab = true; ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        if (m_ProjectConsoleTab) {
            if (ImGui::Button("Clear")) ConsoleLog::Clear();
            ImGui::SameLine();
            if (ImGui::Button("Copy All")) {
                std::string all;
                for (const auto& e : ConsoleLog::Entries()) { all += e.text; all += '\n'; }
                if (!all.empty()) ImGui::SetClipboardText(all.c_str());
            }
            ImGui::SameLine();
            ImGui::Checkbox("Follow", &m_ConsoleFollow);
            ImGui::SameLine();
            ImGui::TextDisabled("%zu строк — клик по строке копирует её", ConsoleLog::Entries().size());

            const auto& entries = ConsoleLog::Entries();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
            ImGui::BeginChild("##console", ImVec2(0, 0), false);
            // до добавления строк: если и так внизу — подстраховать за выводом
            bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (size_t i = 0; i < entries.size(); i++) {
                const auto& e = entries[i];
                ImGui::PushID(static_cast<int>(i));
                ImVec2 p = ImGui::GetCursorScreenPos();
                float lineH = ImGui::GetTextLineHeight();
                float textW = ImGui::CalcTextSize(e.text.c_str()).x;
                ImGui::InvisibleButton("##ln", ImVec2(ImMax(textW + 8.0f, ImGui::GetContentRegionAvail().x), lineH));
                if (ImGui::IsItemHovered())
                    dl->AddRectFilled(p, ImVec2(p.x + ImGui::GetItemRectSize().x, p.y + lineH),
                                      IM_COL32(255, 255, 255, 18));
                if (ImGui::IsItemClicked())
                    ImGui::SetClipboardText(e.text.c_str());
                dl->AddText(ImVec2(p.x + 2.0f, p.y),
                            e.error ? IM_COL32(255, 115, 100, 255) : IM_COL32(215, 215, 215, 255),
                            e.text.c_str());
                ImGui::PopID();
            }
            if (m_ConsoleFollow && atBottom) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::PopStyleVar();

            m_ProjectPanelFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            ImGui::End();
            return;
        }
    }

    std::error_code ec;
    if (!fs::is_directory(m_BrowsePath, ec)) m_BrowsePath = "assets";

    // --- Тулбар ---
    if (ImGui::Button("<")) {
        fs::path p(m_BrowsePath);
        if (p != "assets" && p.has_parent_path()) m_BrowsePath = p.parent_path().string();
    }
    ImGui::SameLine();
    RenderBreadcrumb(m_BrowsePath, "assets");

    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 230.0f);
    ImGui::SetNextItemWidth(150.0f);
    ImGui::InputTextWithHint("##search", "Search...", m_ProjectSearch, sizeof(m_ProjectSearch));
    ImGui::SameLine();
    if (ImGui::Button(m_ProjectGrid ? "[Grid]" : "[List]")) m_ProjectGrid = !m_ProjectGrid;

    if (ImGui::Button("Create")) ImGui::OpenPopup("CreateMenu");
    if (ImGui::BeginPopup("CreateMenu")) {
        m_PopupOpen = true;
        if (ImGui::MenuItem("Folder")) {
            fs::path created = UniquePath(m_BrowsePath, "New Folder", "");
            fs::create_directories(created, ec);
            if (!ec) m_SelectedAsset = created.string();
        }
        if (ImGui::MenuItem("Shader (vert+frag pair)")) {
            fs::path vert = UniquePath(m_BrowsePath == "assets" ? fs::path("assets/shaders") : fs::path(m_BrowsePath),
                                       "NewShader", ".vert");
            fs::create_directories(vert.parent_path(), ec);
            std::string stem = vert.stem().string();
            std::ofstream vf(vert);
            if (vf.is_open()) {
                vf << "// API движка: a_Pos, u_MVP, u_Model, u_ViewProj, u_Time, u_ScreenSize, EngineUV()\n"
                      "out vec2 vUV;\n"
                      "void main() {\n"
                      "    vUV = EngineUV();\n"
                      "    gl_Position = u_MVP * vec4(a_Pos, 0.0, 1.0);\n"
                      "}\n";
                vf.close();
                fs::path frag = vert;
                frag.replace_extension(".frag");
                std::ofstream ff(frag);
                if (ff.is_open()) {
                    ff << "// API движка: fragColor, u_Color, u_Texture, u_Time, u_ScreenSize, EngineCircleMask(uv)\n"
                          "in vec2 vUV;\n"
                          "void main() {\n"
                          "    fragColor = vec4(u_Color, 1.0) * texture(u_Texture, vUV);\n"
                          "}\n";
                    ff.close();
                }
                m_SelectedAsset = vert.string();
            }
        }
        if (ImGui::MenuItem("Script (.cpp)")) {
            fs::path cpp = UniquePath(m_BrowsePath == "assets" ? fs::path("assets/scripts") : fs::path(m_BrowsePath),
                                      "NewScript", ".cpp");
            fs::create_directories(cpp.parent_path(), ec);
            std::string cls = cpp.stem().string();
            char upper = cls.empty() ? 'S' : static_cast<char>(std::toupper(static_cast<unsigned char>(cls[0])));
            cls = upper + cls.substr(1);
            std::ofstream cf(cpp);
            if (cf.is_open()) {
                cf << "// Движок сам подключает ScriptAPI и базовые заголовки — инклюды не нужны.\n"
                      "// Хуки: Start(), Update(dt), OnDestroy(). Хелперы: Owner(), Scene(), WorldPosition(), Translate().\n\n"
                      "class " << cls << " : public Script {\n"
                      "public:\n"
                      "    void Update(float dt) override {\n"
                      "        Entity* e = Owner();\n"
                      "        if (!e) return;\n"
                      "        (void)dt;\n"
                      "    }\n"
                      "};\n\n"
                      "SCRIPT_ENTRY(" << cls << ")\n";
                m_SelectedAsset = cpp.string();
                OpenCodeFile(cpp.string());
            }
        }
        if (ImGui::MenuItem("Import File...")) {
            m_FolderPickerTarget = 3;
            m_FolderPickerPickFile = true;
            m_FolderPickerPath = GuiHomeDir().string();
        }
        ImGui::EndPopup();
    }

    // --- Список содержимого ---
    std::vector<fs::path> dirs, files;
    const std::string filter = ToLower(m_ProjectSearch);
    for (const auto& entry : fs::directory_iterator(m_BrowsePath, ec)) {
        std::string name = ToLower(entry.path().filename().string());
        if (!filter.empty() && name.find(filter) == std::string::npos) continue;
        if (entry.is_directory()) dirs.push_back(entry.path());
        else files.push_back(entry.path());
    }
    auto byName = [](const fs::path& a, const fs::path& b) {
        return a.filename().string() < b.filename().string();
    };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    if (ec) ImGui::TextDisabled("(unreadable folder)");
    else if (dirs.empty() && files.empty()) ImGui::TextDisabled("(empty — use Create)");

    // Общая логика двойного клика и контекстного меню для папки/файла
    auto assetContext = [&](const fs::path& path, bool isDir) {
        if (ImGui::BeginPopup("AssetCtx")) {
            m_PopupOpen = true;
            std::string ext = isDir ? "" : ExtLower(path);
            if (isDir) {
                if (ImGui::MenuItem("Open")) m_BrowsePath = path.string();
            }
            if (ext == ".prefab" && ImGui::MenuItem("Instantiate in Scene")) {
                InstantiatePrefab(ctx, path.string(), ctx.camera->GetPosition());
            }
            if (IsImageExt(ext) && sceneManager->GetSelectedEntityPtr() &&
                ImGui::MenuItem("Assign to Selected Object")) {
                sceneManager->GetSelectedEntityPtr()->sprite.texturePath = path.string();
            }
            if ((ext == ".vert" || ext == ".frag") && sceneManager->GetSelectedEntityPtr() &&
                ImGui::MenuItem("Assign Shader to Selected")) {
                fs::path base = path;
                base.replace_extension("");
                std::string basePath = base.string();
                fs::path other = path;
                other.replace_extension(ext == ".vert" ? ".frag" : ".vert");
                if (ext == ".frag" || fs::exists(other))
                    sceneManager->GetSelectedEntityPtr()->sprite.shaderPath = basePath;
            }
            if (!isDir && IsCodeExt(ext) && ImGui::MenuItem("Edit (built-in IDE)")) {
                OpenCodeFile(path.string());
            }
            if (!isDir && ImGui::MenuItem("Open Externally")) {
                OpenExternally(path.string());
            }
            if (path != "assets") {
                if (ImGui::MenuItem("Rename")) {
                    m_RenameAssetPath = path.string();
                    snprintf(m_AssetNameBuffer, sizeof(m_AssetNameBuffer), "%s",
                             path.filename().string().c_str());
                }
                if (ImGui::MenuItem("Delete")) {
                    m_PendingDelete = path.string();
                    m_ShowDeleteConfirm = true;
                }
            }
            ImGui::EndPopup();
        }
    };

    auto onDouble = [&](const fs::path& path, bool isDir) {
        if (isDir) { m_BrowsePath = path.string(); return; }
        std::string ext = ExtLower(path);
        if (ext == ".scene") {
            RequestOpenScene(ctx, path.string());
        } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") {
            Audio::PlayOneShot(path.string()); // превью клипа в редакторе
        } else if (IsImageExt(ext)) {
            Entity* ent = sceneManager->GetSelectedEntityPtr();
            if (ent) ent->sprite.texturePath = path.string();
        } else if (ext == ".prefab") {
            InstantiatePrefab(ctx, path.string(), ctx.camera->GetPosition());
        } else if (ext == ".vert" || ext == ".frag") {
            Entity* ent = sceneManager->GetSelectedEntityPtr();
            if (ent) {
                fs::path base = path;
                base.replace_extension("");
                fs::path other = path;
                other.replace_extension(ext == ".vert" ? ".frag" : ".vert");
                if (ext == ".frag" || fs::exists(other)) ent->sprite.shaderPath = base.string();
            }
            if (ext == ".vert" || ext == ".frag") OpenCodeFile(path.string()); // и назначили, и открыли
        } else if (IsCodeExt(ext)) {
            OpenCodeFile(path.string());
        }
    };

    auto dragSource = [&](const fs::path& path) {
        std::string ext = ExtLower(path);
        const char* payload = nullptr;
        if (IsImageExt(ext) || ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
            payload = "ASSET_PATH";
        else if (ext == ".prefab") payload = "PREFAB_PATH";
        if (!payload) return;
        if (ImGui::BeginDragDropSource()) {
            std::string s = path.string();
            ImGui::SetDragDropPayload(payload, s.c_str(), s.size() + 1);
            ImGui::Text("%s", path.filename().string().c_str());
            ImGui::EndDragDropSource();
        }
    };

    if (m_ProjectGrid) {
        // --- Grid ---
        const float tileW = 104.0f, tileH = 104.0f;
        int cols = ImMax(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (tileW + 6.0f)));
        int i = 0;
        auto tile = [&](const fs::path& path, bool isDir) {
            if (i > 0 && i % cols == 0) ImGui::NewLine();
            else if (i > 0) ImGui::SameLine();
            i++;
            ImGui::PushID(path.string().c_str());
            bool selected = (m_SelectedAsset == path.string());
            GLuint tex = 0;
            if (!isDir && IsImageExt(ExtLower(path))) tex = ctx.renderer->GetTexture(path.string());

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##tile", ImVec2(tileW, tileH));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) m_SelectedAsset = path.string();
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) onDouble(path, isDir);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                m_SelectedAsset = path.string();
                ImGui::OpenPopup("AssetCtx");
            }
            dragSource(path);

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 sz(tileW, tileH);
            ImVec2 corner(pos.x + tileW, pos.y + tileH);
            if (selected) dl->AddRectFilled(pos, corner, IM_COL32(90, 60, 25, 160));
            if (ImGui::IsItemHovered()) dl->AddRect(pos, corner, IM_COL32(160, 160, 160, 160));

            std::string name = path.filename().string();
            auto textW = [&](const char* t) {
                return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, t).x;
            };
            // иконка/превью по центру
            float iconTop = pos.y + 8.0f;
            if (tex) {
                dl->AddImage((ImTextureID)(intptr_t)tex,
                             ImVec2(pos.x + (tileW - 64) * 0.5f, iconTop),
                             ImVec2(pos.x + (tileW + 64) * 0.5f, iconTop + 64.0f));
            } else {
                DrawSysIcon(dl, ImVec2(pos.x + tileW * 0.5f, iconTop + 32.0f), 40.0f,
                            IconForExt(ExtLower(path), isDir));
            }
            // имя с обрезкой по ширине плитки
            dl->PushClipRect(pos, corner, true);
            if (textW(name.c_str()) > tileW - 6.0f) {
                size_t keep = name.size();
                while (keep > 1 && textW(name.substr(0, keep).c_str()) > tileW - 20.0f) keep--;
                name = name.substr(0, keep) + "..";
            }
            dl->AddText(ImVec2(pos.x + (tileW - textW(name.c_str())) * 0.5f, pos.y + tileH - 24.0f),
                        IM_COL32(220, 220, 220, 255), name.c_str());
            dl->PopClipRect();

            assetContext(path, isDir);
            ImGui::PopID();
        };
        for (const auto& d : dirs) tile(d, true);
        for (const auto& f : files) tile(f, false);
        if (i) ImGui::NewLine();
    } else {
        // --- List ---
        auto row = [&](const fs::path& path, bool isDir) {
            ImGui::PushID(path.string().c_str());
            bool selected = (m_SelectedAsset == path.string());
            GLuint tex = 0;
            if (!isDir && IsImageExt(ExtLower(path))) tex = ctx.renderer->GetTexture(path.string());
            if (tex) {
                float th = ImGui::GetTextLineHeightWithSpacing() * 1.2f;
                ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(th, th));
                ImGui::SameLine();
            }
            std::string label = "        " + path.filename().string();
            if (ImGui::Selectable(label.c_str(), selected, ImGuiTreeNodeFlags_SpanAvailWidth))
                m_SelectedAsset = path.string();
            {
                ImVec2 rmin = ImGui::GetItemRectMin();
                DrawSysIcon(ImGui::GetWindowDrawList(),
                            ImVec2(rmin.x + 14.0f, rmin.y + ImGui::GetItemRectSize().y * 0.5f),
                            ImGui::GetItemRectSize().y - 6.0f, IconForExt(ExtLower(path), isDir));
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                onDouble(path, isDir);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                m_SelectedAsset = path.string();
                ImGui::OpenPopup("AssetCtx");
            }
            if (tex && ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(256, 256));
                ImGui::EndTooltip();
            }
            dragSource(path);
            assetContext(path, isDir);
            ImGui::PopID();
        };
        for (const auto& d : dirs) row(d, true);
        for (const auto& f : files) row(f, false);
    }

    // Дроп объекта из Hierarchy -> сохранить как префаб в текущую папку
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
            size_t idx = *(const size_t*)p->Data;
            auto& ents = sceneManager->GetEntities();
            if (idx < ents.size()) {
                m_ShowPrefabSaveDialog = true;
                m_PrefabSourceIndex = static_cast<int>(idx);
                m_PrefabSaveDir = m_BrowsePath;
                snprintf(m_PrefabNameBuf, sizeof(m_PrefabNameBuf), "%s", ents[idx].name.c_str());
            }
        }
        ImGui::EndDragDropTarget();
    }

    m_ProjectPanelFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    // ПКМ по пустому месту Assets: создать/импортировать
    if (ImGui::BeginPopupContextWindow("##projEmptyCtx",
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        m_PopupOpen = true;
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                std::error_code ec;
                fs::path created = UniquePath(m_BrowsePath, "New Folder", "");
                fs::create_directories(created, ec);
                if (!ec) m_SelectedAsset = created.string();
            }
            if (ImGui::MenuItem("Shader (frag)")) {
                std::error_code ec;
                fs::path frag = UniquePath(m_BrowsePath == "assets" ? fs::path("assets/shaders") : fs::path(m_BrowsePath),
                                           "NewShader", ".frag");
                fs::create_directories(frag.parent_path(), ec);
                std::ofstream ff(frag);
                if (ff.is_open()) {
                    ff << "// API движка: fragColor, u_Color, u_Texture, u_Time, u_Params, u_PColor, v_UV\n"
                          "void main() {\n"
                          "    fragColor = vec4(u_Color, 1.0) * texture(u_Texture, v_UV);\n"
                          "}\n";
                    ff.close();
                    m_SelectedAsset = frag.string();
                }
            }
            if (ImGui::MenuItem("Script (.cpp)")) {
                std::error_code ec;
                fs::path cpp = UniquePath(m_BrowsePath == "assets" ? fs::path("assets/scripts") : fs::path(m_BrowsePath),
                                          "NewScript", ".cpp");
                fs::create_directories(cpp.parent_path(), ec);
                std::string cls = cpp.stem().string();
                char upper = cls.empty() ? 'S' : static_cast<char>(std::toupper(static_cast<unsigned char>(cls[0])));
                cls = upper + cls.substr(1);
                std::ofstream cf(cpp);
                if (cf.is_open()) {
                    cf << "// Движок сам подключает ScriptAPI и базовые заголовки — инклюды не нужны.\n"
                          "class " << cls << " : public Script {\n"
                          "public:\n"
                          "    void Start() override { DefineVar(\"speed\", 90.0f); }\n"
                          "    void Update(float dt) override {\n"
                          "        Entity* e = Owner();\n"
                          "        if (e) e->transform.rotation += GetVar(\"speed\") * dt;\n"
                          "    }\n"
                          "};\n\n"
                          "SCRIPT_ENTRY(" << cls << ")\n";
                    m_SelectedAsset = cpp.string();
                    OpenCodeFile(cpp.string());
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Import File...")) {
            m_FolderPickerTarget = 3;
            m_FolderPickerPickFile = true;
            m_FolderPickerPath = GuiHomeDir().string();
        }
        if (!m_Clipboard.empty() && ImGui::MenuItem("Paste Entity", "Ctrl+V"))
            PasteClipboard(ctx.sceneManager);
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ===== BROWSER СЦЕН =====
void GUI::RenderSceneBrowser(EditorContext& ctx) {
    if (!m_ShowSceneBrowser) return;

    const char* title = m_SceneBrowserSave ? "Save Scene" : "Open Scene";
    ImGui::OpenPopup(title);
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(title, &m_ShowSceneBrowser, 0)) {
        m_PopupOpen = true;
        std::error_code ec;
        if (!fs::is_directory(m_SceneBrowserDir, ec)) m_SceneBrowserDir = "assets";

        if (ImGui::Button("..")) {
            fs::path p(m_SceneBrowserDir);
            if (p != "assets" && p.has_parent_path()) m_SceneBrowserDir = p.parent_path().string();
        }
        ImGui::SameLine();
        RenderBreadcrumb(m_SceneBrowserDir, "assets");
        ImGui::Separator();

        // папки
        std::vector<fs::path> dirs, scenes;
        for (const auto& entry : fs::directory_iterator(m_SceneBrowserDir, ec)) {
            if (entry.is_directory()) dirs.push_back(entry.path());
            else if (ExtLower(entry.path()) == ".scene") scenes.push_back(entry.path());
        }
        auto byName = [](const fs::path& a, const fs::path& b) {
            return a.filename().string() < b.filename().string();
        };
        std::sort(dirs.begin(), dirs.end(), byName);
        std::sort(scenes.begin(), scenes.end(), byName);

        ImGui::BeginChild("SceneList");
        for (const auto& d : dirs) {
            ImGui::PushID(d.string().c_str());
            std::string label = "[dir] " + d.filename().string();
            if (ImGui::Selectable(label.c_str(), false, ImGuiTreeNodeFlags_SpanAvailWidth) ||
                (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)))
                m_SceneBrowserDir = d.string();
            ImGui::PopID();
        }
        for (const auto& s : scenes) {
            ImGui::PushID(s.string().c_str());
            std::string label = "[scn] " + s.filename().string();
            bool sel = (m_SceneBrowserSelected == s.string());
            if (ImGui::Selectable(label.c_str(), sel, ImGuiTreeNodeFlags_SpanAvailWidth)) {
                m_SceneBrowserSelected = s.string();
                if (!m_SceneBrowserSave) {
                    RequestOpenScene(ctx, m_SceneBrowserSelected);
                    m_ShowSceneBrowser = false;
                } else {
                    snprintf(m_SceneBrowserNameBuf, sizeof(m_SceneBrowserNameBuf), "%s",
                             s.filename().string().c_str());
                }
            }
            if (sel && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !m_SceneBrowserSave) {
                RequestOpenScene(ctx, m_SceneBrowserSelected);
                m_ShowSceneBrowser = false;
            }
            ImGui::PopID();
        }
        if (ec) ImGui::TextDisabled("(unreadable folder)");
        else if (dirs.empty() && scenes.empty()) ImGui::TextDisabled("(no scenes here)");
        ImGui::EndChild();

        ImGui::Separator();
        if (m_SceneBrowserSave) {
            ImGui::InputText("Name", m_SceneBrowserNameBuf, sizeof(m_SceneBrowserNameBuf));
            std::string name = m_SceneBrowserNameBuf;
            if (name.empty()) ImGui::TextDisabled("Enter a file name");
            else {
                if (ExtLower(name) != ".scene") name += ".scene";
                ImGui::TextDisabled("-> %s", (fs::path(m_SceneBrowserDir) / name).string().c_str());
            }
        } else {
            ImGui::TextDisabled("%s", m_SceneBrowserSelected.empty()
                ? "Select a scene, double-click to open" : m_SceneBrowserSelected.c_str());
        }

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            if (m_SceneBrowserSave) {
                std::string name = m_SceneBrowserNameBuf;
                if (!name.empty()) {
                    if (ExtLower(name) != ".scene") name += ".scene";
                    fs::path full = fs::path(m_SceneBrowserDir) / name;
                    fs::create_directories(full.parent_path(), ec);
                    ctx.serializer->Save(ctx.sceneManager, full.string());
                    {
                        m_CurrentScenePath = full.string();
                        MarkSceneSaved(ctx);
                        if (!m_SceneBrowserAfterOpen.empty()) {
                            std::string open = m_SceneBrowserAfterOpen;
                            m_SceneBrowserAfterOpen.clear();
                            LoadSceneAsset(ctx, open);
                        } else if (m_SceneBrowserAfterNew) {
                            m_SceneBrowserAfterNew = false;
                            DoNewScene(ctx);
                        }
                        m_ShowSceneBrowser = false;
                    }
                }
            } else if (!m_SceneBrowserSelected.empty()) {
                RequestOpenScene(ctx, m_SceneBrowserSelected);
                m_ShowSceneBrowser = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowSceneBrowser = false;
            m_SceneBrowserAfterOpen.clear();
            m_SceneBrowserAfterNew = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_ShowSceneBrowser = false;
        ImGui::EndPopup();
    }
}

// ===== CONFIRM: несохранённая сцена =====
void GUI::RenderSceneConfirm(EditorContext& ctx) {
    if (!m_ShowSceneConfirm) return;

    ImGui::OpenPopup("Unsaved Changes");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_PopupOpen = true;
        std::string sceneName = m_CurrentScenePath.empty() ? "Untitled"
                             : fs::path(m_CurrentScenePath).filename().string();
        ImGui::Text("Scene '%s' has unsaved changes.", sceneName.c_str());
        ImGui::Spacing();

        auto finish = [&](bool doAction) {
            m_ShowSceneConfirm = false;
            if (!doAction) { m_PendingAction = 0; m_PendingScenePath.clear(); return; }
            int action = m_PendingAction;
            std::string path = m_PendingScenePath;
            m_PendingAction = 0;
            m_PendingScenePath.clear();
            if (action == 1) DoNewScene(ctx);
            else if (action == 2) LoadSceneAsset(ctx, path);
        };

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (m_CurrentScenePath.empty()) {
                // новая сцена: сначала Save As, действие продолжится после сохранения
                m_SceneBrowserAfterOpen = (m_PendingAction == 2) ? m_PendingScenePath : "";
                m_SceneBrowserAfterNew = (m_PendingAction == 1);
                m_PendingAction = 0;
                m_PendingScenePath.clear();
                OpenSceneSaveAs(ctx);
                m_ShowSceneConfirm = false;
            } else {
                SaveSceneNow(ctx);
                finish(true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
            finish(true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            finish(false);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            finish(false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ===== SAVE AS PREFAB =====
void GUI::RenderPrefabSaveDialog(EditorContext& ctx) {
    if (!m_ShowPrefabSaveDialog) return;

    ImGui::OpenPopup("Save as Prefab");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save as Prefab", &m_ShowPrefabSaveDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_PopupOpen = true;
        auto& ents = ctx.sceneManager->GetEntities();
        bool valid = m_PrefabSourceIndex >= 0 &&
                     m_PrefabSourceIndex < static_cast<int>(ents.size());
        if (!valid) {
            ImGui::TextDisabled("Source object is gone.");
            if (ImGui::Button("Close")) m_ShowPrefabSaveDialog = false;
            ImGui::EndPopup();
            return;
        }
        Entity& root = ents[m_PrefabSourceIndex];
        std::vector<size_t> idxs;
        ctx.sceneManager->CollectSubtree(static_cast<size_t>(m_PrefabSourceIndex), idxs);
        ImGui::Text("Object: %s  (subtree: %zu entities)", root.name.c_str(), idxs.size());
        ImGui::InputText("Name", m_PrefabNameBuf, sizeof(m_PrefabNameBuf));
        ImGui::TextDisabled("-> %s/%s.prefab", m_PrefabSaveDir.c_str(), m_PrefabNameBuf);

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string name = m_PrefabNameBuf;
            for (auto& c : name) if (c == '/' || c == '\\') c = '_';
            if (name.empty()) name = "Prefab";
            std::error_code ec;
            fs::create_directories(m_PrefabSaveDir, ec);
            fs::path full = fs::path(m_PrefabSaveDir) / (name + ".prefab");
            if (ctx.serializer->SaveEntities(ctx.sceneManager->GetSubtree(
                    static_cast<size_t>(m_PrefabSourceIndex)), full.string())) {
                ents[m_PrefabSourceIndex].prefabSource = full.string();
                m_ShowPrefabSaveDialog = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowPrefabSaveDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void GUI::RenderAssetDialogs(EditorContext& ctx) {
    (void)ctx;
    // Переименование
    if (!m_RenameAssetPath.empty()) {
        ImGui::OpenPopup("Rename Asset");
        if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_PopupOpen = true;
            ImGui::Text("%s", m_RenameAssetPath.c_str());
            ImGui::InputText("Name", m_AssetNameBuffer, sizeof(m_AssetNameBuffer));
            if (ImGui::Button("OK")) {
                fs::path oldPath(m_RenameAssetPath);
                fs::path newPath = oldPath.parent_path() / m_AssetNameBuffer;
                std::error_code ec;
                fs::rename(oldPath, newPath, ec);
                if (m_SelectedAsset == m_RenameAssetPath) m_SelectedAsset = newPath.string();
                m_RenameAssetPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_RenameAssetPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Удаление с подтверждением
    if (m_ShowDeleteConfirm) {
        ImGui::OpenPopup("Delete Asset");
        if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_PopupOpen = true;
            ImGui::Text("Delete permanently?\n%s", m_PendingDelete.c_str());
            if (ImGui::Button("Delete")) {
                std::error_code ec;
                fs::remove_all(m_PendingDelete, ec);
                if (m_SelectedAsset == m_PendingDelete) m_SelectedAsset.clear();
                if (m_BrowsePath == m_PendingDelete ||
                    ToLower(m_BrowsePath).find(ToLower(m_PendingDelete) + "/") == 0)
                    m_BrowsePath = "assets";
                m_PendingDelete.clear();
                m_ShowDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_PendingDelete.clear();
                m_ShowDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void GUI::RenderProjectDialogs(EditorContext& ctx) {
    if (m_ShowNewProjectDialog) {
        ImGui::OpenPopup("New Project");
        if (ImGui::BeginPopupModal("New Project", &m_ShowNewProjectDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_PopupOpen = true;
            ImGui::InputText("Name", m_NewProjectName, sizeof(m_NewProjectName));
            ImGui::InputText("Location", m_NewProjectLocation, sizeof(m_NewProjectLocation));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                m_FolderPickerTarget = 1;
                m_FolderPickerPath = m_NewProjectLocation[0] ? m_NewProjectLocation : GuiHomeDir().string();
            }
            if (!m_ProjectError.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_ProjectError.c_str());
            }
            if (ImGui::Button("Create")) {
                if (ctx.projectManager->CreateProject(m_NewProjectLocation, m_NewProjectName)) {
                    ApplyProject(ctx);
                    m_ShowNewProjectDialog = false;
                    m_ShowProjectManagerWindow = false;
                } else {
                    m_ProjectError = ctx.projectManager->LastError();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_ShowNewProjectDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (m_ShowOpenProjectDialog) {
        ImGui::OpenPopup("Open Project");
        if (ImGui::BeginPopupModal("Open Project", &m_ShowOpenProjectDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_PopupOpen = true;
            ImGui::InputText("Project folder", m_OpenProjectPath, sizeof(m_OpenProjectPath));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                m_FolderPickerTarget = 2;
                m_FolderPickerPath = m_OpenProjectPath[0] ? m_OpenProjectPath : GuiHomeDir().string();
            }
            if (!m_ProjectError.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_ProjectError.c_str());
            }
            if (ImGui::Button("Open")) {
                if (ctx.projectManager->OpenProject(m_OpenProjectPath)) {
                    ApplyProject(ctx);
                    m_ShowOpenProjectDialog = false;
                } else {
                    m_ProjectError = ctx.projectManager->LastError();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_ShowOpenProjectDialog = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (m_ShowProjectManagerWindow) {
        ImGui::Begin("Project Manager", &m_ShowProjectManagerWindow);
        ImGui::Text("Current project: %s", ctx.projectManager->CurrentProjectName().c_str());
        ImGui::Separator();
        if (ImGui::Button("New Project...")) m_ShowNewProjectDialog = true;
        ImGui::SameLine();
        if (ImGui::Button("Open Project...")) m_ShowOpenProjectDialog = true;
        ImGui::Separator();
        ImGui::Text("Recent projects:");
        const auto& recent = ctx.projectManager->GetRecent();
        if (recent.empty()) ImGui::TextDisabled("(none)");
        for (const auto& path : recent) {
            std::string label = fs::path(path).filename().string() + "  ##" + path;
            if (ImGui::Selectable(label.c_str())) {
                if (ctx.projectManager->OpenProject(path)) {
                    ApplyProject(ctx);
                    m_ShowProjectManagerWindow = false;
                } else {
                    m_ProjectError = ctx.projectManager->LastError();
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
        }
        ImGui::End();
    }
}

void GUI::RenderFolderPicker(EditorContext& ctx) {
    if (m_FolderPickerTarget == 0) return;

    // Обычное плавающее окно (не модалка): вложенные модалки ImGui ломаются,
    // а такое окно работает поверх любого диалога и не блокирует ввод при осечке
    const char* title = m_FolderPickerPickFile ? "Select File to Import###astraFolderPicker"
                                               : "Select Folder###astraFolderPicker";
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + (vp->Size.x - 480) * 0.5f,
                                   vp->Pos.y + (vp->Size.y - 540) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(480, 540), ImGuiCond_Appearing);
    bool open = true;
    if (ImGui::Begin(title, &open, ImGuiWindowFlags_NoDocking)) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());
        std::error_code ec;
        if (!fs::is_directory(m_FolderPickerPath, ec))
            m_FolderPickerPath = GuiHomeDir().string();

        char pathBuf[1024];
        snprintf(pathBuf, sizeof(pathBuf), "%s", m_FolderPickerPath.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
        ImGui::InputText("##path", pathBuf, sizeof(pathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Go To")) {
            if (fs::is_directory(pathBuf)) m_FolderPickerPath = pathBuf;
        }

        ImGui::Separator();
        if (ImGui::Button(".. Parent")) {
            fs::path p(m_FolderPickerPath);
            if (p.has_parent_path()) m_FolderPickerPath = p.parent_path().string();
        }

        ImGui::SeparatorText(m_FolderPickerPickFile ? "Файлы (клик — импортировать)" : "Папки");
        ImGui::BeginChild("FolderList");
        std::vector<fs::path> subDirs, subFiles;
        for (fs::directory_iterator it(m_FolderPickerPath, fs::directory_options::skip_permission_denied, ec), endIt;
             it != endIt; ++it) {
            if (it->is_directory()) subDirs.push_back(it->path());
            else subFiles.push_back(it->path());
        }
        auto byName = [](const fs::path& a, const fs::path& b) {
            return a.filename().string() < b.filename().string();
        };
        std::sort(subDirs.begin(), subDirs.end(), byName);
        std::sort(subFiles.begin(), subFiles.end(), byName);
        for (const auto& dp : subDirs) {
            std::string label = dp.filename().string() + "/";
            if (ImGui::Selectable(label.c_str(), false, ImGuiTreeNodeFlags_SpanAvailWidth))
                m_FolderPickerPath = dp.string();
        }
        if (m_FolderPickerPickFile) {
            for (const auto& fp : subFiles) {
                std::string label = std::string(AssetIcon(ExtLower(fp))) + " " + fp.filename().string();
                if (ImGui::Selectable(label.c_str(), false, ImGuiTreeNodeFlags_SpanAvailWidth)) {
                    ImportFileToAssets(ctx, fp.string());
                    m_FolderPickerTarget = 0;
                    m_FolderPickerPickFile = false;
                }
            }
        }
        if (ec) ImGui::TextDisabled("(нечитаемая папка)");
        else if (subDirs.empty() && subFiles.empty()) ImGui::TextDisabled("(пусто)");
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled("%s", m_FolderPickerPath.c_str());
        if (!m_FolderPickerPickFile && m_FolderPickerTarget != 3) {
            if (ImGui::Button("Use This Folder", ImVec2(160, 0))) {
                if (m_FolderPickerTarget == 4) {
                    snprintf(m_BuildDirBuf, sizeof(m_BuildDirBuf), "%s", m_FolderPickerPath.c_str());
                } else {
                    char* dest = m_FolderPickerTarget == 1 ? m_NewProjectLocation : m_OpenProjectPath;
                    size_t cap = m_FolderPickerTarget == 1 ? sizeof(m_NewProjectLocation) : sizeof(m_OpenProjectPath);
                    snprintf(dest, cap, "%s", m_FolderPickerPath.c_str());
                }
                m_FolderPickerTarget = 0;
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_FolderPickerTarget = 0;
            m_FolderPickerPickFile = false;
        }
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            m_FolderPickerTarget = 0;
            m_FolderPickerPickFile = false;
        }
        if (!open) {
            m_FolderPickerTarget = 0;
            m_FolderPickerPickFile = false;
        }
    }
    ImGui::End();
}

void GUI::RenderSettings(EditorContext& ctx) {
    (void)ctx;
    if (!m_ShowSettings) return;

    ImGui::OpenPopup("Engine Settings");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("Engine Settings", &m_ShowSettings, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_PopupOpen = true;

        if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec2 g = Physics::Gravity / Physics::PixelsPerMeter; // в метрах/с^2, как в Unity
            if (ImGui::DragFloat2("Gravity", &g.x, 0.1f, -50.0f, 50.0f, "%.2f m/s²"))
                Physics::Gravity = g * Physics::PixelsPerMeter;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("0, -9.81 — как в Unity. По оси Y вверх положительно.");
            if (ImGui::Button("Reset##gravity"))
                Physics::Gravity = glm::vec2(0.0f, -9.81f) * Physics::PixelsPerMeter;
            ImGui::SameLine();
            float ppm = Physics::PixelsPerMeter;
            if (ImGui::DragFloat("Pixels per meter", &ppm, 1.0f, 1.0f, 10000.0f, "%.0f px/m")) {
                glm::vec2 g = Physics::Gravity / Physics::PixelsPerMeter;
                Physics::PixelsPerMeter = ppm;
                Physics::Gravity = g * ppm;
            }
        }

        if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit4("Background color", &AstraPrefs::ClearColor.r);
            ImGui::Checkbox("Grid in Scene", &AstraPrefs::ShowGrid);
            ImGui::Checkbox("Colliders in Scene", &AstraPrefs::ShowColliders);
            ImGui::DragFloat("Grid / move snap, px", &AstraPrefs::GridSize, 1.0f, 5.0f, 500.0f, "%.0f");
        }

        if (ImGui::CollapsingHeader("Editor", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Rotation snap, deg", &AstraPrefs::SnapDegrees, 0.5f, 1.0f, 90.0f, "%.1f");
            if (ImGui::Checkbox("Light theme", &AstraPrefs::LightTheme)) m_ThemeDirty = true;
            ImGui::TextDisabled("Undo/Redo: Ctrl+Z / Ctrl+Shift+Z");
        }

        if (ImGui::CollapsingHeader("Lighting (3D)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Sun direction", &AstraPrefs::LightDir.x, 0.02f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Sun color", &AstraPrefs::LightColor.x);
            ImGui::SliderFloat("Ambient", &AstraPrefs::Ambient, 0.0f, 1.0f, "%.2f");
        }

        if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) {
            float ts = Scripting::DefaultTimeScale();
            if (ImGui::SliderFloat("Time scale", &ts, 0.0f, 4.0f, "%.2f"))
                Scripting::SetDefaultTimeScale(ts);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Умножает dt для скриптов и физики. Применяется при входе в Play.");
            if (ImGui::Button("Reset##time"))
                Scripting::SetDefaultTimeScale(1.0f);
        }

        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
            float vol = Audio::MasterVolume();
            if (ImGui::SliderFloat("Master volume", &vol, 0.0f, 1.0f, "%.2f"))
                Audio::SetMasterVolume(vol);
            float sfx = Audio::GroupVolume(0);
            if (ImGui::SliderFloat("SFX volume", &sfx, 0.0f, 1.0f, "%.2f"))
                Audio::SetGroupVolume(0, sfx);
            float mus = Audio::GroupVolume(1);
            if (ImGui::SliderFloat("Music volume", &mus, 0.0f, 1.0f, "%.2f"))
                Audio::SetGroupVolume(1, mus);
            bool muted = Audio::IsMuted();
            if (ImGui::Checkbox("Mute", &muted))
                Audio::SetMuted(muted);
            const std::string& err = Audio::LastError();
            if (!err.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Device: %s", err.c_str());
            else if (!Audio::Init())
                ImGui::TextDisabled("Аудиоустройство ещё не создано (или недоступно).");
            else
                ImGui::TextDisabled("Устройство активно.");
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) { m_ShowSettings = false; SaveUserSettings(); }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { m_ShowSettings = false; SaveUserSettings(); }
        ImGui::EndPopup();
    }
}

void GUI::RenderNewEntityDialog(SceneManager* sceneManager) {
    if (!m_ShowNewEntityDialog) return;

    ImGui::OpenPopup("Create Entity");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Create Entity", &m_ShowNewEntityDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_PopupOpen = true;
        ImGui::Text("Name:");
        ImGui::InputText("##name", m_NewEntityName, sizeof(m_NewEntityName));

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            Entity e;
            e.name = m_NewEntityName;
            e.transform.scale = glm::vec2(100.0f, 100.0f);
            if (m_NewEntityType == 1) {
                e.sprite.type = SpriteType::Quad;
                e.hasCollider = true;
                e.collider.type = ColliderType::Box;
                e.collider.size = glm::vec2(50.0f, 50.0f);
            } else {
                e.sprite.type = SpriteType::Circle;
                e.hasCollider = true;
                e.collider.type = ColliderType::Circle;
                e.collider.radius = 50.0f;
            }
            e.sprite.color = glm::vec3(1.0f, 1.0f, 1.0f);
            sceneManager->AddEntity(e);
            m_ShowNewEntityDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowNewEntityDialog = false;
        }
        ImGui::EndPopup();
    }
}

void GUI::RenderRenameDialog(SceneManager* sceneManager) {
    if (!m_ShowRenameDialog) return;

    ImGui::OpenPopup("Rename Entity");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Rename Entity", &m_ShowRenameDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_PopupOpen = true;
        ImGui::InputText("Name", m_RenameBuffer, sizeof(m_RenameBuffer));

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            auto& entities = sceneManager->GetEntities();
            if (m_RenameIndex >= 0 && m_RenameIndex < static_cast<int>(entities.size())) {
                entities[m_RenameIndex].name = m_RenameBuffer;
            }
            m_ShowRenameDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_ShowRenameDialog = false;
        }
        ImGui::EndPopup();
    }
}

// ===== RUNTIME UI: оверлей в Game-view (мировые координаты, стиль, шрифты) =====
void GUI::RenderGameUIOverlay(EditorContext& ctx) {
    EditorState st = *ctx.state;
    GameUI::BeginFrame();
    if (!m_ShowGame || m_GameImageSize.x < 2) return;

    auto& entities = ctx.sceneManager->GetEntities();
    bool any = false;
    for (const auto& e : entities) {
        if (e.hasUI && e.active) { any = true; break; }
    }
    if (!any) return;

    const bool editable = (st != EditorState::Edit);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBackground;
    // В Edit оверлей — только превью: не должен перехватывать мышь вообще
    if (!editable) flags |= ImGuiWindowFlags_NoInputs;
    ImGui::SetNextWindowPos(m_GameImagePos);
    ImGui::SetNextWindowSize(m_GameImageSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, editable ? 1.0f : 0.9f);
    ImGui::Begin("GameUIOverlay", nullptr, flags);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());

    // мировые единицы -> пиксели Game-view (та же математика, что у орто-камеры)
    const float viewH = 1080.0f * m_GameCamera->GetZoom();
    const float s = m_GameImageSize.y / viewH;
    const glm::vec2 cam = m_GameCamera->GetPosition();

    for (auto& e : entities) {
        if (!e.hasUI || !e.active) continue;

        glm::vec2 wp = Transforms::WorldPosition(entities, e);
        glm::vec2 cpx(m_GameImageSize.x * 0.5f + (wp.x - cam.x) * s,
                      m_GameImageSize.y * 0.5f - (wp.y - cam.y) * s);

        float wpx = std::max(e.transform.scale.x * s, 16.0f);
        float hpx = std::max(e.transform.scale.y * s, 0.0f);

        const ImVec4 tc(e.ui.textColor.r, e.ui.textColor.g, e.ui.textColor.b, e.ui.textColor.a);
        ImGui::PushID(e.id);
        if (e.ui.fontScale > 0) {
            ImFont* f = (e.ui.fontScale == 1) ? m_FontMedium : m_FontLarge;
            if (f) ImGui::PushFont(f);
        }
        ImGui::SetCursorPos(ImVec2(cpx.x - wpx * 0.5f, cpx.y - hpx * 0.5f));

        const ImVec4 bg(e.ui.bgColor.r, e.ui.bgColor.g, e.ui.bgColor.b, e.ui.bgColor.a);
        auto lighter = [](ImVec4 c, float k) {
            return ImVec4(c.x + (1.0f - c.x) * k, c.y + (1.0f - c.y) * k, c.z + (1.0f - c.z) * k, c.w);
        };
        ImGui::PushStyleColor(ImGuiCol_Text, tc);

        if (!editable || !e.ui.interactable) ImGui::BeginDisabled();
        switch (e.ui.kind) {
            case UIKind::Button: {
                ImGui::PushStyleColor(ImGuiCol_Button, bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, lighter(bg, editable ? 0.25f : 0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, lighter(bg, 0.45f));
                ImVec2 sz(wpx, hpx > 4.0f ? hpx : 0.0f);
                ImGui::ButtonEx(e.ui.label.c_str(), sz);
                if (editable && e.ui.interactable && ImGui::IsItemClicked()) GameUI::ReportClick(e.id);
                ImGui::PopStyleColor(3);
            } break;
            case UIKind::Text:
                ImGui::TextUnformatted(e.ui.label.c_str());
                break;
            case UIKind::Slider: {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, lighter(bg, 0.25f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, lighter(bg, 0.4f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, lighter(bg, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1, 1, 1, 1));
                ImGui::PushItemWidth(wpx);
                ImGui::SliderFloat("##v", &e.ui.value, e.ui.minValue, e.ui.maxValue);
                if (editable) GameUI::ReportValue(e.id, e.ui.value);
                ImGui::PopItemWidth();
                ImGui::PopStyleColor(5);
            } break;
            case UIKind::Checkbox: {
                // value трактается как bool: <0.5 — false, иначе true (min/max не влияют)
                bool on = e.ui.value >= 0.5f;
                ImGui::PushStyleColor(ImGuiCol_CheckMark, tc);
                bool changed = ImGui::Checkbox("##c", &on);
                if (editable && e.ui.interactable && changed) {
                    e.ui.value = on ? 1.0f : 0.0f;
                    GameUI::ReportValue(e.id, e.ui.value);
                    GameUI::ReportClick(e.id);
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(e.ui.label.c_str());
                ImGui::PopStyleColor();
            } break;
            case UIKind::ProgressBar: {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1(p0.x + wpx, p0.y + (hpx > 4.0f ? hpx : 16.0f));
                float t = (e.ui.maxValue > e.ui.minValue)
                    ? (e.ui.value - e.ui.minValue) / (e.ui.maxValue - e.ui.minValue) : 0.0f;
                t = std::clamp(t, 0.0f, 1.0f);
                ImU32 bgU = ImGui::GetColorU32(bg);
                ImU32 fillU = ImGui::GetColorU32(lighter(bg, 0.55f));
                dl->AddRectFilled(p0, p1, bgU, 3.0f);
                dl->AddRectFilled(p0, ImVec2(p0.x + (p1.x - p0.x) * t, p1.y), fillU, 3.0f);
                dl->AddRect(p0, p1, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.25f)), 3.0f);
                ImVec2 ts = ImGui::CalcTextSize(e.ui.label.c_str());
                dl->AddText(ImVec2((p0.x + p1.x - ts.x) * 0.5f, (p0.y + p1.y - ts.y) * 0.5f),
                            ImGui::GetColorU32(tc), e.ui.label.c_str());
                ImGui::Dummy(ImVec2(p1.x - p0.x, p1.y - p0.y));
            } break;
        }
        if (!editable || !e.ui.interactable) ImGui::EndDisabled();
        ImGui::PopStyleColor();
        if (e.ui.fontScale > 0) ImGui::PopFont();
        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ===== PLAYER: кадр без редактора =====
void GUI::RenderPlayerFrame(EditorContext& ctx, int w, int h) {
    m_PopupOpen = false;
    {
        static double lastT = -1.0;
        double now = glfwGetTime();
        if (lastT > 0.0) {
            float dt = static_cast<float>(now - lastT);
            m_FpsEma = m_FpsEma * 0.95f + (1.0f / std::max(dt, 1e-5f)) * 0.05f;
        }
        lastT = now;
    }
    UpdateGameCamera(ctx.sceneManager);
    m_GameSize = glm::vec2(static_cast<float>(w), static_cast<float>(h));
    m_ShowGame = true;

    if (m_HasGameCamera)
        ctx.scene->RenderGameView(m_GameCamera.get(), ctx.sceneManager, w, h);

    m_GameImagePos = ImVec2(0, 0);
    m_GameImageSize = ImVec2(static_cast<float>(w), static_cast<float>(h));

    GLuint tex = ctx.scene->GetGameTexture();
    if (tex) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(w, h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##PlayerFrame", nullptr, flags);
        ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(w, h), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();
        ImGui::PopStyleVar();

        char fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", m_FpsEma);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(8, 6), IM_COL32(255, 255, 130, 255), fpsBuf);
    }

    // интерактивный runtime UI поверх
    RenderGameUIOverlay(ctx);
}

// ===== ВСТРОЕННЫЙ РЕДАКТОР КОДА =====
void GUI::OpenCodeFile(const std::string& path) {
    if (m_CodeDirty && !path.empty() && path != m_CodePath) {
        std::cout << "[IDE] Сначала сохраните " << m_CodePath << " (Ctrl+S в окне редактора)\n";
        m_ShowCodeWindow = true;
        return;
    }
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        std::cout << "[IDE] Файл не найден: " << path << "\n";
        return;
    }
    std::ifstream f(path, std::ios::binary);
    m_CodeText.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    m_CodePath = path;
    m_CodeDirty = false;
    m_ShowCodeWindow = true;
}

bool GUI::SaveCodeFile() {
    if (m_CodePath.empty()) return false;
    std::ofstream f(m_CodePath, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        std::cout << "[IDE] Не удалось записать: " << m_CodePath << "\n";
        return false;
    }
    f << m_CodeText;
    f.close();
    m_CodeDirty = false;
    std::cout << "[IDE] Сохранено: " << m_CodePath << "\n";
    return true;
}

void GUI::RenderCodeWindow(EditorContext& ctx) {
    (void)ctx;
    if (!m_ShowCodeWindow) { m_CodeWindowFocused = false; return; }

    std::string filePart = m_CodePath.empty() ? "(нет файла)"
                             : fs::path(m_CodePath).filename().string();
    // Окно докируется к Scene/Game под именем "Script"
    ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Script###astraCodeEditor", &m_ShowCodeWindow);
    m_CodeWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImGuiIO& io = ImGui::GetIO();
    if (m_CodeWindowFocused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveCodeFile();

    ImGui::Text("%s%s", filePart.c_str(), m_CodeDirty ? "  (не сохранено)" : "");
    if (ImGui::Button("Save (Ctrl+S)")) SaveCodeFile();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) OpenCodeFile(m_CodePath);
    ImGui::SameLine();
    if (ImGui::Button("Open Externally")) OpenExternally(m_CodePath);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_CodePath.empty() ? "—" : m_CodePath.c_str());

    if (m_CodePath.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Двойной клик по .cpp/.h/.frag/.vert/.txt/.json в панели Project — открыть здесь. "
                           "Ctrl+C / Ctrl+V / Ctrl+X / Ctrl+Z работают нативно.");
    } else {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.09f, 1.0f));
        ImGui::InputTextMultiline("##code", &m_CodeText,
                                  ImVec2(-8.0f, -ImGui::GetFrameHeight() * 2.0f));
        ImGui::PopStyleColor();
        if (ImGui::IsItemEdited()) m_CodeDirty = true;
    }
    ImGui::End();
}

void GUI::OpenExternally(const std::string& path) {
    if (path.empty() || !fs::exists(path)) return;
    std::string escaped;
    for (char c : path) {
        if (c == '\'') escaped += "'\\''";
        else escaped += c;
    }
    std::string cmd = "xdg-open '" + escaped + "' >/dev/null 2>&1 &";
    int rc = std::system(cmd.c_str());
    (void)rc;
    std::cout << "[Open] " << path << "\n";
}

void GUI::ImportFileToAssets(EditorContext& ctx, const std::string& srcPath) {
    fs::path src(srcPath);
    std::string ext = ExtLower(src);
    std::string destDir;
    if (IsImageExt(ext)) destDir = "assets/textures";
    else if (IsAudioExt(ext)) destDir = "assets/audio";
    else if (ext == ".scene") destDir = "assets/scenes";
    else if (ext == ".cpp" || ext == ".h") destDir = "assets/scripts";
    else if (ext == ".vert" || ext == ".frag") destDir = "assets/shaders";
    else if (ext == ".prefab") destDir = "assets/prefabs";
    else destDir = "assets/imported";

    std::error_code ec;
    fs::create_directories(destDir, ec);
    fs::path dest = UniquePath(destDir, src.stem().string(), ext);
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Import failed: " << srcPath << " (" << ec.message() << ")\n";
        return;
    }
    std::cout << "[Import] " << dest.string() << "\n";
    ctx.renderer->ClearProjectCaches();
    m_BrowsePath = destDir;
    m_SelectedAsset = dest.string();
    if (ext == ".cpp" || ext == ".h") OpenCodeFile(dest.string());
}

// ===== BUILD GAME =====
// ===== BUILD GAME =====
static void PutU32(std::ostream& o, uint32_t v) { o.write(reinterpret_cast<const char*>(&v), 4); }
static void PutU64(std::ostream& o, uint64_t v) { o.write(reinterpret_cast<const char*>(&v), 8); }
static uint32_t GetU32(std::istream& i) { uint32_t v = 0; i.read(reinterpret_cast<char*>(&v), 4); return v; }
static uint64_t GetU64(std::istream& i) { uint64_t v = 0; i.read(reinterpret_cast<char*>(&v), 8); return v; }
static std::string ReadAllBytes(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static const char* kBundleMagic = "ASTRAPKG";

// Сбор реально используемых ассетов: из сцены рекурсивно по ссылкам компонентов (+префабы)
static void CollectSceneDeps(const std::string& scenePath, std::set<std::string>& out, int depth = 0) {
    if (depth > 8) return;
    std::string text = ReadAllBytes(scenePath);
    if (text.empty()) return;
    static const char* keys[] = { "TexturePath: ", "ShaderPath: ", "AnimTexture: ", "SoundPath: ",
                                  "PrefabSource: ", "TilemapTex: ", "ParticleTex: ",
                                  "MeshPath: ", "MeshTex: " };
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        for (const char* key : keys) {
            size_t klen = strlen(key);
            if (line.rfind(key, 0) != 0) continue;
            std::string v = line.substr(klen);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) v.pop_back();
            if (v.empty()) continue;
            std::error_code ec;
            if (fs::is_regular_file(v, ec)) {
                if (!out.count(v)) { out.insert(v); if (v.size() > 7 && v.compare(v.size()-7, 7, ".prefab") == 0) CollectSceneDeps(v, out, depth+1); }
            } else if (fs::is_regular_file(v + ".frag", ec)) {
                if (out.insert(v + ".frag").second) {}
                if (fs::is_regular_file(v + ".vert", ec)) out.insert(v + ".vert");
            }
            break;
        }
    }
}

static std::string launcherQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static bool RunShell(const std::string& cmd, std::string& outLog) {
    char buf[512];
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) { outLog = "не удалось запустить " + cmd; return false; }
    while (fgets(buf, sizeof(buf), pipe)) outLog += buf;
    int rc = pclose(pipe);
    return WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

bool AstraBuildGame(const std::string& exeSrc, const std::string& scenePath,
                    const std::string& destDir, int mode, bool copyEngineLib, bool encrypt,
                    const std::string& exeName, std::string& status) {
    std::error_code ec;
    fs::path dest(destDir);
    fs::create_directories(dest, ec);
    if (ec) {
        status = "Не удалось создать папку: " + ec.message();
        std::cerr << "[Build] " << status << "\n";
        return false;
    }
    if (!fs::exists(scenePath, ec)) {
        status = "Сцена не существует: " + scenePath;
        return false;
    }
    fs::path self = fs::canonical(exeSrc, ec);
    if (ec || self.empty()) {
        status = "Не удалось определить путь к бинарнику";
        return false;
    }

    // 1) предкомпиляция скриптов, на которые ссылается сцена
    std::unordered_set<std::string> scripts;
    {
        std::ifstream sf(scenePath);
        std::string line;
        while (std::getline(sf, line)) {
            if (line.rfind("ScriptPath: ", 0) == 0) {
                std::string p = line.substr(strlen("ScriptPath: "));
                while (!p.empty() && (p.back() == ' ' || p.back() == '\r')) p.pop_back();
                if (!p.empty()) scripts.insert(p);
            }
        }
    }
    fs::create_directories("build-scripts", ec);
    bool ok = true;
    std::vector<std::string> soPaths;
    for (const auto& sp : scripts) {
        std::string so = (fs::path("build-scripts") /
                          (fs::path(sp).stem().string() + ".so")).string();
        std::string err;
        if (!fs::exists(sp, ec)) {
            std::cerr << "[Build] скрипт не найден: " << sp << "\n";
            ok = false;
            continue;
        }
        std::cout << "[Build] compile: " << sp << "\n";
        if (!Scripting::PrecompileScript(sp, so, err)) {
            std::cerr << "[Build] compile FAILED: " << sp << "\n" << err << "\n";
            status = "Ошибка компиляции " + sp;
            ok = false;
            continue;
        }
        soPaths.push_back(so);
    }

    std::string gameJson = "{\n  \"scene\": \"" + scenePath + "\",\n  \"project\": \"astra-game\"\n}\n";

    // Только реально используемые ассеты — и в зашифрованном виде
    std::set<std::string> deps;
    deps.insert(scenePath); // сама стартовая сцена тоже шифруется
    CollectSceneDeps(scenePath, deps);
    auto copyAssetsAndScripts = [&](void) -> bool {
        std::error_code ec;
        bool okc = true;
        size_t totalBytes = 0;
        for (const auto& rel : deps) {
            fs::path target = dest / rel;
            std::error_code ec2;
            fs::create_directories(target.parent_path(), ec2);
            std::string data = ReadAllBytes(rel);
            if (data.empty()) { okc = false; continue; }
            bool wrote;
            if (encrypt) {
                wrote = AssetIO::WriteEncrypted(target.string(), data);
            } else {
                std::ofstream tf(target, std::ios::binary | std::ios::trunc);
                wrote = tf.is_open() && (bool)tf.write(data.data(), (std::streamoff)data.size());
            }
            if (!wrote) {
                std::cerr << "[Build] write failed: " << rel << "\n";
                okc = false;
                continue;
            }
            totalBytes += data.size();
        }
        std::cout << "[Build] assets: " << deps.size() << " файлов (" << (totalBytes >> 10) << " KiB), "
                  << (encrypt ? "зашифровано" : "без шифрования") << "\n";
        fs::create_directories(dest / "build-scripts", ec);
        for (const auto& so : soPaths) {
            std::error_code ec3;   // .so не шифруем: его грузит dlopen
            fs::copy_file(so, dest / so, fs::copy_options::overwrite_existing, ec3);
            if (ec3) { std::cerr << "[Build] copy .so: " << ec3.message() << "\n"; okc = false; }
        }
        if (encrypt) { if (!AssetIO::WriteEncrypted((dest / "game.json").string(), gameJson)) okc = false; }
        else { std::ofstream jf(dest / "game.json"); jf << gameJson; }
        return okc;
    };

    if (mode == 1) {
        // Один exe: бандл приклеивается к бинарнику
        const std::string gameName = exeName.empty() ? fs::path(scenePath).stem().string() : exeName;
        fs::path outExe = dest / gameName;
        fs::copy_file(self, outExe, fs::copy_options::overwrite_existing, ec);
        if (ec) { status = "Копирование бинарника: " + ec.message(); return false; }

        std::vector<std::pair<std::string, std::string>> files;
        for (const auto& rel : deps)
            files.emplace_back(rel, encrypt ? AssetIO::Encrypt(ReadAllBytes(rel)) : ReadAllBytes(rel));
        for (const auto& so : soPaths) {
            if (fs::exists(so, ec)) files.emplace_back(so, ReadAllBytes(so));  // .so — как есть (dlopen)
        }
        files.emplace_back("game.json", encrypt ? AssetIO::Encrypt(gameJson) : gameJson);

        std::ofstream ef(outExe, std::ios::binary | std::ios::app);
        if (!ef.is_open()) { status = "Не удалось дописать бандл в exe"; return false; }
        std::streampos start = ef.tellp();
        PutU32(ef, static_cast<uint32_t>(files.size()));
        for (const auto& [path, data] : files) {
            PutU32(ef, static_cast<uint32_t>(path.size()));
            ef.write(path.data(), path.size());
            PutU64(ef, static_cast<uint64_t>(data.size()));
            ef.write(data.data(), data.size());
        }
        uint64_t payloadSize = static_cast<uint64_t>(ef.tellp()) - static_cast<uint64_t>(start);
        PutU64(ef, payloadSize);
        ef.write(kBundleMagic, 8);
        ef.close();
        fs::permissions(outExe,
                        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec |
                        fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::add, ec);
        status = "Готово: " + outExe.string() + " — один файл, g++ на целевой машине не нужен";
        std::cout << "[Build] " << status << "\n";
        return ok;
    }

    // Общие части для режимов 0 и 2: assets/, build-scripts/*.so, game.json в папку
    bool filesOk = copyAssetsAndScripts();
    ok = ok && filesOk;

    if (mode == 0) {
        // Лаунчер как в Godot: крошечный exe, линкованный с libastra_engine.so.
        // Движок не дублируется — либка кладётся рядом (или берётся по пути сборки).
        fs::path libDir = self.parent_path();
        std::string libName = "libastra_engine.so";
        if (!fs::exists(libDir / libName, ec)) {
            status = "Не найден " + (libDir / libName).string() + " — соберите движок как библиотеку";
            std::cerr << "[Build] " << status << "\n";
            return false;
        }
        fs::path srcDir = ASTRA_SRC_DIR;
        fs::path launcherDir = dest / ".astra-launcher";
        fs::create_directories(launcherDir, ec);
        fs::path launcherCpp = launcherDir / "launcher.cpp";
        {
            std::ofstream lf(launcherCpp);
            lf << "// Сгенерировано Astra Build Game\n"
                  "#include \"core/Application.h\"\n"
                  "#include <filesystem>\n"
                  "int main() {\n"
                  "    std::error_code ec;\n"
                  "    auto self = std::filesystem::canonical(\"/proc/self/exe\", ec);\n"
                  "    if (!ec) std::filesystem::current_path(self.parent_path(), ec); // cwd = папка игры\n"
                  "    AppOptions o; o.player = true; o.scenePath = \"" << scenePath << "\";\n"
                  "    Application app(o); app.Run(); return 0;\n"
                  "}\n";
        }
        if (copyEngineLib) {
            fs::copy_file(libDir / libName, dest / libName, fs::copy_options::overwrite_existing, ec);
            if (ec) { status = "Копирование либки: " + ec.message(); return false; }
        }
        const std::string gameName = exeName.empty() ? fs::path(scenePath).stem().string() : exeName;
        fs::path outExe = dest / gameName;
        std::string rpath = copyEngineLib ? "$ORIGIN" : libDir.string();
        std::string cmd = std::string("g++ -std=c++17 -O2 ") + SCRIPT_INCLUDES + " -I" +
                          launcherQuote(srcDir.string()) + " " + launcherQuote(launcherCpp.string()) +
                          " -L" + launcherQuote(libDir.string()) +
                          " -lastra_engine -Wl,-rpath,'" + rpath + "' -o " + launcherQuote(outExe.string());
        std::string log;
        if (!RunShell(cmd, log)) {
            status = "Компиляция лаунчера: " + log;
            std::cerr << "[Build] " << status << "\n";
            return false;
        }
        fs::remove_all(launcherDir, ec);
        status = "Готово: " + outExe.string() + " — лаунчер к libastra_engine.so" +
                 (copyEngineLib ? " (либка рядом)" : " (либка: " + libDir.string() + ")");
        std::cout << "[Build] " << status << "\n";
        return ok;
    }

    // mode == 2: папка с полной копией бинарника
    fs::path exeOut = dest / "astra";
    fs::copy_file(self, exeOut, fs::copy_options::overwrite_existing, ec);
    if (ec) { status = "Копирование бинарника: " + ec.message(); return false; }
    fs::permissions(exeOut,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec |
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::add, ec);
    status = "Готово: " + dest.string() + "  (запуск: ./astra --play)";
    std::cout << "[Build] " << status << "\n";
    return ok;
}

bool GUI::BuildGame(const std::string& destDir, const std::string& scenePath) {
    return AstraBuildGame("/proc/self/exe", scenePath, destDir, m_BuildMode, m_BuildCopyEngineLib,
                          m_BuildEncrypt, m_BuildProduct, m_BuildStatus);
}

std::string AstraBundleExtract() {
    std::error_code ec;
    fs::path self = fs::canonical("/proc/self/exe", ec);
    if (ec || self.empty()) return "";
    std::ifstream f(self, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return "";
    uint64_t size = static_cast<uint64_t>(f.tellg());
    if (size < 64) return "";

    uint64_t payloadSize = 0;
    char magic[8] = {};
    f.seekg(static_cast<std::streamoff>(size - 16));
    f.read(reinterpret_cast<char*>(&payloadSize), 8);
    f.read(magic, 8);
    if (memcmp(magic, kBundleMagic, 8) != 0 || payloadSize == 0 || payloadSize > size - 16)
        return "";

    fs::path outDir = self.parent_path() / (self.filename().string() + ".bundle");
    fs::path marker = outDir / ".astra_bundle";
    std::string sizeTag = std::to_string(payloadSize);
    if (fs::exists(marker, ec) && ReadAllBytes(marker) == sizeTag)
        return outDir.string(); // уже распаковано

    f.clear();
    f.seekg(static_cast<std::streamoff>(size - 16 - payloadSize));
    uint32_t count = GetU32(f);
    fs::create_directories(outDir, ec);
    for (uint32_t i = 0; i < count && f.good(); i++) {
        uint32_t pathLen = GetU32(f);
        if (pathLen == 0 || pathLen > 4096) break;
        std::string path(pathLen, '\0');
        f.read(path.data(), pathLen);
        uint64_t dataLen = GetU64(f);
        if (!f.good()) break;
        fs::path target = outDir / path;
        // защита от выхода за пределы каталога (../ и абсолютные пути)
        std::error_code ec2;
        fs::path rel = fs::relative(target, outDir, ec2);
        if (ec2 || rel.empty() || *rel.begin() == "..") {
            std::cerr << "[Bundle] пропускаю опасный путь: " << path << "\n";
            f.seekg(static_cast<std::streamoff>(dataLen), std::ios::cur);
            continue;
        }
        fs::create_directories(target.parent_path(), ec2);
        std::ofstream tf(target, std::ios::binary | std::ios::trunc);
        constexpr size_t kChunk = 1 << 20;
        char buf[kChunk];
        uint64_t left = dataLen;
        while (left > 0 && tf.is_open()) {
            size_t n = static_cast<size_t>(std::min<uint64_t>(left, kChunk));
            f.read(buf, static_cast<std::streamoff>(n));
            tf.write(buf, f.gcount());
            left -= static_cast<uint64_t>(f.gcount());
            if (f.gcount() == 0) break;
        }
    }
    {
        std::ofstream mf(marker, std::ios::trunc);
        mf << sizeTag;
    }
    std::cout << "[Bundle] распаковано в " << outDir.string() << "\n";
    return outDir.string();
}

void GUI::RenderBuildDialog(EditorContext& ctx) {
    if (!m_ShowBuildDialog) return;

    // Окно Build Settings (как в Unity): не модалка, докируется
    ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Build Settings###astraBuild", &m_ShowBuildDialog)) {
        std::error_code ec;

        // Список сцен: текущая + все assets/scenes/*.scene
        std::vector<std::string> scenes;
        if (fs::is_directory("assets/scenes", ec)) {
            for (const auto& entry : fs::directory_iterator("assets/scenes", ec))
                if (ExtLower(entry.path()) == ".scene")
                    scenes.push_back(entry.path().string());
        }
        std::sort(scenes.begin(), scenes.end());
        if (!m_CurrentScenePath.empty() &&
            std::find(scenes.begin(), scenes.end(), m_CurrentScenePath) == scenes.end())
            scenes.insert(scenes.begin(), m_CurrentScenePath);
        if ((int)m_BuildSceneList.size() != (int)scenes.size() || m_BuildSceneList != scenes) {
            m_BuildSceneList = scenes;
            m_BuildSceneIncluded.assign(scenes.size(), true);
            if (m_BuildBoot >= (int)scenes.size()) m_BuildBoot = 0;
        }
        if (m_BuildProduct[0] == '\0')
            snprintf(m_BuildProduct, sizeof(m_BuildProduct), "%s",
                     ctx.projectManager->CurrentProjectName().c_str());

        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("Product name (имя exe)", m_BuildProduct, sizeof(m_BuildProduct));
        ImGui::SeparatorText("Scenes Included");
        ImGui::TextDisabled("Отметь сцены игры; радиокнопка — стартовая (первая в билде).");
        if (m_BuildSceneList.empty()) {
            ImGui::TextDisabled("(нет сцен в assets/scenes — сохрани хотя бы одну, Ctrl+S)");
        }
        for (size_t i = 0; i < m_BuildSceneList.size(); i++) {
            const std::string& sc = m_BuildSceneList[i];
            ImGui::PushID(sc.c_str());
            bool boot = (int)i == m_BuildBoot;
            if (ImGui::RadioButton("##boot", boot)) { m_BuildBoot = (int)i; m_BuildSceneIncluded[i] = true; }
            ImGui::SameLine();
            bool inc = m_BuildSceneIncluded[i];
            if (ImGui::Checkbox("##inc", &inc)) m_BuildSceneIncluded[i] = inc;
            ImGui::SameLine();
            std::string shown = fs::path(sc).filename().string();
            if (sc == m_CurrentScenePath) shown += "  (current)";
            ImGui::Text("%s", shown.c_str());
            ImGui::PopID();
        }

        ImGui::SeparatorText("Output");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
        ImGui::InputText("Folder##buildout", m_BuildDirBuf, sizeof(m_BuildDirBuf));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            m_FolderPickerTarget = 4;
            m_FolderPickerPickFile = false;
            std::string cur = m_BuildDirBuf;
            if (fs::is_directory(cur, ec)) m_FolderPickerPath = cur;
            else m_FolderPickerPath = GuiHomeDir().string();
        }
        ImGui::Checkbox("Encrypt used assets (AENC)", &m_BuildEncrypt);
        ImGui::SameLine();
        ImGui::Checkbox("Copy engine library", &m_BuildCopyEngineLib);
        ImGui::TextDisabled("В билд попадают только ассеты, реально используемые сценами "
                            "(+префабы рекурсивно); скрипты предкомпилируются в .so.");

        ImGui::Separator();
        if (ImGui::Button("Build", ImVec2(140, 0))) {
            m_BuildStatus.clear();
            // Стартовая = сцена под радиокнопкой (обязательно Included); иначе — первая Included
            std::string bootScene;
            if (m_BuildBoot >= 0 && m_BuildBoot < (int)m_BuildSceneList.size() &&
                m_BuildSceneIncluded[m_BuildBoot]) {
                bootScene = m_BuildSceneList[m_BuildBoot];
            } else {
                for (size_t i = 0; i < m_BuildSceneList.size(); i++)
                    if (m_BuildSceneIncluded[i]) { bootScene = m_BuildSceneList[i]; m_BuildBoot = (int)i; break; }
            }
            if (bootScene.empty()) {
                m_BuildStatus = "Нет стартовой сцены — отметь хотя бы одну";
            } else {
                if (bootScene == m_CurrentScenePath && m_SceneDirty) SaveSceneNow(ctx);
                m_BuildRunning = true;
                fs::path dest = fs::path(m_BuildDirBuf) / (m_BuildProduct[0] ? m_BuildProduct : "astra_game");
                BuildGame(dest.string(), bootScene);
                m_BuildRunning = false;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("лаунчер + libastra_engine.so, только нужные ассеты; exe = Product name");
        if (!m_BuildStatus.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_BuildStatus.c_str());
        }
    }
    ImGui::End();
}
