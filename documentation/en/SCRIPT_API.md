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
| `OnAnimEvent(const char* name)` | animation frame crossed a marker from the inspector (Events) |

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
Audio::PlayOneShot(path, vol, pitch, group); // 4th arg group: 0=SFX, 1=Music
Audio::PlayLooped(path, vol, pitch, group); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);
Audio::SetGroupVolume(group, 0..1); Audio::GroupVolume(group); // 0=SFX, 1=Music

// Game UI (buttons/sliders are entities with a UI Element component)
GameUI::WasClicked(entityId);              // true for one frame after a click
GameUI::GetValue(entityId);                // float: slider in its 0..1 range, checkbox 0/1

// Sprite animation and clips
PlayAnimation(Owner(), /*fromStart=*/true); StopAnimation(Owner()); IsAnimating(Owner());
bool ok = PlayClip(Owner(), "run");            // a clip from the Clips table in the inspector

// Particles: an instant batch from the entity's emitter (explosion/sparks)
EmitParticles(Owner(), 30);

// Camera shake (game feel): amplitude in world units, duration in seconds
ShakeCamera(15.0f, 0.3f);

// Leaving the game: closes the window in the player, stops Play in the editor
QuitGame();                       // an Exit button: GameUI::WasClicked(exitId) -> QuitGame()
// Mouse capture (shooters/strategy): the system cursor hides, the mouse events stay
CaptureMouse(true); if (IsMouseCaptured()) { ... }
// Spawning a prefab (bullets, enemies): returns the id of the instance's root (0 — error)
uint32_t bullet = InstantiatePrefab("assets/prefabs/bullet.prefab", WorldPosition());
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

## 8. 3D: transform, physics, rays

In a 3D scene an entity lives in `pos3 / rot3 (degrees, X→Y→Z) / scale3`, and the methods below work
against that. The world units are the same as in 2D: 100 units = 1 meter (set in Settings ▸ Physics ▸
Pixels per meter), which is why `Gravity3D()` defaults to ≈ `(0, -981, 0)`.

`Script` methods (protected, on the subclass):

| Method | What it does |
|---|---|
| `bool Is3D() const` | whether the entity is three-axis (otherwise the 3D methods move nothing) |
| `glm::vec3 Position3D() const` | the world position, taking the parent chain into account |
| `void SetPosition3D(const glm::vec3&)` | set a world position (converted back to local) |
| `void Translate3D(const glm::vec3&)` | shifts `pos3` (local, ignoring the parent) |
| `glm::vec3 Rotation3D() const` / `SetRotation3D(const glm::vec3&)` | angles in degrees |
| `void SetScale3D(const glm::vec3&)` | the scale (also syncs the 2D `scale`) |
| `glm::vec3 Velocity3D() const` / `SetVelocity3D(const glm::vec3&)` | the Rigidbody (3D) velocity; the setter creates the component when it is missing |
| `void AddForce3D(const glm::vec3&)` | an impulse: `velocity += impulse / mass` |
| `void SetGravityEnabled3D(bool)` | turn gravity on/off (this also creates the Rigidbody) |
| `void LookAt3D(const glm::vec3&)` | turn the object's `-Z` toward the target (like `transform.LookAt` in Unity) |
| `void AddForceTo3D(Entity*, const glm::vec3&)` / `void SetVelocityOf3D(Entity*, const glm::vec3&)` | the same for **another** entity |

Global functions:

```cpp
struct RayHit3D { uint32_t entityId; std::string name; glm::vec3 point, normal; float distance; };
bool Raycast3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D& out);
int  RaycastAll3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D* out, int max);
glm::vec3 Gravity3D();
uint32_t InstantiatePrefab3D(const std::string& prefabPath, const glm::vec3& pos);
inline void AddForce3D(Entity* e, const glm::vec3& impulse);   // for someone else's entity
inline void SetVelocity3D(Entity* e, const glm::vec3& v);
inline glm::vec3 MoveTowards3D(const glm::vec3& from, const glm::vec3& to, float maxDelta);
```

`Raycast3D` walks the scene's 3D entities: one with a **Collider (3D)** contributes that collider's
bounds (scale and rotation included), one without it contributes a box from the mesh (`0.5 * scale3`).
It returns the nearest target, the normal of the face it entered and the distance; `dir` does not have to
be normalized.

Physics: `Rigidbody (3D)` and `Collider (3D)` are added in the Inspector (`+ Add Component`,
3D entities only) or from a script (`SetGravityEnabled3D`, `AddForce3D`). The events are shared with 2D:
`OnTriggerEnter/Exit(otherId)`, `OnCollisionEnter(otherId)` — turn `otherId` back into an `Entity*`
through `Scene()` or `FindById`.

Example — a bouncing cube (`assets/scripts/bounce3d.cpp`):

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

## 7. Common pitfalls

- `Owner()` may become `nullptr` (the entity was deleted) — check it in every method.
- `DestroyEntity` inside `Update` is safe: the instance survives the frame and dies at SyncInstances.
- Changing `scriptPath` or the `.cpp` on the fly recreates the instance (`Start()` runs again).
- Rigidbody physics is not simulated on children — the parent moves them.
- `timeScale=0` + `UnscaledDelta()` is the only way to do anything while paused.
- Inside a `Script` method a member hides the global function of the same name:
  `AddForce3D(other, v)` won't compile — use `AddForceTo3D(other, v)` or `::AddForce3D(other, v)`.
- A 3D entity is not drawn in the Scene view of a 2D scene (and vice versa): the mode is a property of
  the scene, `View ▸ 3D Scene`.
- `Translate3D` ignores the parent's rotation — recompute the basis yourself for "local" movement.
- `Raycast3D` with no `Collider (3D)` falls back to the mesh bounds: on a rotated model they are larger
  than the actual shape.
