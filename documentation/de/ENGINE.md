# Astra — Engine-Benutzerhandbuch

Astra ist eine 2D-Spiel-Engine mit einem Editor im Unity-Stil: C++17, OpenGL 4.6, GLFW, ImGui (Dockspace).
Editor, Szenen, Skripte, Shader, Audio und der Spiel-Build stecken in einer einzigen Binary — ohne externe Runtimes.

Inhaltsverzeichnis: [Build](#1-build-und-start) · [Projekte](#2-projekte) · [Oberfläche](#3-editor-oberfläche) ·
[Entities](#4-entities-und-komponenten) · [Szenen](#5-szenen) · [Assets](#6-assets-und-import) ·
[UI](#7-spieloberfläche) · [Undo](#8-undo--redo) · [Tastenkürzel](#9-tastenkürzel) ·
[Einstellungen](#10-engine-einstellungen) · [Build](#11-spiel-bauen-und-player) · [Konsole](#12-konsole)

---

## 1. Build und Start

Nur Linux. Erforderlich: `cmake` (>= 3.16), `g++` (C++17), Mesa/`libgl-dev`, die X11-Abhängigkeiten von GLFW.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/Astra                 # Editor
./build/Astra --play          # Player (Spiel ohne Editor)
```

Alle Abhängigkeiten (glfw, glm, glad, imgui, stb, miniaudio) holt beim ersten Konfigurieren FetchContent.
`g++` auf dem System ist Pflicht: damit werden die Spielskripte kompiliert (siehe SCRIPT_API.md).

## 2. Projekte

- **File ▸ New Project...** — Name + Ordner; aus `templates/default_project` entsteht die Struktur
  `assets/{scenes,shaders,scripts,audio,textures,prefabs}` + `project.json`.
- **File ▸ Open Project... / Projects Manager / Open Recent** — zwischen Projekten wechseln.
  Beim Öffnen eines Projekts wechselt die Arbeitsdirectory der Engine: alle Asset-Pfade sind relativ zum Projektstamm.
- **File ▸ Exit** — beenden.

## 3. Editor-Oberfläche

Ein Dockspace: Fenster lassen sich an der Titelleiste ausgliedern, zu Tabs zusammenkleben, die Größe ändert man an den Rändern.

| Fenster | Was es tut |
|---|---|
| **Scene** | Bearbeitung: Gizmo, Auswahl, Drag&Drop, Vorschau von Kollidern und UI |
| **Game** | Ansicht der Spielkamera; hier sitzen die UI-Buttons/-Slider (und ihre Vorschau im Edit-Modus) |
| **Script** | integrierte IDE: Doppelklick auf `.cpp/.frag/...` im Project-Fenster öffnet die Datei hier |
| **Hierarchy** | Entity-Baum; Drag = Reparenting; Ctrl+↑/↓ — Geschwister umsortieren |
| **Inspector** | Komponenten der gewählten Entity + Script Variables + Material |
| **Project** | `assets/`-Browser (Raster/Liste, Suche, Create, Import) und Reiter **Console** |

Symbolleiste: **Play / Pause / Stop / Restart**, Status `PLAYING/PAUSED`, Zähler der Skriptfehler
(roter Button — Klick öffnet die Console), Werkzeuge **Move (W) / Rotate (E) / Scale (R) / Hand (Q)**.

Navigation in der Scene: Rechts-/Mittelklick — pannen, Mausrad — Zoom zum Cursor, F — Fokus auf die Auswahl,
Pfeiltasten — pannen, solange die Scene den Fokus hat. ESC — zurück von Play nach Edit.

**Kontextmenüs (Rechtsklick)**: In der Scene öffnet ein Klick ohne Ziehen das Menü »Create Empty/Quad here,
Paste here, Deselect, Focus selection«; in der Hierarchy auf einer Zeile (Rename, Duplicate, **Move Up/Down**
zum Umsortieren der Geschwister, Detach, Save as Prefab, Revert, Delete) und auf leerer Fläche (Create Empty,
Paste, Deselect); im Project auf leerer Fläche (Create Folder/Shader/Script, Import File...).

**Script Variables**: Die Engine liest `DefineVar("name", default)` direkt aus dem Skriptquelltext —
die Regler erscheinen sofort im Inspector, also auch im Edit-Modus und vor jedem Play (wie [SerializeField]).

## 4. Entities und Komponenten

Eine Entity = ein Satz fest vorgegebener Komponenten (im Moment so; ein reines ECS steht auf der Roadmap):

- **Transform** — Position/Rotation/Scale. Es gibt `Parent`: Kinder übernehmen die Pose des Elternobjekts
  (Weltraum-Matrizen wie in Unity, einschließlich Skalierung/Drehung der Kette).
- **Sprite** — Type (None/Quad/Circle), Color, Texture Path, **Sorting Order** (ein kleinerer Wert
  wird früher/unter den anderen gezeichnet, wie in Unity), Custom Shader (siehe SHADER_API.md).
- **Animation** — Sprite Sheet: `Cols × Rows` (row 0 = oberste Zeile), FPS, Loop, Play On Awake,
  `Active` (im Edit-Modus laufen die Frames als Vorschau). Eigene Sheet Path oder »Use Sprite«.
  Die Tabelle **Clips** — Einträge mit Name, first..last, fps, loop und Play/X-Buttons; ohne Clips
  läuft das ganze Gitter in Schleife. Aus Skripten: `PlayAnimation/StopAnimation/IsAnimating`,
  `PlayClip(e, "run")`.
- **Rigidbody** — Kinematic, Velocity, Mass, Drag, Use Gravity. Physik: fix 60 Hz,
  MTV-Auflösung, Trigger/Kollisionen mit Skriptereignissen.
- **Collider** — Box (halbe Größe) / Circle (Radius), Is Trigger. Visualisierung in der Scene.
- **Camera** — Main Camera (eine aktive), Zoom, Viewport Offset. Der Game-View blickt durch sie;
  UI-Elemente werden in Weltkoordinaten dieser Ansicht positioniert.
- **UI Element** — siehe Abschnitt 7.
- **Audio Source** — Clip Path, Volume, Pitch, Loop, Play On Awake, Buttons Preview/Stop.
- **Tilemap** — ein Kachelraster aus einem Atlas: Atlas Path, Tile Size, Atlas Cols, Grid W×H, Tint,
  Sorting Order (standardmäßig unter den Sprites). Der Pick-Tile-Popup zeigt den Atlas als Raster;
  die aktuelle Kachel setzt das Werkzeug **Tile (T)**: LMB malt die gewählte Kachel ins Raster der
  gewählten Entity, Shift+LMB löscht. `transform.position` der Entity = linke obere Ecke des Rasters.
  Für den schnellen Start gibt es Fill floor/Clear.
- **Particle Emitter** — Textur (oder Quadrat), max/rate, life/speed/angle min-max, gravity,
  size start/end, color start/end (Alpha blendet aus), Loop, Play On Awake, Button Burst.
  Aus Skripten: `EmitParticles(Owner(), 30)`.
- **Script** — C++-Skript (SCRIPT_API.md). **Script Variables** — `DefineVar("speed", 120)`
  in `Start()`, der Regler erscheint im Inspector, der Wert wird in die Szene serialisiert.

GameObject ▸ Create Empty/Quad/Circle/Camera/UI — Schnell-Presets.

**Komponenten werden wie in Unity hinzugefügt**: eine neue Entity hat nur Transform+Sprite;
Rigidbody/Collider/Audio/Script/Particle Emitter/Tilemap/UI/Camera/Animation werden per
**+ Add Component** unten im Inspector hinzugefügt und mit dem **x**-Button im Header einer Sektion entfernt.
Die Flags »Komponente vorhanden« werden mit serialisiert. Alte Scene-Dateien (vor v0.10) laden wie zuvor — dort sind
alle Komponenten »aktiviert« (legacy). Außerdem sind Inspector und Hierarchy jetzt kompakter proportioniert (20%/17%).

## 5. Szenen

- Textformat: `Astra Scene v2` (eine Entity = Block `ENTITY ... END_ENTITY`).
- Ctrl+N / Ctrl+S / Ctrl+Shift+S — neu/speichern/speichern unter; Sternchen in der Titelleiste =
  ungespeicherte Änderungen; Rückfrage beim Schließen einer ungespeicherten Szene.
- Doppelklick auf `.scene` im Project-Fenster — öffnen (mit Rückfrage, wenn die aktuelle Szene geändert ist).
- **Mehrere Szenen im Spiel**: `LoadScene("assets/scenes/level2.scene")` aus einem Skript —
  der Szenen-Manager wechselt mitten im Lauf das Level (funktioniert sowohl im Play-Modus als auch im gebauten Spiel).

## 6. Assets und Import

- Drag&Drop aus dem Dateimanager des Betriebssystems ins Fenster — Import nach Typ (Bilder → `assets/textures`,
  `.scene` → `assets/scenes`, `.cpp` → `assets/scripts`, Audio → `assets/audio`).
- **Project ▸ Create ▸ Import File...** — Dateiauswahl auf der Platte mit demselben Browser.
- **Rechtsklick ▸ Open Externally** — Datei im Systemeditor öffnen (`xdg-open`).
- **Rechtsklick ▸ Edit (built-in IDE)** — im Script-Fenster öffnen. Ctrl+S — speichern.
- Bilder/Shader/Prefabs kann man direkt auf ein Objekt in der Scene oder auf eine Zeile der Hierarchy ziehen —
  sie werden zugewiesen bzw. instanziiert.
- Create ▸ Shader (ein Paar `.vert+.frag`) oder ein einzelnes `.frag` — Vorlage mit API-Hinweisen.

## 7. Spieloberfläche

Entity + Komponente UI Element: **Button / Text / Slider / Checkbox / Progress Bar**.
Position und Größe liegen in **Weltkoordinaten** (das Gizmo in der Scene deckt sich mit dem Sitz des Buttons).
Stil: Text Color, Bg Color, Font (Default/Medium/Large). Die Checkbox ist ein echter Bool-Wert (0/1).
Im Edit-Modus sind die Elemente sowohl im Game-View sichtbar als auch als Rahmen in der Scene. Im Play-Modus
sind sie klickbar; das Skript liest `GameUI::WasClicked(id)` / `GameUI::GetValue(id)`.

## 8. Undo / Redo

- **Ctrl+Z** — ein Schritt zurück, **Ctrl+Shift+Z** (oder Ctrl+Y) — ein Schritt vor.
- Macht Bearbeitungen der Szene rückgängig: Gizmo-Verschiebungen, Änderungen im Inspector, Erzeugen/Löschen,
  Kopieren/Einfügen, Reparenting, Instanziierung von Prefabs. Ein »Stoß« zusammenhängender Bearbeitungen = ein Schritt.
- Tiefe — 60 Schritte. Beim Speichern der Szene wird der Bezugspunkt zurückgesetzt.

## 9. Tastenkürzel

| Tasten | Aktion |
|---|---|
| Ctrl+N / O / S / Shift+S | neue Szene / öffnen / speichern / speichern unter |
| Ctrl+P | Play/Stop | Ctrl+D | Unterbaum duplizieren |
| Ctrl+C / Ctrl+V | Unterbaum der Entity kopieren / einfügen |
| Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y | undo / redo |
| Del / Backspace | Entity löschen | F2 — umbenennen |
| W / E / R / Q | Move / Rotate / Scale / Hand |
| Ctrl+↑ / ↓ | Geschwister umsortieren | F — Kamerafokus auf die Auswahl |
| Ctrl (beim Drag halten) | Gizmo-Snap (50 px / 15°) |
| ESC | zurück von Play; im Player das Spiel schließen |
| Ctrl+S im Script-Fenster | Datei speichern (in der IDE funktionieren ^C/^V/^X/^Z) |

## 10. Engine-Einstellungen

**Edit ▸ Settings**:
- **Physics** — Gravitation (m/s², Unity-style 0,-9.81), Pixels per meter (Maßstab der Welt).
- **Render** — Hintergrundfarbe von Szene/Spiel, Anzeige von Grid und Kollidern in der Scene,
  Grid-/Snap-Weite (px).
- **Editor** — Snap-Weite der Rotation (°).
- **Time** — Time Scale (wird beim Wechsel in Play angewandt, überlebt Stop).
- **Audio** — Master Volume, Mute, Status des Geräts.

Die Dateibrowser (Browse/Import) sind gewöhnliche Fenster: Man kann parallel mit dem Rest der
Oberfläche arbeiten, ESC schließt sie.

## 11. Spiel bauen und Player

**File ▸ Build Game...** — drei Modi (wie in Godot/UE, die Engine wird in jedem Build nicht dupliziert):

1. **Launcher + Engine-Bibliothek (empfohlen)** — das Spiel wird als winzige exe (~18 KB) gebaut,
   die mit `libastra_engine.so` gelinkt ist. Die Option »Bibliothek daneben kopieren«
   (rpath `$ORIGIN`) hält den Ordner portabel; ohne sie kommt die Bibliothek aus dem Build-Ordner.
   Die Skripte sind zu `.so` vorkompiliert — g++ auf dem Zielrechner wird nicht gebraucht.
   Start: einfach `./spielname` (cwd = Ordner der exe).
2. **Eine ausführliche Datei** — an die Binary ist ein Bundle angehängt (Engine + assets + .so +
   game.json); beim ersten Start wird es neben der Datei nach `<name>.bundle/` ausgepackt.
   Eine Datei = Spiel.
3. **Ordner mit der astra-Binary** — vollständige Kopie: `astra` + `assets/` + `build-scripts/` +
   `game.json`, Start mit `./astra --play`.

CLI ohne GUI: `./Astra --build assets/scenes/x.scene --out ./game [--single|--folder]`
(Standard ist Modus 1). Player-Flags: `--play`, `--scene <path>`, `--project <dir>`. Im Spiel steht
die komplette Skript-API zur Verfügung, einschließlich `LoadScene` (Level) und `Log` (schreibt in die Konsole).

**Nur das Notwendige + Verschlüsselung**: in den Build wandern NICHT alle `assets/`, sondern nur die Dateien,
die die Szene tatsächlich benutzt (Texturen/Shader/Audio/Tilemap- & Particle-Atlase + die Prefabs rekursiv).
Alle werden verschlüsselt (`AENC`: ein XOR-Keystream aus splitmix64) und beim Lesen von der Engine transparent
entschlüsselt — im Spielordner liegen keine rohen Texturen/Szenen/Audios. Die `.so` der Skripte bleiben roh
(die lädt dlopen). Das ist Verschleierung »gegen neugierige Blicke«, keine kryptografische Absicherung
gegen Cracking.

## 12. Konsole

Reiter **Console** im Project-Panel: stdout/stderr der Engine, der Skripte und der Kompilierungsfehler.
**Follow** — automatisches Scrollen nur, wenn man sich ohnehin am Ende befindet; **Copy All** — das ganze Log in
die Zwischenablage; Klick auf eine Zeile — diese Zeile kopieren. Der Fehlerzähler sitzt in der Symbolleiste (roter Button).

## 13. Beispiele

- `assets/scenes/black_hole.scene` — Shader-Welt (Akkretionsscheibe, Umlaufbahn per Skript, Mond als Kind-Entity).
- `assets/scenes/animation_demo.scene` — Sprite-Sheet-Ball 4×2 + Live-Material-Parameter.
- `assets/scripts/rotate.cpp`, `player.cpp`, `examples/black_hole/orbit_planet.cpp`.
- `examples/` — Quelltexte der Beispiele; `templates/default_project` — Projektvorlage.
