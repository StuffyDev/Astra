# Astra — API игровых скриптов (C++)

Скрипты — это обычные C++-классы. Движок компилирует их `g++ -std=c++17 -shared -fPIC` в
`build-scripts/<имя>.so` при входе в Play (и при сборке игры) и грузит через `dlopen`.
Горячая перезагрузка: изменённый `.cpp` перекомпилируется, инстансы пересоздаются.
**Инклюды писать не нужно** — движок генерирует wrapper-TU, который сам подключает
`ScriptAPI.h`, `SceneManager.h`, `Transforms.h`, `Physics.h`, `Input.h`, `GameUI.h`,
`Audio.h`, GLFW, glm и `<cmath>/<string>/<vector>/<algorithm>/<random>`.

## 1. Минимальный скрипт

```cpp
class Rotate : public Script {
public:
    void Update(float dt) override {
        Entity* e = Owner();
        if (e) e->transform.rotation += 90.0f * dt;
    }
};
SCRIPT_ENTRY(Rotate)   // ровно одна фабрика на файл
```

Вешается в Inspector ▸ Script (combo по `assets/scripts/*.cpp`). Ошибки компиляции —
в Console и красной кнопкой на тулбаре.

## 2. Класс Script

| Хук | Когда вызывается |
|---|---|
| `Start()` | при создании инстанса (вход в Play, спавн, LoadScene) |
| `Update(float dt)` | каждый кадр (dt уже умножен на timeScale) |
| `OnDestroy()` | перед уничтожением инстанса (Stop/StopAnimation-сценарии) |
| `OnTriggerEnter(uint32_t otherId)` / `OnTriggerExit` | вход/выход из триггера |
| `OnCollisionEnter(uint32_t otherId)` | нерастриггерное соударение |
| `OnAnimEvent(const char* name)` | кадр анимации пересёк метку из инспектора (Events) |

Методы (protected):

```cpp
Entity* Owner();                 // сущность-носитель (может стать nullptr — проверяйте)
::SceneManager* Scene();         // доступ к списку сущностей и выбору

// Переменные-«серелиазуемые поля» (аналог [SerializeField]): объявить в Start(),
// ползунок появится в Inspector; значение живёт в сцене и доступно в Update()
// Движок сканирует DefineVar(...) из исходника — ползунки видны в Edit сразу, до Play.
void  DefineVar(const char* name, float defaultValue); // если уже есть — не трогает
float GetVar(const char* name, float fallback = 0) const;
void  SetVar(const char* name, float value);
// Каждая переменная появляется ползунком в Inspector ▸ Script Variables,
// значение хранится в сцене (сохраняется в .scene/.prefab).

// Поза:
void    Translate(const glm::vec2& localDelta); // локальные координаты (с учётом поворота родителя)
void    SetWorldPosition(const glm::vec2& world);
glm::vec2 WorldPosition() const;
float   AngleTo(const glm::vec2& worldPoint) const; // градусы
void    LookAt(const glm::vec2& worldPoint);        // поворот «верха» на точку
```

## 3. Глобальные функции API

