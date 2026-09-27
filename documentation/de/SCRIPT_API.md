# Astra — API für Spielskripte (C++)

Skripte sind ganz normale C++-Klassen. Die Engine kompiliert sie mit `g++ -std=c++17 -shared -fPIC` nach
`build-scripts/<name>.so`, wenn du in den Play-Modus gehst (und beim Bauen des Spiels), und lädt sie über
`dlopen`. Hot-Reload: eine geänderte `.cpp` wird neu kompiliert, die Instanzen neu angelegt.
**Includes musst du nicht schreiben** — die Engine erzeugt eine Wrapper-Übersetzungseinheit, die
`ScriptAPI.h`, `SceneManager.h`, `Transforms.h`, `Physics.h`, `Input.h`, `GameUI.h`,
`Audio.h`, GLFW, glm und `<cmath>/<string>/<vector>/<algorithm>/<random>` von selbst einbindet.

## 1. Minimales Skript

```cpp
class Rotate : public Script {
public:
    void Update(float dt) override {
        Entity* e = Owner();
        if (e) e->transform.rotation += 90.0f * dt;
    }
};
SCRIPT_ENTRY(Rotate)   // genau eine Fabrik pro Datei
```

Zugewiesen wird im Inspector ▸ Script (Combo über `assets/scripts/*.cpp`). Kompilierungsfehler —
in der Console und als roter Button in der Symbolleiste.

## 2. Die Klasse Script

| Hook | Wann er aufgerufen wird |
|---|---|
| `Start()` | beim Anlegen der Instanz (Wechsel in Play, Spawn, LoadScene) |
| `Update(float dt)` | jeden Frame (dt ist bereits mit timeScale multipliziert) |
| `OnDestroy()` | vor dem Vernichten der Instanz (Stop/StopAnimation-Fälle) |
| `OnTriggerEnter(uint32_t otherId)` / `OnTriggerExit` | Betreten/Verlassen eines Triggers |
| `OnCollisionEnter(uint32_t otherId)` | Kollision ohne Trigger |
| `OnAnimEvent(const char* name)` | Animationsframe überquerte eine Marke aus dem Inspector (Events) |

Methoden (protected):

```cpp
Entity* Owner();                 // tragende Entity (kann nullptr werden — immer prüfen)
::SceneManager* Scene();         // Zugriff auf die Entity-Liste und die Auswahl

// Variablen als »serialisierte Felder« (Analog zu [SerializeField]):
void  DefineVar(const char* name, float defaultValue); // in Start(); Vorhandenes bleibt unangetastet
// Die Engine liest DefineVar(...) auch aus dem Quelltext — die Regler erscheinen sofort
// im Edit-Modus, vor jedem Play.
float GetVar(const char* name, float fallback = 0) const;
void  SetVar(const char* name, float value);
// Jede Variable erscheint als Regler in Inspector ▸ Script Variables,
// der Wert liegt in der Szene (wird in .scene/.prefab gespeichert).

// Pose:
void    Translate(const glm::vec2& localDelta); // lokale Koordinaten (inkl. Drehung des Elternobjekts)
void    SetWorldPosition(const glm::vec2& world);
glm::vec2 WorldPosition() const;
float   AngleTo(const glm::vec2& worldPoint) const; // Grad
void    LookAt(const glm::vec2& worldPoint);        // dreht »oben« auf den Punkt zu
```

## 3. Globale API-Funktionen

