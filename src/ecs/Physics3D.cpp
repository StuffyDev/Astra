#include "ecs/Physics3D.h"
#include "ecs/Entity.h"
#include "ecs/Transforms.h"
#include "ecs/Physics.h"
#include <algorithm>
#include <cmath>

glm::vec3 Physics3D::Gravity = glm::vec3(0.0f, -981.0f, 0.0f);   // 9.81 м/с² при 100 ед = 1 м

namespace {

glm::mat3 RotationOf(const glm::vec3& deg) {
    glm::mat4 m(1.0f);   // glm 0.9.9 умеет крутить только mat4 — берём верхний левый блок
    m = glm::rotate(m, glm::radians(deg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(deg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(deg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::mat3(m);
}

} // namespace

bool Physics3D::IsDynamic(const Entity& e) {
    return e.hasRigidbody3D && !e.rb3.isKinematic && e.parentId == 0;
}

Physics3D::Bounds Physics3D::WorldBounds(const std::vector<Entity>& all, const Entity& e) {
    Bounds b;
    glm::mat3 R = RotationOf(e.rot3);
    b.center = Transforms::WorldPos3(all, e) + R * e.col3.center;
    if (e.col3.type == Collider3DType::Sphere) {
        b.sphere = true;
        float s = std::max({std::fabs(e.scale3.x), std::fabs(e.scale3.y), std::fabs(e.scale3.z)});
        b.radius = std::max(e.col3.radius, 0.001f) * s;
        b.half = glm::vec3(b.radius);
    } else {
        glm::vec3 h = e.col3.half * e.scale3;
        // |R| * h — габарит повёрнутого короба (AABB-приближение, как в дешёвом broad phase)
        glm::vec3 c0 = glm::abs(R[0]), c1 = glm::abs(R[1]), c2 = glm::abs(R[2]);
        b.half = c0 * h.x + c1 * h.y + c2 * h.z;
    }
    return b;
}

bool Physics3D::Overlap(const Bounds& a, const Bounds& b, glm::vec3& normalAtoB, float& depth) {
    if (a.sphere && b.sphere) {
        glm::vec3 d = b.center - a.center;
        float len = glm::length(d);
        float sum = a.radius + b.radius;
        if (len >= sum) return false;
        normalAtoB = len > 1e-6f ? d / len : glm::vec3(0.0f, 1.0f, 0.0f);
        depth = sum - len;
        return true;
    }
    if (a.sphere != b.sphere) {
        const Bounds& box = a.sphere ? b : a;
        const Bounds& sph = a.sphere ? a : b;
        glm::vec3 q = glm::clamp(sph.center, box.center - box.half, box.center + box.half);
        glm::vec3 d = sph.center - q;
        float len = glm::length(d);
        if (len > sph.radius) return false;
        if (len > 1e-6f) {
            normalAtoB = d / len;                       // от короба к сфере
            depth = sph.radius - len;
        } else {                                        // центр сферы внутри короба
            glm::vec3 off = sph.center - box.center;
            int axis = 0;
            float best = -1.0f;
            for (int i = 0; i < 3; i++) {
                float room = box.half[i] + (off[i] >= 0.0f ? -off[i] : -off[i]);
                room = box.half[i] - std::fabs(off[i]);
                if (room > best) { best = room; axis = i; }
            }
            normalAtoB = glm::vec3(0.0f);
            normalAtoB[axis] = off[axis] >= 0.0f ? 1.0f : -1.0f;
            depth = sph.radius + best;
        }
        if (a.sphere) normalAtoB = -normalAtoB;         // нормаль всегда A -> B
        return true;
    }
    // box vs box (AABB)
    glm::vec3 gap = b.half + a.half - glm::abs(b.center - a.center);
    if (gap.x <= 0.0f || gap.y <= 0.0f || gap.z <= 0.0f) return false;
    int axis = 0;
    if (gap.y < gap.x) axis = 1;
    if (gap.z < gap[axis]) axis = 2;
    normalAtoB = glm::vec3(0.0f);
    normalAtoB[axis] = (b.center[axis] >= a.center[axis]) ? 1.0f : -1.0f;
    depth = gap[axis];
    return true;
}

void Physics3D::ClearState() {
    s_Events.clear();
    s_TriggerPairs.clear();
    s_SolidPairs.clear();
}

bool Physics3D::ConsumeEvents() {
    if (s_Events.empty()) return false;
    s_Events.clear();
    return true;
}

void Physics3D::Step(std::vector<Entity>& entities, float dt) {
    if (dt <= 0.0f) return;

    // 1) интеграция динамических тел
    for (auto& e : entities) {
        if (!e.active || !e.is3D || !e.hasRigidbody3D) continue;
        if (!IsDynamic(e)) continue;
        if (e.rb3.useGravity) e.rb3.velocity += Gravity * dt;
        float keep = std::max(0.0f, 1.0f - e.rb3.drag * dt);
        e.rb3.velocity *= keep;
        e.pos3 += e.rb3.velocity * dt;
    }

    std::unordered_set<uint64_t> liveTriggers, liveSolids;

    // 2) контакты: два прохода — стопки коробов расходятся устойчивее
    for (int iter = 0; iter < 2; iter++) {
        for (size_t i = 0; i < entities.size(); i++) {
            if (!entities[i].active || !entities[i].is3D || !entities[i].hasCollider3D) continue;
            for (size_t j = i + 1; j < entities.size(); j++) {
                Entity& A = entities[i];
                Entity& B = entities[j];
                if (!B.active || !B.is3D || !B.hasCollider3D) continue;
                if (!A.hasRigidbody3D && !B.hasRigidbody3D) continue;
                if (A.parentId == B.id || B.parentId == A.id) continue;

                Bounds ba = WorldBounds(entities, A), bb = WorldBounds(entities, B);
                glm::vec3 n; float depth = 0.0f;
                if (!Overlap(ba, bb, n, depth)) continue;

                uint64_t key = PairKey(A.id, B.id);
                bool trigger = A.col3.isTrigger || B.col3.isTrigger;
                if (trigger) {
                    liveTriggers.insert(key);
                    if (iter == 0 && !s_TriggerPairs.count(key))
                        s_Events.push_back({PhysicsEventType::TriggerEnter, A.id, B.id});
                    continue;
                }

                liveSolids.insert(key);
                if (iter == 0 && !s_SolidPairs.count(key))
                    s_Events.push_back({PhysicsEventType::Collision, A.id, B.id});

                float wA = IsDynamic(A) ? 1.0f / std::max(A.rb3.mass, 0.01f) : 0.0f;
                float wB = IsDynamic(B) ? 1.0f / std::max(B.rb3.mass, 0.01f) : 0.0f;
                float sum = wA + wB;
                if (sum <= 0.0f) continue;                 // оба статичны — нечего двигать

                A.pos3 -= n * (depth * (wA / sum));
                B.pos3 += n * (depth * (wB / sum));

                // убираем составляющую скорости, которая тела сближает
                glm::vec3 rel = (IsDynamic(B) ? B.rb3.velocity : glm::vec3(0.0f)) -
                                (IsDynamic(A) ? A.rb3.velocity : glm::vec3(0.0f));
                float vn = glm::dot(rel, n);
                if (vn < 0.0f) {
                    if (wA > 0.0f) A.rb3.velocity += n * (vn * (wA / sum));
                    if (wB > 0.0f) B.rb3.velocity -= n * (vn * (wB / sum));
                }
            }
        }
    }

    // 3) выходы триггеров и завершение контактов
    for (uint64_t key : s_TriggerPairs)
        if (!liveTriggers.count(key)) {
            uint32_t a = uint32_t(key >> 32), b = uint32_t(key & 0xFFFFFFFFu);
            s_Events.push_back({PhysicsEventType::TriggerExit, a, b});
        }
    s_TriggerPairs = liveTriggers;
    s_SolidPairs = liveSolids;
}
