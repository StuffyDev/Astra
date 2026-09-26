# Astra — Engine Manual

Astra is a 2D game engine with a Unity-style editor: C++17, OpenGL 4.6, GLFW, ImGui (dockspace).
The editor, scenes, scripts and shaders all live in a single binary, with no external runtimes.

---

## 1. Building

Linux only. Requirements: `cmake` (>= 3.16), `g++` (C++17), `libgl-dev`/Mesa, and the X11
dependencies of GLFW.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/Astra
```

All dependencies (glfw, glm, glad, imgui, stb, miniaudio) are fetched automatically via
FetchContent on the first configure. A system `g++` is mandatory: it compiles the
game scripts when entering Play.

## 2. Projects

- **File ▸ New Project...** — name + folder; from the `templates/default_project` preset,
  an `assets/{scenes,shaders,sprites,scripts,audio,prefabs}` structure plus `project.json` is created.
- **File ▸ Open Project... / Projects Manager / Open Recent** — switching between projects.
  Opening a project changes the engine's working directory; all asset paths are relative to the project root.
- **File ▸ Exit** — quit.

## 3. The Editor

### Panels (View ▸ ... toggles them on/off)
| Panel | Purpose |
|---|---|
| **Scene** | scene editor: gizmo, navigation, drag & drop of assets straight into the world |
| **Game** | view from the game camera; in Play mode the runtime UI lives here |
| **Hierarchy** | entity tree; dragging reorders entries and changes parents |
| **Inspector** | components of the selected entity |
| **Project** | assets (Assets tab) and the **Console** (script/shader errors, std::cout/cerr output) |

### Scene navigation
- Middle mouse button (or Alt+left click) — panning; arrow keys — keyboard panning.
- Mouse wheel — zoom. **F** (hold) — focus on the selected entity.

### Gizmo and hotkeys
| Key | Action |
|---|---|
| **W / E / R / Q** | Move / Rotate / Scale / Select |
| **Ctrl+N / Ctrl+O** | new scene / open scene |
| **Ctrl+S / Ctrl+Shift+S** | save / save as |
| **Ctrl+D** | duplicate subtree |
| **Ctrl+↑ / Ctrl+↓** | reorder an entity among its siblings |
| **Delete / Backspace** | delete subtree |
| **F2** | rename (an entity, or an asset when the Project panel has focus) |
| **Ctrl+P** | Play/Edit |
| **Esc** | Stop (return to Edit from Play/Pause) |

The window title shows the scene name and a `*` when there are unsaved changes;
closing or switching a scene with pending edits brings up a confirmation dialog.

### Play / Pause / Stop
Toolbar: ▶ Play, ⏸ Pause, ⏹ Stop. On entering Play the engine takes a snapshot of the scene;
stopping restores everything (positions, velocities, created/deleted objects) — you can
play and edit without any risk of breaking the scene. In Edit mode, objects and the Game-view UI
remain clickable and editable exactly as in the inspector, and Scene navigation keeps working.

## 4. Entities and Components

An entity (`src/ecs/Entity.h`) = a Transform plus a set of components:

- **Transform** — position/rotation/scale; for a child it is defined **relative to its parent**
  (as in Unity: a child inherits the motion, rotation and scale of the chain).
- **Sprite** — Quad/Circle, color, texture (`texturePath`), custom shader (`shaderPath`).
- **Rigidbody** — mass, drag, gravity, kinematic flag.
- **Collider** — Box/Circle, trigger or solid; takes rotation/scale and the parent chain into account.
- **Camera** — game camera; `mainCamera` designates the one used for the Game view; a child camera
  follows its parent.
- **UI** — a screen-space element (Button/Text/Slider) in Game-view coordinates.
- **Audio Source** — clip, volume, pitch, loop, Play On Awake; preview in the inspector.
- **Script** — path to a `.cpp` file (see section 5).

### Drag & drop
- An image from Project — onto an entity in the Hierarchy or onto a point in the Scene: the texture
  is applied (or a Quad with that texture is created).
- A `.frag`/shader — assigned as the target's `shaderPath`.
- An audio clip — into the audio component of the entity under the cursor.
- A `.prefab` — instantiated at the drop point (into the hierarchy or straight into the Scene).
- A file from disk (outside the window) — imported into the appropriate `assets/` folder based on its extension.

### Hierarchy
Dragging a node: the top third of the row — insert before, the bottom — insert after,
the middle — become a child. Sibling order = draw order (later ones render on top).

## 5. Scripts (C++)

A script is a plain C++ file in `assets/scripts/`. **You do not need to write includes** — on
entering Play the engine generates a wrapper translation unit that itself pulls in `ScriptAPI.h`, `Input`, `Audio`,
`GameUI`, `Physics`, GLFW, glm, `<cmath>` and the rest, compiles a `.so` via
`g++ -shared -fPIC` and loads it with `dlopen`. Compilation errors appear in the **Console** tab
and in the red counter on the toolbar.

A minimal script:

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
    virtual void Update(float dt);      // dt is already multiplied by TimeScale
    virtual void OnDestroy();
    // physics interactions (id of the other entity):
    virtual void OnTriggerEnter(uint32_t otherId);
    virtual void OnTriggerExit(uint32_t otherId);
    virtual void OnCollisionEnter(uint32_t otherId);
protected:
    Entity* Owner();                    // the owning entity
    SceneManager* Scene();              // access to entities/the tree
    void Translate(const glm::vec2& d); // local translation (parent-aware)
    void SetWorldPosition(const glm::vec2& p);
    glm::vec2 WorldPosition() const;
    float AngleTo(const glm::vec2& worldPoint) const; // degrees
    void LookAt(const glm::vec2& worldPoint);         // point "up" at the target
};

class Time {  // static members
    static float Delta();        // with TimeScale applied
    static float UnscaledDelta();
    static float SinceStart();   // seconds since Play
    static float TimeScale();
    static void SetTimeScale(float);
};

#define SCRIPT_ENTRY(Class)      // one factory per file
```

