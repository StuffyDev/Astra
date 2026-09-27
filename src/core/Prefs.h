// Пользовательские настройки движка/редактора (живут в Process, правятся в Edit ▸ Settings).
// inline-статические — без отдельного .cpp и без правки CMake.
#pragma once
#include <glm/glm.hpp>

struct AstraPrefs {
    static inline glm::vec4 ClearColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f); // фон сцен/игры
    static inline bool ShowGrid = true;      // сетка в Scene
    static inline float GridSize = 50.0f;    // шаг сетки и снапа перемещения, px
    static inline float SnapDegrees = 15.0f; // шаг снапа вращения, °
    static inline bool ShowColliders = true; // отрисовка коллайдеров в Scene
};
