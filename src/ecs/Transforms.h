// Мировые трансформы с учётом parent-цепочки (см. parentId у Entity)
#pragma once
#include "ecs/Entity.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <vector>

namespace Transforms {

inline glm::mat4 LocalMatrix(const Entity& e) {
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(e.transform.position, 0.0f));
    m = glm::rotate(m, glm::radians(e.transform.rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, glm::vec3(e.transform.scale, 1.0f));
    return m;
}

inline const Entity* FindById(const std::vector<Entity>& all, uint32_t id) {
    if (id == 0) return nullptr;
    for (const auto& e : all) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

inline glm::vec2 LocalToWorld(const std::vector<Entity>& all, const glm::mat4& parentMatrix, const glm::vec2& local) {
    glm::vec4 w = parentMatrix * glm::vec4(local, 0.0f, 1.0f);
    return glm::vec2(w.x, w.y);
}

inline glm::mat4 ParentWorldMatrix(const std::vector<Entity>& all, const Entity& e) {
    glm::mat4 m = glm::mat4(1.0f);
    const Entity* parent = FindById(all, e.parentId);
    int guard = 0;
    while (parent && guard++ < 16) {
        m = LocalMatrix(*parent) * m;
        parent = FindById(all, parent->parentId);
    }
    return m;
}

inline glm::vec2 WorldPosition(const std::vector<Entity>& all, const Entity& e) {
    glm::mat4 chain = ParentWorldMatrix(all, e);
    glm::vec4 w = chain * glm::vec4(e.transform.position, 0.0f, 1.0f);
    return glm::vec2(w.x, w.y);
}

inline glm::mat4 WorldMatrix(const std::vector<Entity>& all, const Entity& e) {
    glm::mat4 chain = glm::mat4(1.0f);
    const Entity* parent = FindById(all, e.parentId);
    int guard = 0;
    while (parent && guard++ < 16) {
        chain = LocalMatrix(*parent) * chain;
        parent = FindById(all, parent->parentId);
    }
    return chain * LocalMatrix(e);
}

inline glm::vec2 WorldToLocalPoint(const std::vector<Entity>& all, const Entity& e, const glm::vec2& world) {
    glm::mat4 inv = glm::inverse(ParentWorldMatrix(all, e));
    glm::vec4 l = inv * glm::vec4(world, 0.0f, 1.0f);
    return glm::vec2(l.x, l.y);
}

inline bool IsDescendantOf(const std::vector<Entity>& all, uint32_t candidateChild, uint32_t ancestor) {
    const Entity* e = FindById(all, candidateChild);
    int guard = 0;
    while (e && guard++ < 16) {
        if (e->parentId == ancestor) return true;
        e = FindById(all, e->parentId);
    }
    return false;
}

} // namespace Transforms
