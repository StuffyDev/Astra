# Astra — 着色器 API（GLSL 4.60）

自定义着色器放在 `assets/shaders/` 下。**只给一个 `.frag` 就够了** — 顶点着色器由引擎
自己提供（标准四边形，UV 正确，包含动画帧的切分）。
如果你需要自己的顶点处理流程，就提供 `.vert + .frag` 一对。挂载到实体上：
Inspector ▸ Custom Shader（或把 `.frag` 拖到对象上 / 「Assign Shader to Selected」）。
**Reload** 按钮无需重启即可重新编译着色器。

引擎会自己加上一段前言：`#version 460 core`、uniform、辅助函数。你自己的 `#version`
可以不写（如果写了，引擎会把重复的那行删掉）。

## 1. 片元着色器模型

每个像素在四边形面上得到 0..1 范围内的 UV（`v_UV`），其余内容都是 uniform：

| Uniform | 类型 | 含义 |
|---|---|---|
| `v_UV` | `in vec2` | 当前帧内部的 UV（做动画时已经切好了！） |
| `u_Color` | `vec3` | Inspector 里的精灵颜色 |
| `u_Texture` | `sampler2D` | 精灵纹理（没有时是 1×1 的白图） |
| `u_Time` | `float` | 从启动开始的秒数（glfwGetTime） |
| `u_ScreenSize` | `vec2` | 视口的像素尺寸 |
| `u_Params` | `vec4` | **Material**：Inspector 里的 4 个滑块（0..1） |
| `u_PColor` | `vec4` | **Material**：Inspector 里带 alpha 的颜色 |
| `u_MVP`, `u_Model`, `u_ViewProj` | `mat4` | 矩阵（供顶点着色器使用） |
| `u_UVRect` | `vec4` | 当前动画帧在图集 UV 中的矩形 |

## 2. 辅助函数（已自动引入）

```glsl
vec2  EngineUV();                                  // 由顶点位置得到 0..1（用于自定义顶点着色器）
float EngineCircleMask(vec2 uv);                   // 按 UV 生成圆形遮罩
float EngineRoundedBox(vec2 uv, float radius);     // 圆角矩形
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float angleRad);        // 绕 (0.5,0.5) 旋转
float EngineNoise(vec2 p);                         // value-noise 0..1
float EngineFbm(vec2 p, int octaves);              // 分形布朗运动
vec2  EngineSwirl(vec2 uv, float strength);        // 旋涡式扭转 UV
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // IQ 调色板
vec3  EngineRainbow(float t);                      // 现成的彩虹调色板
float EnginePulse(float freq);                     // 随 u_Time 变化的 0..1 正弦
float EngineGrid(vec2 uv, float cells);            // 网格线
float EngineVignette(vec2 uv, float strength);     // 向边缘压暗
```

## 3. 最小 `.frag`

```glsl
// 用 Material 参数让颜色脉动：u_Params.x 是速度，u_PColor 是色调
void main() {
    float p = EnginePulse(u_Params.x);
    fragColor = vec4(mix(vec3(0.1), u_PColor.rgb, p), 1.0);
}
```

## 4. 自定义顶点着色器（可选）

```glsl
// wob.frag + wob.vert。沿 X 方向的波浪：
void main() {
    vec2 p = a_Pos;
    p.x += 0.05 * sin(u_Time * 3.0 + p.y * 10.0);
    v_UV = EngineUV();          // 或 u_UVRect.xy + EngineUV()*u_UVRect.zw — 带动画时
    gl_Position = u_MVP * vec4(p, 0.0, 1.0);
}
```

如果没有顶点着色器，引擎会代入 `EngineQuadVert()` — 动画的 UV 帧会自动切分。

## 5. Material：实时参数

Inspector ▸ Material：4 个滑块 → `u_Params.xyzw`，颜色 → `u_PColor`。
拖动滑块 — 着色器立即变化（在编辑器里和在打包后的游戏中都一样）。
用这种方式调节：效果强度、尺寸/速度、颜色、透明度 — 无需重新编译。

## 6. 3D 材质（Mesh Shader）

3D 实体上有一个 **Mesh Shader** 字段（`.vert/.frag` 的基础路径，留空 — 用引擎的系统着色器）。
和 2D 一样，只给一个 `.frag` 就够：顶点部分由引擎提供（`EngineMeshVert`）。

顶点着色器前言提供了什么（mesh 的 `in` 属性：`a_Pos`、`a_Normal`、`a_UV`）：

