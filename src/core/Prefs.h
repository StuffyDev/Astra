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
    static inline bool LightTheme = false;   // светлая тема редактора
    // 3D-освещение: направление солнца (нормализуется при использовании), цвет, ambient
    static inline glm::vec3 LightDir = glm::vec3(-0.4f, 0.7f, 0.5f);
    static inline glm::vec3 LightColor = glm::vec3(1.0f, 0.98f, 0.94f);
    static inline float Ambient = 0.25f;
    static inline bool Shadows = true;    // тени от солнца (depth map + PCF)
    static inline int ShadowSize = 2048;  // сторона карты теней: 1024/2048/4096
};
