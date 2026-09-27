#include "core/SceneSerializer.h"
#include "utils/AssetIO.h"
#include "ecs/SceneManager.h"
#include "ecs/Entity.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

namespace {

std::string TrimLead(const std::string& s) {
    return (!s.empty() && s[0] == ' ') ? s.substr(1) : s;
}

void WriteEntity(std::ostream& file, const Entity& e, const std::unordered_map<uint32_t, size_t>& indexOf) {
    file << "ENTITY\n";
    file << "Name: " << e.name << "\n";
    file << "Active: " << (e.active ? 1 : 0) << "\n";
    file << "Pos: " << e.transform.position.x << " " << e.transform.position.y << "\n";
    file << "Rot: " << e.transform.rotation << "\n";
    file << "Scale: " << e.transform.scale.x << " " << e.transform.scale.y << "\n";
    file << "SpriteType: " << static_cast<int>(e.sprite.type) << "\n";
    file << "Color: " << e.sprite.color.r << " " << e.sprite.color.g << " " << e.sprite.color.b << "\n";
    file << "TexturePath: " << e.sprite.texturePath << "\n";
    file << "ShaderPath: " << e.sprite.shaderPath << "\n";
    file << "SortingOrder: " << e.sprite.sortingOrder << "\n";
    file << "MaterialParams: " << e.sprite.materialParams.x << " " << e.sprite.materialParams.y
         << " " << e.sprite.materialParams.z << " " << e.sprite.materialParams.w << "\n";
    file << "MaterialColor: " << e.sprite.materialColor.r << " " << e.sprite.materialColor.g
         << " " << e.sprite.materialColor.b << " " << e.sprite.materialColor.a << "\n";
    file << "AnimActive: " << (e.animation.active ? 1 : 0) << "\n";
    file << "AnimTexture: " << e.animation.texturePath << "\n";
    file << "AnimGrid: " << e.animation.cols << " " << e.animation.rows << "\n";
    file << "AnimFps: " << e.animation.fps << "\n";
    file << "AnimLoop: " << (e.animation.loop ? 1 : 0) << "\n";
    file << "AnimPlayOnAwake: " << (e.animation.playOnAwake ? 1 : 0) << "\n";
    file << "PrefabSource: " << e.prefabSource << "\n";
    file << "ScriptPath: " << e.scriptPath << "\n";
    for (const auto& [name, value] : e.vars)
        file << "ScriptVar: " << name << "=" << value << "\n";
    file << "ParentIndex: " << [&] {
        if (e.parentId == 0) return -1;
        auto it = indexOf.find(e.parentId);
        return it != indexOf.end() ? static_cast<int>(it->second) : -1;
    }() << "\n";
    file << "HasRigidbody: " << (e.hasRigidbody ? 1 : 0) << "\n";
    file << "HasCollider: " << (e.hasCollider ? 1 : 0) << "\n";
    file << "HasAudio: " << (e.hasAudio ? 1 : 0) << "\n";
    file << "HasScript: " << (e.hasScript ? 1 : 0) << "\n";
    file << "HasParticles: " << (e.hasParticles ? 1 : 0) << "\n";
    file << "RigidbodyKinematic: " << (e.rigidbody.isKinematic ? 1 : 0) << "\n";
    file << "RigidbodyVelocity: " << e.rigidbody.velocity.x << " " << e.rigidbody.velocity.y << "\n";
    file << "RigidbodyMass: " << e.rigidbody.mass << "\n";
    file << "RigidbodyDrag: " << e.rigidbody.drag << "\n";
    file << "RigidbodyGravity: " << (e.rigidbody.useGravity ? 1 : 0) << "\n";
    file << "ColliderType: " << static_cast<int>(e.collider.type) << "\n";
    file << "ColliderTrigger: " << (e.collider.isTrigger ? 1 : 0) << "\n";
    file << "ColliderSize: " << e.collider.size.x << " " << e.collider.size.y << "\n";
    file << "ColliderRadius: " << e.collider.radius << "\n";
    file << "HasCamera: " << (e.hasCamera ? 1 : 0) << "\n";
    file << "CameraMain: " << (e.camera.mainCamera ? 1 : 0) << "\n";
    file << "CameraZoom: " << e.camera.zoom << "\n";
    file << "CameraOffset: " << e.camera.offset.x << " " << e.camera.offset.y << "\n";
    file << "HasUI: " << (e.hasUI ? 1 : 0) << "\n";
    file << "UIKind: " << static_cast<int>(e.ui.kind) << "\n";
    file << "UILabel: " << e.ui.label << "\n";
    file << "UIRange: " << e.ui.minValue << " " << e.ui.maxValue << "\n";
    file << "UIValue: " << e.ui.value << "\n";
    file << "UIInteractable: " << (e.ui.interactable ? 1 : 0) << "\n";
    file << "UITextColor: " << e.ui.textColor.r << " " << e.ui.textColor.g << " " << e.ui.textColor.b << " " << e.ui.textColor.a << "\n";
    file << "UIBgColor: " << e.ui.bgColor.r << " " << e.ui.bgColor.g << " " << e.ui.bgColor.b << " " << e.ui.bgColor.a << "\n";
    file << "UIFontScale: " << e.ui.fontScale << "\n";
    file << "SoundPath: " << e.audio.path << "\n";
    file << "SoundVolume: " << e.audio.volume << "\n";
    file << "SoundPitch: " << e.audio.pitch << "\n";
    file << "SoundLoop: " << (e.audio.loop ? 1 : 0) << "\n";
    file << "SoundPlayOnAwake: " << (e.audio.playOnAwake ? 1 : 0) << "\n";
    if (!e.animation.clips.empty()) {
        file << "AnimClips: ";
        for (size_t i = 0; i < e.animation.clips.size(); i++) {
            const AnimClip& c = e.animation.clips[i];
            file << (i ? ";" : "") << c.name << "|" << c.first << "|" << c.last << "|" << c.fps << "|" << (c.loop ? 1 : 0);
        }
        file << "\n";
        file << "AnimActiveClip: " << e.animation.activeClip << "\n";
    }
    file << "HasTilemap: " << (e.hasTilemap ? 1 : 0) << "\n";
    if (e.hasTilemap) {
        file << "TilemapTex: " << e.tilemap.texturePath << "\n";
        file << "TilemapTile: " << e.tilemap.tileW << " " << e.tilemap.tileH << " " << e.tilemap.atlasCols << "\n";
        file << "TilemapSize: " << e.tilemap.width << " " << e.tilemap.height << "\n";
        file << "TilemapColor: " << e.tilemap.color.r << " " << e.tilemap.color.g << " " << e.tilemap.color.b << " " << e.tilemap.color.a << "\n";
        file << "TilemapSort: " << e.tilemap.sortingOrder << "\n";
        file << "TilemapCells:";
        for (int v : e.tilemap.cells) file << " " << v;
        file << "\n";
    }
    file << "ParticleActive: " << (e.emitter.active ? 1 : 0) << "\n";
    file << "ParticleTex: " << e.emitter.texturePath << "\n";
    file << "ParticleMax: " << e.emitter.maxCount << " " << e.emitter.rate << "\n";
    file << "ParticleLife: " << e.emitter.lifeMin << " " << e.emitter.lifeMax << "\n";
    file << "ParticleSpeed: " << e.emitter.speedMin << " " << e.emitter.speedMax << "\n";
    file << "ParticleAngle: " << e.emitter.angleMin << " " << e.emitter.angleMax << "\n";
    file << "ParticleGravity: " << e.emitter.gravity << "\n";
    file << "ParticleSize: " << e.emitter.sizeMin << " " << e.emitter.sizeMax << "\n";
    file << "ParticleC1: " << e.emitter.colorStart.r << " " << e.emitter.colorStart.g << " " << e.emitter.colorStart.b << " " << e.emitter.colorStart.a << "\n";
    file << "ParticleC2: " << e.emitter.colorEnd.r << " " << e.emitter.colorEnd.g << " " << e.emitter.colorEnd.b << " " << e.emitter.colorEnd.a << "\n";
    file << "ParticleLoop: " << (e.emitter.loop ? 1 : 0) << " " << (e.emitter.playOnAwake ? 1 : 0) << "\n";
    file << "END_ENTITY\n";
}

// id -> индекс внутри набора
std::unordered_map<uint32_t, size_t> BuildIndexOf(const std::vector<Entity>& entities) {
    std::unordered_map<uint32_t, size_t> indexOf;
    for (size_t i = 0; i < entities.size(); i++) indexOf[entities[i].id] = i;
    return indexOf;
}

} // namespace

