#pragma once
#include "ecs/Entity.h"
#include <vector>
#include <memory>

class SceneManager {
public:
    SceneManager();

    void AddEntity(const Entity& entity);
    void RemoveEntity(size_t index);
    void MoveEntityUp(size_t index);
    void MoveEntityDown(size_t index);
    void DuplicateEntity(size_t index);

    std::vector<Entity>& GetEntities() { return m_Entities; }
    const std::vector<Entity>& GetEntities() const { return m_Entities; }

    int GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(int index) { m_SelectedEntity = index; }

    Entity* GetSelectedEntityPtr();

    void Clear();

    // Для drag-and-drop в иерархии
    void SwapEntities(size_t from, size_t to);

    // parentIndex >= m_Entities.size() — сбросить в root
    bool SetParent(size_t childIndex, size_t parentIndex);

    // mode: -1 вставить перед ref как сиблинг, +1 после ref, 0 — сделать ребёнком ref
    bool MoveEntity(uint32_t movedId, uint32_t refId, int mode);

    // --- Префабы ---
    // Индексы корня и всех потомков (корень идёт первым)
    void CollectSubtree(size_t rootIndex, std::vector<size_t>& out) const;
    void DeleteSubtree(size_t rootIndex);
    // Копия поддерева (корень первым, id реальные) — для сохранения в префаб
    std::vector<Entity> GetSubtree(size_t rootIndex) const;
    // Клон поддерева с небольшим смещением (Ctrl+D)
    void DuplicateSubtree(size_t rootIndex);
    // Добавить набор прототипов (временные id 1..n, корень — элемент 0) как новые сущности.
    // worldOffset смещает корень. Возвращает индекс нового корня или -1.
    int InstantiateProtos(const std::vector<Entity>& protos, const glm::vec2& worldOffset);

    uint32_t NextId() { return m_NextId++; }

    // Снимок сцены для Play-mode (Stop восстанавливает состояние редактора)
    struct SceneSnapshot {
        std::vector<Entity> entities;
        int selection = -1;
        uint32_t nextId = 1;
    };
    SceneSnapshot TakeSnapshot() const { return { m_Entities, m_SelectedEntity, m_NextId }; }
    void Restore(const SceneSnapshot& snap) {
        m_Entities = snap.entities;
        m_SelectedEntity = snap.selection;
        m_NextId = snap.nextId;
    }

private:
    std::vector<Entity> m_Entities;
    int m_SelectedEntity = -1;
    uint32_t m_NextId = 1;
};
