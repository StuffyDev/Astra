#pragma once
#include <string>
#include <vector>

class SceneManager;
struct Entity;

class SceneSerializer {
public:
    static void Save(SceneManager* sceneManager, const std::string& path);
    static bool Load(SceneManager* sceneManager, const std::string& path);

    // Поддерево/префаб: набор сущностей «как есть», родители сохраняются индексами внутри набора
    static bool SaveEntities(const std::vector<Entity>& entities, const std::string& path);

    // Читает набор: временные id 1..n, parentId уже пересобран по этим id;
    // outParentIndices дублирует связи индексами (удобно для инстанцирования в сцену)
    static bool LoadEntities(const std::string& path, std::vector<Entity>& out);
};
