#pragma once
#include "ecs/Component.h"
#include <cstdint>
#include <map>
#include <string>

struct Entity {
    uint32_t id = 0;
    uint32_t parentId = 0; // 0 = корень сцены
    std::string name = "New Entity";
    std::string prefabSource; // путь .prefab, если это инстанс (только у корня дерева)
    std::string scriptPath;   // assets/scripts/*.cpp, компилируется при входе в Play
    // Серелиазуемые переменные скрипта (как [SerializeField] в Unity): DefineVar в Start(),
    // ползунки появляются в Inspector, значения живут в сцене
    std::map<std::string, float> vars;
    Transform transform;
    // 3D-режим: poz/rot/scale по трём осям (градусы), рисуется mesh вместо спрайта
    bool is3D = false;
    glm::vec3 pos3 = glm::vec3(0.0f);
    glm::vec3 rot3 = glm::vec3(0.0f);
    glm::vec3 scale3 = glm::vec3(100.0f);
    MeshComponent mesh;
    Sprite sprite;
    SpriteAnimation animation;
    float animTime = 0.0f; // runtime-состояние, в файл не пишется
    bool hasRigidbody = false; // компоненты добавляются/убираются, как в Unity
    bool hasCollider = false;
    Rigidbody rigidbody;
    Collider collider;
    bool hasCamera = false;
    CameraComponent camera;
    bool hasUI = false;
    UIComponent ui;
    bool hasAudio = false;
    AudioSource audio;
    bool hasScript = false;
    bool hasTilemap = false;
    Tilemap tilemap;
    bool hasParticles = false;
    ParticleEmitter emitter;
    std::vector<Particle> particles; // runtime-состояние частиц
    bool active = true;
};
