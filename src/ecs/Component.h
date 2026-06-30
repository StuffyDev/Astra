#pragma once
#include <glm/glm.hpp>

enum class SpriteType {
    None,
    Quad,
    Circle
};

struct Transform {
    glm::vec2 position = glm::vec2(0.0f);
    float rotation = 0.0f; // degrees
    glm::vec2 scale = glm::vec2(100.0f, 100.0f);
};

struct Sprite {
    SpriteType type = SpriteType::Quad;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
};