```cpp
// Управление сущностями
bool DestroyEntity(uint32_t id);          // удалить сущность (инстансы скриптов снимутся)
void LoadScene(const std::string& path);  // менеджер сцен: следующий уровень
void Log(const std::string& message);     // строка в панель Console

// Время
Time::Delta(); Time::UnscaledDelta(); Time::SinceStart();
Time::TimeScale(); Time::SetTimeScale(float);   // 0 = пауза всей игры

// Физика/математика
AddForce(Entity* e, glm::vec2 impulse);   // +velocity/масса
Lerp(a,b,t); Clamp(v,lo,hi); Radians(deg); Degrees(rad);
RandomRange(0.f,1.f); RandomInt(1,6);
Physics::Gravity;                          // glm::vec2, мировые px/s²

// Ввод (у скрипта свой Input-синглтон, тот же, что у движка)
Input::Get().IsKeyPressed(GLFW_KEY_SPACE); // GLFW-коды
Input::Get().IsActionHeld("jump");         // именованные бинды: BindKey в Start()
Input::Get().GetAxis("move");              // -1..1
Input::Get().mousePosition(); mouseDelta(); // и т.п.

// Звук
Audio::PlayOneShot("assets/audio/hit.wav", 1.0f, 1.0f, 0); // -> uint32 voiceId; group: 0=SFX, 1=Music
Audio::PlayLooped(path, vol, pitch); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);
Audio::SetGroupVolume(group, 0..1); Audio::GroupVolume(group); // громкости SFX/Music
Audio::SetGroupVolume(group, 0..1); Audio::GroupVolume(group); // 0=SFX, 1=Music

// Игровой UI (кнопки/слайдеры — сущности с компонентом UI Element)
GameUI::WasClicked(entityId);              // true один кадр после клика
GameUI::GetValue(entityId);                // float: слайдер 0..1-диапазон, чекбокс 0/1

// Спрайт-анимация и клипы
PlayAnimation(Owner(), /*fromStart=*/true); StopAnimation(Owner()); IsAnimating(Owner());
bool ok = PlayClip(Owner(), "run");            // клип из таблицы Clips в инспекторе

// Частицы: мгновенная пачка из эмиттера сущности (взрыв/искры)
EmitParticles(Owner(), 30);

// Тряска game-камеры (game-feel): амплитуда в мировых единицах, длительность в секундах
ShakeCamera(15.0f, 0.3f);

// Выход из игры: в плеере закрывает окно, в редакторе — останавливает Play
QuitGame();                       // кнопка Exit: GameUI::WasClicked(exitId) -> QuitGame()
// Захват мыши (шутеры/стратегии): системный курсор скрывается, события остаются
CaptureMouse(true); if (IsMouseCaptured()) { ... }
// Спавн префаба (пули, враги): возвращает id корня инстанса (0 — ошибка)
uint32_t bullet = InstantiatePrefab("assets/prefabs/bullet.prefab", WorldPosition());
```

## 4. Доступ к сцене

`Scene()` возвращает `::SceneManager*` (глобальная область, без неймспейсов):

```cpp
for (Entity& e : Scene()->GetEntities()) { ... }
Entity* sel = Scene()->GetSelectedEntityPtr();
int idx = Scene()->IndexOf(someId);
Scene()->RemoveEntityById(someId);   // то же, что DestroyEntity
```

## 5. Пример: персонаж с сериализуемыми настройками

```cpp
class SpaceBody : public Script {
public:
    void Start() override {
        DefineVar("orbitRadius", 430.0f);
        DefineVar("speed", 60.0f);      // град/с — ползунок появится в Inspector
        angle = std::atan2(WorldPosition().y, WorldPosition().x);
    }
    void Update(float dt) override {
        angle += GetVar("speed") * dt;
        float r = GetVar("orbitRadius");
        SetWorldPosition(glm::vec2(std::cos(angle) * r, std::sin(angle) * r));
        LookAt(glm::vec2(0.0f));
    }
    void OnTriggerEnter(uint32_t otherId) override {
        Log("столкновение с id=" + std::to_string(otherId));
        DestroyEntity(otherId);        // например, съедаем звезду
    }
private:
    float angle = 0.0f;
};
SCRIPT_ENTRY(SpaceBody)
```

## 6. Как это работает под капотом

- Файл `.cpp` → wrapper `build-scripts/<stem>_gen.cpp` (авто-инклюды + `#include "<абсолютный путь>"`).
- Компиляция кэшируется по времени: `.so` новее исходника — g++ не запускается.
- `dlopen(RTLD_NOW)`: символы движка видны скрипту благодаря `-rdynamic`; точка входа —
  `extern "C" CreateGameScript` (макрос `SCRIPT_ENTRY`).
- `SyncInstances` раз в кадр: новые носители скрипта → `new + Start()`, удалённые/сменившие
  путь → `OnDestroy + delete`. Ошибки компиляции не валят игру — видны в Console.
- В собранной игре скрипты не компилируются: используются готовые `.so` из `build-scripts/`.

## 8. 3D: поза, физика, лучи

В 3D-сцене сущность живёт в `pos3 / rot3 (градусы, X→Y→Z) / scale3` — методы ниже работают
с ней. Мировые единицы те же, что в 2D: 100 единиц = 1 метр (настраивается в
Settings ▸ Physics ▸ Pixels per meter), поэтому `Gravity3D()` по умолчанию ≈ `(0, -981, 0)`.

Методы `Script` (protected, у наследника):

