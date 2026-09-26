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

## 6. Готовые примеры из проекта

| Файл | Что показывает |
|---|---|
| `material_demo.frag` | волна на `u_Params` + `u_PColor` (сцена animation_demo) |
| `black_hole.frag` | аккреционный диск: `EngineSwirl`, `EngineFbm`, `EnginePalette`, фотонное кольцо |
| `effect_rounded.frag` | `EngineRoundedBox` + градиент |
| `effect_glow.frag` | `EngineRing` + мягкое свечение |
| `effect_rainbow.frag` | `EngineRainbow` + `EnginePulse` |
| `effect_plasma.frag` | `EngineFbm`-плазма |
| `example_wobble.vert/.frag` | свой вертекс с волной |

## 7. Отладка

- Ошибка компиляции — в Console (текст от glslang/GLSL), объект рисуется системным шейдером.
- Быстрая проверка без движка: `glslangValidator` + преамбулы движка (их печатает сам движок
  при ошибке; или смотри `kUserFragPrelude` в `src/core/Renderer.cpp`).
- Пиксельные артефакты при анимации: ставь `Cols/Rows` так, чтобы кадры не дробились
  (текстура режется строго по сетке, row 0 = верх).
- Прозрачность: пиши alpha в `fragColor` — движок рендерит с альфа-блендингом.
