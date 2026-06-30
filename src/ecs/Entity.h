#pragma once
#include "ecs/Component.h"
#include <string>

struct Entity {
    std::string name = "New Entity";
    Transform transform;
    Sprite sprite;
    bool active = true;
};
