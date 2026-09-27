# Astra — 游戏脚本 API（C++）

脚本就是普通的 C++ 类。引擎在进入 Play 时（以及打包游戏时）用 `g++ -std=c++17 -shared -fPIC`
把它们编译成 `build-scripts/<名称>.so`，再通过 `dlopen` 加载。
热重载：改动过的 `.cpp` 会重新编译，实例会重新创建。
**不需要自己写 include** — 引擎会生成一个 wrapper-TU，由它自动引入
`ScriptAPI.h`、`SceneManager.h`、`Transforms.h`、`Physics.h`、`Input.h`、`GameUI.h`、
`Audio.h`、GLFW、glm 以及 `<cmath>/<string>/<vector>/<algorithm>/<random>`。

## 1. 最小脚本

```cpp
class Rotate : public Script {
public:
    void Update(float dt) override {
        Entity* e = Owner();
        if (e) e->transform.rotation += 90.0f * dt;
    }
};
SCRIPT_ENTRY(Rotate)   // 每个文件里恰好一个工厂函数
```

挂载方式：Inspector ▸ Script（针对 `assets/scripts/*.cpp` 的下拉框）。编译错误会出现在
Console 中，并在工具栏上以红色按钮提示。

## 2. Script 类

| 钩子 | 调用时机 |
|---|---|
| `Start()` | 实例创建时（进入 Play、spawn、LoadScene） |
| `Update(float dt)` | 每一帧（dt 已经乘过 timeScale） |
| `OnDestroy()` | 实例销毁之前（Stop/StopAnimation 这类场景） |
| `OnTriggerEnter(uint32_t otherId)` / `OnTriggerExit` | 进入/离开触发器 |
| `OnCollisionEnter(uint32_t otherId)` | 非触发器（trigger）的碰撞 |
| `OnAnimEvent(const char* name)` | 动画帧越过了检视面板（Events）里的标记 |

方法（protected）：

```cpp
Entity* Owner();                 // 承载脚本的实体（可能变成 nullptr — 请判空）
::SceneManager* Scene();         // 访问实体列表与选择集

// 「可序列化字段」变量（相当于 [SerializeField]）：
void  DefineVar(const char* name, float defaultValue); // 在 Start() 中调用；已存在的不会改动
// 引擎也会直接从源码中扫描 DefineVar(...) — 滑块在 Edit 模式下就会立即出现，无需先进入 Play。
float GetVar(const char* name, float fallback = 0) const;
void  SetVar(const char* name, float value);
// 每个变量都会在 Inspector ▸ Script Variables 中以滑块出现，
// 数值保存在场景里（会写入 .scene/.prefab）。

// 姿态：
void    Translate(const glm::vec2& localDelta); // 局部坐标（已考虑父物体旋转）
void    SetWorldPosition(const glm::vec2& world);
glm::vec2 WorldPosition() const;
float   AngleTo(const glm::vec2& worldPoint) const; // 角度
void    LookAt(const glm::vec2& worldPoint);        // 把「上方」转向该点
```

## 3. 全局 API 函数

