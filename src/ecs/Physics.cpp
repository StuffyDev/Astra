#include "ecs/Physics.h"
#include "ecs/Entity.h"
#include "ecs/Transforms.h"
#include <algorithm>
#include <cmath>
#include <limits>

glm::vec2 Physics::Gravity = glm::vec2(0.0f, -9.81f * PixelsPerMeter);

void Physics::ClearState() {
    s_Events.clear();
    s_LastTriggerPairs.clear();
    s_LastSolidPairs.clear();
}

// Центр — из полной цепочки; угол — свой + все родительские; масштаб — произведение
// родительских масштабов (собственный scale на коллайдер не влияет — как раньше:
// collider.size хранит половину размера в мировых единицах при масштабе 1).
ColliderPose Physics::WorldPose(const std::vector<Entity>& all, const Entity& e) {
    ColliderPose p;
    p.center = Transforms::WorldPosition(all, e);
    p.angleDeg = e.transform.rotation;
    p.scale = glm::vec2(1.0f);
    const Entity* parent = Transforms::FindById(all, e.parentId);
    int guard = 0;
    while (parent && guard++ < 16) {
        p.angleDeg += parent->transform.rotation;
        p.scale *= parent->transform.scale;
        parent = Transforms::FindById(all, parent->parentId);
    }
    return p;
}

static float CircleRadius(const Entity& e, const ColliderPose& p) {
    return e.collider.radius * std::max(p.scale.x, p.scale.y);
}

// Полупротяжения OBB в мировых координатах
static glm::vec2 HalfExtents(const Entity& e, const ColliderPose& p) {
    return e.collider.size * p.scale;
}

static void OrientAxes(float angleDeg, glm::vec2& u, glm::vec2& v) {
    float a = glm::radians(angleDeg);
    u = glm::vec2(std::cos(a), std::sin(a));
    v = glm::vec2(-std::sin(a), std::cos(a));
}

// Проекция OBB на произвольную ось
static float ProjectOBB(const Entity& e, const ColliderPose& p, const glm::vec2& axis) {
    glm::vec2 u, v;
    OrientAxes(p.angleDeg, u, v);
    glm::vec2 he = HalfExtents(e, p);
    return std::fabs(glm::dot(u * he.x, axis)) + std::fabs(glm::dot(v * he.y, axis));
}

bool Physics::OBBvsOBB(const Entity& a, const ColliderPose& pa,
                       const Entity& b, const ColliderPose& pb,
                       glm::vec2& normalOut, float& depthOut) {
    glm::vec2 au, av, bu, bv;
    OrientAxes(pa.angleDeg, au, av);
    OrientAxes(pb.angleDeg, bu, bv);
    const glm::vec2 axes[4] = { au, av, bu, bv };
    glm::vec2 d = pb.center - pa.center;

    float minOverlap = std::numeric_limits<float>::infinity();
    glm::vec2 best(0.0f);
    for (const glm::vec2& n : axes) {
        float dist = std::fabs(glm::dot(d, n));
        float overlap = ProjectOBB(a, pa, n) + ProjectOBB(b, pb, n) - dist;
        if (overlap <= 0.0f) return false;
        if (overlap < minOverlap) {
            minOverlap = overlap;
            best = n;
        }
    }
    if (glm::dot(d, best) < 0.0f) best = -best;
    normalOut = best;
    depthOut = minOverlap;
    return true;
}

bool Physics::OBBvsCircle(const Entity& box, const ColliderPose& pb,
                          const Entity& circle, const ColliderPose& pc,
                          glm::vec2& normalOut, float& depthOut) {
    float r = CircleRadius(circle, pc);
    glm::vec2 he = HalfExtents(box, pb);

    // В локальную систему координат коробка
    float a = -glm::radians(pb.angleDeg);
    glm::mat2 rot(std::cos(a), std::sin(a), -std::sin(a), std::cos(a));
    glm::vec2 local = rot * (pc.center - pb.center);

    glm::vec2 closest = glm::clamp(local, -he, he);
    glm::vec2 delta = local - closest;
    float dist = glm::length(delta);

    glm::vec2 localNormal;
    if (dist > 1e-6f) {
        if (dist >= r) return false;
        localNormal = delta / dist;
        depthOut = r - dist;
    } else {
        // центр круга внутри коробка — выталкиваем по ближайшей грани
        glm::vec2 toEdge = he - glm::abs(local);
        if (toEdge.x < toEdge.y) {
            localNormal = glm::vec2(local.x >= 0.0f ? 1.0f : -1.0f, 0.0f);
            depthOut = toEdge.x + r;
        } else {
            localNormal = glm::vec2(0.0f, local.y >= 0.0f ? 1.0f : -1.0f);
            depthOut = toEdge.y + r;
        }
    }
    // назад в мировые (rot был поворотом на -angle, обратно — transpose)
    normalOut = glm::transpose(rot) * localNormal;
    return true;
}

bool Physics::CirclevsCircle(const Entity& a, const ColliderPose& pa,
                             const Entity& b, const ColliderPose& pb,
                             glm::vec2& normalOut, float& depthOut) {
    glm::vec2 delta = pb.center - pa.center;
    float dist = glm::length(delta);
    float rr = CircleRadius(a, pa) + CircleRadius(b, pb);
    if (dist >= rr) return false;
    normalOut = dist > 1e-6f ? delta / dist : glm::vec2(0.0f, 1.0f);
    depthOut = rr - dist;
    return true;
}