```cpp
// Entity-Steuerung
bool DestroyEntity(uint32_t id);          // Entity löschen (die Skript-Instanzen werden abgehängt)
void LoadScene(const std::string& path);  // Szenen-Manager: nächstes Level
void Log(const std::string& message);     // Zeile ins Console-Panel

// Zeit
Time::Delta(); Time::UnscaledDelta(); Time::SinceStart();
Time::TimeScale(); Time::SetTimeScale(float);   // 0 pausiert das ganze Spiel

// Physik/Mathematik
AddForce(Entity* e, glm::vec2 impulse);   // +velocity/Masse
Lerp(a,b,t); Clamp(v,lo,hi); Radians(deg); Degrees(rad);
RandomRange(0.f,1.f); RandomInt(1,6);
Physics::Gravity;                          // glm::vec2, Welt-px/s²

// Eingabe (das Skript hat seinen eigenen Input-Singleton, denselben wie die Engine)
Input::Get().IsKeyPressed(GLFW_KEY_SPACE); // GLFW-Codes
Input::Get().IsActionHeld("jump");         // benannte Bindings: BindKey in Start()
Input::Get().GetAxis("move");              // -1..1
Input::Get().mousePosition(); mouseDelta(); // usw.

// Audio
Audio::PlayOneShot("assets/audio/hit.wav", 1.0f, 1.0f); // -> uint32 voiceId
Audio::PlayOneShot(path, vol, pitch, group); // 4. Argument group: 0=SFX, 1=Music
Audio::PlayLooped(path, vol, pitch, group); Audio::Stop(voiceId); Audio::StopAll();
Audio::SetVolume(voiceId, v); Audio::SetPitch(voiceId, p);
Audio::SetMasterVolume(0..1); Audio::SetMuted(bool);
Audio::SetGroupVolume(group, 0..1); Audio::GroupVolume(group); // 0=SFX, 1=Music

// Spiel-UI (Buttons/Slider sind Entities mit der Komponente UI Element)
GameUI::WasClicked(entityId);              // true für einen Frame nach dem Klick
GameUI::GetValue(entityId);                // float: Slider im 0..1-Bereich, Checkbox 0/1

// Sprite-Animation und Clips
PlayAnimation(Owner(), /*fromStart=*/true); StopAnimation(Owner()); IsAnimating(Owner());
bool ok = PlayClip(Owner(), "run");            // ein Clip aus der Clips-Tabelle im Inspector

// Partikel: sofortiger Schwall aus dem Emitter der Entity (Explosion/Funken)
EmitParticles(Owner(), 30);

// Screenshake der Spiel-Kamera (Game Feel): Amplitude in Welteinheiten, Dauer in Sekunden
ShakeCamera(15.0f, 0.3f);

// Spiel verlassen: im Player schließt es das Fenster, im Editor stoppt es Play
QuitGame();                       // Exit-Button: GameUI::WasClicked(exitId) -> QuitGame()
// Mauserfassung (Shooter/Strategie): der System-Cursor verschwindet, die Maus-Ereignisse bleiben
CaptureMouse(true); if (IsMouseCaptured()) { ... }
// Prefab spawnen (Kugeln, Gegner): gibt die id der Instanz-Wurzel zurück (0 — Fehler)
uint32_t bullet = InstantiatePrefab("assets/prefabs/bullet.prefab", WorldPosition());
```

## 4. Zugriff auf die Szene

`Scene()` gibt `::SceneManager*` zurück (globaler Geltungsbereich, ohne Namespaces):

```cpp
for (Entity& e : Scene()->GetEntities()) { ... }
Entity* sel = Scene()->GetSelectedEntityPtr();
int idx = Scene()->IndexOf(someId);
Scene()->RemoveEntityById(someId);   // dasselbe wie DestroyEntity
```

## 5. Beispiel: Figur mit serialisierten Einstellungen

```cpp
class SpaceBody : public Script {
public:
    void Start() override {
        DefineVar("orbitRadius", 430.0f);
        DefineVar("speed", 60.0f);      // Grad/s — der Regler erscheint im Inspector
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
        DestroyEntity(otherId);        // zum Beispiel verschlingen wir den Stern
    }
private:
    float angle = 0.0f;
};
SCRIPT_ENTRY(SpaceBody)
```

## 6. Wie das unter der Haube funktioniert

- Die `.cpp`-Datei → Wrapper `build-scripts/<stem>_gen.cpp` (Auto-Includes + `#include "<absoluter Pfad>"`).
- Kompilierung wird nach Zeitstempel gecacht: ist die `.so` neuer als der Quelltext, startet g++ nicht.
- `dlopen(RTLD_NOW)`: die Symbole der Engine sind für das Skript dank `-rdynamic` sichtbar; Einstiegspunkt ist
  `extern "C" CreateGameScript` (Makro `SCRIPT_ENTRY`).
- `SyncInstances` einmal pro Frame: neue Träger des Skripts → `new + Start()`, gelöschte/andere Pfade →
  `OnDestroy + delete`. Kompilierungsfehler werfen das Spiel nicht raus — sie sind in der Console sichtbar.
- Im gebauten Spiel wird nichts kompiliert: es werden fertige `.so` aus `build-scripts/` verwendet.

## 7. 3D: Pose, Physik, Raycasts

In einer 3D-Szene lebt die Entity in `pos3 / rot3 (Grad, X→Y→Z) / scale3` — die Methoden unten
arbeiten genau damit. Die Welteinheiten sind dieselben wie im 2D: 100 Einheiten = 1 Meter
(umgestellt in Settings ▸ Physics ▸ Pixels per meter), deshalb liefert `Gravity3D()` per Default
etwa `(0, -981, 0)`.

Methoden von `Script` (protected, in der abgeleiteten Klasse):

