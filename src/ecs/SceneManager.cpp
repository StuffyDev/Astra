#include "ecs/SceneManager.h"
#include "ecs/Physics.h"
#include "ecs/Transforms.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

SceneManager::SceneManager() = default;

void SceneManager::AddEntity(const Entity& entity) {
    m_Entities.push_back(entity);
    m_Entities.back().id = NextId();
}

void SceneManager::RemoveEntity(size_t index) {
    if (index < m_Entities.size()) {
        uint32_t removedId = m_Entities[index].id;
        m_Entities.erase(m_Entities.begin() + index);
        if (m_SelectedEntity == static_cast<int>(index)) m_SelectedEntity = -1;
        else if (m_SelectedEntity > static_cast<int>(index)) m_SelectedEntity--;
        // Дети удалённого родителя становятся корнями
        for (auto& e : m_Entities) {
            if (e.parentId == removedId) e.parentId = 0;
        }
    }
}

int SceneManager::IndexOf(uint32_t id) const {
    for (size_t i = 0; i < m_Entities.size(); i++)
        if (m_Entities[i].id == id) return static_cast<int>(i);
    return -1;
}

bool SceneManager::RemoveEntityById(uint32_t id) {
    int idx = IndexOf(id);
    if (idx < 0) return false;
    RemoveEntity(static_cast<size_t>(idx));
    return true;
}

bool SceneManager::SetParent(size_t childIndex, size_t parentIndex) {
    if (childIndex >= m_Entities.size()) return false;
    if (childIndex == parentIndex) return false;

    Entity& child = m_Entities[childIndex];

    if (parentIndex >= m_Entities.size()) { // SIZE_MAX = сбросить на root
        child.parentId = 0;
        return true;
    }

    Entity& parent = m_Entities[parentIndex];
    // нельзя сделать родителем собственного потомка (цикл)
    if (Transforms::IsDescendantOf(m_Entities, parent.id, child.id)) return false;
    if (parent.id == child.id) return false;

    child.parentId = parent.id;
    return true;
}

bool SceneManager::MoveEntity(uint32_t movedId, uint32_t refId, int mode) {
    if (movedId == refId) return false;
    size_t movedIdx = SIZE_MAX, refIdx = SIZE_MAX;
    for (size_t i = 0; i < m_Entities.size(); i++) {
        if (m_Entities[i].id == movedId) movedIdx = i;
        if (m_Entities[i].id == refId) refIdx = i;
    }
    if (movedIdx == SIZE_MAX || refIdx == SIZE_MAX) return false;

    if (mode == 0) return SetParent(movedIdx, refIdx);

    // Вставка сиблингом рядом с ref: нельзя вставить внутрь самого себя
    uint32_t newParent = m_Entities[refIdx].parentId;
    if (newParent == movedId) return false;
    if (Transforms::IsDescendantOf(m_Entities, refId, movedId)) return false;

    Entity moved = m_Entities[movedIdx];
    moved.parentId = newParent;
    m_Entities.erase(m_Entities.begin() + movedIdx);
    if (movedIdx < refIdx) refIdx--; // ref сдвинулся после удаления
    size_t insertAt = (mode < 0) ? refIdx : refIdx + 1;
    m_Entities.insert(m_Entities.begin() + static_cast<long>(insertAt), moved);
    m_SelectedEntity = static_cast<int>(insertAt);
    return true;
}

void SceneManager::MoveEntityUp(size_t index) {
    if (index > 0 && index < m_Entities.size()) {
        std::swap(m_Entities[index], m_Entities[index - 1]);
        if (m_SelectedEntity == static_cast<int>(index)) m_SelectedEntity = static_cast<int>(index - 1);
        else if (m_SelectedEntity == static_cast<int>(index - 1)) m_SelectedEntity = static_cast<int>(index);
    }
}

void SceneManager::MoveEntityDown(size_t index) {
    if (index < m_Entities.size() - 1) {
        std::swap(m_Entities[index], m_Entities[index + 1]);
        if (m_SelectedEntity == static_cast<int>(index)) m_SelectedEntity = static_cast<int>(index + 1);
        else if (m_SelectedEntity == static_cast<int>(index + 1)) m_SelectedEntity = static_cast<int>(index);
    }
}