bool Physics::TestOverlap(const std::vector<Entity>& all, const Entity& a, const Entity& b,
                          const ColliderPose& pa, const ColliderPose& pb) {
    glm::vec2 n;
    float depth;
    if (a.collider.type == ColliderType::Box && b.collider.type == ColliderType::Box)
        return OBBvsOBB(a, pa, b, pb, n, depth);
    if (a.collider.type == ColliderType::Circle && b.collider.type == ColliderType::Circle)
        return CirclevsCircle(a, pa, b, pb, n, depth);
    if (a.collider.type == ColliderType::Box)
        return OBBvsCircle(a, pa, b, pb, n, depth);
    if (b.collider.type == ColliderType::Box)
        return OBBvsCircle(b, pb, a, pa, n, depth);
    (void)all;
    return false;
}

void Physics::CancelApproachingVelocity(Entity& a, Entity& b, const glm::vec2& normalAtoB,
                                        float weightA, float weightB) {
    float relNormalVel = glm::dot(b.rigidbody.velocity - a.rigidbody.velocity, normalAtoB);
    if (relNormalVel >= 0.0f) return; // тела уже расходятся

    float total = weightA + weightB;
    a.rigidbody.velocity += normalAtoB * (relNormalVel * weightA / total);
    b.rigidbody.velocity -= normalAtoB * (relNormalVel * weightB / total);
}

void Physics::ResolvePair(const std::vector<Entity>& all, Entity& a, Entity& b,
                          const ColliderPose& pa, const ColliderPose& pb,
                          float weightA, float weightB) {
    glm::vec2 n(0.0f);
    float depth = 0.0f;
    bool ok = false;
    if (a.collider.type == ColliderType::Box && b.collider.type == ColliderType::Box) {
        ok = OBBvsOBB(a, pa, b, pb, n, depth); // n: A -> B
    } else if (a.collider.type == ColliderType::Circle && b.collider.type == ColliderType::Circle) {
        ok = CirclevsCircle(a, pa, b, pb, n, depth);
    } else if (a.collider.type == ColliderType::Box) {
        ok = OBBvsCircle(a, pa, b, pb, n, depth); // n: box -> circle = A -> B
    } else if (b.collider.type == ColliderType::Box) {
        ok = OBBvsCircle(b, pb, a, pa, n, depth); // n: box -> circle = B -> A
        n = -n;                                    // привести к A -> B
    }
    if (!ok) return;

    float total = weightA + weightB;
    if (total <= 0.0f) return;
    // Двигаем только тела без родителя: у них локальные координаты = мировые
    a.transform.position -= n * depth * (weightA / total);
    b.transform.position += n * depth * (weightB / total);
    CancelApproachingVelocity(a, b, n, weightA, weightB);
    (void)all;
}

// кинематические тела и дети (их ведёт родитель) — вес 0
static float BodyWeight(const Entity& e) {
    return (e.rigidbody.isKinematic || e.parentId != 0) ? 0.0f : 1.0f;
}

void Physics::Step(std::vector<Entity>& entities, float fixedDeltaTime) {
    float dt = fixedDeltaTime;
    s_Events.clear();

    for (auto& entity : entities) {
        if (!entity.active || entity.rigidbody.isKinematic) continue;
        if (entity.parentId != 0) continue; // дети следуют за родителем, не симулируются
        if (entity.rigidbody.useGravity) {
            entity.rigidbody.velocity += Gravity * dt;
        }
        entity.transform.position += entity.rigidbody.velocity * dt;
        float dragFactor = std::min(entity.rigidbody.drag * dt, 1.0f);
        entity.rigidbody.velocity *= (1.0f - dragFactor);
    }

    std::unordered_set<uint64_t> currentTriggerPairs;
    std::unordered_set<uint64_t> currentSolidPairs;

    for (size_t i = 0; i < entities.size(); i++) {
        for (size_t j = i + 1; j < entities.size(); j++) {
            auto& a = entities[i];
            auto& b = entities[j];
            if (!a.active || !b.active) continue;
            if (a.collider.type == ColliderType::None || b.collider.type == ColliderType::None) continue;
            // родственные пары не коллидируют (иначе закрученный родитель расталкивает своих детей)
            if (Transforms::IsDescendantOf(entities, b.id, a.id) ||
                Transforms::IsDescendantOf(entities, a.id, b.id)) continue;

            ColliderPose pa = WorldPose(entities, a);
            ColliderPose pb = WorldPose(entities, b);
            if (!TestOverlap(entities, a, b, pa, pb)) continue;

            uint64_t key = PairKey(a.id, b.id);

            if (a.collider.isTrigger || b.collider.isTrigger) {
                currentTriggerPairs.insert(key);
                continue;
            }

            currentSolidPairs.insert(key);
            if (s_LastSolidPairs.count(key) == 0) {
                s_Events.push_back({ PhysicsEventType::Collision, a.id, b.id });
            }

            float weightA = BodyWeight(a);
            float weightB = BodyWeight(b);
            if (weightA + weightB > 0.0f) {
                ResolvePair(entities, a, b, pa, pb, weightA, weightB);
            }
        }
    }

    for (uint64_t key : currentTriggerPairs) {
        if (s_LastTriggerPairs.count(key) == 0) {
            s_Events.push_back({ PhysicsEventType::TriggerEnter, uint32_t(key >> 32), uint32_t(key & 0xFFFFFFFFu) });
        }
    }
    for (uint64_t key : s_LastTriggerPairs) {
        if (currentTriggerPairs.count(key) == 0) {
            s_Events.push_back({ PhysicsEventType::TriggerExit, uint32_t(key >> 32), uint32_t(key & 0xFFFFFFFFu) });
        }
    }

    s_LastTriggerPairs = std::move(currentTriggerPairs);
    s_LastSolidPairs = std::move(currentSolidPairs);
    s_EventSerial++;
}

bool Physics::ConsumeEvents() {
    if (s_ConsumedSerial == s_EventSerial) return false;
    s_ConsumedSerial = s_EventSerial;
    return true;
}
