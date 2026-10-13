# Bibliothek lib3d – Referenz

`lib3d` ist eine kleine 3D-Zeichenbibliothek auf OpenGL 3.3 (GLFW). Sie
stellt Szenen-/Kamera-Bündel, Farb-, Mathe- und Mesh-Bausteine bereit und
sammelt alle Zeichenaufrufe eines Frames zu effizienten Batches. Das
Zusatzmodul `bodies` ergänzt Körper um Flächen/Normalen und
Schnittabfragen (Raycasting), zeichnet aber selbst nicht.

- Umbrella-Header der Zeichen-API: `#include "lib3d.hpp"`
- Zusatzmodul Körper/Kollision: `#include "bodies.hpp"`
- Bibliothek: `lib/lib3d.a` (siehe `docs/build.md`)

Namenskonvention (Port aus dem TypeScript-Original): Typen in
`PascalCase`, Funktionen/Methoden in `PascalCase`, Felder in `PascalCase`.

---

## 1. Grundgerüst eines Programms

```cpp
#include "lib3d.hpp"

#include <exception>
using std::exception;

int main() {
    Scene scene;                        // Kamera + Welt-Attribute (Defaults s.u.)

    auto draw = [&]() {
        BeginScene(scene);              // Background, Nebel, Kamera, Licht
        // ... hier zeichnen ...
    };

    try {
        Init(1200, 800);                // Fenster + GL-Kontext + Shader
    } catch (const exception& e) {
        return 1;
    }
    StartAnimation(draw);               // Animations-Schleife, ruft draw pro Frame
    return 0;
}
```

**Ablauf pro Frame:** `StartAnimation` setzt die Zeit-Uniform, ruft die
übergebene `draw`-Funktion auf und zeichnet danach automatisch alle im Frame
gesammelten Zeichenbefehle (`FlushBatch`). Man zeichnet also *nur* über die
`Draw*`-Funktionen; das eigentliche Rendern/Austauschen der Puffer
übernimmt die Bibliothek.

| Funktion | Bedeutung |
| --- | --- |
| `Init(int w, int h)` | Fenster, OpenGL-Kontext, Shader, Batch-Zustand. Wirft `runtime_error`. |
| `StartAnimation(const function<void()>& draw)` | Startet die Schleife; `draw` wird jedes Frame aufgerufen. |
| `NoLoop()` | Beendet die Animations-Schleife. |
| `GetWidth()` / `GetHeight()` | Fenstergröße in Pixeln. |
| `IsMouseDown()` | `true`, solange linke Maustaste gedrückt. |
| `IsMouseUp()` | einmalig `true` beim Loslassen. |

---

## 2. Koordinatensystem, Kamera und Szene

**Koordinatensystem:** `+Y` zeigt nach oben, die Kamera blickt in `+Z`.
View- und Projection-Matrix laufen im Vertex-Shader; die Modell-Matrix wird
pro Zeichenaufruf als Argument übergeben (kein globaler Modell-Zustand).

```cpp
struct Camera {
    Vec3   Pos{50, 100, 200};   // Kameraposition
    Vec3   Target{0, 0, 0};     // Blickpunkt
    Vec3   Up{0, 1, 0};         // Oben-Vektor
    double FovY = 1.2;          // vertikales Sichtfeld (Bogenmaß)
    double Near = 0.1;          // nahe Clipping-Ebene
    double Far  = 1000;         // ferne Clipping-Ebene
    Matrix4x4 View() const;                 // Look-At-Matrix
    Matrix4x4 Projection(double aspect) const; // Perspective
};

struct Scene {
    Camera     Cam;
    Vec3       SunDir{0.5, 1.0, 0.3};       // Lichtrichtung (Weltraum)
    Fog        Fog_;                        // Tiefen-Nebel
    ColorState Background{40/255.0, 40/255.0, 40/255.0, 1};
    bool       BatchSort = false;           // Batch sortieren (s. u.)
};

struct Fog {
    bool       Enabled = false;
    double     Near = 0;
    double     Far  = 1;
    ColorState Color{0, 0, 0, 0};
};
```

