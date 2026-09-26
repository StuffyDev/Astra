# Astra — 引擎使用手册

Astra 是一款带 Unity 风格编辑器的 2D 游戏引擎：C++17、OpenGL 4.6、GLFW、ImGui（dockspace）。
编辑器、场景、脚本、着色器、音频与游戏打包全部在同一个可执行文件里，不依赖任何外部运行时。

目录：[构建](#1-构建与运行) · [项目](#2-项目) · [界面](#3-编辑器界面) ·
[实体](#4-实体与组件) · [场景](#5-场景) · [资源](#6-资源与导入) ·
[UI](#7-游戏界面) · [undo](#8-undo--redo) · [快捷键](#9-快捷键) ·
[设置](#10-引擎设置) · [打包](#11-游戏打包与播放器) · [控制台](#12-控制台)

---

## 1. 构建与运行

仅支持 Linux。需要：`cmake`（>= 3.16）、`g++`（C++17）、Mesa/`libgl-dev`，以及 GLFW 的 X11 依赖。

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Astra                 # 编辑器
./build/Astra --play          # 播放器（不含编辑器的游戏）
```

所有依赖（glfw、glm、glad、imgui、stb、miniaudio）会在首次配置时由 FetchContent 自动拉取。
系统里必须有 `g++`：游戏脚本由它编译（见 SCRIPT_API.md）。

## 2. 项目

- **File ▸ New Project...** — 名称 + 目录；从 `templates/default_project` 生成
  `assets/{scenes,shaders,scripts,audio,textures,prefabs}` + `project.json` 结构。
- **File ▸ Open Project... / Projects Manager / Open Recent** — 切换项目。
  打开项目会改变引擎的工作目录：所有资源路径都相对于项目根目录。
- **File ▸ Exit** — 退出。

## 3. 编辑器界面

单个 dockspace：窗口可以用标题栏拖出来、合并成标签页，通过边缘调整大小。

| 窗口 | 作用 |
|---|---|
| **Scene** | 编辑：gizmo、选择、drag&drop、碰撞体与 UI 预览 |
| **Game** | 游戏相机视图；UI 的按钮/滑块在这里生效（在 Edit 模式下也有它们的预览） |
| **Script** | 内置 IDE：在 Project 中双击 `.cpp/.frag/...` 会在这里打开该文件 |
| **Hierarchy** | 实体树；拖动 = 重新指定父级；Ctrl+↑/↓ — 调整同级顺序 |
| **Inspector** | 选中实体的组件 + Script Variables + Material |
| **Project** | `assets/` 浏览器（网格/列表、搜索、Create、Import）以及 **Console** 标签页 |

工具栏：**Play / Pause / Stop / Restart**，`PLAYING/PAUSED` 状态，脚本错误计数
（红色按钮 — 点击打开 Console），**Move (W) / Rotate (E) / Scale (R) / Hand (Q)** 工具。

Scene 中的导航：右键/中键 — 平移，滚轮 — 向光标处缩放，F — 聚焦选中对象，
方向键 — 焦点在 Scene 窗口时平移视图。ESC — 从 Play 退出到 Edit。

## 4. 实体与组件

实体 = 一组固定的组件（目前如此；纯 ECS 在路线图中）：

- **Transform** — Position/Rotation/Scale。支持 `Parent`：子物体继承父物体的姿态
  （世界矩阵行为与 Unity 一致，包括整条链上的缩放/旋转）。
- **Sprite** — Type（None/Quad/Circle）、Color、Texture Path、Custom Shader（见 SHADER_API.md）。
- **Animation** — 图集动画：`Cols × Rows`（row 0 = 最上面一行）、FPS、Loop、Play On Awake、
  `Active`（在 Edit 模式下帧也会作为预览播放）。可以使用独立的 Sheet Path，或「Use Sprite」。
  脚本侧接口：`PlayAnimation/StopAnimation/IsAnimating`。
- **Rigidbody** — Kinematic、Velocity、Mass、Drag、Use Gravity。物理：固定 60 Hz、
  MTV 分离解算，触发器/碰撞会派发脚本事件。
- **Collider** — Box（一半的尺寸）/ Circle（半径）、Is Trigger。在 Scene 中有可视化。
- **Camera** — Main Camera（只有一个处于激活状态）、Zoom、Viewport Offset。Game 视图透过它观察；
  UI 元素按该视图的世界坐标定位。
- **UI Element** — 见第 7 节。
- **Audio Source** — Clip Path、Volume、Pitch、Loop、Play On Awake、Preview/Stop 按钮。
- **Script** — C++ 脚本（SCRIPT_API.md）。**Script Variables** — 在 `Start()` 里调用
  `DefineVar("speed", 120)`，滑块就会出现在 Inspector 中，数值会序列化进场景。

GameObject ▸ Create Empty/Quad/Circle/Camera/UI — 快速预设。

## 5. 场景

- 文本格式：`Astra Scene v2`（一个实体 = 一个 `ENTITY ... END_ENTITY` 块）。
- Ctrl+N / Ctrl+S / Ctrl+Shift+S — 新建/保存/另存为；标题里的星号 =
  有未保存的改动；关闭未保存的场景时会二次确认。
- 在 Project 中双击 `.scene` — 打开（如果当前场景是脏的会先确认）。
- **游戏中使用多个场景**：在脚本里调用 `LoadScene("assets/scenes/level2.scene")` —
  场景管理器会在运行中切换关卡（Play 模式和打包后的游戏里都可用）。

## 6. 资源与导入

- 从操作系统的文件管理器 drag&drop 到窗口 — 按类型导入（图片 → `assets/textures`、
  `.scene` → `assets/scenes`、`.cpp` → `assets/scripts`、音频 → `assets/audio`）。
- **Project ▸ Create ▸ Import File...** — 用同一个浏览器从磁盘选择文件。
- **右键 ▸ Open Externally** — 用系统默认编辑器打开文件（`xdg-open`）。
- **右键 ▸ Edit (built-in IDE)** — 在 Script 窗口中打开。Ctrl+S — 保存。
- 图片/着色器/预设可以直接拖到 Scene 中的对象上，或拖到 Hierarchy 的行上 —
  会自动赋值/实例化。
- Create ▸ Shader（一对 `.vert+.frag`）或单个 `.frag` — 带 API 提示的模板。

## 7. 游戏界面

Entity + UI Element 组件：**Button / Text / Slider / Checkbox / Progress Bar**。
位置与尺寸使用**世界坐标**（Scene 中的 gizmo 与按钮所在位置完全对应）。
样式：Text Color、Bg Color、Font（Default/Medium/Large）。复选框是真正的 bool（0/1）。
在 Edit 模式下，元素既显示在 Game 视图里，也在 Scene 中以线框显示。在 Play 中它们可以点击；
脚本读取 `GameUI::WasClicked(id)` / `GameUI::GetValue(id)`。

## 8. Undo / Redo

- **Ctrl+Z** — 后退一步，**Ctrl+Shift+Z**（或 Ctrl+Y）— 前进一步。
- 可回退的场景编辑：移动 gizmo、Inspector 中的修改、创建/删除、
  复制/粘贴、重新指定父级、实例化预设。连续的一次改动「批次」= 一步。
- 深度 — 60 步。保存场景会重置基准点。

## 9. 快捷键

| 按键 | 动作 |
|---|---|
| Ctrl+N / O / S / Shift+S | 场景 新建 / 打开 / 保存 / 另存为 |
| Ctrl+P | Play/Stop | Ctrl+D | 复制子树 |
| Ctrl+C / Ctrl+V | 复制 / 粘贴实体子树 |
| Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y | undo / redo |
| Del / Backspace | 删除实体 | F2 — 重命名 |
| W / E / R / Q | Move / Rotate / Scale / Hand |
| Ctrl+↑ / ↓ | 调整同级顺序 | F — 相机聚焦选中对象 |
| Ctrl（拖动时按住） | gizmo 吸附（50 px / 15°） |
| ESC | 退出 Play；在播放器里 — 关闭游戏 |
| Script 窗口中 Ctrl+S | 保存文件（IDE 中 ^C/^V/^X/^Z 可用） |

## 10. 引擎设置

**Edit ▸ Settings**：重力（m/s²，Unity 风格的 0,-9.81）、Time Scale（在进入 Play 时生效，
并且 Stop 之后仍然保留）、Master Volume、Mute、音频系统状态。

## 11. 游戏打包与播放器

**File ▸ Build Game...** — 两种模式：

1. **文件夹**：`astra`（可执行文件）+ `assets/` + `build-scripts/*.so`（预编译好的脚本 —
   目标机器上不需要 g++）+ `game.json`（起始场景）。运行方式：`./astra --play`。
2. **单个可执行文件**（勾选）：可执行文件后面附加了一个 bundle（内容与上面完全相同）。
   首次运行会把它解压到自身旁边的 `<名称>.bundle/` 并启动游戏。一个文件 = 一个游戏。

不带 GUI 的 CLI：`./Astra --build assets/scenes/x.scene --out ./game [--folder]`。
播放器参数：`--play`、`--scene <path>`、`--project <dir>`。在游戏中完整的脚本 API 都可用，
包括 `LoadScene`（关卡）和 `Log`（输出到控制台）。

## 12. 控制台

Project 面板中的 **Console** 标签页：引擎、脚本以及编译错误的 stdout/stderr。
**Follow** — 只有当你在最底部时才自动滚动；**Copy All** — 把整份日志复制到剪贴板；
点击某一行 — 复制该行。错误计数显示在工具栏上（红色按钮）。

## 13. 示例

- `assets/scenes/black_hole.scene` — 着色器世界（吸积盘、由脚本驱动的轨道、作为子物体的月球）。
- `assets/scenes/animation_demo.scene` — 4×2 图集小球 + 实时的 Material 参数。
- `assets/scripts/rotate.cpp`、`player.cpp`、`examples/black_hole/orbit_planet.cpp`。
- `examples/` — 示例的源码；`templates/default_project` — 项目模板。