| 名称 | 类型 | 含义 |
|---|---|---|
| `u_VP`, `u_Model` | `mat4` | 视投影矩阵与模型矩阵 |
| `u_Time`, `u_ScreenSize` | `float`, `vec2` | 时间与视口尺寸 |
| `v_World`, `v_Normal`, `v_UV` | `out` | 世界位置、法线、uv |
| `EngineMeshVert()` | void | 标准顶点流程（在 `main()` 里调用它） |
| `EngineClip(localPos)` | `vec4` | 模型空间某点的裁剪坐标 |
| `EngineWorld(localPos)` | `vec3` | 模型空间某点的世界坐标 |
| `EngineNormalWorld()` | `vec3` | 世界空间的法线 |
| `EngineWaveVert(amp, freq)` | `vec3` | 沿高度叠了波浪之后的顶点位置 |

片元着色器则能拿到光照、阴影与材质：

| 名称 | 类型/签名 | 含义 |
|---|---|---|
| `u_Color`, `u_Texture`, `u_UseTex` | `vec3`, `sampler2D`, `float` | mesh 的颜色与贴图 |
| `u_Params`, `u_PColor` | `vec4` | Inspector 里的「Material」（和 2D 是同一批滑块） |
| `u_LightDir`, `u_LightColor`, `u_Ambient` | `vec3`, `vec3`, `float` | 太阳与补光 |
| `u_LightVP`, `u_ShadowMap`, `u_Texel`, `u_UseShadow` | `mat4`, `sampler2D`, `float`, `float` | 太阳的阴影图 |
| `u_EyePos` | `vec3` | 相机的位置（用于菲涅尔/雾/高光） |
| `EngineBaseColor()` | `vec4` | 贴图 × 颜色（考虑了 `u_UseTex`） |
| `EngineShadow(world, n)` | `float` | 1 — 被照亮，0 — 在阴影里（3×3 PCF，沿法线做偏移） |
| `EngineLambert(n)` | `float` | `max(dot(n, sun), 0)` |
| `EngineLit(albedo, n, world)` | `vec3` | `albedo * (太阳 × 阴影 + ambient)` — 与系统着色器一致 |
| `EngineLightDir()`, `EngineViewDir()` | `vec3` | 方向向量 |
| `EngineFresnel(n, power)` | `float` | 边缘光 |
| `EngineSpecular(n, power)` | `float` | 沿「太阳 + 视线」半向量的高光 |
| `EngineFog(density)` | `float` | 按到相机的距离得到 0..1 |

最小的 3D 材质：

```glsl
void main() {
    vec3 n = normalize(v_Normal);
    vec3 col = EngineLit(EngineBaseColor().rgb, n, v_World);
    col += vec3(0.35, 0.6, 1.0) * EngineFresnel(n, 2.5) * u_Params.x;   // 边缘光
    col += vec3(1.0, 0.85, 0.6) * EngineSpecular(n, 48.0) * u_Params.y; // 高光
    fragColor = vec4(col, 1.0);
}
```

完整示例 — `assets/shaders/metal3d.frag`（`3d_demo.scene` 场景里的「Cube Metal」对象）。
想形变几何体时，才需要自己的顶点着色器：

```glsl
void main() {
    v_World = EngineWorld(EngineWaveVert(12.0, 2.0));
    v_Normal = EngineNormalWorld();
    v_UV = a_UV;
    gl_Position = u_VP * vec4(v_World, 1.0);
}
```

## 7. 项目中的现成示例

| 文件 | 演示内容 |
|---|---|
| `material_demo.frag` | 基于 `u_Params` + `u_PColor` 的波浪（animation_demo 场景） |
| `black_hole.frag` | 吸积盘：`EngineSwirl`、`EngineFbm`、`EnginePalette`、光子环 |
| `effect_rounded.frag` | `EngineRoundedBox` + 渐变 |
| `effect_glow.frag` | `EngineRing` + 柔和光晕 |
| `effect_rainbow.frag` | `EngineRainbow` + `EnginePulse` |
| `effect_plasma.frag` | `EngineFbm` 等离子效果 |
| `example_wobble.vert/.frag` | 带波浪的自定义顶点着色器 |
| `metal3d.frag` | 3D 材质：`EngineLit` + 阴影 + 菲涅尔 + 高光（3d_demo 场景） |

## 8. 调试

- 编译错误在 Console 中（glslang/GLSL 给出的文本），对象此时用系统着色器绘制
  （3D 也是如此：坏掉的 `Mesh Shader` 会静默回退到系统着色器）。
- 不启动引擎的快速验证：`glslangValidator` + 引擎的那段前言（出错时引擎自己会打印前言；
  或者直接看 `src/core/Renderer.cpp` 里的 `kUserFragPrelude`）。
- 动画下出现像素瑕疵：把 `Cols/Rows` 设成不会把帧切碎的取值
  （纹理严格按网格切分，row 0 = 最上面一行）。
- 透明度：把 alpha 写进 `fragColor` — 引擎开启了 alpha 混合渲染。
