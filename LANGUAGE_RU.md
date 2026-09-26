# Astra — руководство движка

Astra — 2D-игровой движок с редактором наподобие Unity: C++17, OpenGL 4.6, GLFW, ImGui (dockspace).
Редактор, сцены, скрипты и шейдеры — всё в одном бинаре, без внешних рантаймов.

---

## 1. Сборка

Только Linux. Нужны: `cmake` (>= 3.16), `g++` (C++17), `libgl-dev`/Mesa, `X11`-зависимости GLFW.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/Astra
```

Все зависимости (glfw, glm, glad, imgui, stb, miniaudio) тянутся автоматически через
FetchContent при первом конфигурировании. `g++` в системе обязателен: им компилируются
игровые скрипты при входе в Play.

## 2. Проекты

- **File ▸ New Project...** — имя + папка; из пресета `templates/default_project`
  создаётся структура `assets/{scenes,shaders,sprites,scripts,audio,prefabs}` + `project.json`.
- **File ▸ Open Project... / Projects Manager / Open Recent** — переключение между проектами.
  Открытие проекта меняет рабочую директорию движка, все пути ассетов относительны корня проекта.
- **File ▸ Exit** — выход.

## 3. Редактор

### Панели (View ▸ ... включает/выключает)
| Панель | Назначение |
|---|---|
| **Scene** | редактор сцены: гизмо, навигация, drag&drop ассетов прямо в мир |
| **Game** | вид с game-камеры; в Play здесь живёт runtime-UI |
| **Hierarchy** | дерево сущностей; перетаскивание меняет порядок и родителя |
| **Inspector** | компоненты выбранной сущности |
| **Project** | ассеты (вкладка Assets) и **Console** (ошибки скриптов/шейдеров, вывод std::cout/cerr) |

### Навигация по Scene
- Средняя кнопка мыши (или Alt+ЛКМ) — панорама; стрелки — панорама с клавиатуры.
- Колесо — зум. **F** (удерживать) — фокус на выбранной сущности.

### Гизмо и горячие клавиши
| Клавиша | Действие |
|---|---|
| **W / E / R / Q** | Move / Rotate / Scale / Select |
| **Ctrl+N / Ctrl+O** | новая сцена / открыть сцену |
| **Ctrl+S / Ctrl+Shift+S** | сохранить / сохранить как |
| **Ctrl+D** | дублировать поддерево |
| **Ctrl+↑ / Ctrl+↓** | переставить сущность среди сиблингов |
| **Delete / Backspace** | удалить поддерево |
| **F2** | переименовать (сущность или ассет, если фокус в Project) |
| **Ctrl+P** | Play/Edit |
| **Esc** | Stop (вернуться в Edit из Play/Pause) |

Заголовок окна показывает имя сцены и `*`, если есть несохранённые изменения;
при закрытии/смене сцены с правками — диалог подтверждения.

### Play / Pause / Stop
Toolbar: ▶ Play, ⏸ Pause, ⏹ Stop. При входе в Play движок делает снимок сцены;
стоп восстанавливает всё (позиции, скорости, созданные/удалённые объекты) — можно
играть и редактировать без риска сломать сцену. В Edit-режиме объекты и UI Game-view
остаются кликабельными/редактируемыми ровно как в инспекторе, а навигация Scene не ломается.

## 4. Сущности и компоненты

Сущность (`src/ecs/Entity.h`) = Transform + набор компонентов:

- **Transform** — позиция/поворот/масштаб; у ребёнка задаются **относительно родителя**
  (как в Unity: ребёнок наследует движение, поворот и масштаб цепочки).
- **Sprite** — Quad/Circle, цвет, текстура (`texturePath`), пользовательский шейдер (`shaderPath`).
- **Rigidbody** — масса, сопротивление (drag), гравитация, кинематичность.
- **Collider** — Box/Circle, триггер или solid; учитывает поворот/масштаб и parent-цепочку.
- **Camera** — game-камера; `mainCamera` выбирает её для Game-view; камера-ребёнок
  следует за родителем.
- **UI** — экранный элемент (Button/Text/Slider) в координатах Game-view.
- **Audio Source** — клип, громкость, тонца, loop, Play On Awake; превью в инспекторе.
- **Script** — путь к `.cpp` (см. раздел 5).

### Drag & drop
- Картинку из Project — на сущность в Hierarchy или в точку Scene: текстура применится
  (или создастся Quad с этой текстурой).
- `.frag`/шейдер — назначается как `shaderPath` цели.
- Аудиоклип — в аудио-компонент сущности под курсором.
- `.prefab` — инстанцируется в точку броска (в иерархию или прямо в Scene).
- Файл с диска (вне окна) — импортируется в нужную папку `assets/` по расширению.

### Hierarchy
Перетаскивание узла: верхняя треть строки — вставить перед, низ — после,
середина — стать ребёнком. Порядок сиблингов = порядок отрисовки (позже — поверх).

## 5. Скрипты (C++)

Скрипт — обычный C++-файл в `assets/scripts/`. **Инклюды писать не нужно** — при входе в
Play движок генерирует wrapper-TU, который сам подключает `ScriptAPI.h`, `Input`, `Audio`,
`GameUI`, `Physics`, GLFW, glm, `<cmath>` и прочее, компилирует `.so` через
`g++ -shared -fPIC` и грузит через `dlopen`. Ошибки компиляции — во вкладке **Console**
и в красном счётчике на тулбаре.

Минимальный скрипт:

```cpp
class RotateScript : public Script {
public:
    void Update(float dt) override {
        Entity* e = Owner();
        if (e) e->transform.rotation += 90.0f * dt;
    }
};
SCRIPT_ENTRY(RotateScript)
```

### API (`src/scripts/ScriptAPI.h`)
```cpp
class Script {
    uint32_t ownerId;
    virtual void Start();
    virtual void Update(float dt);      // dt уже умножен на TimeScale
    virtual void OnDestroy();
    // физическая связь (id второй сущности):
    virtual void OnTriggerEnter(uint32_t otherId);
    virtual void OnTriggerExit(uint32_t otherId);
    virtual void OnCollisionEnter(uint32_t otherId);
protected:
    Entity* Owner();                    // сущность-носитель
    SceneManager* Scene();              // доступ к сущностям/дереву
    void Translate(const glm::vec2& d); // локальный сдвиг (с учётом родителя)
    void SetWorldPosition(const glm::vec2& p);
    glm::vec2 WorldPosition() const;
    float AngleTo(const glm::vec2& worldPoint) const; // градусы
    void LookAt(const glm::vec2& worldPoint);         // «верх» на точку
};