void SceneSerializer::Save(SceneManager* sceneManager, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to save scene: " << path << "\n";
        return;
    }
    const auto& entities = sceneManager->GetEntities();
    file << "Astra Scene v2\n";
    file << "EntityCount: " << entities.size() << "\n";
    auto indexOf = BuildIndexOf(entities);
    for (const auto& e : entities) WriteEntity(file, e, indexOf);
}

bool SceneSerializer::SaveEntities(const std::vector<Entity>& entities, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << "Astra Scene v2\n";
    file << "EntityCount: " << entities.size() << "\n";
    auto indexOf = BuildIndexOf(entities);
    for (const auto& e : entities) WriteEntity(file, e, indexOf);
    return file.good();
}

bool SceneSerializer::Load(SceneManager* sceneManager, const std::string& path) {
    std::vector<Entity> protos;
    if (!LoadEntities(path, protos)) return false;

    sceneManager->Clear();
    for (const auto& e : protos) sceneManager->AddEntity(e); // AddEntity выдаёт реальные id по порядку

    // parentId пока ссылается на временные id (index+1) — переводим в реальные
    auto& ents = sceneManager->GetEntities();
    for (size_t i = 0; i < ents.size() && i < protos.size(); i++) {
        if (ents[i].parentId != 0) {
            size_t p = ents[i].parentId - 1;
            ents[i].parentId = (p < ents.size() && p != i) ? ents[p].id : 0;
        }
    }
    return true;
}

