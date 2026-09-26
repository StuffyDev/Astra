#pragma once
#include "ecs/Component.h"
#include <cstdint>
#include <string>

struct Entity {
    uint32_t id = 0;
    uint32_t parentId = 0; // 0 = корень сцены
    std::string name = "New Entity";
    std::string prefabSource; // путь .prefab, если это инстанс (только у корня дерева)
    std::string scriptPath;   // assets/scripts/*.cpp, компилируется при входе в Play
    Transform transform;
    Sprite sprite;
    Rigidbody rigidbody;
    Collider collider;
    bool hasCamera = false;
    CameraComponent camera;
    bool hasUI = false;
    UIComponent ui;
    AudioSource audio;
    bool active = true;
};
