# Astra — руководство пользователя движка

Astra — 2D-игровой движок с редактором в стиле Unity: C++17, OpenGL 4.6, GLFW, ImGui (dockspace).
Редактор, сцены, скрипты, шейдеры, звук и сборка игры — в одном бинаре, без внешних рантаймов.

Содержание: [сборка](#1-сборка-и-запуск) · [проекты](#2-проекты) · [интерфейс](#3-интерфейс-редактора) ·
[сущности](#4-сущности-и-компоненты) · [сцены](#5-сцены) · [ассеты](#6-ассеты-и-импорт) ·
[UI](#7-игровой-интерфейс) · [undo](#8-undo--redo) · [горячие клавиши](#9-горячие-клавиши) ·
[настройки](#10-настройки-движка) · [билд](#11-сборка-игры-и-плеер) · [консоль](#12-консоль)

---

## 1. Сборка и запуск

Только Linux. Нужны: `cmake` (>= 3.16), `g++` (C++17), Mesa/`libgl-dev`, X11-зависимости GLFW.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Astra                 # редактор
./build/Astra --play          # плеер (игра без редактора)
```

Все зависимости (glfw, glm, glad, imgui, stb, miniaudio) подтягивает FetchContent при первом
конфигурировании. `g++` в системе обязателен: им компилируются игровые скрипты (см. SCRIPT_API.md).

## 2. Проекты

- **File ▸ New Project...** — имя + папка; из `templates/default_project` создаётся структура
  `assets/{scenes,shaders,scripts,audio,textures,prefabs}` + `project.json`.
- **File ▸ Open Project... / Projects Manager / Open Recent** — переключение проектов.
  Открытие проекта меняет рабочую директорию движка: все пути ассетов относительны корня проекта.
- **File ▸ Exit** — выход.

## 3. Интерфейс редактора

Один докспейс: окна можно вытаскивать за заголовки, склеивать в табы, resizing — за границы.

| Окно | Что делает |
|---|---|
| **Scene** | редактирование: гизмо, выделение, drag&drop, превью коллайдеров и UI |
| **Game** | вид игровой камеры; здесь живут кнопки/слайдеры UI (и их превью в Edit) |
| **Script** | встроенный IDE: двойной клик по `.cpp/.frag/...` в Project открывает файл здесь |
| **Hierarchy** | дерево сущностей; drag = репарентинг; Ctrl+↑/↓ — перестановка сиблингов |
| **Inspector** | компоненты выбранной сущности + Script Variables + Material |
| **Project** | браузер `assets/` (grid/list, поиск, Create, Import) и вкладка **Console** |

Тулбар: **Play / Pause / Stop / Restart**, статус `PLAYING/PAUSED`, счётчик ошибок скриптов
(красная кнопка — клик открывает Console), инструменты **Move (W) / Rotate (E) / Scale (R) / Hand (Q)**.

Навигация в Scene: ПКМ/СКМ — пан, колесо — зум к курсору, F — фокус на выбранном,
стрелки — пан при фокусе в Scene. ESC — выход из Play в Edit.

## 4. Сущности и компоненты

Сущность = набор фиксированных компонентов (пока так; чистый ECS — в роадмапе):

- **Transform** — Position/Rotation/Scale. Есть `Parent`: дети наследуют позу родителя
  (мировые матрицы как в Unity, включая масштаб/поворот цепочки).
- **Sprite** — Type (None/Quad/Circle), Color, Texture Path, Custom Shader (см. SHADER_API.md).
- **Animation** — спрайтшит: `Cols × Rows` (row 0 = верхний ряд), FPS, Loop, Play On Awake,
  `Active` (в Edit кадры крутятся как превью). Отдельная Sheet Path или «Use Sprite».
  Из скриптов: `PlayAnimation/StopAnimation/IsAnimating`.
- **Rigidbody** — Kinematic, Velocity, Mass, Drag, Use Gravity. Физика: fixed 60 Гц,
  MTV-разрешения, триггеры/коллизии с событиями скриптов.
- **Collider** — Box (половинки размера) / Circle (радиус), Is Trigger. Визуализация в Scene.
- **Camera** — Main Camera (одна активная), Zoom, Viewport Offset. Game-view смотрит
  через неё; UI-элементы позиционируются в мировых координатах этого вида.
- **UI Element** — см. раздел 7.
- **Audio Source** — Clip Path, Volume, Pitch, Loop, Play On Awake, кнопки Preview/Stop.
- **Script** — C++-скрипт (SCRIPT_API.md). **Script Variables** — `DefineVar("speed", 120)`
  в `Start()`, ползунок появляется в инспекторе, значение сериализуется в сцену.

GameObject ▸ Create Empty/Quad/Circle/Camera/UI — быстрые пресеты.

## 5. Сцены

- Формат текста: `Astra Scene v2` (одна сущность = блок `ENTITY ... END_ENTITY`).
- Ctrl+N / Ctrl+S / Ctrl+Shift+S — новая/сохранить/сохранить как; звёздочка в заголовке =
  несохранённые изменения; подтверждение при закрытии несохранённой.
- Двойной клик по `.scene` в Project — открыть (с подтверждением, если текущая грязная).
- **Несколько сцен в игре**: `LoadScene("assets/scenes/level2.scene")` из скрипта —
  менеджер сцен переключает уровень на ходу (работает и в Play, и в собранной игре).

## 6. Ассеты и импорт

- Drag&drop из файлового менеджера ОС в окно — импорт по типу (картинки → `assets/textures`,
  `.scene` → `assets/scenes`, `.cpp` → `assets/scripts`, аудио → `assets/audio`).
- **Project ▸ Create ▸ Import File...** — выбор файла с диска тем же браузером.
- **ПКМ ▸ Open Externally** — открыть файл в системном редакторе (`xdg-open`).
- **ПКМ ▸ Edit (built-in IDE)** — открыть в окне Script. Ctrl+S — сохранить.
- Картинки/шейдеры/префабы можно тащить прямо на объект в Scene или на строку Hierarchy —
  назначатся/инстанцируются.
- Create ▸ Shader (пару `.vert+.frag`) или один `.frag` — шаблон с подсказками API.

## 7. Игровой интерфейс

Entity + компонент UI Element: **Button / Text / Slider / Checkbox / Progress Bar**.
Позиция и размер — в **мировых координатах** (gizmo в Scene совпадает с местом кнопки).
Стиль: Text Color, Bg Color, Font (Default/Medium/Large). Чекбокс — честный bool (0/1).
В Edit элементы видны и в Game-view, и рамками в Scene. В Play они кликабельны;
скрипт читает `GameUI::WasClicked(id)` / `GameUI::GetValue(id)`.

## 8. Undo / Redo

- **Ctrl+Z** — шаг назад, **Ctrl+Shift+Z** (или Ctrl+Y) — шаг вперёд.
- Откатывает правки сцены: перемещение гизмо, изменения в инспекторе, создание/удаление,
  copy/paste, репарентинг, инстанс префабов. Одна «пачка» непрерывных правок = один шаг.
- Глубина — 60 шагов. Сохранение сцены сбрасывает точку отсчёта.

## 9. Горячие клавиши

| Клавиши | Действие |
|---|---|
| Ctrl+N / O / S / Shift+S | сцена новая / открыть / сохранить / сохранить как |
| Ctrl+P | Play/Stop | Ctrl+D | дублировать поддерево |
| Ctrl+C / Ctrl+V | копировать / вставить поддерево сущности |
| Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y | undo / redo |
| Del / Backspace | удалить сущность | F2 — переименовать |
| W / E / R / Q | Move / Rotate / Scale / Hand |
| Ctrl+↑ / ↓ | переставить сиблинга | F — фокус камеры на выбранном |
| Ctrl (удерживать при drag) | снап гизмо (50 px / 15°) |
| ESC | выход из Play; в плеере — закрыть игру |
| Ctrl+S в окне Script | сохранить файл (в IDE работают ^C/^V/^X/^Z) |

## 10. Настройки движка

**Edit ▸ Settings**: гравитация (м/с², Unity-style 0,-9.81), Time Scale (применяется при входе
в Play, переживает Stop), Master Volume, Mute, статус аудиосистемы.

## 11. Сборка игры и плеер

**File ▸ Build Game...** — два режима:

1. **Папка**: `astra` (бинарь) + `assets/` + `build-scripts/*.so` (предкомпилированные скрипты —
   g++ на целевой машине НЕ нужен) + `game.json` (стартовая сцена). Запуск: `./astra --play`.
2. **Один исполняемый файл** (галочка): к бинарнику приклеен бандл (всё то же самое).
   Первый запуск распакует его в `<имя>.bundle/` рядом с собой и запустит игру. Один файл = игра.

CLI без GUI: `./Astra --build assets/scenes/x.scene --out ./game [--folder]`.
Флаги плеера: `--play`, `--scene <path>`, `--project <dir>`. В игре доступен весь скриптовый
API, включая `LoadScene` (уровни) и `Log` (пишет в консоль).

## 12. Консоль

Вкладка **Console** в панели Project: stdout/stderr движка, скриптов и ошибок компиляции.
**Follow** — автопрокрутка только если вы и так внизу; **Copy All** — весь лог в буфер;
клик по строке — копирование строки. Счётчик ошибок — на тулбаре (красная кнопка).

## 13. Примеры

- `assets/scenes/black_hole.scene` — шейдерный мир (аккреционный диск, орбита скриптом, луна-ребёнок).
- `assets/scenes/animation_demo.scene` — спрайтшит-мяч 4×2 + живые Material-параметры.
- `assets/scripts/rotate.cpp`, `player.cpp`, `examples/black_hole/orbit_planet.cpp`.
- `examples/` — исходники примеров; `templates/default_project` — шаблон проекта.
