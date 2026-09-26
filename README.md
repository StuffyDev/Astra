# Astra

2D-игровой движок с редактором в стиле Unity. C++17, OpenGL 4.6, GLFW, ImGui (dockspace),
miniaudio. Всё в одном бинарнике: редактор, сцены, C++-скрипты с hot-load, шейдерные
материалы, звук, UI, сборка игры в один исполняемый файл.

Linux-only. Сборка:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/Astra
```

## Документация

| Язык | Руководство движка | API скриптов (C++) | API шейдеров (GLSL) |
|---|---|---|---|
| 🇷 Русский | [ENGINE](documentation/ru/ENGINE.md) | [SCRIPT_API](documentation/ru/SCRIPT_API.md) | [SHADER_API](documentation/ru/SHADER_API.md) |
| 🇬 English | [ENGINE](documentation/en/ENGINE.md) | [SCRIPT_API](documentation/en/SCRIPT_API.md) | [SHADER_API](documentation/en/SHADER_API.md) |
| 🇨🇳 中文 | [ENGINE](documentation/zh/ENGINE.md) | [SCRIPT_API](documentation/zh/SCRIPT_API.md) | [SHADER_API](documentation/zh/SHADER_API.md) |
| 🇩🇪 Deutsch | [ENGINE](documentation/de/ENGINE.md) | [SCRIPT_API](documentation/de/SCRIPT_API.md) | [SHADER_API](documentation/de/SHADER_API.md) |

## Возможности

- **Редактор**: докспейс (Scene/Game/Script/Hierarchy/Inspector/Project+Console), гизмо
  W/E/R/Q, drag&drop ассетов, undo/redo (Ctrl+Z / Ctrl+Shift+Z), copy/paste поддеревьев,
  встроенный IDE (^C/^V/^X/^Z, Ctrl+S), Play/Pause/Stop/Restart со снапшотом.
- **Скрипты**: C++ без инклюдов (движок генерирует wrapper-TU), кэш компиляции по mtime,
  сериализуемые переменные с ползунками в инспекторе (аналог [SerializeField]),
  триггеры/коллизии-хуки, `DestroyEntity`, `LoadScene` (менеджер сцен), `Log`.
- **Графика**: спрайт-анимации (сетка Cols×Rows), кастомные `.frag` без вертекса,
  GLSL-хелперы (Fbm, Swirl, Palette, Grid…), **Material** — живые `u_Params`/`u_PColor`
  из инспектора.
- **Звук**: miniaudio, AudioSource (volume/pitch/loop/playOnAwake), мастер-шина.
- **Релиз**: File ▸ Build Game — папка или **один exe** с приклеенным бандлом ассетов
  и предкомпилированными скриптами (g++ на целевой машине не нужен). `--play` без редактора.

Примеры: `assets/scenes/black_hole.scene` (шейдерный мир + орбита скриптом),
`assets/scenes/animation_demo.scene` (спрайтшит + материалы).
