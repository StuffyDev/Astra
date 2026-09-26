# Astra 引擎手册

Astra 是一款带有类 Unity 编辑器的 2D 游戏引擎：C++17、OpenGL 4.6、GLFW、ImGui（dockspace）。
编辑器、场景、脚本与着色器全部内置于同一个可执行文件，无需任何外部运行时。

---

## 1. 构建

仅支持 Linux。需要：`cmake`（>= 3.16）、`g++`（C++17）、`libgl-dev`/Mesa、GLFW 的 `X11` 依赖。

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/Astra
```

所有依赖（glfw、glm、glad、imgui、stb、miniaudio）在首次配置时通过 FetchContent
自动拉取。系统中必须安装 `g++`：进入 Play 模式时游戏脚本由它编译。

## 2. 项目

- **File ▸ New Project...** — 填写名称 + 选择文件夹；从预设 `templates/default_project`
  生成 `assets/{scenes,shaders,sprites,scripts,audio,prefabs}` 目录结构 + `project.json`。
- **File ▸ Open Project... / Projects Manager / Open Recent** — 在项目之间切换。
  打开项目会改变引擎的工作目录，所有资源路径均相对于项目根目录。
- **File ▸ Exit** — 退出。

## 3. 编辑器

### 面板（通过 View ▸ ... 显示/隐藏）
| 面板 | 用途 |
|---|---|
| **Scene** | 场景编辑器：gizmo、导航、可直接把资源拖拽进世界 |
| **Game** | game 摄像机视图；Play 模式下运行时 UI 呈现于此 |
| **Hierarchy** | 实体树；拖拽可调整顺序与父级 |
| **Inspector** | 所选实体的组件 |
| **Project** | 资源（Assets 标签页）与 **Console**（脚本/着色器错误、std::cout/cerr 输出） |

### Scene 视图导航
- 鼠标中键（或 Alt+左键）— 平移；方向键 — 键盘平移。
- 滚轮 — 缩放。**F**（按住）— 聚焦到所选实体。

### Gizmo 与快捷键
| 按键 | 操作 |
|---|---|
| **W / E / R / Q** | 移动 / 旋转 / 缩放 / 选择 |
| **Ctrl+N / Ctrl+O** | 新建场景 / 打开场景 |
| **Ctrl+S / Ctrl+Shift+S** | 保存 / 另存为 |
| **Ctrl+D** | 复制子树 |
| **Ctrl+↑ / Ctrl+↓** | 在同级之间移动实体 |
| **Delete / Backspace** | 删除子树 |
| **F2** | 重命名（实体；若焦点在 Project 面板则为资源） |
| **Ctrl+P** | Play/Edit 切换 |
| **Esc** | Stop（从 Play/Pause 返回 Edit） |

标题栏显示场景名称，若有未保存的修改则附带 `*`；
关闭或切换有改动的场景时会弹出确认对话框。

### Play / Pause / Stop
工具栏：▶ Play、⏸ Pause、⏹ Stop。进入 Play 时引擎会对场景做一份快照；
停止时恢复一切（位置、速度、创建/删除的对象）——你可以放心游玩和编辑，
不必担心弄坏场景。在 Edit 模式下，Game 视图中的对象与 UI 依然像在检查器中
一样可点击、可编辑，且 Scene 导航不受影响。

## 4. 实体与组件

实体（`src/ecs/Entity.h`）= Transform + 一组组件：

- **Transform** — 位置/旋转/缩放；子实体的值**相对于父实体**定义
  （与 Unity 相同：子实体继承整条父链的移动、旋转与缩放）。
- **Sprite** — Quad/Circle、颜色、纹理（`texturePath`）、自定义着色器（`shaderPath`）。
- **Rigidbody** — 质量、阻尼（drag）、重力、是否运动学。
- **Collider** — Box/Circle，触发器或实体碰撞体；会考虑旋转/缩放及父级链。
- **Camera** — game 摄像机；`mainCamera` 指定 Game 视图使用哪一台；作为子实体的摄像机
  会跟随父实体。
- **UI** — 以 Game 视图坐标表示的屏幕元素（Button/Text/Slider）。
- **Audio Source** — 音频片段、音量、音高、循环、Play On Awake；检查器中可试听。
- **Script** — 指向 `.cpp` 文件的路径（见第 5 节）。

### 拖拽放置
- 从 Project 把图片拖到 Hierarchy 中的实体或 Scene 里的某一点：会应用该纹理
  （或以该纹理新建一个 Quad）。
- `.frag`/着色器 — 作为目标的 `shaderPath` 赋值。
- 音频片段 — 赋给光标所在实体的音频组件。
- `.prefab` — 在投放点实例化（放入层级或直接放入 Scene）。
- 来自磁盘（窗口之外）的文件 — 按扩展名导入到对应的 `assets/` 文件夹。

### Hierarchy
拖动节点：行的上三分之一 — 插到其前，下沿 — 插到其后，
中间 — 成为其子节点。同级顺序即绘制顺序（靠后的绘制在上层）。

## 5. 脚本（C++）

脚本就是 `assets/scripts/` 中普通的 C++ 文件。**无需自己写 include** — 进入 Play 时
引擎会生成一个 wrapper-TU，自动接入 `ScriptAPI.h`、`Input`、`Audio`、
`GameUI`、`Physics`、GLFW、glm、`<cmath>` 等，再用 `g++ -shared -fPIC`
编译为 `.so` 并通过 `dlopen` 加载。编译错误显示在 **Console**
标签页以及工具栏的红色计数器上。

最小脚本：

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

### API（`src/scripts/ScriptAPI.h`）
```cpp
class Script {
    uint32_t ownerId;
    virtual void Start();
    virtual void Update(float dt);      // dt 已乘以 TimeScale
    virtual void OnDestroy();
    // 物理交互（另一个实体的 id）：
    virtual void OnTriggerEnter(uint32_t otherId);
    virtual void OnTriggerExit(uint32_t otherId);
    virtual void OnCollisionEnter(uint32_t otherId);
protected:
    Entity* Owner();                    // 挂载本脚本的实体
    SceneManager* Scene();              // 访问实体/树
    void Translate(const glm::vec2& d); // 本地平移（考虑父级）
    void SetWorldPosition(const glm::vec2& p);
    glm::vec2 WorldPosition() const;
    float AngleTo(const glm::vec2& worldPoint) const; // 角度
    void LookAt(const glm::vec2& worldPoint);         // 让「上方向」指向该点
};

