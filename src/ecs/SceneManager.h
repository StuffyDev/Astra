#pragma once
#include "ecs/Entity.h"
#include <vector>
#include <memory>

class SceneManager {
public:
    SceneManager();

    void AddEntity(const Entity& entity);
    void RemoveEntity(size_t index);

    std::vector<Entity>& GetEntities() { return m_Entities; }
    const std::vector<Entity>& GetEntities() const { return m_Entities; }

    int GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(int index) { m_SelectedEntity = index; }

    Entity* GetSelectedEntityPtr();

private:
    std::vector<Entity> m_Entities;
    int m_SelectedEntity = -1;
};