- `BeginScene(const Scene&)` – pro Frame aufrufen. Setzt Hintergrund, (falls
  `Fog_.Enabled`) Nebel, Kamera (`ApplyCamera`) und die Sonnenrichtung in
  Kamerakoordinaten.
- `ApplyCamera(const Camera&)` – setzt View + Projection (Aspect aus
  `GetWidth/GetHeight`), ohne Hintergrund/Nebel/Licht.
- `SetView(const Matrix4x4&)` / `SetProjection(const Matrix4x4&)` – manuelle
  Kamera-Matrizen.
- `SetFog(near, far, r, g, b, a)` – Tiefen-Nebel direkt.
- `SetLightDirection(x, y, z)` – Lichtrichtung (Kamerakoordinaten) fürs
  Flat-Shading.
- `SetBatchSort(bool)` – fasst auch nicht-benachbarte Primitive mit gleichem
  Zustand zusammen. **Achtung:** ändert die Zeichenreihenfolge, nur für opake
  Geometrie geeignet. `CompactBatch()` sortiert/fasst manuell zusammen.
- `GetPendingDrawCalls()` / `GetPendingInstancedDrawCalls()` – Diagnose, kein
  GL-Zugriff.

---

## 3. Farben

```cpp
struct ColorState { double R=1, G=1, B=1, A=1; }; // Werte 0..1
```

`ColorInput` kapselt eine Farbangabe und wird in den Style-Objekten benutzt.
Ist es **nicht gesetzt** (`Set == false`, der Default), überschreibt es den
aktuellen Low-Level-Zustand nicht.

```cpp
// Direkte Konstruktoren von ColorInput:
ColorInput("#ff8800");        // Hex (#rgb oder #rrggbb)
ColorInput(128.0);            // Graustufe 0..255
ColorInput(255, 128, 0);      // r,g,b 0..255
ColorInput(255, 128, 0, 200); // r,g,b,a 0..255

// Parser (liefern ColorState mit 0..1):
ParseHex("#44aaff");
ParseColor(0.5, 0.8, 1.0);    // 0..255-Varianten ebenfalls vorhanden

bool ColEq(const ColorState& a, const ColorState& b); // exakter Vergleich
```

---

## 4. Objekte erstellen

Es gibt zwei Wege: **ohne Body** (nur Zeichengeometrie) und **mit Body**
(Mesh + lokale Flächen/Normalen für Kollision/Raycasting). In beiden Fällen
wird die Geometrie als `Mesh` erzeugt und über `DrawMesh` gezeichnet.

```cpp
struct Mesh {
    vector<float> Triangles; // xyz-Tripel je Dreieck (9 Floats/Dreieck)
    vector<float> Lines;     // xyz-Tripel je Linien-Endpunkt (Paare)
};

// Mesh-Bausteine (lokal um den Ursprung):
Mesh MakeCubeMesh(double w, double h, double d);
Mesh MakePyramidMesh(double base, double height); // Basis y=0, Spitze y=height
Mesh MakeSphereMesh(double radius, int slices, int stacks);
```

> `MakeSphereMesh(radius, slices, stacks)` – `slices` = Längengrade,
> `stacks` = Breitengrade. Mehr = glatter, aber teurer.

### 4.1 Objekt **ohne** Body

Das Mesh wird einmal erzeugt und pro Frame mit einer Modell-Matrix
(Translation/Rotation) gezeichnet. Das ist der einfachste Fall und ideal,
wenn keine Kollision/Raycasting gebraucht wird.

```cpp
const ShapeStyle cubeStyle{
    .stroke = ColorInput("#1b1b1b"),
    .fill   = ColorInput("#d3ff44"),
    .lineWidth = 1,
    .mode = StyleBoth,
};

const Mesh cubeMesh = MakeCubeMesh(30, 30, 30); // EINMAL erzeugen

double angle = 0;
auto draw = [&]() {
    BeginScene(scene);
    angle -= 0.01;

    const Matrix4x4 model =
        MultMatrix(RotateMatrix(0, angle, 0),      // Szene drehen
                   TranslateMatrix(-45, 15, 0));   // Platzieren
    DrawMesh(cubeMesh, cubeStyle, model);
};
```