| Methode | Was sie tut |
|---|---|
| `bool Is3D() const` | ob die Entity dreiachsig ist (sonst bewegen die 3D-Methoden nichts) |
| `glm::vec3 Position3D() const` | Weltposition inklusive der Parent-Kette |
| `void SetPosition3D(const glm::vec3&)` | in die Welt setzen (rechnet zurück auf die lokale Position) |
| `void Translate3D(const glm::vec3&)` | `pos3` verschieben (lokal, ohne Parent) |
| `glm::vec3 Rotation3D() const` / `SetRotation3D(const glm::vec3&)` | Winkel in Grad |
| `void SetScale3D(const glm::vec3&)` | Maßstab (gleicht dabei auch den 2D-`scale` an) |
| `glm::vec3 Velocity3D() const` / `SetVelocity3D(const glm::vec3&)` | Geschwindigkeit des Rigidbody (3D); der Setter erzeugt die Komponente, falls sie fehlt |
| `void AddForce3D(const glm::vec3&)` | Impuls: `velocity += impulse / mass` |
| `void SetGravityEnabled3D(bool)` | Schwerkraft ein-/ausschalten (erzeugt ebenfalls den Rigidbody) |
| `void LookAt3D(const glm::vec3&)` | das Objekt mit `-Z` auf das Ziel drehen (wie `transform.LookAt` in Unity) |
| `void AddForceTo3D(Entity*, const glm::vec3&)` / `void SetVelocityOf3D(Entity*, const glm::vec3&)` | dasselbe für eine **fremde** Entity |

Globale Funktionen:

```cpp
struct RayHit3D { uint32_t entityId; std::string name; glm::vec3 point, normal; float distance; };
bool Raycast3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D& out);
int  RaycastAll3D(const glm::vec3& origin, const glm::vec3& dir, float maxDist, RayHit3D* out, int max);
glm::vec3 Gravity3D();
uint32_t InstantiatePrefab3D(const std::string& prefabPath, const glm::vec3& pos);
inline void AddForce3D(Entity* e, const glm::vec3& impulse);   // für fremde Entities
inline void SetVelocity3D(Entity* e, const glm::vec3& v);
inline glm::vec3 MoveTowards3D(const glm::vec3& from, const glm::vec3& to, float maxDelta);
```

`Raycast3D` geht über die 3D-Entities: wer einen **Collider (3D)** hat, liefert dessen Ausdehnung
(mit Maßstab und Drehung), wer keinen hat, eine Box um das Mesh (`0.5 * scale3`). Zurückkommen
das nächstgelegene Ziel, die Normale der Eintrittsfläche und die Distanz; `dir` musst du nicht
normalisieren.

Physik: `Rigidbody (3D)` und `Collider (3D)` fügt man im Inspector hinzu (`+ Add Component`,
nur bei 3D-Entities) oder per Skript (`SetGravityEnabled3D`, `AddForce3D`). Die Ereignisse sind
dieselben wie im 2D: `OnTriggerEnter/Exit(otherId)`, `OnCollisionEnter(otherId)` — aus `otherId`
wird die `Entity*` über `Scene()` oder `FindById`.

Beispiel — ein springender Würfel (`assets/scripts/bounce3d.cpp`):

```cpp
class Bounce3D : public Script {
public:
    void Start() override {
        DefineVar("kick", 900.0f);
        SetGravityEnabled3D(true);
    }
    void Update(float dt) override {
        RayHit3D hit;
        bool grounded = Raycast3D(Position3D(), glm::vec3(0, -1, 0), 70.0f, hit);
        if (grounded && Velocity3D().y <= 1.0f)
            SetVelocity3D(glm::vec3(Velocity3D().x, GetVar("kick"), Velocity3D().z));
        glm::vec3 r = Rotation3D(); r.y += dt * 20.0f; SetRotation3D(r);
    }
};
SCRIPT_ENTRY(Bounce3D)
```

## 8. Häufige Stolperfallen

- `Owner()` kann `nullptr` werden (die Entity wurde gelöscht) — in jeder Methode prüfen.
- `DestroyEntity` innerhalb von `Update` ist sicher: die Instanz überlebt den Frame und stirbt bei SyncInstances.
- Wechsel von `scriptPath` oder der `.cpp` zur Laufzeit legt die Instanz neu an (`Start()` läuft erneut).
- Die Physik von Kind-Rigidbodies wird nicht simuliert — sie werden vom Elternobjekt bewegt.
- `timeScale=0` + `UnscaledDelta()` — der einzige Weg, im pausierten Zustand noch etwas zu tun.
- Innerhalb der `Script`-Methoden überdeckt das gleichnamige Mitglied die globale Funktion:
  `AddForce3D(other, v)` kompiliert nicht — nimm `AddForceTo3D(other, v)` oder `::AddForce3D(other, v)`.
- Eine 3D-Entity ist in einer 2D-Szene nicht im Scene-View sichtbar (und umgekehrt): der Modus ist
  eine Eigenschaft der Szene, `View ▸ 3D Scene`.
- `Translate3D` berücksichtigt die Drehung des Elternobjekts nicht — für »lokale« Bewegung selbst umrechnen.
- `Raycast3D` ohne `Collider (3D)` nutzt die Ausdehnung des Meshes: bei einem gedrehten Modell ist sie
  breiter als das eigentliche Objekt.