void SceneManager::DuplicateEntity(size_t index) {
    if (index < m_Entities.size()) {
        Entity copy = m_Entities[index];
        copy.name = copy.name + " (Copy)";
        m_Entities.insert(m_Entities.begin() + index + 1, copy);
        m_Entities[index + 1].id = NextId();
    }
}

void SceneManager::CollectSubtree(size_t rootIndex, std::vector<size_t>& out) const {
    if (rootIndex >= m_Entities.size()) return;
    out.push_back(rootIndex);
    uint32_t rootId = m_Entities[rootIndex].id;
    for (size_t i = 0; i < m_Entities.size(); i++) {
        if (i != rootIndex && m_Entities[i].parentId == rootId)
            CollectSubtree(i, out);
    }
}

void SceneManager::DeleteSubtree(size_t rootIndex) {
    std::vector<size_t> indices;
    CollectSubtree(rootIndex, indices);
    std::sort(indices.begin(), indices.end(), std::greater<size_t>());
    for (size_t i : indices) RemoveEntity(i);
}

std::vector<Entity> SceneManager::GetSubtree(size_t rootIndex) const {
    std::vector<size_t> indices;
    CollectSubtree(rootIndex, indices);
    std::vector<Entity> out;
    out.reserve(indices.size());
    for (size_t i : indices) out.push_back(m_Entities[i]);
    return out;
}

void SceneManager::DuplicateSubtree(size_t rootIndex) {
    if (rootIndex >= m_Entities.size()) return;
    std::vector<size_t> indices;
    CollectSubtree(rootIndex, indices);

    std::unordered_map<uint32_t, uint32_t> idMap; // реальный id -> временный
    std::vector<Entity> protos;
    protos.reserve(indices.size());
    for (size_t i = 0; i < indices.size(); i++) {
        const Entity& e = m_Entities[indices[i]];
        idMap[e.id] = static_cast<uint32_t>(i + 1);
        Entity p = e;
        p.id = static_cast<uint32_t>(i + 1);
        p.parentId = 0;
        protos.push_back(p);
    }
    for (size_t i = 0; i < indices.size(); i++) {
        auto it = idMap.find(m_Entities[indices[i]].parentId);
        if (it != idMap.end()) protos[i].parentId = it->second;
    }
    protos.front().name += " (Copy)";
    InstantiateProtos(protos, glm::vec2(25.0f, 25.0f));
}

int SceneManager::InstantiateProtos(const std::vector<Entity>& protos, const glm::vec2& worldOffset) {
    if (protos.empty()) return -1;

    size_t base = m_Entities.size();
    std::unordered_map<uint32_t, uint32_t> idMap; // временный id -> реальный
    for (const auto& p : protos) {
        Entity e = p;
        uint32_t tmpId = p.id;
        e.parentId = 0; // переставим ниже
        AddEntity(e);
        idMap[tmpId] = m_Entities.back().id;
    }
    for (size_t i = 0; i < protos.size(); i++) {
        if (protos[i].parentId == 0) continue;
        auto it = idMap.find(protos[i].parentId);
        if (it != idMap.end()) m_Entities[base + i].parentId = it->second;
    }
    m_Entities[base].transform.position += worldOffset;
    SetSelectedEntity(static_cast<int>(base));
    return static_cast<int>(base);
}

Entity* SceneManager::GetSelectedEntityPtr() {
    if (m_SelectedEntity >= 0 && m_SelectedEntity < static_cast<int>(m_Entities.size())) {
        return &m_Entities[m_SelectedEntity];
    }
    return nullptr;
}

void SceneManager::Clear() {
    m_Entities.clear();
    m_SelectedEntity = -1;
    Physics::ClearState();
}

void SceneManager::SwapEntities(size_t from, size_t to) {
    if (from < m_Entities.size() && to < m_Entities.size()) {
        std::swap(m_Entities[from], m_Entities[to]);
        if (m_SelectedEntity == static_cast<int>(from)) m_SelectedEntity = static_cast<int>(to);
        else if (m_SelectedEntity == static_cast<int>(to)) m_SelectedEntity = static_cast<int>(from);
    }
}
