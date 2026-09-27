# Astra — API шейдеров (GLSL 4.60)

Свои шейдеры кладутся в `assets/shaders/`. **Достаточно одного `.frag`** — вертекс движок
даст сам (стандартный квад с корректными UV, включая нарезку кадров анимации).
Пара `.vert + .frag` — если нужен свой вершинный процесс. Вешается на сущность:
Inspector ▸ Custom Shader (или drag&drop `.frag` на объект / «Assign Shader to Selected»).
Кнопка **Reload** пересобирает шейдер без рестарта.

Движок добавляет преамбулу сам: `#version 460 core`, униформы, хелперы. Свой `#version`
можно не писать (если написан — движок вырежет дубликат).

## 1. Фрагментная шейдерная модель

Пиксель получает UV в диапазоне 0..1 по лицу квада (`v_UV`), всё остальное — униформы:

| Униформа | Тип | Что это |
|---|---|---|
| `v_UV` | `in vec2` | UV внутри текущего кадра (при анимации уже нарезан!) |
| `u_Color` | `vec3` | цвет спрайта из инспектора |
| `u_Texture` | `sampler2D` | текстура спрайта (белая 1×1, если нет) |
| `u_Time` | `float` | секунды с запуска (glfwGetTime) |
| `u_ScreenSize` | `vec2` | размер вьюпорта в пикселях |
| `u_Params` | `vec4` | **Material**: 4 слайдера из инспектора (0..1) |
| `u_PColor` | `vec4` | **Material**: цвет с альфой из инспектора |
| `u_MVP`, `u_Model`, `u_ViewProj` | `mat4` | матрицы (для вертекса) |
| `u_UVRect` | `vec4` | прямоугольник текущего кадра анимации в UV листа |

## 2. Хелперы (уже подключены)

```glsl
vec2  EngineUV();                                  // 0..1 из позиции вершины (для своих вертексов)
float EngineCircleMask(vec2 uv);                   // круг по UV
float EngineRoundedBox(vec2 uv, float radius);     // скруглённый прямоугольник
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float angleRad);        // поворот вокруг (0.5,0.5)
float EngineNoise(vec2 p);                         // value-noise 0..1
float EngineFbm(vec2 p, int octaves);              // fractal brownian motion
vec2  EngineSwirl(vec2 uv, float strength);        // закручивание UV
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // IQ-палитра
vec3  EngineRainbow(float t);                      // готовая радужная палитра
float EnginePulse(float freq);                     // 0..1 синусоида по u_Time
float EngineGrid(vec2 uv, float cells);            // линии сетки
float EngineVignette(vec2 uv, float strength);     // затемнение к краям
```

## 3. Минимальный `.frag`

```glsl
// Пульсация цвета Material'ом: u_Params.x — скорость, u_PColor — оттенок
void main() {
    float p = EnginePulse(u_Params.x);
    fragColor = vec4(mix(vec3(0.1), u_PColor.rgb, p), 1.0);
}
```

## 4. Свой вертекс (опционально)

```glsl
// wob.frag + wob.vert. Волна по X:
void main() {
    vec2 p = a_Pos;
    p.x += 0.05 * sin(u_Time * 3.0 + p.y * 10.0);
    v_UV = EngineUV();          // или u_UVRect.xy + EngineUV()*u_UVRect.zw — с анимацией
    gl_Position = u_MVP * vec4(p, 0.0, 1.0);
}
```

Если вертекса нет, движок подставляет `EngineQuadVert()` — и UV-кадры анимации режутся сами.

## 5. Material: живые параметры

Inspector ▸ Material: 4 слайдера → `u_Params.xyzw`, цвет → `u_PColor`.
Крутишь ползунок — шейдер меняется мгновенно (в редакторе и в собранной игре).
Так настраивают: силу эффекта, размер/скорость, цвет, прозрачность — без перекомпиляции.

## 6. 3D-материалы (меш-шейдеры)

У 3D-сущности есть поле **Mesh Shader** (базовый путь `.vert/.frag`, пусто — системный шейдер
движка). Как и в 2D, достаточно только `.frag`: вершинную часть даст движок (`EngineMeshVert`).

Что даёт преамбула вертексного шейдера (`in`-атрибуты меша: `a_Pos`, `a_Normal`, `a_UV`):

