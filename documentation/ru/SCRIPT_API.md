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
Audio::PlayOneShot("assets/audio/hit.wav", 1.0f, 1.0f); // -> uint32 voiceId
Audio::PlayLooped(path, vol, pitch); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);

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

## 7. Частые грабли

- `Owner()` может стать `nullptr` (сущность удалили) — проверяйте в каждом методе.
- `DestroyEntity` внутри `Update` безопасен: инстанс переживёт кадр, умрёт на SyncInstances.
- Смена `scriptPath` или `.cpp` на лету пересоздаёт инстанс (`Start()` заново).
- Физика дочерних Rigidbody не симулируется — их двигает родитель.
- `timeScale=0` + `UnscaledDelta()` — единственный способ что-то делать на паузе.
