# Astra — Engine-Benutzerhandbuch

Astra ist eine 2D/3D-Spiel-Engine mit einem Editor im Unity-Stil: C++17, OpenGL 4.6, GLFW, ImGui (Dockspace).
Editor, Szenen, Skripte, Shader, Audio und der Spiel-Build stecken in einer einzigen Binary — ohne externe Runtimes.

Inhaltsverzeichnis: [Build](#1-build-und-start) · [Projekte](#2-projekte) · [Oberfläche](#3-editor-oberfläche) ·
[Entities](#4-entities-und-komponenten) · [Szenen](#5-szenen) · [Assets](#6-assets-und-import) ·
[UI](#7-spieloberfläche) · [Undo](#8-undo--redo) · [Tastenkürzel](#9-tastenkürzel) ·
[Einstellungen](#10-engine-einstellungen) · [Build](#11-spiel-bauen-und-player) · [Konsole](#12-konsole) ·
[3D](#14-3d-szenen)

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
  Das Häkchen **3D Object** stellt die Entity auf den dreiachsigen Modus um: Position 3 / Rotation 3 (°) /
  Scale 3 + **Mesh** statt eines Sprites (siehe Abschnitt 14).
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
  **Follow Target** — die Kamera folgt sanft der gewählten Entity (Damping = Sekunden,
  Offset = Zielversatz). **Level Bounds** — ein Rechteck des Levels: die Kameramitte bleibt darin
  (unter Berücksichtigung des Zooms). Screenshake aus Skripten: `ShakeCamera(15.0f, 0.3f)`.
  **Perspective (3D)** + **Field of View** — perspektive Projektion statt der orthografischen (Abschnitt 14).
- **UI Element** — siehe Abschnitt 7.
- **Animations-Ereignisse**: Animation > Events — Marken (Clip: beliebig/bestimmter, Frame, Name).
  Wenn der Frame eine Marke überquert, erhält das Träger-Skript `OnAnimEvent("step")` —
  Schritte/Schüsse/Treffer exakt auf den Frames.
- **Audio Source** — Clip Path, Volume, Pitch, Loop, Play On Awake, **Group** (SFX/Music),
  Buttons Preview/Stop. Die Gruppenlautstärken sitzen in Edit ▸ Settings ▸ Audio.
- **Tilemap** — ein Kachelraster aus einem Atlas: Atlas Path, Tile Size, Atlas Cols, Grid W×H, Tint,
  Sorting Order (standardmäßig unter den Sprites). Der Pick-Tile-Popup zeigt den Atlas als Raster;
  die aktuelle Kachel setzt das Werkzeug **Tile (T)**: LMB malt die gewählte Kachel ins Raster der
  gewählten Entity, Shift+LMB löscht. `transform.position` der Entity = linke obere Ecke des Rasters.
  Für den schnellen Start gibt es Fill floor/Clear.
  **Solid (physics)** — nicht leere Zellen werden zu statischen AABB-Kollidern (Boden/Wände
  für einen Plattformler; ein Körper landet korrekt darauf und die Geschwindigkeit entlang der
  Einschlagachse wird nullgesetzt).
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
- **Editor** — Snap-Weite der Rotation (°), UI-Thema **Light theme** (helles/dunkles UI zur Laufzeit umschaltbar).
- **Time** — Time Scale (wird beim Wechsel in Play angewandt, überlebt Stop).
- **Audio** — Master Volume, Mute, Status des Geräts.
- **Lighting (3D)** — Richtung der Sonne (Light Dir, drei Zahlen — ein Vektor, von der Engine normalisiert),
  Sonnenfarbe (Light Color) und Aufhelllicht (Ambient). Gelten für alle Mesh-Objekte (Abschnitt 14).

Die Dateibrowser (Browse/Import) sind gewöhnliche Fenster: Man kann parallel mit dem Rest der
Oberfläche arbeiten, ESC schließt sie. Alle Einstellungen werden in `~/.astra/config.ini` gespeichert
und beim Start wiederhergestellt (Thema, Hintergrund, Grid, Snap, px/m, Lautstärke, Time Scale,
das Licht der 3D-Szene). Die Oberfläche ist weich und abgerundet, in zwei Themen: das dunkle
»Astra Slate« und das helle »Astra Paper«. Die Datei-Icons (Ordner/Bild/Skript/Shader/Szene/Prefab/Audio)
sind als Vektoren im
Engine-Code gezeichnet — gar keine externen Assets.

## 11. Spiel bauen und Player

**File ▸ Build Settings...** (**Ctrl+Shift+B**) — ein andockbares Fenster im Unity-Stil:

- **Product name** — der Name der Spiel-exe;
- **Scenes Included** — alle Szenen des Projekts: das Häkchen entscheidet, welche Szenen in den Build
  gehen, der Radio-Button wählt die Startszene (die das gebaute Spiel beim Start lädt);
- Ausgabe-**Folder** (+ Browse...) — wohin der Build geschrieben wird;
- **Encrypt used assets** — `AENC`-Verschlüsselung aller Assets, die der Build mitnimmt;
- **Copy engine library** — `libastra_engine.so` neben die exe kopieren.

**Build** erzeugt immer genau eine Art Release: einen winzigen Launcher (~18 KB), der mit
`libastra_engine.so` gelinkt ist und nur die von den Szenen wirklich genutzten (verschlüsselten) Assets
sowie die vorkompilierten `.so`-Skripte enthält — g++ auf dem Zielrechner wird nicht gebraucht. Mit
»Copy engine library« liegt die Bibliothek direkt neben der exe, und der rpath `$ORIGIN` hält den Ordner
portabel; ohne sie kommt die Bibliothek aus dem Build-Ordner. Start: einfach `./spielname`
(cwd = Ordner der exe).

Die Varianten »eine ausführliche Datei« und »Ordner mit der astra-Binary« gibt es jetzt nur noch per
CLI: `./Astra --build assets/scenes/x.scene --out ./game [--single|--folder]` — `--single` hängt ein
Bundle (Engine + assets + .so + game.json) an die Binary, das sich beim ersten Start neben sich selbst
nach `<name>.bundle/` auspackt (eine Datei = Spiel); `--folder` legt die vollständige Kopie an: `astra`
+ `assets/` + `build-scripts/` + `game.json`, Start mit `./astra --play`. Player-Flags: `--play`,
`--scene <path>`, `--project <dir>`. Im Spiel steht die komplette Skript-API zur Verfügung,
einschließlich `LoadScene` (Level) und `Log` (schreibt in die Konsole).

**Nur das Notwendige + Verschlüsselung**: in den Build wandern NICHT alle `assets/`, sondern nur die Dateien,
die die Szene tatsächlich benutzt (Texturen/Shader/Audio/Tilemap- & Particle-Atlase + die Prefabs rekursiv).
Ist **Encrypt used assets** angeschaltet, werden alle verschlüsselt (`AENC`: ein XOR-Keystream aus
splitmix64) und beim Lesen von der Engine transparent entschlüsselt — im Spielordner liegen keine rohen
Texturen/Szenen/Audios. Die `.so` der Skripte bleiben roh (die lädt dlopen). Das ist Verschleierung
»gegen neugierige Blicke«, keine kryptografische Absicherung gegen Cracking.

## 12. Konsole

Reiter **Console** im Project-Panel: stdout/stderr der Engine, der Skripte und der Kompilierungsfehler.
**Follow** — automatisches Scrollen nur, wenn man sich ohnehin am Ende befindet; **Copy All** — das ganze Log in
die Zwischenablage; Klick auf eine Zeile — diese Zeile kopieren. Der Fehlerzähler sitzt in der Symbolleiste (roter Button).

## 13. Beispiele

- `assets/scenes/black_hole.scene` — Shader-Welt (Akkretionsscheibe, Umlaufbahn per Skript, Mond als Kind-Entity).
- `assets/scenes/animation_demo.scene` — Sprite-Sheet-Ball 4×2 + Live-Material-Parameter.
- `assets/scripts/rotate.cpp`, `player.cpp`, `examples/black_hole/orbit_planet.cpp`.
- `examples/` — Quelltexte der Beispiele; `templates/default_project` — Projektvorlage.

## 14. 3D-Szenen

3D ist eine **Eigenschaft der Szene**, keine »Zusatzschicht« über dem 2D: `View ▸ 3D Scene`
(in der Datei die Zeile `Scene3D: 1`). Eine 3D-Szene hat ihren eigenen Boden, ihre eigene
Kamera-Navigation und ihren eigenen Werkzeugkasten; flache Sprites, Tilemaps, Kollider, der Rahmen
der 2D-Kamera und die UI-Vorschau werden dort in der Scene nicht gezeichnet, und das Werkzeug Tile
ist ausgeblendet. Die 2D-Szenen sind völlig unverändert.

**Navigation in der 3D-Szene** (wie in Unity, nicht wie beim 2D-Pannen):
- **Rechtsklick + Mausbewegung** — umschauen (yaw/pitch, der Pitch ist auf ±89,5° begrenzt);
- **WASD + Q/E bei gehaltenem Rechtsklick** — fliegen: W/S vor und zurück auf der Horizontalen,
  A/D seitlich, E/Q hoch/runter, **Shift** — ×4 Tempo, **Ctrl** — ×0,25 (solange der Rechtsklick
  gehalten wird, schalten W/E/R/Q nicht die Werkzeuge um);
- **Mausrad** — ein Schub entlang der Blickrichtung (schneller, wenn man weit weg ist);
- **Mittelklick** (oder Linksklick mit dem Werkzeug **Hand**, Q) — Pannen in der Bildebene;
- **Linksklick** — ein Mesh-Objekt auswählen: von der Kamera geht ein Strahl gegen die AABB des
  Meshes, näher an der Kamera gewinnt; ein Klick auf freie Fläche hebt die Auswahl auf;
- **F** — auf die gewählte Entity zufliegen (die Distanz kommt aus ihrem Scale);
- **NUMPAD 1/2/3/4/5/7** — die Ansichten Front/Back/Right/Left/Top/Bottom, **6** — perspektivisch;
- **Kompass** oben rechts in der Scene: die Achsen X/Y/Z sind anklickbar (und die Mitte setzt auf
  Perspektive zurück).

**Gizmo 3D**: am gewählten 3D-Objekt werden Achsen (Move), Ringe (Rotate) oder Achsen mit Griffen
(Scale) gezeichnet — umgeschaltet mit den Werkzeugen **W / E / R** oder den Buttons der Symbolleiste.
- Move: an der Achse X/Y/Z ziehen — das Objekt bewegt sich nur längs dieser Achse; die Raute in der
  Mitte ziehen — freie Bewegung in der Bildebene;
- Rotate: einen Ring ziehen — Drehung um die zugehörige Achse, der Winkel wird in der Ringebene
  gemessen;
- Scale: das Quadrat am Ende einer Achse ziehen — der Scale nur dieser Achse; die Mitte ziehen —
  gleichmäßig.
- **Ctrl** — Snap: Positionen an `GridSize`, Winkel an `SnapDegrees`. Das Gizmo bleibt auf dem
  Bildschirm gleich groß; Griffe, die von der Kante her »verwischt« aussehen, fangen keinen Klick ab
  (es gewinnt der besser sichtbare).

**Boden und Grid**: Das Grid liegt in der XZ-Ebene (Schrittweite `GridSize`), folgt unter der Kamera
mit, und die Achsen X/Z/Y sind eingefärbt. Die Einheiten in der Welt sind dieselben wie im 2D: der
Würfel per Default misst 100×100×100.

**Erzeugen**: `GameObject ▸ Create 3D ▸ Cube / Plane / Sphere / OBJ Model` (wer in einer 2D-Szene
ein 3D-Objekt erzeugt, schaltet die Szene automatisch auf 3D um). Im Inspector hat jede Entity die
Checkbox **3D Object**: Position 3 / Rotation 3 (in Grad, Reihenfolge X→Y→Z) / Scale 3, **Mesh**
(Cube/Plane/Sphere/OBJ), `.obj`-Pfad, Mesh Texture, Mesh Color. `transform.position/scale` bleiben
bei einer 3D-Entity erhalten — sie werden für das 2D-Erbe gebraucht, die Position im 3D kommt aus
`pos3`.

**Licht** — lambertsch mit Schatten: `diffuse = max(dot(n, sun), 0) * shadow + Ambient`,
Sonnenrichtung/-farbe und Ambient werden in Edit ▸ Settings ▸ Lighting (3D) gesetzt und in
`~/.astra/config.ini` gespeichert. Die Textur wird mit Farbe und Licht multipliziert; ohne Textur
entsteht ein einfarbiges Material. Die Normalen der Primitive erzeugt die Engine selbst, die der
`.obj` stammen aus der Datei (`vn`) oder werden aus den Flächen neu berechnet.

**Schatten der Sonne**: Ein eigener Durchgang schreibt eine Tiefenkarte (depth map) aus einer
Ortho-Kamera, die an die Ausdehnung des 3D-Inhalts angepasst wird; danach holt der Mesh-Shader eine
3×3-PCF-Abtastung und vergleicht die Tiefe mit einer Verschiebung (der `bias` hängt vom Winkel der
Normalen zur Sonne ab — auf schrägen Flächen gibt es weniger »Streifen«). Die Einstellungen
**Shadows** (an/aus) und **Shadow map** 1024/2048/4096 sitzen ebenfalls in Lighting (3D) und
ebenfalls in `config.ini`. Die Schatten entstehen aus allen 3D-Meshes zusammen und fallen auf
alles, auch auf den Boden.

**Die Spielkamera in der 3D-Szene**: die Camera-Komponente hat **Perspective (3D)** und
**Field of View**; die Pose der Kamera kommt aus **Position 3 / Rotation 3** der Entity (das Auge
in `pos3`, die Blickrichtung aus der Rotation) — die Kamera lässt sich also wie ein gewöhnliches
3D-Objekt bewegen und drehen, per Skript oder Gizmo. Follow/Level Bounds sind 2D-Mechaniken, im
3D-Zweig sind sie nicht beteiligt. Der Depth-Buffer wird vor den Meshes gelöscht, deshalb
»scheinen« Würfel nicht mehr durcheinander hindurch.

**Serialisierung**: die Zeile `Scene3D: 0|1` im Dateikopf plus die Entity-Schlüssel `Is3D`, `Pos3`,
`Rot3`, `Scale3`, `MeshType` (0=Cube, 1=Plane, 2=Sphere, 3=OBJ), `MeshPath`, `MeshTex`, `MeshColor`,
`CamPersp`, `CamFov`. Alte Dateien laden unverändert — dort stehen `Is3D: 0` und `Scene3D: 0`.

**Spiel-Build**: Die `.obj` aus `MeshPath` und das Bild aus `MeshTex` rücken in die
Abhängigkeitsliste der Szene, sie werden also genauso kopiert/verschlüsselt wie Texturen und Sounds
(Abschnitt 11).

**Beispiel**: `assets/scenes/3d_demo.scene` — Boden, drei Würfel und eine Kugel unter einer
Perspektivkamera mit Schatten (per Doppelklick im Project-Fenster öffnen).

**Weiterer 3D-Plan**: 3D-Physik (Rigidbody/Collider für drei Achsen), Skelett-Animation, glTF statt
OBJ, Mesh Renderer als eigene Komponente getrennt vom Sprite, ein orthografischer Ansichtmodus,
Beleuchtung durch mehrere Lichtquellen.
