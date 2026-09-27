# Astra — 引擎使用手册

Astra 是一款带 Unity 风格编辑器的 2D/3D 游戏引擎：C++17、OpenGL 4.6、GLFW、ImGui（dockspace）。
编辑器、场景、脚本、着色器、音频与游戏打包全部在同一个可执行文件里，不依赖任何外部运行时。

目录：[构建](#1-构建与运行) · [项目](#2-项目) · [界面](#3-编辑器界面) ·
[实体](#4-实体与组件) · [场景](#5-场景) · [资源](#6-资源与导入) ·
[UI](#7-游戏界面) · [undo](#8-undo--redo) · [快捷键](#9-快捷键) ·
[设置](#10-引擎设置) · [打包](#11-游戏打包与播放器) · [控制台](#12-控制台) ·
[3D](#14-3d-场景)

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

**右键上下文菜单**：在 Scene 中 — 不拖动的单击会打开「Create Empty/Quad here、Paste here、
Deselect、Focus selection」；在 Hierarchy 中 — 在某一行上（Rename、Duplicate、**Move Up/Down**
用于调整同级顺序、Detach、Save as Prefab、Revert、Delete）以及在空白处（Create Empty、Paste、
Deselect）；在 Project 中 — 在空白处（Create Folder/Shader/Script、Import File...）。

**Script Variables**：引擎会直接扫描脚本源码里的 `DefineVar("name", default)` —
滑块会立刻出现在 Inspector 中，在 Edit 模式下、在任何 Play 之前就能用（相当于 [SerializeField]）。

## 4. 实体与组件

实体 = 一组固定的组件（目前如此；纯 ECS 在路线图中）：

- **Transform** — Position/Rotation/Scale。支持 `Parent`：子物体继承父物体的姿态
  （世界矩阵行为与 Unity 一致，包括整条链上的缩放/旋转）。
  **3D Object** 复选框会把实体切换为三轴模式：Position 3 / Rotation 3（度）/ Scale 3，
  并用 **Mesh** 取代 sprite（见第 14 节）。
- **Sprite** — Type（None/Quad/Circle）、Color、Texture Path、**Sorting Order**（值越小越早绘制、
  压在其它物体下面，与 Unity 一致）、Custom Shader（见 SHADER_API.md）。
- **Animation** — 图集动画：`Cols × Rows`（row 0 = 最上面一行）、FPS、Loop、Play On Awake、
  `Active`（在 Edit 模式下帧也会作为预览播放）。可以使用独立的 Sheet Path，或「Use Sprite」。
  **Clips** 表 — 每行包含名称、first..last、fps、loop 以及 Play/X 按钮；没有剪辑时整张网格循环播放。
  脚本侧接口：`PlayAnimation/StopAnimation/IsAnimating`、`PlayClip(e, "run")`。
- **Rigidbody** — Kinematic、Velocity、Mass、Drag、Use Gravity。物理：固定 60 Hz、
  MTV 分离解算，触发器/碰撞会派发脚本事件。
- **Collider** — Box（一半的尺寸）/ Circle（半径）、Is Trigger。在 Scene 中有可视化。
- **Camera** — Main Camera（只有一个处于激活状态）、Zoom、Viewport Offset。Game 视图透过它观察；
  UI 元素按该视图的世界坐标定位。
  **Follow Target** — 相机平滑跟随选中的实体（Damping = 秒，Offset = 瞄准偏移）。
  **Level Bounds** — 关卡的一个矩形：相机中心不会被放出边界（考虑缩放）。
  脚本触发屏幕震动：`ShakeCamera(15.0f, 0.3f)`。
  **Perspective (3D)** + **Field of View** — 用透视投影取代正交投影（见第 14 节）。
- **UI Element** — 见第 7 节。
- **动画事件**：Animation > Events — 标记（clip：任意/指定、帧、名字）。当帧越过标记时，
  载体脚本收到 `OnAnimEvent("step")` — 脚步声/射击/命中精确落在帧上。
- **Audio Source** — Clip Path、Volume、Pitch、Loop、Play On Awake、**Group**（SFX/Music）、Preview/Stop 按钮。
  各组音量在 Edit ▸ Settings ▸ Audio 里调整。
- **Tilemap** — 由图集拼出的瓦片网格：Atlas Path、Tile Size、Atlas Cols、Grid W×H、Tint、
  Sorting Order（默认在精灵下面）。Pick Tile 弹窗把图集显示成网格；选中的瓦片由 **Tile (T)** 笔刷绘制：
  LMB（鼠标左键）在选中的 Tilemap 实体的网格上放置当前瓦片，Shift+LMB 擦除。
  实体的 `transform.position` = 网格的左上角。Fill floor/Clear 按钮用于快速上手。
  **Solid (physics)** — 非空单元格会变成静态 AABB 碰撞体（平台跳跃的地面/墙壁；
  刚体能稳稳落在上面，撞击轴方向的速度会被归零）。
- **Particle Emitter** — 粒子发射器：纹理（或纯色方块）、max/rate、life/speed/angle 的最小-最大值、
  gravity、size start/end、color start/end（alpha 逐渐衰减）、Loop、Play On Awake、Burst 按钮。
  脚本侧接口：`EmitParticles(Owner(), 30)`。
- **Script** — C++ 脚本（SCRIPT_API.md）。**Script Variables** — 在 `Start()` 里调用
  `DefineVar("speed", 120)`，滑块就会出现在 Inspector 中，数值会序列化进场景。

GameObject ▸ Create Empty/Quad/Circle/Camera/UI — 快速预设。

**组件按 Unity 的方式添加**：新实体只有 Transform+Sprite；
Rigidbody/Collider/Audio/Script/Particle Emitter/Tilemap/UI/Camera/Animation 通过 Inspector 底部的
**+ Add Component** 按钮添加，用 section 标题里的 **x** 按钮移除。组件的「有无」标记会序列化进场景文件。
旧场景文件（v0.10 之前）按原来的方式加载 —— 那里的组件都是「已启用」的（legacy）。
此外 Inspector 和 Hierarchy 的比例现在更紧凑（20%/17%）。

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

**Edit ▸ Settings**：
- **Physics** — 重力（m/s²，Unity 风格的 0,-9.81）、Pixels per meter（世界缩放比例）。
- **Render** — 场景/游戏的背景色、在 Scene 中显示网格与碰撞体、网格/吸附步长（px）。
- **Editor** — 旋转吸附步长（°）、界面主题 **Light theme**（亮色/暗色 UI 在运行时切换）。
- **Time** — Time Scale（在进入 Play 时生效，Stop 之后仍然保留）。
- **Audio** — Master Volume、Mute、音频设备状态。
- **Lighting (3D)** — 太阳方向（Light Dir，三个数构成一个向量，由引擎归一化）、
  太阳颜色（Light Color）与环境补光（Ambient）。作用于所有 mesh 物体（见第 14 节）。

文件浏览器（Browse/Import）是普通窗口：可以同时操作界面的其它部分，按 ESC 关闭。
所有设置都会保存到 `~/.astra/config.ini`，并在启动时恢复（主题、背景、网格、吸附、px/米、音量、
time scale、3D 场景光照）。界面是柔和的圆角风格，分两套主题：暗色「Astra Slate」与亮色「Astra Paper」。
文件图标（文件夹/图片/脚本/着色器/场景/预设/音频）全部以矢量方式画在引擎代码里 — 不使用任何第三方资源。

## 11. 游戏打包与播放器

**File ▸ Build Settings...**（**Ctrl+Shift+B**）— 一个 Unity 风格的可停靠窗口：

- **Product name** — 游戏 exe 的名字；
- **Scenes Included** — 项目里的所有场景：复选框决定哪些场景进入构建，单选按钮指定启动场景
  （打包后的游戏就从它开始）；
- 输出 **Folder**（以及 Browse...）— 构建结果放到哪里；
- **Encrypt used assets** — 对构建带走的资源做 `AENC` 加密；
- **Copy engine library** — 把 `libastra_engine.so` 复制到 exe 旁边。

**Build** 只产出一种发布形式：一个很小的启动器（约 18 KB），链接到 `libastra_engine.so`，里面只有场景
真正用到的资源（已加密）以及预编译成 `.so` 的脚本 — 目标机器上不需要 g++。勾选 Copy engine library 后，
库就放在 exe 旁边，配合 rpath `$ORIGIN` 整个文件夹可随身携带；不勾选则库从构建目录里取用。运行方式：
直接 `./游戏名`（cwd = exe 所在的目录）。

单个可执行文件与「文件夹」两种模式现在只在 CLI 里提供：`./Astra --build assets/scenes/x.scene
--out ./game [--single|--folder]` —— `--single` 会在可执行文件后面附加一个 bundle（引擎 + assets +
.so + game.json），首次运行时解压到自身旁边的 `<名称>.bundle/`（一个文件 = 一个游戏）；`--folder`
是完整拷贝 `astra` + `assets/` + `build-scripts/` + `game.json`，运行方式 `./astra --play`。
播放器参数：`--play`、`--scene <path>`、`--project <dir>`。在游戏中完整的脚本 API 都可用，
包括 `LoadScene`（关卡）和 `Log`（输出到控制台）。

**只打包所需 + 加密**：进入构建的并不是整个 `assets/`，而是场景真正用到的文件（纹理/着色器/音频/
Tilemap 与粒子的图集 + 递归收集的预设）。勾选 **Encrypt used assets** 后它们全部会被加密（`AENC`：
splitmix64 生成的 XOR 密钥流），引擎读取时透明解密 — 游戏目录里不存在原始的纹理/场景/音频。脚本的
`.so` 不加密（由 dlopen 加载）。这是「防好奇眼睛」的混淆，不是真正防破解的加密。

## 12. 控制台

Project 面板中的 **Console** 标签页：引擎、脚本以及编译错误的 stdout/stderr。
**Follow** — 只有当你在最底部时才自动滚动；**Copy All** — 把整份日志复制到剪贴板；
点击某一行 — 复制该行。错误计数显示在工具栏上（红色按钮）。

## 13. 示例

- `assets/scenes/black_hole.scene` — 着色器世界（吸积盘、由脚本驱动的轨道、作为子物体的月球）。
- `assets/scenes/animation_demo.scene` — 4×2 图集小球 + 实时的 Material 参数。
- `assets/scripts/rotate.cpp`、`player.cpp`、`examples/black_hole/orbit_planet.cpp`。
- `examples/` — 示例的源码；`templates/default_project` — 项目模板。

## 14. 3D 场景

3D 是**场景的一个属性**，而不是叠加在 2D 之上的「外挂层」：`View ▸ 3D Scene`（在文件里就是一行
`Scene3D: 1`）。3D 场景有自己的地面、自己的相机导航和自己的工具集；平面 sprite、Tilemap、碰撞体、
2D 相机框以及 Scene 里的 UI 预览在那里都不会绘制，Tile 工具也会被隐藏。2D 场景完全没有变化。

**3D 场景中的导航**（和 Unity 一样，而不是 2D 平移那一套）：
- **右键 + 移动鼠标** — 环视（yaw/pitch，俯仰限制在 ±89.5°）；
- **按住右键 + WASD + Q/E** — 飞行：W/S 在水平面内前后移动，A/D 侧移，E/Q 上升/下降，
  **Shift** — 速度 ×4，**Ctrl** — 速度 ×0.25（右键按住期间，W/E/R/Q 不再切换工具）；
- **滚轮** — 沿视线方向推进（离得越远，推得越快）；
- **中键**（或用 **Hand** 工具（Q）+ 左键）— 在屏幕平面内平移；
- **左键** — 选中 mesh 物体：从相机发出一条射线与 mesh 的 AABB 求交，离相机近的胜出；
  点空白处则取消选中；
- **F** — 对准当前选中的实体（距离取自它的 scale）；
- **NUMPAD 1/2/3/4/5/7** — Front/Back/Right/Left/Top/Bottom 视图，**6** — 透视视图；
- Scene 右上角的**罗盘**：X/Y/Z 轴都可以点击（点中心则复位到透视视图）。

**3D gizmo**：被选中的 3D 物体会画出坐标轴（Move）、圆环（Rotate）或带手柄的坐标轴（Scale）——
用 **W / E / R** 或工具栏上的按钮切换。
- Move：拖动 X/Y/Z 轴 — 物体只沿该轴移动；拖动中心的菱形 — 在屏幕平面内自由移动；
- Rotate：拖动圆环 — 绕对应的轴旋转，角度在该圆环所在的平面内计算；
- Scale：拖动轴末端的小方块 — 只缩放这一根轴；拖动中心 — 等比缩放。
- **Ctrl** — 吸附：位置吸附到 `GridSize`，角度吸附到 `SnapDegrees`。gizmo 在屏幕上保持恒定大小，
  侧视时被压扁的手柄不会抢占点击（取最容易看到的那一个）。

**地面与网格**：网格躺在 XZ 平面内（步长 `GridSize`），跟随相机移动，X/Z/Y 三轴做了着色区分。
世界单位制和 2D 一致：默认的立方体是 100×100×100。

**创建**：`GameObject ▸ Create 3D ▸ Cube / Plane / Sphere / OBJ Model`（在 2D 场景里创建 3D 物体会
自动把该场景切换成 3D）。Inspector 里任何实体都有一个 **3D Object** 复选框：Position 3 /
Rotation 3（以度为单位，顺序 X→Y→Z）/ Scale 3、**Mesh**（Cube/Plane/Sphere/OBJ）、`.obj` 路径、
Mesh Texture、Mesh Color。3D 实体仍然保留 `transform.position/scale` —— 它们是 2D 遗留所需要的，
而 3D 中的位置取自 `pos3`。

**光照** —— 带阴影的 Lambertian 漫反射：`diffuse = max(dot(n, sun), 0) * shadow + Ambient`，
太阳方向/颜色与 ambient 在 Edit ▸ Settings ▸ Lighting (3D) 里设置，并保存到 `~/.astra/config.ini`。
纹理会乘以颜色和光照；没有纹理时就是纯色材质。基元的法线由引擎生成，`.obj` 的法线取自文件里的
`vn`，没有则按面重新计算。

**太阳阴影**：一个单独的 pass 从正交相机写出深度图（depth map），这台正交相机会根据 3D 内容的
包围范围自动拟合；随后 mesh shader 做 3×3 PCF 采样，并把深度与带偏移的值比较（`bias` 取决于
法线相对太阳的夹角 —— 斜面上的「条纹」更少）。设置项：**Shadows**（开/关）和 **Shadow map**
1024/2048/4096 —— 同样在 Lighting (3D) 里，同样写进 `config.ini`。阴影由所有 3D mesh 一起合成，
并落在包括地面在内的一切东西上。

**3D 场景中的游戏相机**：Camera 组件上有 **Perspective (3D)** 和 **Field of View**；
相机的姿态取自实体的 **Position 3 / Rotation 3**（眼位在 `pos3`，视线方向来自 rotation），
也就是说相机可以像普通 3D 物体一样被移动和旋转，用脚本或 gizmo 都行。Follow/Level Bounds
是 2D 机制，在 3D 分支里不参与。绘制 mesh 之前会清空深度缓冲，因此立方体之间不会互相「透视穿透」。

**序列化**：文件头部的一行 `Scene3D: 0|1`，加上实体关键字 `Is3D`、`Pos3`、`Rot3`、`Scale3`、
`MeshType`（0=Cube、1=Plane、2=Sphere、3=OBJ）、`MeshPath`、`MeshTex`、`MeshColor`、`CamPersp`、
`CamFov`。旧文件照常加载（`Is3D: 0`、`Scene3D: 0`）。

**游戏打包**：`MeshPath` 里的 `.obj` 与 `MeshTex` 里的图片会被加入场景的依赖列表，
也就是和纹理、音频一样被复制/加密（见第 11 节）。

**示例**：`assets/scenes/3d_demo.scene` —— 一台带阴影的透视相机下的一块地面、三个立方体和一个球
（在 Project 里双击打开）。

**3D 后续计划**：3D 物理（面向三轴的 Rigidbody/Collider）、骨骼动画、用 glTF 取代 OBJ、
把 Mesh Renderer 从 Sprite 拆成独立组件、正交视图模式、多光源照明。