class Time {  // 全静态
    static float Delta();        // 含 TimeScale
    static float UnscaledDelta();
    static float SinceStart();   // 自 Play 起的秒数
    static float TimeScale();
    static void SetTimeScale(float);
};

#define SCRIPT_ENTRY(Class)      // 每个文件一个工厂
```

以下同样可用（引擎会自动包含相应头文件）：
- `Input::Get()` — 按键/鼠标/动作/轴：`IsActionHeld`、`GetAxis`，绑定写在 `Start()` 中。
- `Audio::PlayOneShot/PlayLooped/Stop/SetVolume/SetPitch` — 以 id 控制音频片段。
- `GameUI::WasClicked(entityId)`、`GameUI::GetValue(entityId)` — 响应按钮/滑块。
- `Physics::Gravity` — 世界重力。

## 6. 着色器

资源位于 `assets/shaders/*.frag`（`*.vert` 可选）。在 Inspector ▸ Custom Shader
中按名称选择；或在 Project 中右键 ▸ Assign Shader to Selected。

**只写一个 `.frag` 就够了** — 几何由引擎提供（四边形，`v_UV` 从 0 到 1）。

引擎已向着色器注入：
```glsl
// 顶点：a_Pos、u_MVP/u_Model/u_ViewProj、u_Time、u_ScreenSize、
//       v_UV、EngineUV()、EngineQuadVert()
// 片元：fragColor、v_UV、u_Color、u_Texture、u_Time、u_ScreenSize
float EngineCircleMask(vec2 uv);
float EngineRoundedBox(vec2 uv, float radius);
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float deg);
float EngineNoise(vec2 p);
float EngineFbm(vec2 p, int octaves);
vec2  EngineSwirl(vec2 uv, vec2 center, float strength, float radius);
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // Inigo Quilez 调色板
vec3  EngineRainbow(float t);
float EnginePulse(float freq); // 0..1，基于 u_Time 的正弦脉冲
```

`assets/shaders/` 中的示例：`effect_rounded`、`effect_glow`、`effect_rainbow`、
`effect_plasma`、`example_wobble`（vert+frag 一对）、`black_hole`（来自 demos）。
检查器中的 **Reload** 按钮会重建着色器与纹理缓存。

## 7. 声音

Audio Source 组件：路径、音量（0..2）、音高（0.1..3）、循环、Play On Awake、
Preview/Stop。资源放在 `assets/audio/*.{wav,mp3,ogg,flac}`（miniaudio）。
混音：Edit ▸ Settings — 主音量与静音。设备错误同样显示在那里。

## 8. 物理

固定时间步长 60 Hz。默认重力 (0, -9.81) m/s²，1 m = 100 px —
均可在 Edit ▸ Settings 中调整。碰撞体：Box/Circle，触发器（产生事件）与 solid
（接触解算）。父级的移动/旋转/缩放会影响子级碰撞体。
事件以 `OnTriggerEnter/Exit`、`OnCollisionEnter` 的形式送达脚本。

## 9. 预制体

实体右键（或检查器中的图标）▸ Save as Prefab → `assets/prefabs/`。
把预制体拖入层级/场景即得到一个实例；Revert To Prefab — 回退实例的修改。
复制（`Ctrl+D`）与序列化都按子树进行。

## 10. 引擎设置

**Edit ▸ Settings**：重力（m/s²）、时间缩放、主音量、静音。
时间缩放在进入 Play 时生效。

## 11. 示例

- `examples/black_hole/` — 黑洞：吸积盘着色器（仅 `.frag`）、
  使用 `LookAt` 的轨道脚本、带月亮子实体的场景。已同步复制到
  `assets/scenes/black_hole.scene` — File ▸ Open Scene 后 Play 即可。
- `assets/scripts/rotate.cpp`、`player.cpp` — 永动旋转与 WASD 控制的角色。

## 12. 内置代码编辑器（IDE-lite）

- 在 Project 面板双击 `.cpp`/`.h`/`.frag`/`.vert`/`.txt`/`.json` 即在内置编辑器窗口打开；
  也可右键 ▸ **Edit (built-in IDE)**。
- **View ▸ Script Editor** 显示/隐藏编辑器窗口。
- 窗口内 **Ctrl+S** 保存文件（编辑器获得焦点时场景热键被旁路）。
- **Ctrl+C / Ctrl+V / Ctrl+X / Ctrl+Z** 原生可用（ImGui InputTextMultiline + GLFW 剪贴板）。
- **Open Externally** 用系统默认程序打开文件（`xdg-open`）。

## 13. 从外部导入

- **Project ▸ Create ▸ Import File...** — 磁盘文件浏览器；按扩展名把所选文件复制到对应的
  `assets/` 子目录（textures/audio/scenes/scripts/shaders/prefabs），其余进入 `assets/imported`。
  拖放导入依然可用。

## 14. 控制台

- Assets 旁边的 **Console** 标签页；标签名显示错误计数。
- **Follow** — 仅当已经位于底部时才自动滚动（可关闭）。
- **Copy All** 复制整个日志；单击某行即复制该行。

## 15. 运行时 UI（Game GUI）

- 控件：Button/Text/Slider/Checkbox/Progress Bar。
- `transform.position/scale` 使用**世界坐标**（Scene 中 gizmo 与按钮位置一致）。
- 每个元素可设样式：`UITextColor`、`UIBgColor`、`UIFontScale`（0/1/2 = 常规/中/大）。
- Edit 模式下元素同时显示在 Game-view 和 Scene 的预览框中。

## 16. Build Game 与 Player

- **File ▸ Build Game...** 生成独立游戏目录：`astra`（当前二进制副本）、`assets/`、
  `build-scripts/*.so`（**预编译脚本**，目标机无需 g++）、含启动场景的 `game.json`。
- 运行：`cd <目录> && ./astra --play`（或 `Astra --play --scene ... --project ...`）。
- Player 中 ESC 关闭窗口；只有游戏，没有编辑器。

## 17. 当前版本限制

- 内存中仅有一个项目/一个场景；不支持叠加式（additive）场景。
- 子实体的 Rigidbody 不参与物理模拟（由父级带动）。
- 暂无动画与瓦片渲染。