Modell-Matrix-Helfer: `IdentityMatrix()`, `TranslateMatrix(dx,dy,dz)`,
`RotateMatrix(ax,ay,az)` (kombiniert `Ry*Rx*Rz`), `MultMatrix(a,b)`
(`a*b`, d. h. rechts wird zuerst angewendet).

### 4.2 Objekt **mit** Body

Ein `Body` bündelt das Mesh, die daraus abgeleiteten lokalen **Faces**
(inkl. Normalen), eine Position und Euler-Rotationen. Er ist der Träger für
Kollision/Raycasting. Farbe/Linienbreite gehören **nicht** in den Body,
sondern in den `ShapeStyle` beim Zeichnen.

```cpp
struct Body {
    Mesh         Geometry;   // Zeichengeometrie (lokal)
    vector<Face> Faces;      // lokale Faces inkl. Normalen
    Vec3         Pos;
    double       RotX = 0, RotY = 0, RotZ = 0;

    Matrix4x4     ModelMatrix() const; // Translation * Rotation
    vector<Face>  WorldFaces() const;  // Faces in Weltkoordinaten
};

// Faces werden automatisch aus der Geometrie ermittelt (Single Source of Truth):
Body MakeBody(const Mesh& geometry, const Vec3& pos,
              double rotX = 0, double rotY = 0, double rotZ = 0);
```

```cpp
const Mesh ballMesh = MakeSphereMesh(40, 40, 40);
Body ballBody = MakeBody(ballMesh, {0, 40, 0}); // Faces automatisch

double angle = 0;
auto draw = [&]() {
    BeginScene(scene);
    angle += 0.01;
    ballBody.RotY = angle;                        // Body drehen
    DrawMesh(ballBody.Geometry, ballStyle,        // Rendern wie ohne Body
             ballBody.ModelMatrix());             // Körper-Transform nutzen
};
```

**Rendern ist identisch zum Fall ohne Body** – man zeichnet `body.Geometry`
mit `body.ModelMatrix()`. Der Mehrwert des Bodys liegt in den Faces:

```cpp
body.WorldFaces();                 // Faces in Weltkoordinaten (z. B. für Rays)
BuildFaces(mesh);                  // Faces neu aus einem Mesh ableiten
TransformFaces(body.Faces, body.ModelMatrix()); // lokale Faces -> Welt
```

| | ohne Body | mit Body |
| --- | --- | --- |
| Typ | `Mesh` | `Body` (enthält `Mesh`) |
| Erzeugen | `Make*Mesh(...)` | `MakeBody(mesh, pos, rotX, rotY, rotZ)` |
| Zeichnen | `DrawMesh(mesh, style, model)` | `DrawMesh(body.Geometry, style, body.ModelMatrix())` |
| Rotation | Modell-Matrix | `body.RotX/RotY/RotZ` + `ModelMatrix()` |
| Kollision/Raycast | nicht möglich | `body.Faces`, `body.WorldFaces()`, `RaycastSegment` |
| Geeignet für | reine Darstellung | Physik, Trefferabfragen, Picking |

---

## 5. Rendermethoden

Alle Zeichenaufrufe nehmen ihre **Modell-Matrix als letztes Argument**
(Default `IdentityMatrix()`) und sammeln sich im aktuellen Batch. Die
High-Level-Fassade (`Draw*`) übergibt Farbe/Linienbreite als Options-Objekt.

### 5.1 Zeichenmodi und Style-Objekte