Also available (the engine includes the headers for you):
- `Input::Get()` — keys/mouse/actions/axes: `IsActionHeld`, `GetAxis`, bindings set up in `Start()`.
- `Audio::PlayOneShot/PlayLooped/Stop/SetVolume/SetPitch` — id-based clip control.
- `GameUI::WasClicked(entityId)`, `GameUI::GetValue(entityId)` — responses from buttons/sliders.
- `Physics::Gravity` — world gravity.

## 6. Shaders

Assets in `assets/shaders/*.frag` (plus optional `*.vert`). In Inspector ▸ Custom Shader
one is selected by name; in Project — right-click ▸ Assign Shader to Selected.

**A single `.frag` is enough** — the engine supplies the geometry (a quad, `v_UV` ranging from 0 to 1).

The engine already injects the following into shaders:
```glsl
// vertex: a_Pos, u_MVP/u_Model/u_ViewProj, u_Time, u_ScreenSize,
//         v_UV, EngineUV(), EngineQuadVert()
// fragment: fragColor, v_UV, u_Color, u_Texture, u_Time, u_ScreenSize
float EngineCircleMask(vec2 uv);
float EngineRoundedBox(vec2 uv, float radius);
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float deg);
float EngineNoise(vec2 p);
float EngineFbm(vec2 p, int octaves);
vec2  EngineSwirl(vec2 uv, vec2 center, float strength, float radius);
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // Inigo Quilez palette
vec3  EngineRainbow(float t);
float EnginePulse(float freq); // 0..1, sine wave driven by u_Time
```

Examples in `assets/shaders/`: `effect_rounded`, `effect_glow`, `effect_rainbow`,
`effect_plasma`, `example_wobble` (a vert+frag pair), `black_hole` (from the demos).
The **Reload** button in the inspector rebuilds the shader and texture caches.

