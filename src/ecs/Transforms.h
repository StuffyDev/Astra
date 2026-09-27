// Мировые трансформы с учётом parent-цепочки (см. parentId у Entity)
#pragma once
#include "ecs/Entity.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Transforms {

inline glm::mat4 LocalMatrix(const Entity& e) {
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(e.transform.position, 0.0f));
    m = glm::rotate(m, glm::radians(e.transform.rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, glm::vec3(e.transform.scale, 1.0f));
    return m;
}

// Модель 3D-сущности: T(pos3) * Rx * Ry * Rz * S(scale3) — одна и та же для рендера и picking'а
inline glm::mat4 Model3D(const Entity& e) {
    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, e.pos3);
    m = glm::rotate(m, glm::radians(e.rot3.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(e.rot3.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(e.rot3.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, e.scale3);
    return m;
}

// Луч в локальных координатах меша против AABB [-h..h] — tOut расстояние до попадания
inline bool RayHitsBox(const glm::vec3& o, const glm::vec3& d, const glm::vec3& h, float& tOut) {
    float tmin = -1e18f, tmax = 1e18f;
    const float* oo = &o.x;
    const float* dd = &d.x;
    const float* hh = &h.x;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(dd[i]) < 1e-8f) {
            if (std::fabs(oo[i]) > hh[i]) return false;
            continue;
        }
        float t1 = (-hh[i] - oo[i]) / dd[i];
        float t2 = (hh[i] - oo[i]) / dd[i];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    tOut = tmin > 0.0f ? tmin : tmax;
    return true;
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
