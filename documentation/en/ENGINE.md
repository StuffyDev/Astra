# Astra — Engine User Guide

Astra is a 2D/3D game engine with a Unity-style editor: C++17, OpenGL 4.6, GLFW, ImGui (dockspace).
The editor, scenes, scripts, shaders, audio and game building all live in one binary, with no external runtimes.

Contents: [build](#1-build-and-run) · [projects](#2-projects) · [interface](#3-editor-interface) ·
[entities](#4-entities-and-components) · [scenes](#5-scenes) · [assets](#6-assets-and-import) ·
[UI](#7-game-ui) · [undo](#8-undo--redo) · [hotkeys](#9-hotkeys) ·
[settings](#10-engine-settings) · [build](#11-building-the-game-and-the-player) · [console](#12-console) ·
[3D](#14-3d-mode)

---

## 1. Build and Run

Linux only. You need: `cmake` (>= 3.16), `g++` (C++17), Mesa/`libgl-dev`, and GLFW's X11 dependencies.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Astra                 # editor
./build/Astra --play          # player (the game without the editor)
```

All dependencies (glfw, glm, glad, imgui, stb, miniaudio) are pulled in by FetchContent on the first
configure. A system `g++` is mandatory: it compiles the game scripts (see SCRIPT_API.md).

## 2. Projects

- **File ▸ New Project...** — name + folder; from `templates/default_project` the
  `assets/{scenes,shaders,scripts,audio,textures,prefabs}` structure plus `project.json` is created.
- **File ▸ Open Project... / Projects Manager / Open Recent** — switching between projects.
  Opening a project changes the engine's working directory: all asset paths are relative to the project root.
- **File ▸ Exit** — quit.

## 3. Editor Interface

A single dockspace: windows can be detached by their title bars, docked into tabs, and resized by their borders.

| Window | What it does |
|---|---|
| **Scene** | editing: gizmo, selection, drag&drop, collider and UI previews |
| **Game** | the game camera's view; UI buttons/sliders live here (and their previews in Edit) |
| **Script** | built-in IDE: double-clicking a `.cpp/.frag/...` in Project opens the file here |
| **Hierarchy** | the entity tree; drag = reparenting; Ctrl+↑/↓ — reorder siblings |
| **Inspector** | the selected entity's components + Script Variables + Material |
| **Project** | a browser for `assets/` (grid/list, search, Create, Import) and the **Console** tab |

Toolbar: **Play / Pause / Stop / Restart**, the `PLAYING/PAUSED` status, the script error
counter (red button — clicking it opens the Console), and the **Move (W) / Rotate (E) / Scale (R) / Hand (Q)** tools.

Navigation in Scene: RMB/MMB — pan, wheel — zoom toward the cursor, F — focus on the selection,
arrow keys — pan while the Scene has focus. ESC — leave Play and return to Edit.

**Context menus (right-click)**: in Scene — a click without drag opens "Create Empty/Quad here,
Paste here, Deselect, Focus selection"; in Hierarchy — on a row (Rename, Duplicate, **Move Up/Down**
for sibling reordering, Detach, Save as Prefab, Revert, Delete) and on empty space (Create Empty,
Paste, Deselect); in Project — on empty space (Create Folder/Shader/Script, Import File...).

**Script Variables**: the engine scans `DefineVar("name", default)` straight from the script
source — the sliders show up in the Inspector immediately, in Edit mode too, before any Play
(like [SerializeField]).

## 4. Entities and Components

An entity = a set of fixed components (for now; a pure ECS is on the roadmap):

- **Transform** — Position/Rotation/Scale. There is a `Parent`: children inherit the parent's pose
  (world matrices just like in Unity, including the chain's scale/rotation).
  The **3D Object** checkbox switches an entity to three-axis mode: Position 3 / Rotation 3 (degrees) /
  Scale 3 + **Mesh** instead of a sprite (see section 14).
- **Sprite** — Type (None/Quad/Circle), Color, Texture Path, **Sorting Order** (a smaller value draws
  first/under the rest, just like in Unity), Custom Shader (see SHADER_API.md).
- **Animation** — a sprite sheet: `Cols × Rows` (row 0 = top row), FPS, Loop, Play On Awake,
  `Active` (in Edit the frames keep playing as a preview). Either a separate Sheet Path or "Use Sprite".
  The **Clips** table — name, first..last, fps, loop, Play/X buttons; with no clips the whole grid loops.
  From scripts: `PlayAnimation/StopAnimation/IsAnimating`, `PlayClip(e, "run")`.
- **Rigidbody** — Kinematic, Velocity, Mass, Drag, Use Gravity. Physics: fixed 60 Hz,
  MTV resolution, triggers/collisions with script events.
- **Collider** — Box (half sizes) / Circle (radius), Is Trigger. Visualized in the Scene.
- **Camera** — Main Camera (one active), Zoom, Viewport Offset. The Game view looks
  through it; UI elements are positioned in world coordinates of this view.
  **Follow Target** — the camera smoothly follows the selected entity (Damping = seconds,
  Offset = aim shift). **Level Bounds** — a rectangle of the level: the camera center is kept inside
  (zoom-aware). Screen shake from scripts: `ShakeCamera(15.0f, 0.3f)`.
  **Perspective (3D)** + **Field of View** — perspective projection instead of the orthographic one
  (section 14).
- **UI Element** — see section 7.
- **Animation events**: Animation > Events — markers (clip: any/specific, frame, name). When the frame
  crosses a marker, the carrier script gets `OnAnimEvent("step")` — footsteps/shots/hits exactly on frames.
- **Audio Source** — Clip Path, Volume, Pitch, Loop, Play On Awake, **Group** (SFX/Music),
  Preview/Stop buttons. Group volumes live in Edit ▸ Settings ▸ Audio.
- **Tilemap** — a grid of tiles from an atlas: Atlas Path, Tile Size, Atlas Cols, Grid W×H, Tint,
  Sorting Order (under the sprites by default). The Pick Tile popup shows the atlas as a grid; the current
  tile is applied with the **Tile (T)** brush: LMB paints the selected tile on the selected entity's grid,
  Shift+LMB erases. The entity's `transform.position` = the grid's top-left corner.
  Fill floor/Clear buttons for a quick start.
  **Solid (physics)** — non-empty cells become static AABB colliders (a platformer floor/walls;
  a body lands on them and the velocity along the impact axis is zeroed).
- **Particle Emitter** — a texture (or a plain square), max/rate, life/speed/angle min-max, gravity,
  size start/end, color start/end (the alpha fades out), Loop, Play On Awake, the Burst button.
  From scripts: `EmitParticles(Owner(), 30)`.
- **Script** — a C++ script (SCRIPT_API.md). **Script Variables** — `DefineVar("speed", 120)`
  in `Start()`, a slider appears in the inspector, and the value is serialized into the scene.

GameObject ▸ Create Empty/Quad/Circle/Camera/UI — quick presets.

**Components are added Unity-style**: a new entity has only Transform+Sprite;
Rigidbody/Collider/Audio/Script/Particle Emitter/Tilemap/UI/Camera/Animation are added with the
**+ Add Component** button at the bottom of the Inspector and removed with the **x** button in a section header.
The presence flags are serialized into the scene file. Old scene files (before v0.10) load as before — there
the components are all "enabled" (legacy). Also, the Inspector and Hierarchy docks are now more compact (20%/17%).

## 5. Scenes

- Text format: `Astra Scene v2` (one entity = one `ENTITY ... END_ENTITY` block).
- Ctrl+N / Ctrl+S / Ctrl+Shift+S — new/save/save as; an asterisk in the title bar means
  unsaved changes; closing a dirty scene asks for confirmation.
- Double-click a `.scene` in Project to open it (with a confirmation prompt if the current one is dirty).
- **Several scenes in one game**: `LoadScene("assets/scenes/level2.scene")` from a script —
  the scene manager switches the level on the fly (works both in Play and in the built game).

## 6. Assets and Import

- Drag&drop from the OS file manager into the window — imports by type (images → `assets/textures`,
  `.scene` → `assets/scenes`, `.cpp` → `assets/scripts`, audio → `assets/audio`).
- **Project ▸ Create ▸ Import File...** — pick a file from disk with the same browser.
- **RMB ▸ Open Externally** — open the file in the system editor (`xdg-open`).
- **RMB ▸ Edit (built-in IDE)** — open it in the Script window. Ctrl+S — save.
- Images/shaders/prefabs can be dragged straight onto an object in the Scene or onto a Hierarchy row —
  they get assigned/instanced.
- Create ▸ Shader (a `.vert+.frag` pair) or a single `.frag` — a template with API hints.

## 7. Game UI

Entity + UI Element component: **Button / Text / Slider / Checkbox / Progress Bar**.
Position and size are in **world coordinates** (the gizmo in the Scene matches where the button sits).
Style: Text Color, Bg Color, Font (Default/Medium/Large). A checkbox is a real bool (0/1).
In Edit, elements are visible both in the Game view and as outlines in the Scene. In Play they are clickable;
the script reads `GameUI::WasClicked(id)` / `GameUI::GetValue(id)`.

## 8. Undo / Redo

- **Ctrl+Z** — one step back, **Ctrl+Shift+Z** (or Ctrl+Y) — one step forward.
- Rolls back scene edits: gizmo moves, inspector changes, creation/deletion,
  copy/paste, reparenting, prefab instantiation. One "batch" of continuous edits = one step.
- Depth — 60 steps. Saving the scene resets the reference point.

## 9. Hotkeys

| Keys | Action |
|---|---|
| Ctrl+N / O / S / Shift+S | scene new / open / save / save as |
| Ctrl+P | Play/Stop | Ctrl+D | duplicate subtree |
| Ctrl+C / Ctrl+V | copy / paste an entity subtree |
| Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y | undo / redo |
| Del / Backspace | delete entity | F2 — rename |
| W / E / R / Q / T | Move / Rotate / Scale / Hand / Tile (tilemap brush) |
| Ctrl+↑ / ↓ | move a sibling | F — focus the camera on the selection |
| Ctrl (hold while dragging) | snap the gizmo (50 px / 15°) |
| ESC | leave Play; in the player — close the game |
| Ctrl+S in the Script window | save the file (^C/^V/^X/^Z work in the IDE) |

## 10. Engine Settings

**Edit ▸ Settings**:
- **Physics** — gravity (m/s², Unity-style 0,-9.81), Pixels per meter (the world scale).
- **Render** — the scene/game background color, showing the grid and the colliders in the Scene,
  grid/snap step (px).
- **Editor** — the rotation snap step (°), UI theme **Light theme** (light/dark UI switched at runtime).
- **Time** — Time Scale (applied when entering Play, survives Stop).
- **Audio** — Master Volume, Mute, device status.
- **Lighting (3D)** — the sun direction (Light Dir, three numbers — a vector the engine normalizes),
  the sun color (Light Color) and the fill light (Ambient). They affect every mesh object (section 14).

The file browsers (Browse/Import) are ordinary windows: you can keep working with the rest of the
interface, ESC closes them. All the settings are saved to `~/.astra/config.ini` and restored on launch
(theme, background, grid, snap, px/m, volume, time scale, the 3D scene's light). The interface is soft and
rounded, in two themes: the dark "Astra Slate" and the light "Astra Paper". The file icons (folder/image/
script/shader/scene/prefab/audio) are drawn as vectors in the engine code — no third-party assets at all.

## 11. Building the Game and the Player

**File ▸ Build Settings...** (**Ctrl+Shift+B**) — a dockable window in the Unity style:

- **Product name** — the name of the game's exe;
- **Scenes Included** — every scene of the project: the checkbox marks the scenes that go into the
  build, the radio button picks the startup scene (the one the built game opens with);
- the output **Folder** (+ Browse...) — where the build lands;
- **Encrypt used assets** — the `AENC` encryption of everything the build takes along;
- **Copy engine library** — put `libastra_engine.so` next to the exe.

**Build** always produces ONE kind of release: a tiny launcher (~18 KB) linked against
`libastra_engine.so`, holding only the assets the scenes really use (encrypted) and the scripts
precompiled to `.so` — no g++ needed on the target machine. With "Copy engine library" the library sits
next to the exe and the rpath `$ORIGIN` keeps the folder portable (without it the library is taken from
the build folder). Run: just `./game_name` (cwd = the exe's folder).

The single-exe and folder builds are CLI-only now:
`./Astra --build assets/scenes/x.scene --out ./game [--single|--folder]` — `--single` appends a bundle
(engine + assets + .so + game.json) to the binary, which unpacks into `<name>.bundle/` next to itself on
the first run (one file = the game); `--folder` makes a full copy: `astra` + `assets/` + `build-scripts/`
+ `game.json`, run `./astra --play`. Player flags: `--play`, `--scene <path>`, `--project <dir>`.
In the game, the whole script API is available, including `LoadScene` (levels) and `Log` (writes to the
console).

**Only what's needed + encryption**: the build contains NOT all of `assets/`, only the files the scene
actually references (textures/shaders/audio/tilemap & particle atlases + prefabs, recursively). With
**Encrypt used assets** on, all of them are encrypted (`AENC`: an XOR keystream from splitmix64) and
decrypted transparently by the engine on read — the game folder holds no raw textures/scenes/audio.
The scripts' `.so` files stay raw (dlopen loads them). This is obfuscation "from curious eyes", not real
crypto protection against cracking.

## 12. Console

The **Console** tab in the Project panel: the engine's stdout/stderr, the scripts' output, and compile errors.
**Follow** — auto-scroll only if you were already at the bottom; **Copy All** — the whole log to the clipboard;
clicking a line — copies that line. The error counter lives on the toolbar (red button).

## 13. Examples

- `assets/scenes/black_hole.scene` — a shader world (accretion disk, orbit driven by a script, a child moon).
- `assets/scenes/animation_demo.scene` — a 4×2 sprite-sheet ball + live Material parameters.
- `assets/scripts/rotate.cpp`, `player.cpp`, `examples/black_hole/orbit_planet.cpp`.
- `examples/` — sources of the examples; `templates/default_project` — the project template.

## 14. 3D Mode

Stage one: a perspective camera, mesh primitives, `.obj` import and simple lighting.
The 2D tools (sprites, UI, tilemap, colliders, scripts) keep working next to it.

**Enable**: `View ▸ 3D Mode`. The Scene viewport turns three-dimensional:
- **RMB + mouse movement** — orbit around the focus (pitch clamped to ±89°);
- **mouse wheel** — the distance to the focus (50…40000 world units);
- **MMB** (or LMB with the **Hand** tool, key Q) — pan: the focus slides in the screen plane;
- **LMB** — select a mesh object by clicking: a ray is cast from the camera against each mesh's AABB,
  the one nearest the camera wins; clicking empty space clears the selection;
- **F** — move the orbit focus onto the selected entity.

**3D gizmo**: the selected 3D entity shows axes (Move), rings (Rotate) or axes with handles (Scale),
switched with the **W / E / R** tools or the toolbar buttons.
- Move: drag an X/Y/Z axis to translate along that axis only; drag the centre diamond to move freely
  in the screen plane;
- Rotate: drag a ring to rotate about its axis, the angle is measured in the ring's plane;
- Scale: drag the square at the end of an axis to scale that one axis; drag the centre for a uniform scale.
- **Ctrl** snaps — positions to `GridSize`, angles to `SnapDegrees`. The gizmo keeps a constant on-screen
  size, and a handle seen edge-on does not steal the click (the most visible one wins).

The 2D gizmo and 2D mouse pan/zoom stay off in 3D mode so they don't fight navigation.

**Creating**: `GameObject ▸ Create 3D ▸ Cube / Plane / Sphere / OBJ Model`.
Any entity's Inspector has a **3D Object** checkbox: Position 3 / Rotation 3 (in degrees, order X→Y→Z) /
Scale 3, **Mesh** (Cube/Plane/Sphere/OBJ), the `.obj` path, Mesh Texture, Mesh Color.
A 3D entity still has `transform.position/scale` — the 2D legacy and the physics need them;
the 3D position comes from `pos3`.

**Lighting** is Lambertian: `diffuse = max(dot(n, sun), 0) * LightColor + Ambient`. The sun
direction/color and the ambient term are set in Edit ▸ Settings ▸ Lighting (3D) and saved to
`~/.astra/config.ini`. The texture is multiplied by the color and the light; with no texture you get a
flat material. Primitive normals are generated by the engine; for `.obj` they come from the file (`vn`)
or are recomputed per face.

**The game camera**: the Camera component gains **Perspective (3D)** and **Field of View**.
The Game view renders the 3D entities through that camera (2D objects still go through the orthographic
one). The depth buffer is cleared before the meshes, so cubes don't show through each other.

**Serialization** (scene/prefab file): `Is3D`, `Pos3`, `Rot3`, `Scale3`, `MeshType`
(0=Cube, 1=Plane, 2=Sphere, 3=OBJ), `MeshPath`, `MeshTex`, `MeshColor`, `CamPersp`, `CamFov`.
Old files load unchanged — there `Is3D: 0` is the default.

**Game build**: the `.obj` from `MeshPath` and the image from `MeshTex` join the scene's dependency list,
so they are copied and encrypted exactly like textures and sounds (section 11).

**Next on the 3D plan**: shadows, skeletal animation, glTF instead of OBJ, separating Mesh Renderer from
Sprite, 3D physics, orthographic 3D camera views (front/top/side).