```cpp
// 实体管理
bool DestroyEntity(uint32_t id);          // 删除实体（其脚本实例会被卸载）
void LoadScene(const std::string& path);  // 场景管理器：切换到下一个关卡
void Log(const std::string& message);     // 向 Console 面板输出一行

// 时间
Time::Delta(); Time::UnscaledDelta(); Time::SinceStart();
Time::TimeScale(); Time::SetTimeScale(float);   // 0 = 暂停整个游戏

// 物理/数学
AddForce(Entity* e, glm::vec2 impulse);   // 速度增量/质量
Lerp(a,b,t); Clamp(v,lo,hi); Radians(deg); Degrees(rad);
RandomRange(0.f,1.f); RandomInt(1,6);
Physics::Gravity;                          // glm::vec2，世界单位 px/s²

// 输入（脚本有自己的 Input 单例，与引擎用的是同一个）
Input::Get().IsKeyPressed(GLFW_KEY_SPACE); // GLFW 键码
Input::Get().IsActionHeld("jump");         // 具名绑定：在 Start() 里 BindKey
Input::Get().GetAxis("move");              // -1..1
Input::Get().mousePosition(); mouseDelta(); // 等等

// 音频
Audio::PlayOneShot("assets/audio/hit.wav", 1.0f, 1.0f); // -> uint32 voiceId
Audio::PlayOneShot(path, vol, pitch, group); // 第 4 个参数 group：0=SFX, 1=Music
Audio::PlayLooped(path, vol, pitch, group); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);
Audio::SetGroupVolume(group, 0..1); Audio::GroupVolume(group); // 0=SFX, 1=Music

// 游戏 UI（按钮/滑块 — 即带 UI Element 组件的实体）
GameUI::WasClicked(entityId);              // 点击后的那一帧返回 true
GameUI::GetValue(entityId);                // float：滑块是 0..1 区间，复选框是 0/1

// 精灵动画与剪辑（Clip）
PlayAnimation(Owner(), /*fromStart=*/true); StopAnimation(Owner()); IsAnimating(Owner());
bool ok = PlayClip(Owner(), "run");            // Inspector 中 Clips 表里定义的剪辑

// 粒子：从实体的发射器瞬时喷出一批（爆炸/火花）
EmitParticles(Owner(), 30);

// 游戏相机震动（game feel）：振幅为世界单位，时长以秒计
ShakeCamera(15.0f, 0.3f);

// 退出游戏：在播放器里关闭窗口，在编辑器里只是停止 Play
QuitGame();                       // Exit 按钮：GameUI::WasClicked(exitId) -> QuitGame()
// 鼠标捕获（射击/策略游戏）：系统光标被隐藏，鼠标事件照常送达
CaptureMouse(true); if (IsMouseCaptured()) { ... }
// 生成预设（子弹、敌人）：返回实例根节点的 id（0 — 出错）
uint32_t bullet = InstantiatePrefab("assets/prefabs/bullet.prefab", WorldPosition());
```

## 4. 访问场景

`Scene()` 返回 `::SceneManager*`（全局作用域，没有命名空间）：

```cpp
for (Entity& e : Scene()->GetEntities()) { ... }
Entity* sel = Scene()->GetSelectedEntityPtr();
int idx = Scene()->IndexOf(someId);
Scene()->RemoveEntityById(someId);   // 与 DestroyEntity 等价
```

## 5. 示例：带可序列化设置的角色

```cpp
class SpaceBody : public Script {
public:
    void Start() override {
        DefineVar("orbitRadius", 430.0f);
        DefineVar("speed", 60.0f);      // 度/秒 — 滑块会出现在 Inspector 中
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
        DestroyEntity(otherId);        // 例如：把恒星吃掉
    }
private:
    float angle = 0.0f;
};
SCRIPT_ENTRY(SpaceBody)
```

## 6. 底层是如何工作的

- `.cpp` 文件 → wrapper `build-scripts/<stem>_gen.cpp`（自动 include + `#include "<绝对路径>"`）。
- 编译按时间戳缓存：`.so` 比源文件新 — 就不会再调用 g++。
- `dlopen(RTLD_NOW)`：得益于 `-rdynamic`，脚本能看到引擎的符号；入口点是
  `extern "C" CreateGameScript`（由 `SCRIPT_ENTRY` 宏生成）。
- `SyncInstances` 每帧一次：新的脚本承载者 → `new + Start()`，被删除/换了路径的 →
  `OnDestroy + delete`。编译错误不会让游戏崩溃 — 而是在 Console 中显示出来。
- 在打包后的游戏里不编译脚本：直接使用 `build-scripts/` 中现成的 `.so`。

## 8. 3D：姿态、物理、射线

在 3D 场景里，实体活在 `pos3 / rot3（角度，顺序 X→Y→Z）/ scale3` 中 — 下面的方法作用的就是它。
世界单位与 2D 相同：100 单位 = 1 米（在 Settings ▸ Physics ▸ Pixels per meter 里设置），
因此 `Gravity3D()` 的默认值约为 `(0, -981, 0)`。

`Script` 的方法（protected，在子类里可用）：