```cpp
enum DrawStyle { StyleStroke = 0, StyleFill = 1, StyleBoth = 2 };

struct PrimitiveStyle {   // für Punkte/Linien
    ColorInput stroke;    // Linienfarbe (unbeleuchtet)
    ColorInput fill;      // Füllfarbe (beleuchtet)
    double lineWidth = 0; // 0 = Zustand nicht ändern
    double pointSize = 0; // 0 = Zustand nicht ändern
};

struct ShapeStyle {       // für Flächen/Primitive mit Modus
    ColorInput stroke;
    ColorInput fill;
    double lineWidth = 0;
    double pointSize = 0;
    DrawStyle mode = StyleStroke; // was gezeichnet wird
    double z = 0;                 // Z-Ebene für Rect
    int segments = 0;             // Segmentzahl für Circle (0 -> 64)
};
```

Nicht gesetzte Felder (`ColorInput.Set == false` bzw. `0`) überschreiben den
aktuellen Low-Level-Zustand **nicht** – man kann so Stile schrittweise
vererben.

### 5.2 Übersicht der Renderfunktionen

| Funktion | Beschreibung |
| --- | --- |
| `DrawMesh(const Mesh&, const ShapeStyle&, model)` | Mesh mit Fill (beleuchtet) und/oder Drahtgitter-Linien. Zentral für alle Mesh-Objekte. |
| `DrawMeshInstanced(const Mesh&, const ShapeStyle&, const vector<Instance>&)` | **GPU-Instancing**: dieselbe Geometrie in *einem* Draw-Call mehrfach, je Instanz mit eigener Modell-Matrix und Farbe. |
| `DrawPoint(x,y,z, const PrimitiveStyle&, model)` | Einzelner Punkt (`pointSize`). |
| `DrawLine(x1,y1,z1,x2,y2,z2, const PrimitiveStyle&, model)` | Einzelne Linie. |
| `DrawTriangle(x1..z3, const ShapeStyle&, model)` | Dreieck (Fill/Stroke/Both). |
| `DrawShape(x1..z4, const ShapeStyle&, model)` | Viereck aus 4 Punkten (Fill als 2 Dreiecke, Stroke als Umriss). |
| `DrawRect(x,y,w,h, const ShapeStyle&, model)` | Rechteck in der X/Y-Ebene auf `style.z`. |
| `DrawCircle(x,y,z, radius, const ShapeStyle&, model)` | Kreis (`style.segments`; 0 → 64). |
| `DrawPolygon(const vector<double>& pts, const ShapeStyle&, model)` | Konvexes Polygon aus xyz-Tripeln (`pts.size()/3` Ecken; Fill als Fächer). |

**GPU-Instancing** (`Instance`):

```cpp
struct Instance {
    Matrix4x4  Model;              // row-major wie Matrix4x4
    ColorState Color{1, 1, 1, 1};  // überschreibt die Style-Farbe
};

vector<Instance> cubes(n);
for (size_t i = 0; i < n; i++) {
    cubes[i].Model = MultMatrix(TranslateMatrix(px, py, pz),
                                RotateMatrix(0, angle, 0));
    cubes[i].Color = ColorState{0.25 + 0.6*t, 0.45, 0.9 - 0.6*t, 1};
}
DrawMeshInstanced(cubeMesh, cubeStyle, cubes); // EIN Draw-Call für alle
```

> Beim Instancing wird die Farbe aus dem `ShapeStyle` ignoriert; sie kommt
> aus jeder `Instance`.

### 5.3 Low-Level-Zustand und Effekte

Die `Draw*`-Fassade ist nur eine bequeme Hülle über Low-Level-Primitiven,
die den *aktuellen* Zeichenzustand verwenden. Wer den Zustand direkt steuern
will, kann diese (aus `renderer.hpp`) verwenden:

| Zustand | Setter | Default |
| --- | --- | --- |
| Füllfarbe | `FillColor(ColorInput/ColorState)` | weiß |
| Linienfarbe | `StrokeColor(ColorInput/ColorState)` | schwarz |
| Linienbreite | `StrokeWidth(double)` | 1 |
| Punktgröße | `PointSize(double)` | 4 |
| Effekt | `SetEffect(EffectMode)` | `EffectFlat` |
| Gradient-Endfarbe | `SetGradient(ColorInput/ColorState)` | – |
| Gradient-Zentrum/Radius | `SetGradientCenter(cx,cy,cz,radius)` | – |
| Hintergrund (löscht Canvas) | `Background(ColorInput/ColorState)` | – |

Primitive im Low-Level-Stil (nur bei Bedarf, sonst `Draw*` nutzen):
`Point`, `Line`, `Triangle`, `Shape`, `Rect`, `Circle`, `Polygon` – gleiche
Argumente wie die `Draw*`-Variante, aber ohne Style-Objekt.

**Shader-Effekte** (`enum EffectMode`):

| Wert | Effekt |
| --- | --- |
| `EffectFlat` (0) | einfache Farbe (Default). |
| `EffectGradient` (1) | radialer Verlauf von der aktuellen Farbe zur `SetGradient`-Farbe, ausgehend von `SetGradientCenter(...)`. |
| `EffectPulse` (2) | Grundfarbe mit sinusförmiger Helligkeitspulsation (nutzt die Frame-Zeit). |

```cpp
SetEffect(EffectPulse);            // pulsieren lassen
SetEffect(EffectGradient);
SetGradient(ColorInput("#ff0000"));// Endfarbe
SetGradientCenter(0, 40, 0, 200);  // Zentrum + Radius des Verlaufs
DrawMesh(ballMesh, ballStyle, IdentityMatrix());

SetEffect(EffectFlat);             // zurück zum Normalzustand
```

Zusätzliche globale Zustände: `SetFog(...)`, `SetLightDirection(...)`,
`Background(...)`, `SetBatchSort(...)`, `CompactBatch()`, `SetView(...)`,
`SetProjection(...)`.

---

## 6. Modul `bodies` – Flächen, Normalen, Raycasting

`bodies` zeichnet nicht, sondern arbeitet auf `Mesh`/`Body`:

```cpp
struct Face { Vec3 A, B, C, Normal; }; // Dreieck + normierte Flächennormale

vector<Face> BuildFaces(const Mesh& mesh);              // CCW -> Normale außen
Face         TransformFace(const Face&, const Matrix4x4&);
vector<Face> TransformFaces(const vector<Face>&, const Matrix4x4&);

struct RayHit {
    bool isHit = false;
    double T = 0;    // Parameter entlang der Strecke [0..1]
    Vec3 Point;      // Trefferpunkt
    Vec3 Normal;     // Flächennormale am Treffer
};
RayHit RaycastSegment(const Vec3& p1, const Vec3& p2, const vector<Face>& faces);

struct ClosestFaceHit {
    Vec3   Point;          // nächster Punkt auf dem Face
    Vec3   Normal;         // Flächennormale (Außenseite)
    double Distance = 0;   // |Point - p| (unsigned)
    double SignedDistance = 0; // >0 außen, <0 innen (bzgl. Normale)
};
ClosestFaceHit ClosestPointToFace(const Vec3& p, const Face& face);
```

Typischer Einsatz (Strahlen gegen alle Körper):

```cpp
vector<Face> worldFaces;
for (const auto& b : bodies) {
    const vector<Face> wf = b.Object.WorldFaces();
    worldFaces.insert(worldFaces.end(), wf.begin(), wf.end());
}

const RayHit hit = RaycastSegment(apex, end, worldFaces);
const Vec3 endpoint = hit.isHit ? hit.Point : end; // Strahl am Treffer kappen

DrawLine(apex.X, apex.Y, apex.Z, endpoint.X, endpoint.Y, endpoint.Z,
         PrimitiveStyle{.stroke = ColorInput("#ff8800"), .lineWidth = 1});
```

---

## 7. Mathe-Helfer

**`Vec3`** – 3D-Vektor (`X, Y, Z`):