| Имя | Тип | Что это |
|---|---|---|
| `u_VP`, `u_Model` | `mat4` | камера-проекция и модель |
| `u_Time`, `u_ScreenSize` | `float`, `vec2` | время и размер вьюпорта |
| `v_World`, `v_Normal`, `v_UV` | `out` | мировая позиция, нормаль, uv |
| `EngineMeshVert()` | void | стандартный вертекс (вызвать из `main()`) |
| `EngineClip(localPos)` | `vec4` | клип-координаты точки модели |
| `EngineWorld(localPos)` | `vec3` | мировые координаты точки модели |
| `EngineNormalWorld()` | `vec3` | нормаль в мире |
| `EngineWaveVert(amp, freq)` | `vec3` | позиция с волной по высоте |

Фрагментный шейдер получает свет, тени и материал:

| Имя | Тип/сигнатура | Что это |
|---|---|---|
| `u_Color`, `u_Texture`, `u_UseTex` | `vec3`, `sampler2D`, `float` | цвет и текстура меша |
| `u_Params`, `u_PColor` | `vec4` | «Material» из инспектора (те же ползунки, что в 2D) |
| `u_LightDir`, `u_LightColor`, `u_Ambient` | `vec3`, `vec3`, `float` | солнце и заполняющий свет |
| `u_LightVP`, `u_ShadowMap`, `u_Texel`, `u_UseShadow` | `mat4`, `sampler2D`, `float`, `float` | карта теней солнца |
| `u_EyePos` | `vec3` | положение камеры (для фреснеля/тумана/бликов) |
| `EngineBaseColor()` | `vec4` | текстура × цвет (учитывает `u_UseTex`) |
| `EngineShadow(world, n)` | `float` | 1 — светло, 0 — в тени (3×3 PCF, смещение по нормали) |
| `EngineLambert(n)` | `float` | `max(dot(n, sun), 0)` |
| `EngineLit(albedo, n, world)` | `vec3` | `albedo * (солнце × тень + ambient)` — как у системного шейдера |
| `EngineLightDir()`, `EngineViewDir()` | `vec3` | направления |
| `EngineFresnel(n, power)` | `float` | кромка |
| `EngineSpecular(n, power)` | `float` | блик по половине вектора «солнце + взгляд» |
| `EngineFog(density)` | `float` | 0..1 по дистанции до камеры |

Минимальный 3D-материал:

```glsl
void main() {
    vec3 n = normalize(v_Normal);
    vec3 col = EngineLit(EngineBaseColor().rgb, n, v_World);
    col += vec3(0.35, 0.6, 1.0) * EngineFresnel(n, 2.5) * u_Params.x;   // кромка
    col += vec3(1.0, 0.85, 0.6) * EngineSpecular(n, 48.0) * u_Params.y; // блик
    fragColor = vec4(col, 1.0);
}
```

Полный пример — `assets/shaders/metal3d.frag` (сцена `3d_demo.scene`, объект «Cube Metal»).
Свой вертекс нужен, если хочешь деформировать геометрию:

```glsl
void main() {
    v_World = EngineWorld(EngineWaveVert(12.0, 2.0));
    v_Normal = EngineNormalWorld();
    v_UV = a_UV;
    gl_Position = u_VP * vec4(v_World, 1.0);
}
```

## 7. Готовые примеры из проекта

| Файл | Что показывает |
|---|---|
| `material_demo.frag` | волна на `u_Params` + `u_PColor` (сцена animation_demo) |
| `black_hole.frag` | аккреционный диск: `EngineSwirl`, `EngineFbm`, `EnginePalette`, фотонное кольцо |
| `effect_rounded.frag` | `EngineRoundedBox` + градиент |
| `effect_glow.frag` | `EngineRing` + мягкое свечение |
| `effect_rainbow.frag` | `EngineRainbow` + `EnginePulse` |
| `effect_plasma.frag` | `EngineFbm`-плазма |
| `example_wobble.vert/.frag` | свой вертекс с волной |
| `metal3d.frag` | 3D-материал: `EngineLit` + тень + френель + блик (сцена 3d_demo) |

## 8. Отладка

- Ошибка компиляции — в Console (текст от glslang/GLSL), объект рисуется системным шейдером
  (то же касается 3D: битый `Mesh Shader` молча откатывается на системный).
- Быстрая проверка без движка: `glslangValidator` + преамбулы движка (их печатает сам движок
  при ошибке; или смотри `kUserFragPrelude` в `src/core/Renderer.cpp`).
- Пиксельные артефакты при анимации: ставь `Cols/Rows` так, чтобы кадры не дробились
  (текстура режется строго по сетке, row 0 = верх).
- Прозрачность: пиши alpha в `fragColor` — движок рендерит с альфа-блендингом.
