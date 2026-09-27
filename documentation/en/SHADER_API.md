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

## 6. 3D Materials (Mesh Shaders)

A 3D entity has a **Mesh Shader** field (the base path of a `.vert/.frag`; empty means the engine's stock
shader). As in 2D, a single `.frag` is enough — the engine supplies the vertex stage (`EngineMeshVert`).

What the vertex preamble gives you (the mesh's `in` attributes are `a_Pos`, `a_Normal`, `a_UV`):

| Name | Type | What it is |
|---|---|---|
| `u_VP`, `u_Model` | `mat4` | the view-projection and the model matrices |
| `u_Time`, `u_ScreenSize` | `float`, `vec2` | time and the viewport size |
| `v_World`, `v_Normal`, `v_UV` | `out` | world position, normal, uv |
| `EngineMeshVert()` | void | the standard vertex stage (call it from `main()`) |
| `EngineClip(localPos)` | `vec4` | the clip-space position of a model-space point |
| `EngineWorld(localPos)` | `vec3` | the world position of a model-space point |
| `EngineNormalWorld()` | `vec3` | the normal in world space |
| `EngineWaveVert(amp, freq)` | `vec3` | the position with a wave riding on its height |

The fragment shader gets light, shadows and the material:

| Name | Type/signature | What it is |
|---|---|---|
| `u_Color`, `u_Texture`, `u_UseTex` | `vec3`, `sampler2D`, `float` | the mesh's color and texture |
| `u_Params`, `u_PColor` | `vec4` | the inspector's "Material" — the same sliders as in 2D |
| `u_LightDir`, `u_LightColor`, `u_Ambient` | `vec3`, `vec3`, `float` | the sun and the fill light |
| `u_LightVP`, `u_ShadowMap`, `u_Texel`, `u_UseShadow` | `mat4`, `sampler2D`, `float`, `float` | the sun's shadow map |
| `u_EyePos` | `vec3` | the camera position (for fresnel/fog/specular) |
| `EngineBaseColor()` | `vec4` | texture × color (honors `u_UseTex`) |
| `EngineShadow(world, n)` | `float` | 1 — lit, 0 — shadowed (3×3 PCF, offset along the normal) |
| `EngineLambert(n)` | `float` | `max(dot(n, sun), 0)` |
| `EngineLit(albedo, n, world)` | `vec3` | `albedo * (sun × shadow + ambient)` — exactly what the stock shader does |
| `EngineLightDir()`, `EngineViewDir()` | `vec3` | direction vectors |
| `EngineFresnel(n, power)` | `float` | the rim |
| `EngineSpecular(n, power)` | `float` | a highlight off the half-vector of "sun + view" |
| `EngineFog(density)` | `float` | 0..1 by distance to the camera |

The minimal 3D material:

```glsl
void main() {
    vec3 n = normalize(v_Normal);
    vec3 col = EngineLit(EngineBaseColor().rgb, n, v_World);
    col += vec3(0.35, 0.6, 1.0) * EngineFresnel(n, 2.5) * u_Params.x;   // rim
    col += vec3(1.0, 0.85, 0.6) * EngineSpecular(n, 48.0) * u_Params.y; // specular highlight
    fragColor = vec4(col, 1.0);
}
```

The complete example is `assets/shaders/metal3d.frag` (the `3d_demo.scene` scene, the "Cube Metal"
object). You need your own vertex stage only when you want to deform the geometry:

```glsl
void main() {
    v_World = EngineWorld(EngineWaveVert(12.0, 2.0));
    v_Normal = EngineNormalWorld();
    v_UV = a_UV;
    gl_Position = u_VP * vec4(v_World, 1.0);
}
```

## 7. Ready-Made Examples from the Project

| File | What it shows |
|---|---|
| `material_demo.frag` | a wave on `u_Params` + `u_PColor` (the animation_demo scene) |
| `black_hole.frag` | accretion disk: `EngineSwirl`, `EngineFbm`, `EnginePalette`, photon ring |
| `effect_rounded.frag` | `EngineRoundedBox` + gradient |
| `effect_glow.frag` | `EngineRing` + soft glow |
| `effect_rainbow.frag` | `EngineRainbow` + `EnginePulse` |
| `effect_plasma.frag` | `EngineFbm` plasma |
| `example_wobble.vert/.frag` | a custom vertex shader with a wave |
| `metal3d.frag` | a 3D material: `EngineLit` + shadow + fresnel + specular (the 3d_demo scene) |

## 8. Debugging

- A compile error appears in the Console (text from glslang/GLSL) and the object is drawn with the stock
  shader — 3D behaves the same way: a broken `Mesh Shader` silently falls back to the stock one.
- A quick check without the engine: `glslangValidator` + the engine's preambles (the engine prints them itself
  on an error; or look at `kUserFragPrelude` in `src/core/Renderer.cpp`).
- Pixel artifacts during animation: set `Cols/Rows` so that the frames don't end up split
  (the texture is cut strictly along the grid, row 0 = top).
- Transparency: write alpha into `fragColor` — the engine renders with alpha blending.