| Метод | Что делает |
|---|---|
| `bool Is3D() const` | трёхосевая ли сущность (иначе 3D-методы двигают пустоту) |
| `glm::vec3 Position3D() const` | мировая позиция с учётом parent-цепочки |
| `void SetPosition3D(const glm::vec3&)` | поставить в мир (пересчитывает в локальную) |
| `void Translate3D(const glm::vec3&)` | сдвиг `pos3` (локально, без родителя) |
| `glm::vec3 Rotation3D() const` / `SetRotation3D(const glm::vec3&)` | углы в градусах |
| `void SetScale3D(const glm::vec3&)` | масштаб (заодно синхронизирует 2D `scale`) |
| `glm::vec3 Velocity3D() const` / `SetVelocity3D(const glm::vec3&)` | скорость Rigidbody (3D); сеттер создаёт компонент, если его нет |
| `void AddForce3D(const glm::vec3&)` | импульс: `velocity += impulse / mass` |
| `void SetGravityEnabled3D(bool)` | включить/выключить гравитацию (тоже создаёт Rigidbody) |
| `void LookAt3D(const glm::vec3&)` | развернуть объект `-Z` на цель (как `transform.LookAt` в Unity) |
| `void AddForceTo3D(Entity*, const glm::vec3&)` / `void SetVelocityOf3D(Entity*, const glm::vec3&)` | то же для **чужой** сущности |

Глобальные функции:

```cpp
struct RayHit3D { uint32_t entityId; std::string name; glm::vec3 point, normal; float distance; };
bool Raycast3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D& out);
int  RaycastAll3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D* out, int max);
glm::vec3 Gravity3D();
uint32_t InstantiatePrefab3D(const std::string& prefabPath, const glm::vec3& pos);
inline void AddForce3D(Entity* e, const glm::vec3& impulse);   // для чужих сущностей
inline void SetVelocity3D(Entity* e, const glm::vec3& v);
inline glm::vec3 MoveTowards3D(const glm::vec3& from, const glm::vec3& to, float maxDelta);
```

`Raycast3D` идёт по 3D-сущностям: у кого есть **Collider (3D)** — берётся его габарит (с учётом
масштаба и поворота), у кого нет — коробка по межу (`0.5 * scale3`). Возвращает ближайшую цель,
нормаль по грани входа и дистанцию; `dir` нормализовывать не обязательно.

Физика: `Rigidbody (3D)` и `Collider (3D)` добавляются в инспекторе (`+ Add Component`,
только у 3D-сущностей) или скриптом (`SetGravityEnabled3D`, `AddForce3D`). События общие с 2D:
`OnTriggerEnter/Exit(otherId)`, `OnCollisionEnter(otherId)` — `otherId` можно вернуть в `Entity*`
через `Scene()` или `FindById`.

Пример — прыгающий куб (`assets/scripts/bounce3d.cpp`):

```cpp
class Bounce3D : public Script {
public:
    void Start() override {
        DefineVar("kick", 900.0f);
        SetGravityEnabled3D(true);
    }
    void Update(float dt) override {
        RayHit3D hit;
        bool grounded = Raycast3D(Position3D(), glm::vec3(0, -1, 0), 70.0f, hit);
        if (grounded && Velocity3D().y <= 1.0f)
            SetVelocity3D(glm::vec3(Velocity3D().x, GetVar("kick"), Velocity3D().z));
        glm::vec3 r = Rotation3D(); r.y += dt * 20.0f; SetRotation3D(r);
    }
};
SCRIPT_ENTRY(Bounce3D)
```

## 7. Частые грабли

- `Owner()` может стать `nullptr` (сущность удалили) — проверяйте в каждом методе.
- `DestroyEntity` внутри `Update` безопасен: инстанс переживёт кадр, умрёт на SyncInstances.
- Смена `scriptPath` или `.cpp` на лету пересоздаёт инстанс (`Start()` заново).
- Физика дочерних Rigidbody не симулируется — их двигает родитель.
- `timeScale=0` + `UnscaledDelta()` — единственный способ что-то делать на паузе.
- Внутри методов `Script` имя глобальной функции перекрывается членом с тем же именем:
  `AddForce3D(other, v)` не скомпилируется — используй `AddForceTo3D(other, v)` или `::AddForce3D(other, v)`.
- 3D-сущность в 2D-сцене не видна в Scene-вью (и наоборот): режим — свойство сцены, `View ▸ 3D Scene`.
- `Translate3D` не учитывает поворот родителя — для «локального» движения пересчитывай сам.
- `Raycast3D` без `Collider (3D)` использует габарит меша: для повёрнутой модели он шире реального.
