# Astra — Shader API (GLSL 4.60)

Custom shaders go into `assets/shaders/`. **A single `.frag` is enough** — the engine supplies the vertex
shader itself (a standard quad with correct UVs, including the slicing of animation frames). A `.vert + .frag`
pair — if you need your own vertex stage. It is attached to an entity via:
Inspector ▸ Custom Shader (or drag&drop the `.frag` onto an object / "Assign Shader to Selected").
The **Reload** button rebuilds the shader without a restart.

The engine adds the preamble itself: `#version 460 core`, the uniforms, the helpers. You may skip your own
`#version` (if you do write one, the engine strips the duplicate).

## 1. Fragment Shader Model

A pixel gets UV in the 0..1 range across the face of the quad (`v_UV`), everything else — uniforms:

| Uniform | Type | What it is |
|---|---|---|
| `v_UV` | `in vec2` | UV within the current frame (already sliced when animating!) |
| `u_Color` | `vec3` | the sprite's color from the inspector |
| `u_Texture` | `sampler2D` | the sprite's texture (a white 1×1 if there is none) |
| `u_Time` | `float` | seconds since startup (glfwGetTime) |
| `u_ScreenSize` | `vec2` | viewport size in pixels |
| `u_Params` | `vec4` | **Material**: 4 sliders from the inspector (0..1) |
| `u_PColor` | `vec4` | **Material**: the color with alpha from the inspector |
| `u_MVP`, `u_Model`, `u_ViewProj` | `mat4` | matrices (for the vertex shader) |
| `u_UVRect` | `vec4` | the rectangle of the current animation frame in the sheet's UV space |

## 2. Helpers (already included)

```glsl
vec2  EngineUV();                                  // 0..1 from the vertex position (for your own vertex shaders)
float EngineCircleMask(vec2 uv);                   // a circle from UV
float EngineRoundedBox(vec2 uv, float radius);     // a rounded rectangle
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float angleRad);        // rotate around (0.5,0.5)
float EngineNoise(vec2 p);                         // value-noise 0..1
float EngineFbm(vec2 p, int octaves);              // fractal brownian motion
vec2  EngineSwirl(vec2 uv, float strength);        // swirl the UV
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // IQ palette
vec3  EngineRainbow(float t);                      // a ready-made rainbow palette
float EnginePulse(float freq);                     // a 0..1 sine wave over u_Time
float EngineGrid(vec2 uv, float cells);            // grid lines
float EngineVignette(vec2 uv, float strength);     // darkening toward the edges
```

## 3. Minimal `.frag`

```glsl
// Color pulsing driven by the Material: u_Params.x is the speed, u_PColor is the tint
void main() {
    float p = EnginePulse(u_Params.x);
    fragColor = vec4(mix(vec3(0.1), u_PColor.rgb, p), 1.0);
}
```

## 4. Custom Vertex Shader (optional)

```glsl
// wob.frag + wob.vert. A wave along X:
void main() {
    vec2 p = a_Pos;
    p.x += 0.05 * sin(u_Time * 3.0 + p.y * 10.0);
    v_UV = EngineUV();          // or u_UVRect.xy + EngineUV()*u_UVRect.zw — with animation
    gl_Position = u_MVP * vec4(p, 0.0, 1.0);
}
```

If there is no vertex shader, the engine substitutes `EngineQuadVert()` — and the UV frames of the animation are sliced automatically.

## 5. Material: Live Parameters

Inspector ▸ Material: 4 sliders → `u_Params.xyzw`, color → `u_PColor`.
Move a slider and the shader changes instantly (both in the editor and in the built game).
This is how you tune: effect strength, size/speed, color, transparency — without recompiling.

## 6. Ready-Made Examples from the Project

| File | What it shows |
|---|---|
| `material_demo.frag` | a wave on `u_Params` + `u_PColor` (the animation_demo scene) |
| `black_hole.frag` | accretion disk: `EngineSwirl`, `EngineFbm`, `EnginePalette`, photon ring |
| `effect_rounded.frag` | `EngineRoundedBox` + gradient |
| `effect_glow.frag` | `EngineRing` + soft glow |
| `effect_rainbow.frag` | `EngineRainbow` + `EnginePulse` |
| `effect_plasma.frag` | `EngineFbm` plasma |
| `example_wobble.vert/.frag` | a custom vertex shader with a wave |

## 7. Debugging

- A compile error appears in the Console (text from glslang/GLSL); the object is drawn with the stock shader.
- A quick check without the engine: `glslangValidator` + the engine's preambles (the engine prints them itself
  on an error; or look at `kUserFragPrelude` in `src/core/Renderer.cpp`).
- Pixel artifacts during animation: set `Cols/Rows` so that the frames don't end up split
  (the texture is cut strictly along the grid, row 0 = top).
- Transparency: write alpha into `fragColor` — the engine renders with alpha blending.
