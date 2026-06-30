#include "ecs/SceneManager.h"

SceneManager::SceneManager() = default;

void SceneManager::AddEntity(const Entity& entity) {
    m_Entities.push_back(entity);
}

void SceneManager::RemoveEntity(size_t index) {
    if (index < m_Entities.size()) {
        m_Entities.erase(m_Entities.begin() + index);
        if (m_SelectedEntity == static_cast<int>(index)) m_SelectedEntity = -1;
        else if (m_SelectedEntity > static_cast<int>(index)) m_SelectedEntity--;
    }
}

Entity* SceneManager::GetSelectedEntityPtr() {
    if (m_SelectedEntity >= 0 && m_SelectedEntity < static_cast<int>(m_Entities.size())) {
        return &m_Entities[m_SelectedEntity];
    }
    return nullptr;
}
