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
Audio::PlayLooped(path, vol, pitch); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);

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

## 7. 常见的坑

- `Owner()` 可能变成 `nullptr`（实体已被删除）— 每个方法里都要判空。
- 在 `Update` 里调用 `DestroyEntity` 是安全的：实例会活过本帧，在 SyncInstances 时才销毁。
- 运行时更换 `scriptPath` 或改动 `.cpp` 会重建实例（重新执行 `Start()`）。
- 子物体上的 Rigidbody 不参与模拟 — 它们由父物体带动。
- `timeScale=0` + `UnscaledDelta()` — 这是在暂停状态下还能做点事情的唯一办法。