## 7. Audio

The Audio Source component: path, volume (0..2), pitch (0.1..3), loop, Play On Awake,
Preview/Stop. Resources live in `assets/audio/*.{wav,mp3,ogg,flac}` (miniaudio).
Mixer: Edit ▸ Settings — master volume and mute. Device errors are reported there too.

## 8. Physics

Fixed timestep at 60 Hz. Default gravity (0, -9.81) m/s², 1 m = 100 px —
configurable in Edit ▸ Settings. Colliders: Box/Circle, triggers (events) and solid
(contact resolution). The parent's motion/rotation/scale affects its child colliders.
Events are delivered to scripts as `OnTriggerEnter/Exit` and `OnCollisionEnter`.

## 9. Prefabs

Right-click an entity (or use the icon in the inspector) ▸ Save as Prefab → `assets/prefabs/`.
Dropping a prefab into the hierarchy/scene creates an instance; Revert To Prefab rolls back
the instance's changes. Duplication (`Ctrl+D`) and serialization operate on subtrees.

## 10. Engine Settings

**Edit ▸ Settings**: gravity (m/s²), time scale, master volume, mute.
The time scale is applied when entering Play.

## 11. Examples

- `examples/black_hole/` — a black hole: accretion-disk shader (a `.frag` only),
  an orbit script using `LookAt`, a scene with a child moon. Already duplicated into
  `assets/scenes/black_hole.scene` — File ▸ Open Scene and Play.
- `assets/scripts/rotate.cpp`, `player.cpp` — endless rotation and a WASD-controlled character.

## 12. Built-in code editor (IDE-lite)

- Double-click a `.cpp`/`.h`/`.frag`/`.vert`/`.txt`/`.json` file in the Project panel —
  it opens in the built-in editor window; also **right click ▸ Edit (built-in IDE)**.
- **View ▸ Script Editor** — show/hide the editor window.
- **Ctrl+S** inside the window saves the file (while it has focus, scene hotkeys are bypassed).
- **Ctrl+C / Ctrl+V / Ctrl+X / Ctrl+Z** work natively (ImGui InputTextMultiline + GLFW clipboard).
- **Open Externally** — open the file in the system default app (`xdg-open`).

## 13. Import from other sources

- **Project ▸ Create ▸ Import File...** — a disk file browser; the chosen file is copied
  into the matching `assets/` subfolder by extension (textures/audio/scenes/scripts/shaders/prefabs),
  anything else goes to `assets/imported`. Drag&drop still works.

## 14. Console

- **Console** tab next to Assets; the error counter shows in the tab name.
- **Follow** — auto-scroll to the bottom only if you are already there (can be turned off).
- **Copy All** puts the whole log on the clipboard; a single click on a line copies that line.

## 15. Runtime UI (Game GUI)

- Widgets: Button/Text/Slider/Checkbox/Progress Bar.
- `transform.position/scale` are in **world units** (the Scene gizmo matches the button location).
- Style per element: `UITextColor`, `UIBgColor`, `UIFontScale` (0/1/2 = normal/medium/large).
- In Edit mode elements are visible both in Game-view and as preview rectangles right in Scene.

## 16. Build Game and Player

- **File ▸ Build Game...** produces a standalone folder:
  `astra` (a copy of the current binary), `assets/` (scenes/shaders/textures/audio/scripts),
  `build-scripts/*.so` — **precompiled scripts** (no g++ needed on the target machine),
  and `game.json` with the start scene `{"scene": "assets/scenes/..."}`.
- Run: `cd <folder> && ./astra --play` (or `Astra --play --scene ... --project ...`).
- In the player, ESC closes the window; there is no editor, only the game.

## 17. Limitations of the current version

- One project / one scene in memory; no additive scenes.
- Rigidbodies on children are not simulated (they are moved by their parent).
- No animations or tilemap rendering.
