# Astra — Handbuch der Engine

Astra — ein 2D-Spiel-Engine mit einem Editor im Stil von Unity: C++17, OpenGL 4.6, GLFW, ImGui (Dockspace).
Editor, Szenen, Skripte und Shader — alles in einer einzigen Binary, ohne externe Runtimes.

---

## 1. Build

Nur Linux. Erforderlich: `cmake` (>= 3.16), `g++` (C++17), `libgl-dev`/Mesa, `X11`-Abhängigkeiten von GLFW.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/Astra
```

Alle Abhängigkeiten (glfw, glm, glad, imgui, stb, miniaudio) werden beim ersten Konfigurieren
automatisch über FetchContent bezogen. `g++` im System ist Pflicht: Damit werden
die Spiel-Skripte beim Start von Play kompiliert.

## 2. Projekte

- **File ▸ New Project...** — Name + Ordner; aus der Vorlage `templates/default_project`
  entsteht die Struktur `assets/{scenes,shaders,sprites,scripts,audio,prefabs}` + `project.json`.
- **File ▸ Open Project... / Projects Manager / Open Recent** — Wechsel zwischen Projekten.
  Das Öffnen eines Projekts wechselt das Arbeitsverzeichnis der Engine; alle Asset-Pfade sind relativ zum Projektstamm.
- **File ▸ Exit** — Beenden.

## 3. Editor

### Panels (View ▸ ... schaltet sie ein/aus)
| Panel | Zweck |
|---|---|
| **Scene** | Szenen-Editor: Gizmos, Navigation, Drag&Drop von Assets direkt in die Welt |
| **Game** | Ansicht der Game-Kamera; in Play lebt hier das Runtime-UI |
| **Hierarchy** | Baum der Entitäten; Ziehen ändert Reihenfolge und Elternobjekt |
| **Inspector** | Komponenten der gewählten Entität |
| **Project** | Assets (Reiter Assets) und **Console** (Skript-/Shader-Fehler, Ausgabe von std::cout/cerr) |

### Navigation in der Scene
- Mittlere Maustaste (oder Alt+linksklick) — pannen; Pfeiltasten — Pannen per Tastatur.
- Mausrad — Zoom. **F** (halten) — Fokus auf die gewählte Entität.

### Gizmos und Hotkeys
| Taste | Aktion |
|---|---|
| **W / E / R / Q** | Bewegen / Drehen / Skalieren / Auswählen |
| **Ctrl+N / Ctrl+O** | neue Szene / Szene öffnen |
| **Ctrl+S / Ctrl+Shift+S** | speichern / speichern unter |
| **Ctrl+D** | Teilbaum duplizieren |
| **Ctrl+↑ / Ctrl+↓** | Entität innerhalb der Geschwisterknoten verschieben |
| **Delete / Backspace** | Teilbaum löschen |
| **F2** | umbenennen (Entität oder Asset, wenn der Fokus im Project-Panel liegt) |
| **Ctrl+P** | Play/Edit |
| **Esc** | Stop (zurück zu Edit aus Play/Pause) |

Der Fenstertitel zeigt den Szenennamen und `*`, wenn ungespeicherte Änderungen vorliegen;
beim Schließen/Wechseln einer Szene mit Änderungen erscheint ein Bestätigungsdialog.

### Play / Pause / Stop
Toolbar: ▶ Play, ⏸ Pause, ⏹ Stop. Beim Eintritt in Play erstellt die Engine eine Momentaufnahme
der Szene; Stop stellt alles wieder her (Positionen, Geschwindigkeiten, erzeugte/gelöschte Objekte) —
so kann man spielen und editieren, ohne die Szene zu beschädigen. Im Edit-Modus bleiben
Objekte und das UI der Game-Ansicht genauso klickbar/bearbeitbar wie im Inspector, und die
Scene-Navigation funktioniert weiterhin.

## 4. Entitäten und Komponenten

Eine Entität (`src/ecs/Entity.h`) = Transform + ein Satz Komponenten:

- **Transform** — Position/Drehung/Skalierung; bei Kindern wird sie **relativ zum Elternobjekt**
  angegeben (wie in Unity: das Kind erbt Bewegung, Drehung und Skalierung der Kette).
- **Sprite** — Quad/Kreis, Farbe, Textur (`texturePath`), eigener Shader (`shaderPath`).
- **Rigidbody** — Masse, Widerstand (drag), Schwerkraft, Kinematik.
- **Collider** — Box/Kreis, Trigger oder fest; berücksichtigt Drehung/Skalierung und Elternkette.
- **Camera** — Game-Kamera; `mainCamera` bestimmt sie für die Game-Ansicht; eine Kamera als
  Kind folgt dem Elternobjekt.
- **UI** — Bildschirm-Element (Button/Text/Slider) in Koordinaten der Game-Ansicht.
- **Audio Source** — Clip, Lautstärke, Tonhöhe, Loop, Play On Awake; Vorschau im Inspector.
- **Script** — Pfad zu einer `.cpp`-Datei (siehe Abschnitt 5).

### Drag & Drop
- Bild aus dem Project — auf eine Entität in der Hierarchy oder an einen Punkt der Scene:
  Die Textur wird angewendet (oder es entsteht ein Quad mit dieser Textur).
- `.frag`/Shader — wird als `shaderPath` des Ziels zugewiesen.
- Audioclip — in die Audio-Komponente der Entität unter dem Cursor.
- `.prefab` — wird am Ablagepunkt instanziiert (in die Hierarchie oder direkt in die Scene).
- Datei von der Festplatte (außerhalb des Fensters) — wird je nach Endung in den passenden
  Ordner unter `assets/` importiert.

### Hierarchy
Ziehen eines Knotens: oberes Drittel der Zeile — davor einfügen, unteres — danach,
Mitte — zum Kind werden. Die Reihenfolge der Geschwister = Zeichenreihenfolge (spätere liegen oben).

## 5. Skripte (C++)

Ein Skript ist eine gewöhnliche C++-Datei in `assets/scripts/`. **Includes muss man nicht
selbst schreiben** — beim Eintritt in Play erzeugt die Engine eine Wrapper-Übersetzungseinheit,
die selbst `ScriptAPI.h`, `Input`, `Audio`, `GameUI`, `Physics`, GLFW, glm, `<cmath>` und
Weiteres einbindet, die `.so` über `g++ -shared -fPIC` kompiliert und per `dlopen` lädt.
Kompilierfehler erscheinen im Reiter **Console** und im roten Zähler in der Toolbar.

Minimalles Skript:

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

### API (`src/scripts/ScriptAPI.h`)
```cpp
class Script {
    uint32_t ownerId;
    virtual void Start();
    virtual void Update(float dt);      // dt ist bereits mit TimeScale multipliziert
    virtual void OnDestroy();
    // Physikalische Interaktion (id der zweiten Entität):
    virtual void OnTriggerEnter(uint32_t otherId);
    virtual void OnTriggerExit(uint32_t otherId);
    virtual void OnCollisionEnter(uint32_t otherId);
protected:
    Entity* Owner();                    // tragende Entität
    SceneManager* Scene();              // Zugriff auf Entitäten/Baum
    void Translate(const glm::vec2& d); // lokale Verschiebung (Elternobjekt berücksichtigt)
    void SetWorldPosition(const glm::vec2& p);
    glm::vec2 WorldPosition() const;
    float AngleTo(const glm::vec2& worldPoint) const; // Grad
    void LookAt(const glm::vec2& worldPoint);         // „oben“ auf den Punkt richten
};