bool SceneSerializer::LoadEntities(const std::string& path, std::vector<Entity>& out) {
    std::string text = AssetIO::ReadAll(path);
    if (text.empty()) {
        std::cerr << "Failed to load: " << path << "\n";
        return false;
    }
    std::istringstream file(text);

    std::string line;
    std::getline(file, line); // header
    if (line != "Engine2D Scene v1" && line != "Engine2D Scene v2" && line != "Astra Scene v2") {
        std::cerr << "Invalid scene/prefab file format\n";
        return false;
    }

    out.clear();
    std::getline(file, line); // EntityCount

    Entity current;
    bool inEntity = false;
    int currentParent = -1;
    std::vector<int> parentIndices;

    while (std::getline(file, line)) {
        if (line == "ENTITY") {
            inEntity = true;
            current = Entity();
            // Файлы до v0.10 не имели флагов: считаем компоненты включёнными (legacy)
            current.hasRigidbody = current.hasCollider = current.hasAudio = current.hasParticles = true;
            currentParent = -1;
        } else if (line == "END_ENTITY") {
            if (inEntity) {
                parentIndices.push_back(currentParent);
                if (current.hasScript == false && !current.scriptPath.empty()) current.hasScript = true;
                out.push_back(current);
                inEntity = false;
            }
        } else if (inEntity) {
            std::istringstream iss(line);
            std::string key;
            iss >> key;

            if (key == "Name:") { std::getline(iss, current.name); current.name = TrimLead(current.name); }
            else if (key == "Active:") { int v; iss >> v; current.active = v; }
            else if (key == "Pos:") { iss >> current.transform.position.x >> current.transform.position.y; }
            else if (key == "Rot:") { iss >> current.transform.rotation; }
            else if (key == "Scale:") { iss >> current.transform.scale.x >> current.transform.scale.y; }
            else if (key == "SpriteType:") { int v; iss >> v; current.sprite.type = static_cast<SpriteType>(v); }
            else if (key == "Color:") { iss >> current.sprite.color.r >> current.sprite.color.g >> current.sprite.color.b; }
            else if (key == "TexturePath:") { std::getline(iss, current.sprite.texturePath); current.sprite.texturePath = TrimLead(current.sprite.texturePath); }
            else if (key == "ShaderPath:") { std::getline(iss, current.sprite.shaderPath); current.sprite.shaderPath = TrimLead(current.sprite.shaderPath); }
            else if (key == "SortingOrder:") { iss >> current.sprite.sortingOrder; }
            else if (key == "MaterialParams:") { iss >> current.sprite.materialParams.x >> current.sprite.materialParams.y >> current.sprite.materialParams.z >> current.sprite.materialParams.w; }
            else if (key == "MaterialColor:") { iss >> current.sprite.materialColor.r >> current.sprite.materialColor.g >> current.sprite.materialColor.b >> current.sprite.materialColor.a; }
            else if (key == "AnimActive:") { int v; iss >> v; current.animation.active = v; }
            else if (key == "AnimTexture:") { std::getline(iss, current.animation.texturePath); current.animation.texturePath = TrimLead(current.animation.texturePath); }
            else if (key == "AnimGrid:") { iss >> current.animation.cols >> current.animation.rows; }
            else if (key == "AnimFps:") { iss >> current.animation.fps; }
            else if (key == "AnimLoop:") { int v; iss >> v; current.animation.loop = v; }
            else if (key == "AnimPlayOnAwake:") { int v; iss >> v; current.animation.playOnAwake = v; }
            else if (key == "PrefabSource:") { std::getline(iss, current.prefabSource); current.prefabSource = TrimLead(current.prefabSource); }
            else if (key == "ScriptPath:") { std::getline(iss, current.scriptPath); current.scriptPath = TrimLead(current.scriptPath); }
            else if (key == "ScriptVar:") {
                std::string line; std::getline(iss, line); line = TrimLead(line);
                size_t eq = line.rfind('=');
                if (eq != std::string::npos) {
                    try { current.vars[line.substr(0, eq)] = std::stof(line.substr(eq + 1)); }
                    catch (...) {}
                }
            }
            else if (key == "ParentIndex:") { iss >> currentParent; }
            else if (key == "HasRigidbody:") { int v; iss >> v; current.hasRigidbody = v; }
            else if (key == "HasCollider:") { int v; iss >> v; current.hasCollider = v; }
            else if (key == "HasAudio:") { int v; iss >> v; current.hasAudio = v; }
            else if (key == "HasScript:") { int v; iss >> v; current.hasScript = v; }
            else if (key == "HasParticles:") { int v; iss >> v; current.hasParticles = v; }
            else if (key == "RigidbodyKinematic:") { int v; iss >> v; current.rigidbody.isKinematic = v; }
            else if (key == "RigidbodyVelocity:") { iss >> current.rigidbody.velocity.x >> current.rigidbody.velocity.y; }
            else if (key == "RigidbodyMass:") { iss >> current.rigidbody.mass; }
            else if (key == "RigidbodyDrag:") { iss >> current.rigidbody.drag; }
            else if (key == "RigidbodyGravity:") { int v; iss >> v; current.rigidbody.useGravity = v; }
            else if (key == "ColliderType:") { int v; iss >> v; current.collider.type = static_cast<ColliderType>(v); }
            else if (key == "ColliderTrigger:") { int v; iss >> v; current.collider.isTrigger = v; }
            else if (key == "ColliderSize:") { iss >> current.collider.size.x >> current.collider.size.y; }
            else if (key == "ColliderRadius:") { iss >> current.collider.radius; }
            else if (key == "HasCamera:") { int v; iss >> v; current.hasCamera = v; }
            else if (key == "CameraMain:") { int v; iss >> v; current.camera.mainCamera = v; }
            else if (key == "CameraZoom:") { iss >> current.camera.zoom; }
            else if (key == "CameraOffset:") { iss >> current.camera.offset.x >> current.camera.offset.y; }
            else if (key == "HasUI:") { int v; iss >> v; current.hasUI = v; }
            else if (key == "UIKind:") { int v; iss >> v; current.ui.kind = static_cast<UIKind>(v); }
            else if (key == "UILabel:") { std::getline(iss, current.ui.label); current.ui.label = TrimLead(current.ui.label); }
            else if (key == "UIRange:") { iss >> current.ui.minValue >> current.ui.maxValue; }
            else if (key == "UIValue:") { iss >> current.ui.value; }
            else if (key == "UIInteractable:") { int v; iss >> v; current.ui.interactable = v; }
            else if (key == "UITextColor:") { iss >> current.ui.textColor.r >> current.ui.textColor.g >> current.ui.textColor.b >> current.ui.textColor.a; }
            else if (key == "UIBgColor:") { iss >> current.ui.bgColor.r >> current.ui.bgColor.g >> current.ui.bgColor.b >> current.ui.bgColor.a; }
            else if (key == "UIFontScale:") { iss >> current.ui.fontScale; }
            else if (key == "SoundPath:") { std::getline(iss, current.audio.path); current.audio.path = TrimLead(current.audio.path); }
            else if (key == "SoundVolume:") { iss >> current.audio.volume; }
            else if (key == "SoundPitch:") { iss >> current.audio.pitch; }
            else if (key == "SoundLoop:") { int v; iss >> v; current.audio.loop = v; }
            else if (key == "SoundPlayOnAwake:") { int v; iss >> v; current.audio.playOnAwake = v; }
            else if (key == "AnimClips:") {
                std::string line; std::getline(iss, line); line = TrimLead(line);
                current.animation.clips.clear();
                size_t pos = 0;
                while (pos < line.size()) {
                    size_t semi = line.find(';', pos);
                    std::string part = line.substr(pos, (semi == std::string::npos ? line.size() : semi) - pos);
                    size_t p1 = part.find('|'), p2 = part.find('|', p1 + 1), p3 = part.find('|', p2 + 1), p4 = part.find('|', p3 + 1);
                    if (p1 != std::string::npos && p4 != std::string::npos) {
                        AnimClip c;
                        c.name = part.substr(0, p1);
                        c.first = std::stoi(part.substr(p1 + 1, p2 - p1 - 1));
                        c.last = std::stoi(part.substr(p2 + 1, p3 - p2 - 1));
                        c.fps = std::stof(part.substr(p3 + 1, p4 - p3 - 1));
                        c.loop = std::stoi(part.substr(p4 + 1)) != 0;
                        current.animation.clips.push_back(c);
                    }
                    if (semi == std::string::npos) break;
                    pos = semi + 1;
                }
            }
            else if (key == "AnimActiveClip:") { iss >> current.animation.activeClip; }
            else if (key == "HasTilemap:") { int v; iss >> v; current.hasTilemap = v; }
            else if (key == "TilemapTex:") { std::getline(iss, current.tilemap.texturePath); current.tilemap.texturePath = TrimLead(current.tilemap.texturePath); }
            else if (key == "TilemapTile:") { iss >> current.tilemap.tileW >> current.tilemap.tileH >> current.tilemap.atlasCols; }
            else if (key == "TilemapSize:") { iss >> current.tilemap.width >> current.tilemap.height; }
            else if (key == "TilemapColor:") { iss >> current.tilemap.color.r >> current.tilemap.color.g >> current.tilemap.color.b >> current.tilemap.color.a; }
            else if (key == "TilemapSort:") { iss >> current.tilemap.sortingOrder; }
            else if (key == "TilemapCells:") {
                current.tilemap.cells.clear();
                int v;
                while (iss >> v) current.tilemap.cells.push_back(v);
            }
            else if (key == "ParticleActive:") { int v; iss >> v; current.emitter.active = v; }
            else if (key == "ParticleTex:") { std::getline(iss, current.emitter.texturePath); current.emitter.texturePath = TrimLead(current.emitter.texturePath); }
            else if (key == "ParticleMax:") { iss >> current.emitter.maxCount >> current.emitter.rate; }
            else if (key == "ParticleLife:") { iss >> current.emitter.lifeMin >> current.emitter.lifeMax; }
            else if (key == "ParticleSpeed:") { iss >> current.emitter.speedMin >> current.emitter.speedMax; }
            else if (key == "ParticleAngle:") { iss >> current.emitter.angleMin >> current.emitter.angleMax; }
            else if (key == "ParticleGravity:") { iss >> current.emitter.gravity; }
            else if (key == "ParticleSize:") { iss >> current.emitter.sizeMin >> current.emitter.sizeMax; }
            else if (key == "ParticleC1:") { iss >> current.emitter.colorStart.r >> current.emitter.colorStart.g >> current.emitter.colorStart.b >> current.emitter.colorStart.a; }
            else if (key == "ParticleC2:") { iss >> current.emitter.colorEnd.r >> current.emitter.colorEnd.g >> current.emitter.colorEnd.b >> current.emitter.colorEnd.a; }
            else if (key == "ParticleLoop:") { int a, b; iss >> a >> b; current.emitter.loop = a; current.emitter.playOnAwake = b; }
        }
    }

    // временные id 1..n, родители — по индексам
    for (size_t i = 0; i < out.size(); i++) out[i].id = static_cast<uint32_t>(i + 1);
    for (size_t i = 0; i < out.size(); i++) {
        int p = parentIndices[i];
        if (p >= 0 && static_cast<size_t>(p) < out.size() && static_cast<size_t>(p) != i)
            out[i].parentId = out[p].id;
    }
    return true;
}
