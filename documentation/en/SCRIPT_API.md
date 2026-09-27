# Astra — Game Script API (C++)

Scripts are plain C++ classes. The engine compiles them with `g++ -std=c++17 -shared -fPIC` into
`build-scripts/<name>.so` when you enter Play (and when building the game) and loads them via `dlopen`.
Hot reload: a changed `.cpp` is recompiled and the instances are recreated.
**You do not need to write includes** — the engine generates a wrapper TU that pulls in
`ScriptAPI.h`, `SceneManager.h`, `Transforms.h`, `Physics.h`, `Input.h`, `GameUI.h`,
`Audio.h`, GLFW, glm and `<cmath>/<string>/<vector>/<algorithm>/<random>` on its own.

## 1. Minimal script

```cpp
class Rotate : public Script {
public:
    void Update(float dt) override {
        Entity* e = Owner();
        if (e) e->transform.rotation += 90.0f * dt;
    }
};
SCRIPT_ENTRY(Rotate)   // exactly one factory per file
```

Attach it via Inspector ▸ Script (a combo over `assets/scripts/*.cpp`). Compile errors go
to the Console and to the red button on the toolbar.

## 2. The Script class

| Hook | When it is called |
|---|---|
| `Start()` | when the instance is created (entering Play, spawning, LoadScene) |
| `Update(float dt)` | every frame (dt is already multiplied by timeScale) |
| `OnDestroy()` | before the instance is destroyed (Stop/StopAnimation scenarios) |
| `OnTriggerEnter(uint32_t otherId)` / `OnTriggerExit` | entering/leaving a trigger |
| `OnCollisionEnter(uint32_t otherId)` | a non-trigger collision |

Methods (protected):

```cpp
Entity* Owner();                 // the host entity (may become nullptr — check it)
::SceneManager* Scene();         // access to the entity list and to the selection

// Variables as "serializable fields" (the equivalent of [SerializeField]):
void  DefineVar(const char* name, float defaultValue); // in Start(); leaves an existing one untouched
// The engine also scans DefineVar(...) from the source — sliders appear in the Inspector
// in Edit mode immediately, before any Play.
float GetVar(const char* name, float fallback = 0) const;
void  SetVar(const char* name, float value);
// Each variable shows up as a slider in Inspector ▸ Script Variables,
// and its value is stored in the scene (saved into .scene/.prefab).

// Pose:
void    Translate(const glm::vec2& localDelta); // local coordinates (takes the parent's rotation into account)
void    SetWorldPosition(const glm::vec2& world);
glm::vec2 WorldPosition() const;
float   AngleTo(const glm::vec2& worldPoint) const; // degrees
void    LookAt(const glm::vec2& worldPoint);        // turn the "up" direction toward the point
```

## 3. Global API functions

```cpp
// Entity management
bool DestroyEntity(uint32_t id);          // remove an entity (its script instances are released)
void LoadScene(const std::string& path);  // scene manager: the next level
void Log(const std::string& message);     // a line in the Console panel

// Time
Time::Delta(); Time::UnscaledDelta(); Time::SinceStart();
Time::TimeScale(); Time::SetTimeScale(float);   // 0 = pause the whole game

// Physics/math
AddForce(Entity* e, glm::vec2 impulse);   // += velocity/mass
Lerp(a,b,t); Clamp(v,lo,hi); Radians(deg); Degrees(rad);
RandomRange(0.f,1.f); RandomInt(1,6);
Physics::Gravity;                          // glm::vec2, world px/s²

// Input (a script gets its own Input singleton, the same one the engine uses)
Input::Get().IsKeyPressed(GLFW_KEY_SPACE); // GLFW key codes
Input::Get().IsActionHeld("jump");         // named binds: BindKey in Start()
Input::Get().GetAxis("move");              // -1..1
Input::Get().mousePosition(); mouseDelta(); // and so on

// Audio
Audio::PlayOneShot("assets/audio/hit.wav", 1.0f, 1.0f); // -> uint32 voiceId
Audio::PlayLooped(path, vol, pitch); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);

// Game UI (buttons/sliders are entities with a UI Element component)
GameUI::WasClicked(entityId);              // true for one frame after a click
GameUI::GetValue(entityId);                // float: slider in its 0..1 range, checkbox 0/1

// Sprite animation and clips
PlayAnimation(Owner(), /*fromStart=*/true); StopAnimation(Owner()); IsAnimating(Owner());
bool ok = PlayClip(Owner(), "run");            // a clip from the Clips table in the inspector

// Particles: an instant batch from the entity's emitter (explosion/sparks)
EmitParticles(Owner(), 30);
```

## 4. Accessing the scene

`Scene()` returns a `::SceneManager*` (global scope, no namespaces):

```cpp
for (Entity& e : Scene()->GetEntities()) { ... }
Entity* sel = Scene()->GetSelectedEntityPtr();
int idx = Scene()->IndexOf(someId);
Scene()->RemoveEntityById(someId);   // the same thing as DestroyEntity
```

## 5. Example: a character with serializable settings

```cpp
class SpaceBody : public Script {
public:
    void Start() override {
        DefineVar("orbitRadius", 430.0f);
        DefineVar("speed", 60.0f);      // deg/s — a slider shows up in the Inspector
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
        DestroyEntity(otherId);        // for example, we devour the star
    }
private:
    float angle = 0.0f;
};
SCRIPT_ENTRY(SpaceBody)
```

## 6. How it works under the hood

- The `.cpp` file → a wrapper `build-scripts/<stem>_gen.cpp` (auto-includes + `#include "<absolute path>"`).
- Compilation is cached by timestamp: if the `.so` is newer than the source, g++ is not run.
- `dlopen(RTLD_NOW)`: the engine's symbols are visible to the script thanks to `-rdynamic`; the entry point is
  `extern "C" CreateGameScript` (the `SCRIPT_ENTRY` macro).
- `SyncInstances` runs once per frame: new script hosts → `new + Start()`, removed/changed
  path → `OnDestroy + delete`. Compile errors don't take the game down — they show in the Console.
- In a built game scripts are not compiled: ready-made `.so` files from `build-scripts/` are used.

## 7. Common pitfalls

- `Owner()` may become `nullptr` (the entity was deleted) — check it in every method.
- `DestroyEntity` inside `Update` is safe: the instance survives the frame and dies at SyncInstances.
- Changing `scriptPath` or the `.cpp` on the fly recreates the instance (`Start()` runs again).
- Rigidbody physics is not simulated on children — the parent moves them.
- `timeScale=0` + `UnscaledDelta()` is the only way to do anything while paused.
