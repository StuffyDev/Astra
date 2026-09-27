# Astra — Engine User Guide

Astra is a 2D game engine with a Unity-style editor: C++17, OpenGL 4.6, GLFW, ImGui (dockspace).
The editor, scenes, scripts, shaders, audio and game building all live in one binary, with no external runtimes.

Contents: [build](#1-build-and-run) · [projects](#2-projects) · [interface](#3-editor-interface) ·
[entities](#4-entities-and-components) · [scenes](#5-scenes) · [assets](#6-assets-and-import) ·
[UI](#7-game-ui) · [undo](#8-undo--redo) · [hotkeys](#9-hotkeys) ·
[settings](#10-engine-settings) · [build](#11-building-the-game-and-the-player) · [console](#12-console)

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

## 4. Entities and Components

An entity = a set of fixed components (for now; a pure ECS is on the roadmap):

- **Transform** — Position/Rotation/Scale. There is a `Parent`: children inherit the parent's pose
  (world matrices just like in Unity, including the chain's scale/rotation).
- **Sprite** — Type (None/Quad/Circle), Color, Texture Path, **Sorting Order** (a smaller value draws
  first/under the rest, just like in Unity), Custom Shader (see SHADER_API.md).
- **Animation** — a sprite sheet: `Cols × Rows` (row 0 = top row), FPS, Loop, Play On Awake,
  `Active` (in Edit the frames keep playing as a preview). Either a separate Sheet Path or "Use Sprite".
  From scripts: `PlayAnimation/StopAnimation/IsAnimating`.
- **Rigidbody** — Kinematic, Velocity, Mass, Drag, Use Gravity. Physics: fixed 60 Hz,
  MTV resolution, triggers/collisions with script events.
- **Collider** — Box (half sizes) / Circle (radius), Is Trigger. Visualized in the Scene.
- **Camera** — Main Camera (one active), Zoom, Viewport Offset. The Game view looks
  through it; UI elements are positioned in world coordinates of this view.
- **UI Element** — see section 7.
- **Audio Source** — Clip Path, Volume, Pitch, Loop, Play On Awake, Preview/Stop buttons.
- **Script** — a C++ script (SCRIPT_API.md). **Script Variables** — `DefineVar("speed", 120)`
  in `Start()`, a slider appears in the inspector, and the value is serialized into the scene.

GameObject ▸ Create Empty/Quad/Circle/Camera/UI — quick presets.

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
| W / E / R / Q | Move / Rotate / Scale / Hand |
| Ctrl+↑ / ↓ | move a sibling | F — focus the camera on the selection |
| Ctrl (hold while dragging) | snap the gizmo (50 px / 15°) |
| ESC | leave Play; in the player — close the game |
| Ctrl+S in the Script window | save the file (^C/^V/^X/^Z work in the IDE) |

## 10. Engine Settings

**Edit ▸ Settings**:
- **Physics** — gravity (m/s², Unity-style 0,-9.81), Pixels per meter (the world scale).
- **Render** — the scene/game background color, showing the grid and the colliders in the Scene,
  grid/snap step (px).
- **Editor** — the rotation snap step (°).
- **Time** — Time Scale (applied when entering Play, survives Stop).
- **Audio** — Master Volume, Mute, device status.

The file browsers (Browse/Import) are ordinary windows: you can keep working with the rest of the
interface, ESC closes them.

## 11. Building the Game and the Player

**File ▸ Build Game...** — three modes (Godot/UE-style; the engine is not duplicated per build):

1. **Launcher + engine library (recommended)** — the game builds as a tiny exe (~18 KB) linked
   against `libastra_engine.so`. The "copy the library next to the game" option (rpath `$ORIGIN`)
   keeps the folder portable; without it the library is taken from the build folder. Scripts are
   precompiled to `.so` — no g++ needed on the target machine. Run: just `./game_name`
   (cwd = the exe's folder).
2. **Single executable** — a bundle (engine + assets + .so + game.json) is appended to the binary;
   the first run unpacks it into `<name>.bundle/` next to itself. One file = the game.
3. **Folder with the astra binary** — a full copy: `astra` + `assets/` + `build-scripts/` +
   `game.json`. Run: `./astra --play`.

CLI without a GUI: `./Astra --build assets/scenes/x.scene --out ./game [--single|--folder]`
(default is mode 1). Player flags: `--play`, `--scene <path>`, `--project <dir>`. In the game,
the whole script API is available, including `LoadScene` (levels) and `Log` (writes to the console).

## 12. Console

The **Console** tab in the Project panel: the engine's stdout/stderr, the scripts' output, and compile errors.
**Follow** — auto-scroll only if you were already at the bottom; **Copy All** — the whole log to the clipboard;
clicking a line — copies that line. The error counter lives on the toolbar (red button).

## 13. Examples

- `assets/scenes/black_hole.scene` — a shader world (accretion disk, orbit driven by a script, a child moon).
- `assets/scenes/animation_demo.scene` — a 4×2 sprite-sheet ball + live Material parameters.
- `assets/scripts/rotate.cpp`, `player.cpp`, `examples/black_hole/orbit_planet.cpp`.
- `examples/` — sources of the examples; `templates/default_project` — the project template.