| 方法 | 作用 |
|---|---|
| `bool Is3D() const` | 实体是否为三轴模式（否则 3D 方法只是在搬动空气） |
| `glm::vec3 Position3D() const` | 考虑 parent 链之后的世界位置 |
| `void SetPosition3D(const glm::vec3&)` | 放到世界的某个位置（会换算回局部坐标） |
| `void Translate3D(const glm::vec3&)` | 平移 `pos3`（局部，不含父物体） |
| `glm::vec3 Rotation3D() const` / `SetRotation3D(const glm::vec3&)` | 以度为单位的角度 |
| `void SetScale3D(const glm::vec3&)` | 缩放（同时同步 2D 的 `scale`） |
| `glm::vec3 Velocity3D() const` / `SetVelocity3D(const glm::vec3&)` | Rigidbody (3D) 的速度；setter 在组件不存在时会把它创建出来 |
| `void AddForce3D(const glm::vec3&)` | 冲量：`velocity += impulse / mass` |
| `void SetGravityEnabled3D(bool)` | 开启/关闭重力（同样会创建 Rigidbody） |
| `void LookAt3D(const glm::vec3&)` | 把物体的 `-Z` 转向目标（如同 Unity 的 `transform.LookAt`） |
| `void AddForceTo3D(Entity*, const glm::vec3&)` / `void SetVelocityOf3D(Entity*, const glm::vec3&)` | 对**另一个**实体做同样的事 |

全局函数：

```cpp
struct RayHit3D { uint32_t entityId; std::string name; glm::vec3 point, normal; float distance; };
bool Raycast3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D& out);
int  RaycastAll3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D* out, int max);
glm::vec3 Gravity3D();
uint32_t InstantiatePrefab3D(const std::string& prefabPath, const glm::vec3& pos);
inline void AddForce3D(Entity* e, const glm::vec3& impulse);   // 用于别的实体
inline void SetVelocity3D(Entity* e, const glm::vec3& v);
inline glm::vec3 MoveTowards3D(const glm::vec3& from, const glm::vec3& to, float maxDelta);
```

`Raycast3D` 会遍历 3D 实体：有 **Collider (3D)** 的用它的外形尺寸（考虑缩放与旋转），没有的
就用按 mesh 推出的盒子（`0.5 * scale3`）。返回最近的目标、入射面的法线以及距离；`dir` 不需要
事先归一化。

物理：`Rigidbody (3D)` 与 `Collider (3D)` 在 Inspector 里添加（`+ Add Component`，仅限 3D 实体），
也可以由脚本创建（`SetGravityEnabled3D`、`AddForce3D`）。事件与 2D 是共用的：
`OnTriggerEnter/Exit(otherId)`、`OnCollisionEnter(otherId)` — `otherId` 可以通过 `Scene()`
或 `FindById` 换回 `Entity*`。

示例 — 会跳的立方体（`assets/scripts/bounce3d.cpp`）：

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

## 7. 常见的坑

- `Owner()` 可能变成 `nullptr`（实体已被删除）— 每个方法里都要判空。
- 在 `Update` 里调用 `DestroyEntity` 是安全的：实例会活过本帧，在 SyncInstances 时才销毁。
- 运行时更换 `scriptPath` 或改动 `.cpp` 会重建实例（重新执行 `Start()`）。
- 子物体上的 Rigidbody 不参与模拟 — 它们由父物体带动。
- `timeScale=0` + `UnscaledDelta()` — 这是在暂停状态下还能做点事情的唯一办法。
- 在 `Script` 方法内部，同名成员会遮蔽全局函数：
  `AddForce3D(other, v)` 编译不过 —— 请用 `AddForceTo3D(other, v)` 或 `::AddForce3D(other, v)`。
- 2D 场景里的 3D 实体在 Scene 视图里看不到（反之亦然）：模式是场景的属性，`View ▸ 3D Scene`。
- `Translate3D` 不考虑父物体的旋转 — 需要「局部」运动时自己换算。
- `Raycast3D` 在没有 `Collider (3D)` 时使用 mesh 的外轮廓：对旋转过的模型来说它比实际更大。
