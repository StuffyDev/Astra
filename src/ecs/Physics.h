#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_set>
#include <vector>

struct Entity;

enum class PhysicsEventType {
    TriggerEnter,
    TriggerExit,
    Collision,
};

struct PhysicsEvent {
    PhysicsEventType type;
    uint32_t entityA;
    uint32_t entityB;
};

// Мировая поза коллайдера: центр из parent-цепочки, угол = сумма поворотов,
// масштаб = произведение масштабов
struct ColliderPose {
    glm::vec2 center;
    float angleDeg = 0.0f;
    glm::vec2 scale = glm::vec2(1.0f);
};

class Physics {
public:
    // 100 пикселей = 1 метр (дефолтный scale сущности)
    static constexpr float PixelsPerMeter = 100.0f;
    static constexpr float FixedDeltaTime = 1.0f / 60.0f;
    static glm::vec2 Gravity;

    static void Step(std::vector<Entity>& entities, float fixedDeltaTime);

    static const std::vector<PhysicsEvent>& GetEvents() { return s_Events; }
    // true, если с прошлого вызова был хотя бы один шаг симуляции (забирает события)
    static bool ConsumeEvents();
    static void ClearState();
    static ColliderPose WorldPose(const std::vector<Entity>& all, const Entity& e);

private:
    static bool TestOverlap(const std::vector<Entity>& all, const Entity& a, const Entity& b,
                            const ColliderPose& pa, const ColliderPose& pb);
    static bool OBBvsOBB(const Entity& a, const ColliderPose& pa,
                         const Entity& b, const ColliderPose& pb,
                         glm::vec2& normalOut, float& depthOut); // normal: A -> B
    static bool OBBvsCircle(const Entity& box, const ColliderPose& pb,
                            const Entity& circle, const ColliderPose& pc,
                            glm::vec2& normalOut, float& depthOut); // normal: box -> circle
    static bool CirclevsCircle(const Entity& a, const ColliderPose& pa,
                               const Entity& b, const ColliderPose& pb,
                               glm::vec2& normalOut, float& depthOut);
    static void ResolvePair(const std::vector<Entity>& all, Entity& a, Entity& b,
                            const ColliderPose& pa, const ColliderPose& pb,
                            float weightA, float weightB);
    // Неупругий контакт: убираем составляющую скорости, сближающую тела
    static void CancelApproachingVelocity(Entity& a, Entity& b, const glm::vec2& normalAtoB,
                                          float weightA, float weightB);

    static inline std::vector<PhysicsEvent> s_Events;
    static inline std::unordered_set<uint64_t> s_LastTriggerPairs;
    static inline std::unordered_set<uint64_t> s_LastSolidPairs;
    static inline uint64_t s_EventSerial = 0;
    static inline uint64_t s_ConsumedSerial = 0;

    static uint64_t PairKey(uint32_t a, uint32_t b) {
        return a < b ? (uint64_t(a) << 32) | b : (uint64_t(b) << 32) | a;
    }
};