`Add`, `Sub`, `Scale`, `Mag(m)`, `Limit(max)`, `Negate`, `Dot`, `Cross`,
`SquaredLength`, `Length`, `DistanceTo`, `Normalize`, `Lerp(v,t)`, `Clone`,
`Equals(v,eps)`, `Transform(Matrix4x4)`, `TransformDir(Matrix4x4)`.

**`Matrix4x4`** – `array<array<double,4>,4>`, **row-major** (wird für OpenGL
intern in column-major konvertiert): `TranslateMatrix`, `RotateMatrix`,
`MultMatrix`, `IdentityMatrix`, `ViewWorldMatrix`, `LookAtMatrix`
(blickt in `+Z`), `WorldToCamera`, `RotateAround(point,pivot,rotation)`
(Punkt um Drehpunkt rotieren), `PerspectiveMatrix(fovY,aspect,near,far)`.

**`math.hpp`**: `Project(fov, Vec3)` (3D→Bildschirm, für 2D-Overlays),
`Constrain(value, lo, hi)`, `Random(n1, n2)` (int, `[n1,n2)`),
`RandomFloat(lo, hi)` (double, `[lo,hi)`).

---

## 8. Vollständiges Minimalbeispiel (mit Body)

```cpp
#include "lib3d.hpp"
#include "bodies.hpp"

#include <exception>
using std::exception;

int main() {
    Scene scene;
    scene.Cam.Pos = {50, 100, 200};
    scene.SunDir  = {0.5, 1.0, 0.3};
    scene.Fog_    = {true, 100, 400, ColorState{0.25, 0.25, 0.25, 1}};

    const ShapeStyle ballStyle{
        .stroke = ColorInput("#2f3a00"),
        .fill   = ColorInput("#d3ff44"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };

    Body ball = MakeBody(MakeSphereMesh(40, 40, 40), {0, 40, 0});
    double angle = 0;

    auto draw = [&]() {
        BeginScene(scene);
        ball.RotY = angle += 0.01;
        DrawMesh(ball.Geometry, ballStyle, ball.ModelMatrix());

        // Kollision/Raycasting-Beispiel: Abstand zur Oberfläche
        const vector<Face> faces = ball.WorldFaces();
        const auto c = ClosestPointToFace(Vec3{0, 0, 0}, faces[0]);
        (void)c;
    };

    try { Init(1200, 800); } catch (const exception&) { return 1; }
    SetBatchSort(true);
    StartAnimation(draw);
    return 0;
}
```

Weitere vollständige Beispiele liegen unter `apps/`:
`beispiel` (Meshes + Szenenrotation), `vorlage` (Body + Gitter),
`instanzen` (GPU-Instancing), `ray` (Body-Faces + Raycasting + Effekte).

---

## 9. Hinweise & Fallstricke

- **Keine globale Modell-Matrix:** Position/Rotation immer als `model`-
  Argument bzw. über `Body::ModelMatrix()` übergeben.
- **Stile erben:** Nur gesetzte Felder überschreiben den Zustand. Für
  reproduzierbare Ergebnisse im Zweifel `ColorInput(...)` explizit setzen.
- **Batching:** Alle `Draw*`-Aufrufe eines Frames werden gesammelt und am
  Frame-Ende automatisch gezeichnet. `Background(...)` zeichnet den Batch
  vorher aus (nicht mitten im Frame aufrufen, außer gewollt).
- **`SetBatchSort(true)`** nur für opake Geometrie (ändert die
  Zeichenreihenfolge; Blending wäre dann nicht korrekt sortiert).
- **Meshes wiederverwenden:** Geometrie einmal erzeugen und über alle
  Frames/Instanzen teilen – nicht pro Frame neu bauen.
- **Body vs. Mesh:** Renderdaten (Farbe/Linienbreite) gehören in den
  `ShapeStyle`, nicht in den `Body`. Der `Body` trägt nur Geometrie,
  Faces und Transform.
- **Linken** gegen die statische Bibliothek: Objekt **vor** der Bibliothek
  und Systembibliotheken (siehe `docs/build.md`).