class Time {  // statisch
    static float Delta();        // mit TimeScale
    static float UnscaledDelta();
    static float SinceStart();   // Sekunden seit Play
    static float TimeScale();
    static void SetTimeScale(float);
};

#define SCRIPT_ENTRY(Class)      // eine Fabrik pro Datei
```

Zusätzlich verfügbar (die Engine bindet die Header selbst ein):
- `Input::Get()` — Tasten/Maus/Aktionen/Achsen: `IsActionHeld`, `GetAxis`, Bindings in `Start()`.
- `Audio::PlayOneShot/PlayLooped/Stop/SetVolume/SetPitch` — id-gesteuerte Clip-Steuerung.
- `GameUI::WasClicked(entityId)`, `GameUI::GetValue(entityId)` — Antworten von Buttons/Slidern.
- `Physics::Gravity` — Welt-Schwerkraft.

## 6. Shader

Assets `assets/shaders/*.frag` (+ optional `*.vert`). In Inspector ▸ Custom Shader
werden sie nach Namen ausgewählt; im Project-Panel — Rechtsklick ▸ Assign Shader to Selected.

**Ein einzelnes `.frag` genügt** — die Geometrie liefert die Engine (Quad, `v_UV` von 0 bis 1).

Von der Engine in den Shader werden bereits injiziert:
```glsl
// Vertex: a_Pos, u_MVP/u_Model/u_ViewProj, u_Time, u_ScreenSize,
//          v_UV, EngineUV(), EngineQuadVert()
// Fragment: fragColor, v_UV, u_Color, u_Texture, u_Time, u_ScreenSize
float EngineCircleMask(vec2 uv);
float EngineRoundedBox(vec2 uv, float radius);
float EngineRing(vec2 uv, float radius, float thickness);
vec2  EngineRotate(vec2 p, float deg);
float EngineNoise(vec2 p);
float EngineFbm(vec2 p, int octaves);
vec2  EngineSwirl(vec2 uv, vec2 center, float strength, float radius);
vec3  EnginePalette(float t, vec3 a, vec3 b, vec3 c, vec3 d); // Palette nach Inigo Quilez
vec3  EngineRainbow(float t);
float EnginePulse(float freq); // 0..1, sinusförmig aus u_Time
```

Beispiele in `assets/shaders/`: `effect_rounded`, `effect_glow`, `effect_rainbow`,
`effect_plasma`, `example_wobble` (Paar aus vert+frag), `black_hole` (aus den Demos).
Die Schaltfläche **Reload** im Inspector baut den Shader- und Textur-Cache neu auf.

## 7. Ton

Komponente Audio Source: Pfad, Lautstärke (0..2), Tonhöhe (0.1..3), Loop, Play On Awake,
Preview/Stop. Ressourcen — `assets/audio/*.{wav,mp3,ogg,flac}` (miniaudio).
Mixer: Edit ▸ Settings — Master-Lautstärke und Stummschaltung. Gerätefehler — ebenda.

## 8. Physik

Fester Zeitschritt mit 60 Hz. Schwerkraft standardmäßig (0, -9.81) m/s², 1 m = 100 px —
in Edit ▸ Settings einstellbar. Kollider: Box/Kreis, Trigger (Ereignisse) und fest
(Kontaktauflösung). Bewegung/Drehung/Skalierung des Elternobjekts wirkt auf die Kollider der Kinder.
Ereignisse erreichen die Skripte als `OnTriggerEnter/Exit`, `OnCollisionEnter`.

## 9. Prefabs

Rechtsklick auf eine Entität (oder das Symbol im Inspector) ▸ Save as Prefab → `assets/prefabs/`.
Ablegen eines Prefabs in Hierarchie/Szene — eine Instanz; Revert To Prefab — die Änderungen
der Instanz zurücknehmen. Duplizieren (`Ctrl+D`) und Serialisierung wirken auf den Teilbaum.

## 10. Einstellungen der Engine

**Edit ▸ Settings**: Schwerkraft (m/s²), Zeitfaktor (time scale), Master-Lautstärke, Stummschaltung.
Der Zeitfaktor wird beim Eintritt in Play angewendet.

## 11. Beispiele

- `examples/black_hole/` — ein schwarzes Loch: Shader der Akkretionsscheibe (nur `.frag`),
  Skript-Orbit mit `LookAt`, Szene mit einem Mond als Kind. Bereits nach
  `assets/scenes/black_hole.scene` kopiert — File ▸ Open Scene und Play.
- `assets/scripts/rotate.cpp`, `player.cpp` — endlose Drehung und WASD-Charakter.

## 12. Eingebauter Code-Editor (IDE-lite)

- Doppelklick auf `.cpp`/`.h`/`.frag`/`.vert`/`.txt`/`.json` im Project-Panel öffnet die
  eingebaute Editorfenster; auch **Rechtsklick ▸ Edit (built-in IDE)**.
- **View ▸ Script Editor** — Fenster ein-/ausblenden.
- **Strg+S** im Fenster speichert die Datei (während es den Fokus hat, werden die
  Szenen-Hotkeys umgangen).
- **Strg+C / Strg+V / Strg+X / Strg+Z** funktionieren nativ (ImGui InputTextMultiline + GLFW-Clipboard).
- **Open Externally** — Datei mit der Standardanwendung öffnen (`xdg-open`).

## 13. Import aus anderen Quellen

- **Project ▸ Create ▸ Import File...** — Dateibrowser auf der Platte; die gewählte Datei
  wird je nach Erweiterung in den passenden `assets/`-Ordner kopiert
  (textures/audio/scenes/scripts/shaders/prefabs), der Rest nach `assets/imported`.
  Drag&Drop funktioniert weiterhin.

## 14. Konsole

- Tab **Console** neben Assets; der Fehlerzähler erscheint im Tab-Namen.
- **Follow** — automatisches Scrollen nach unten nur, wenn man bereits unten ist (abschaltbar).
- **Copy All** kopiert das komplette Log; ein Klick auf eine Zeile kopiert diese Zeile.

## 15. Runtime-UI (Game-GUI)

- Widgets: Button/Text/Slider/Checkbox/Progress Bar.
- `transform.position/scale` sind **Welteinheiten** (Gizmo in der Scene und Buttonsitzung stimmen überein).
- Stil pro Element: `UITextColor`, `UIBgColor`, `UIFontScale` (0/1/2 = normal/mittel/groß).
- Im Edit-Modus sind Elemente sowohl im Game-View als auch als Vorschau-Rechtecke direkt in der Scene sichtbar.

## 16. Build Game und Player

- **File ▸ Build Game...** erzeugt einen Standalone-Ordner: `astra` (Kopie der aktuellen Binary),
  `assets/` (Szenen/Shader/Texturen/Audio/Skripte), `build-scripts/*.so` —
  **vorkompilierte Skripte** (kein g++ auf dem Zielrechner nötig) und `game.json`
  mit der Startszene `{"scene": "assets/scenes/..."}`.
- Start: `cd <Ordner> && ./astra --play` (oder `Astra --play --scene ... --project ...`).
- Im Player beendet ESC das Fenster; es gibt nur das Spiel, keinen Editor.

## 17. Einschränkungen der aktuellen Version

- Ein Projekt/eine Szene im Speicher; keine additiven Szenen.
- Bei Rigidbody-Kindern wird keine Physik simuliert (sie werden vom Elternobjekt bewegt).
- Keine Animationen und kein Kachel-Rendering.
