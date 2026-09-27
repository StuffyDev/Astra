// Загрузка glTF 2.0 (.gltf + .glb) без внешних зависимостей: первый mesh, все примитивы,
// POSITION/NORMAL/TEXCOORD_0 + индексы. Узловые трансформации не применяются (модель
// в локальных координатах), текстуры не связываются — путь к картинке отдаётся отдельно.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Gltf {

struct Data {
    bool ok = false;
    std::string error;
    std::vector<float> verts;        // pos3 + nrm3 + uv2 (8 float на вершину), как у Renderer
    std::vector<uint32_t> idx;
    std::string baseColorPath;       // путь к текстуре из материала (если есть внутри файла)
};

// Полный путь к модели (.gltf или .glb); для .gltf соседние .bin/изображения читаются
// относительно папки модели
Data Load(const std::string& path);

// Файлы, которые нужны модели (для сборки игры): сам файл + внешние .bin
std::vector<std::string> CollectFiles(const std::string& path);

} // namespace Gltf