class Time {  // статические
    static float Delta();        // с TimeScale
    static float UnscaledDelta();
    static float SinceStart();   // секунды с Play
    static float TimeScale();
    static void SetTimeScale(float);
};

#define SCRIPT_ENTRY(Class)      // одна фабрика на файл
```

Доступны также (движок подключает заголовки сам):
- `Input::Get()` — клавиши/мышь/экшены/оси: `IsActionHeld`, `GetAxis`, бинды в `Start()`.
- `Audio::PlayOneShot/PlayLooped/Stop/SetVolume/SetPitch` — id-управление клипами.
- `GameUI::WasClicked(entityId)`, `GameUI::GetValue(entityId)` — ответы на кнопки/слайдеры.
- `Physics::Gravity` — мировая гравитация.

## 6. Шейдеры

Ассеты `assets/shaders/*.frag` (+ опционально `*.vert`). В Inspector ▸ Custom Shader
выбирается по имени; в Project — ПКМ ▸ Assign Shader to Selected.

**Достаточно одного `.frag`** — геометрию отдаёт движок (квад, `v_UV` от 0 до 1).

Из движка в шейдер уже инжектятся:
```glsl
// вертекс: a_Pos, u_MVP/u_Model/u_ViewProj, u_Time, u_ScreenSize,
//          v_UV, EngineUV(), EngineQuadVert()
// фрагмент: fragColor, v_UV, u_Color, u_Texture, u_Time, u_ScreenSize
float EngineCircleMask(vec2 uv);
float EngineRoundedBox(vec2 uv, float radius);
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float deg);
float EngineNoise(vec2 p);
float EngineFbm(vec2 p, int octaves);
vec2  EngineSwirl(vec2 uv, vec2 center, float strength, float radius);
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // палитра ИК Квентина
vec3  EngineRainbow(float t);
float EnginePulse(float freq); // 0..1, синусона от u_Time
```

Примеры в `assets/shaders/`: `effect_rounded`, `effect_glow`, `effect_rainbow`,
`effect_plasma`, `example_wobble` (пара vert+frag), `black_hole` (из demos).
Кнопка **Reload** в инспекторе пересобирает кэш шейдеров и текстур.

## 7. Звук

Компонент Audio Source: путь, громкость (0..2), тон (0.1..3), loop, Play On Awake,
Preview/Stop. Ресурсы — `assets/audio/*.{wav,mp3,ogg,flac}` (miniaudio).
Микшер: Edit ▸ Settings — master volume и mute. Ошибки устройства — там же.

## 8. Физика

Fixed timestep 60 Гц. Гравитация по умолчанию (0, -9.81) м/с², 1 м = 100 px —
настраивается в Edit ▸ Settings. Коллайдеры: Box/Circle, триггеры (события) и solid
(разрешение контакта). Движение/поворот/масштаб родителя влияют на дочерние коллайдеры.
События приходят в скрипты как `OnTriggerEnter/Exit`, `OnCollisionEnter`.

## 9. Префабы

ПКМ по сущности (или иконка в инспекторе) ▸ Save as Prefab → `assets/prefabs/`.
Бросок префаба в иерархию/сцену — инстанс; Revert To Prefab — откат изменений инстанса.
Дублирование (`Ctrl+D`) и сериализация работают поддеревом.

## 10. Настройки движка

**Edit ▸ Settings**: гравитация (м/с²), time scale, master volume, mute.
Time scale применяется при входе в Play.

## 11. Примеры

- `examples/black_hole/` — чёрная дыра: шейдер аккреционного диска (только `.frag`),
  скрипт-орбита с `LookAt`, сцена с луной-ребёнком. Уже продублирована в
  `assets/scenes/black_hole.scene` — File ▸ Open Scene и Play.
- `assets/scripts/rotate.cpp`, `player.cpp` — вечное вращение и WASD-персонаж.

## 12. Встроенный редактор кода (IDE-lite)

- Двойной клик по `.cpp`/`.h`/`.frag`/`.vert`/`.txt`/`.json` в панели Project — файл
  открывается во встроенном окне-редакторе. Там же: **ПКМ ▸ Edit (built-in IDE)**.
- **View ▸ Script Editor** — показать/скрыть окно редактора.
- **Ctrl+S** в окне редактора — сохранить файл (пока окно в фокусе, горячие клавиши
  редактора сцены не срабатывают).
- **Ctrl+C / Ctrl+V / Ctrl+X / Ctrl+Z** работают нативно (ImGui InputTextMultiline +
  GLFW clipboard).
- **Open Externally** — открыть файл в системном редакторе (`xdg-open`).

## 13. Import из других источников

- **Project ▸ Create ▸ Import File...** — файловый браузер диска; выбранный файл
  копируется в нужную папку `assets/` по расширению (textures/audio/scenes/scripts/shaders/prefabs),
  остальное — в `assets/imported`.
- Drag&drop из файлового менеджера работает как раньше.

## 14. Console

- Вкладка **Console** рядом с Assets; счётчик ошибок в имени вкладки.
- **Follow** — автопрокрутка вниз только если вы уже внизу (можно отключить).
- **Copy All** — весь лог в буфер; клик по строке копирует её.

## 15. Runtime UI (Game GUI)

- Элементы Button/Text/Slider/Checkbox/Progress Bar.
- `transform.position/scale` — **мировые координаты** (gizmo в Scene совпадает с
  положением кнопки).
- Стиль: `UITextColor`, `UIBgColor`, `UIFontScale` (0/1/2 = обычный/средний/крупный).
- В Edit-режиме элементы видны и в Game-view, и как рамка-превью прямо в Scene.

## 16. Build Game и Player

- **File ▸ Build Game...** — собирает standalone-папку:
  - `astra` (копия текущего бинара),
  - `assets/` (сцены/шейдеры/текстуры/звук/скрипты),
  - `build-scripts/*.so` — **предкомпилированные скрипты** (g++ на целевой машине не нужен),
  - `game.json` — `{"scene": "assets/scenes/...", ...}` (стартовая сцена).
- Запуск: `cd <папка> && ./astra --play` (или `Astra --play --scene ... --project ...`).
- В плеере ESC закрывает окно; редактирования нет, только игра.

## 17. Ограничения текущей версии

- Один проект/одна сцена в памяти; нет аддитивных сцен.
- У дочерних Rigidbody физика не симулируется (двигаются родителем).
- Нет анимаций и тайлкового рендера.
