# Astra — Engine User Guide

Astra is a 2D/3D game engine with a Unity-style editor: C++17, OpenGL 4.6, GLFW, ImGui (dockspace).
The editor, scenes, scripts, shaders, audio and game building all live in one binary, with no external runtimes.

Contents: [build](#1-build-and-run) · [projects](#2-projects) · [interface](#3-editor-interface) ·
[entities](#4-entities-and-components) · [scenes](#5-scenes) · [assets](#6-assets-and-import) ·
[UI](#7-game-ui) · [undo](#8-undo--redo) · [hotkeys](#9-hotkeys) ·
[settings](#10-engine-settings) · [build](#11-building-the-game-and-the-player) · [console](#12-console) ·
[3D](#14-3d-scenes)

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

## 14. 3D Scenes

3D is a **property of the scene**, not a "layer" bolted on top of 2D: `View ▸ 3D Scene` (the
`Scene3D: 1` line in the file). A 3D scene has its own floor, its own camera navigation and its own
tool set; flat sprites, tilemaps, colliders, the 2D camera rect and the UI previews are not drawn in
the Scene there, and the Tile tool is hidden. 2D scenes are unchanged.

**Navigating a 3D scene** (Unity-style, not the 2D pan):
- **RMB + mouse movement** — look around (yaw/pitch, pitch clamped to ±89.5°);
- **WASD + Q/E with RMB held** — fly: W/S forward/backward along the horizon, A/D sideways,
  E/Q up/down, **Shift** — ×4 speed, **Ctrl** — ×0.25 (while RMB is held, W/E/R/Q do not switch tools);
- **wheel** — a push along the view direction (faster the farther out you are);
- **MMB** (or LMB with the **Hand** tool, Q) — pan in the screen plane;
- **LMB** — select a mesh object: a ray is cast from the camera against each mesh's AABB, the one
  closer to the camera wins; clicking empty space clears the selection;
- **F** — fly onto the selected entity (the distance is taken from its scale);
- **NUMPAD 1/2/3/4/5/7** — Front/Back/Right/Left/Top/Bottom views, **6** — perspective;
- the **compass** in the Scene's top-right corner: clickable X/Y/Z axes (and its center resets to perspective).

**3D gizmo**: the selected 3D object draws axes (Move), rings (Rotate) or axes with handles
(Scale) — switched with the **W / E / R** tools or the toolbar buttons.
- Move: drag an X/Y/Z axis — the object travels along that axis only; drag the centre diamond — free
  movement in the screen plane;
- Rotate: drag a ring — rotation about the matching axis, the angle is measured in the ring's plane;
- Scale: drag the square at the end of an axis — scale along that one axis; drag the centre — uniform.
- **Ctrl** — snapping: positions to `GridSize`, angles to `SnapDegrees`. The gizmo keeps a constant
  on-screen size, and a handle seen edge-on does not steal the click (the best visible one wins).

**Floor and grid**: the grid lies in the XZ plane (`GridSize` step), follows the camera, and the X/Z/Y
axes are tinted. The world's unit scheme is the same as in 2D: the default cube is 100×100×100.

**Creating**: `GameObject ▸ Create 3D ▸ Cube / Plane / Sphere / OBJ Model` (creating a 3D object in a
2D scene switches that scene to 3D automatically). Any entity's Inspector has a **3D Object** checkbox:
Position 3 / Rotation 3 (in degrees, order X→Y→Z) / Scale 3, **Mesh** (Cube / Plane / Sphere / **OBJ** /
**glTF**), the model path, Mesh Texture, Mesh Color. A 3D entity keeps `transform.position/scale` — the
2D legacy needs them; the 3D position comes from `pos3`.

**Models: glTF and OBJ.** glTF 2.0 (`Mesh` = glTF) is the preferred format: both `.glb` (everything in
one file) and `.gltf` (JSON + an external `.bin` or a base64 `data:` URI) load. Supported are the first
mesh with all of its primitives, `POSITION`/`NORMAL`/`TEXCOORD_0`, u32/u16/u8 indices, interleaved
`byteStride` and `normalized` attributes; if the file has no normals, the engine derives flat per-face
ones. The texture path from `baseColorTexture` shows up in the Inspector as a hint — put the image into
`Mesh Texture` yourself. Node transforms, skinning, animations, PBR metallic and Draco are not supported
(yet — they are on the plan). OBJ (`Mesh` = OBJ) stays: v/vt/vn, polygons → triangles, the face normal
when there is no `vn`. A `.gltf`'s external `.bin` files join the scene's dependencies and are encrypted
at build time exactly like the model itself.

**Light** is Lambertian with a shadow term: `diffuse = max(dot(n, sun), 0) * shadow + Ambient`, the sun
direction/color and the ambient term are set in Edit ▸ Settings ▸ Lighting (3D) and saved to
`~/.astra/config.ini`. The texture is multiplied by the color and the light; with no texture you get a
flat material. Primitive normals are generated by the engine; for `.obj` they come from the file (`vn`)
or are recomputed from the faces.

**Sun shadows**: a separate pass writes a depth map from an ortho camera fitted to the bounds of the 3D
content, then the mesh shader takes a 3×3 PCF sample and compares depth with an offset (`bias` depends on
the angle of the normal to the sun — less "acne" on slanted faces). Settings: **Shadows** (on/off) and
**Shadow map** 1024/2048/4096 — also in Lighting (3D), also in `config.ini`. Shadows accumulate from all
3D meshes and fall on everything, including the floor.

**The game camera in a 3D scene**: the Camera component has **Perspective (3D)** and **Field of View**;
the camera's pose comes from the entity's **Position 3 / Rotation 3** (the eye sits at `pos3`, the view
direction from the rotation), so you can move and rotate the camera like any other 3D object, by script
or by gizmo. Follow/Level Bounds are 2D mechanics and play no part in the 3D branch. The depth buffer is
cleared before the meshes, so cubes don't "show through" each other.

**Serialization**: the `Scene3D: 0|1` line in the file header + the entity keys `Is3D`, `Pos3`, `Rot3`,
`Scale3`, `MeshType` (0=Cube, 1=Plane, 2=Sphere, 3=OBJ, 4=glTF), `MeshPath`, `MeshTex`, `MeshColor`,
`CamPersp`, `CamFov`, plus the 3D physics keys `HasRigidbody3D`, `Rb3Kinematic`, `Rb3Velocity`,
`Rb3Mass`, `Rb3Drag`, `Rb3Gravity`, `HasCollider3D`, `Col3Type`, `Col3Trigger`, `Col3Center`,
`Col3Half`, `Col3Radius`. Old files load as before (`Is3D: 0`, `Scene3D: 0`).

**Game build**: the `.obj` from `MeshPath` and the image from `MeshTex` join the scene's dependencies,
so they are copied/encrypted exactly like textures and sounds (section 11).

**Example**: `assets/scenes/3d_demo.scene` — a floor, three cubes and a sphere under a perspective
camera with shadows (open it with a double-click in Project).

**3D physics**: the **Rigidbody (3D)** and **Collider (3D)** components are added with the
`+ Add Component` button (3D entities only). Gravity pulls along `-Y` (tuned in Edit ▸ Settings ▸
Physics ▸ Gravity (3D), in m/s², just like in 2D), colliders come as Box and Sphere; sizes are given in
mesh units (`Half Size 0.5` + `Scale 3 = 100` = a 100×100×100 cube), rotation is handled as the bounds of
the rotated box (an AABB), and scale through `Scale 3`. `Is Kinematic` — the body is driven by a
script/parent, `Is Trigger` — no pushing apart, and the events are the same ones: `OnTriggerEnter/Exit`,
`OnCollisionEnter`. The collider wireframes show up in the Scene (green — solid, cyan — trigger).
Child bodies are not integrated — the parent moves them.

**Next on the 3D plan**: skeletal animation and glTF animations, a Mesh Renderer that is its own
component next to Sprite, an orthographic view mode, several light sources, PBR materials instead of
Lambert.
