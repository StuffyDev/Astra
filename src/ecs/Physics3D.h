// Твёрдотельная физика для 3D-сцен: гравитация по -Y, короб/сфера, MTV-разрешения,
// триггеры и события скриптам. Коллайдеры учитывают поворот как габарит повёрнутого короба
// и масштаб scale3; дочерние тела не интегрируются (двигаются родителем), но держат контакт.
#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_set>
#include <vector>
#include "ecs/Physics.h"

struct Entity;

class Physics3D {
public:
    static glm::vec3 Gravity;   // мировых единиц/с², по умолчанию 9.81 м/с² при 100 ед = 1 м
    static constexpr float FixedDeltaTime = 1.0f / 60.0f;

    static void Step(std::vector<Entity>& entities, float dt);
    static void ClearState();

    static const std::vector<PhysicsEvent>& GetEvents() { return s_Events; }
    static bool ConsumeEvents();   // true — были события, список очищен

    struct Bounds {
        glm::vec3 center = glm::vec3(0.0f);
        glm::vec3 half = glm::vec3(0.5f);
        float radius = 0.5f;
        bool sphere = false;
    };
    static Bounds WorldBounds(const std::vector<Entity>& all, const Entity& e);

private:
    static bool IsDynamic(const Entity& e);
    static bool Overlap(const Bounds& a, const Bounds& b, glm::vec3& normalAtoB, float& depth);

    static inline std::vector<PhysicsEvent> s_Events;
    static inline std::unordered_set<uint64_t> s_TriggerPairs;
    static inline std::unordered_set<uint64_t> s_SolidPairs;

    static uint64_t PairKey(uint32_t a, uint32_t b) {
        return a < b ? (uint64_t(a) << 32) | b : (uint64_t(b) << 32) | a;
    }
};
