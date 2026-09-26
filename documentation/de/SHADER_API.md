# Astra — Shader-API (GLSL 4.60)

Eigene Shader kommen nach `assets/shaders/`. **Ein einzelnes `.frag` genügt** — den Vertex-Shader liefert die
Engine selbst (Standard-Quad mit korrekten UV, inklusive Zuschnitt der Animationsframes).
Ein Paar `.vert + .frag` brauchst du nur, wenn du eine eigene Vertex-Berechnung willst. Zugewiesen wird der
Entity über Inspector ▸ Custom Shader (oder Drag&Drop der `.frag` auf ein Objekt / »Assign Shader to Selected«).
Der Button **Reload** baut den Shader ohne Neustart neu.

Die Engine fügt die Präambel selbst hinzu: `#version 460 core`, Uniforms, Helfer. Einen eigenen `#version`
kannst du weglassen (wenn er doch dasteht, entfernt die Engine das Duplikat).

## 1. Das Fragment-Shader-Modell

Ein Pixel bekommt UV im Bereich 0..1 über die Quad-Fläche (`v_UV`), alles andere kommt als Uniform:

| Uniform | Typ | Was das ist |
|---|---|---|
| `v_UV` | `in vec2` | UV innerhalb des aktuellen Frames (bei Animation schon zugeschnitten!) |
| `u_Color` | `vec3` | Sprite-Farbe aus dem Inspector |
| `u_Texture` | `sampler2D` | Sprite-Textur (weiße 1×1, falls keine gesetzt ist) |
| `u_Time` | `float` | Sekunden seit dem Start (glfwGetTime) |
| `u_ScreenSize` | `vec2` | Viewport-Größe in Pixeln |
| `u_Params` | `vec4` | **Material**: 4 Regler aus dem Inspector (0..1) |
| `u_PColor` | `vec4` | **Material**: Farbe mit Alpha aus dem Inspector |
| `u_MVP`, `u_Model`, `u_ViewProj` | `mat4` | Matrizen (für den Vertex-Shader) |
| `u_UVRect` | `vec4` | Rechteck des aktuellen Animationsframes im Blatt-UV |

## 2. Helfer (sind bereits eingebunden)

```glsl
vec2  EngineUV();                                  // 0..1 aus der Vertex-Position (für eigene Vertex-Shader)
float EngineCircleMask(vec2 uv);                   // Kreis über UV
float EngineRoundedBox(vec2 uv, float radius);     // abgerundetes Rechteck
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float angleRad);        // Drehung um (0.5,0.5)
float EngineNoise(vec2 p);                         // Value-Noise 0..1
float EngineFbm(vec2 p, int octaves);              // Fractal Brownian Motion
vec2  EngineSwirl(vec2 uv, float strength);        // UV-Wirbel
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // IQ-Palette
vec3  EngineRainbow(float t);                      // fertige Regenbogen-Palette
float EnginePulse(float freq);                     // 0..1 Sinus über u_Time
float EngineGrid(vec2 uv, float cells);            // Gitterlinien
float EngineVignette(vec2 uv, float strength);     // Abdunklung zu den Rändern
```

## 3. Ein minimales `.frag`

```glsl
// Pulsieren der Farbe über Material: u_Params.x — Tempo, u_PColor — Farbton
void main() {
    float p = EnginePulse(u_Params.x);
    fragColor = vec4(mix(vec3(0.1), u_PColor.rgb, p), 1.0);
}
```

## 4. Eigener Vertex-Shader (optional)

```glsl
// wob.frag + wob.vert. Welle entlang X:
void main() {
    vec2 p = a_Pos;
    p.x += 0.05 * sin(u_Time * 3.0 + p.y * 10.0);
    v_UV = EngineUV();          // oder u_UVRect.xy + EngineUV()*u_UVRect.zw — mit Animation
    gl_Position = u_MVP * vec4(p, 0.0, 1.0);
}
```

Fehlt der Vertex-Shader, setzt die Engine `EngineQuadVert()` ein — und die UV-Frames der Animation werden von allein zugeschnitten.

## 5. Material: Live-Parameter

Inspector ▸ Material: 4 Slider → `u_Params.xyzw`, Farbe → `u_PColor`.
Du drehst an einem Regler — der Shader ändert sich sofort (im Editor wie im gebauten Spiel).
Damit stellt man ein: Effektstärke, Größe/Tempo, Farbe, Transparenz — ohne Neukompilierung.

## 6. Fertige Beispiele aus dem Projekt

| Datei | Was sie zeigt |
|---|---|
| `material_demo.frag` | Welle auf `u_Params` + `u_PColor` (Szene animation_demo) |
| `black_hole.frag` | Akkretionsscheibe: `EngineSwirl`, `EngineFbm`, `EnginePalette`, Photonring |
| `effect_rounded.frag` | `EngineRoundedBox` + Verlauf |
| `effect_glow.frag` | `EngineRing` + weiches Leuchten |
| `effect_rainbow.frag` | `EngineRainbow` + `EnginePulse` |
| `effect_plasma.frag` | `EngineFbm`-Plasma |
| `example_wobble.vert/.frag` | eigener Vertex-Shader mit Welle |

## 7. Fehlersuche

- Kompilierungsfehler — in der Console (Text von glslang/GLSL), das Objekt wird mit dem System-Shader gezeichnet.
- Schnelle Prüfung ohne Engine: `glslangValidator` + die Präambeln der Engine (bei einem Fehler gibt die Engine
  sie selbst aus; oder schau dir `kUserFragPrelude` in `src/core/Renderer.cpp` an).
- Pixel-Artefakte bei Animationen: setze `Cols/Rows` so, dass die Frames nicht zerteilt werden
  (die Textur wird exakt am Gitter geschnitten, row 0 = oben).
- Transparenz: schreibe Alpha in `fragColor` — die Engine rendert mit Alpha-Blending.
