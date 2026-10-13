#pragma once

#include <functional>
#include <vector>

#include "color.hpp"
#include "mat4.hpp"
#include "vec.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::function;
using std::vector;

// =================================================================
// OEFFENTLICHE API - Setup, Zustand, 3D-Primitive
//
// Koordinatensystem: +Y zeigt nach oben, Kamera blickt in +Z.
// View/Projection laufen im Vertex-Shader; die Modell-Matrix wird beim
// Sammeln in die Vertices gebacken und pro Zeichenaufruf uebergeben.
// =================================================================

// DrawStyle: 0 = stroke | 1 = fill | 2 = beide.
enum DrawStyle {
    StyleStroke = 0,
    StyleFill = 1,
    StyleBoth = 2,
};

// EffectMode ist der aktive Shader-Effekt.
enum EffectMode {
    EffectFlat = 0,
    EffectGradient = 1,
    EffectPulse = 2,
};

// Key ist eine Auswahl haeufiger Tasten. Die Werte entsprechen exakt den
// GLFW_KEY_*-Codes, daher koennen die Polling-Funktionen auch direkt mit einem
// GLFW-Code aufgerufen werden (int). Weitere Tasten lassen sich per
// static_cast<Key>(glfwCode) ergaenzen.
enum Key {
    KeySpace = 32,
    KeyApostrophe = 39,
    KeyComma = 44,
    KeyMinus = 45,
    KeyPeriod = 46,
    KeySlash = 47,
    Key0 = 48, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
    KeySemicolon = 59,
    KeyEqual = 61,
    KeyA = 65, KeyB, KeyC, KeyD, KeyE, KeyF, KeyG, KeyH, KeyI, KeyJ, KeyK,
    KeyL, KeyM, KeyN, KeyO, KeyP, KeyQ, KeyR, KeyS, KeyT, KeyU, KeyV, KeyW,
    KeyX, KeyY, KeyZ,
    KeyLeftBracket = 91,
    KeyBackslash = 92,
    KeyRightBracket = 93,
    KeyGraveAccent = 96,
    KeyEscape = 256,
    KeyEnter = 257,
    KeyTab = 258,
    KeyBackspace = 259,
    KeyInsert = 260,
    KeyDelete = 261,
    KeyRight = 262,
    KeyLeft = 263,
    KeyDown = 264,
    KeyUp = 265,
    KeyPageUp = 266,
    KeyPageDown = 267,
    KeyHome = 268,
    KeyEnd = 269,
    KeyCapsLock = 280,
    KeyF1 = 290, KeyF2, KeyF3, KeyF4, KeyF5, KeyF6,
    KeyF7, KeyF8, KeyF9, KeyF10, KeyF11, KeyF12,
    KeyLeftShift = 340,
    KeyLeftControl = 341,
    KeyLeftAlt = 342,
    KeyLeftSuper = 343,
    KeyRightShift = 344,
    KeyRightControl = 345,
    KeyRightAlt = 346,
    KeyRightSuper = 347,
};

// Instance beschreibt eine Instanz fuer das GPU-Instancing: eine Modell-Matrix
// (row-major, wie Matrix4x4) und eine Farbe.
struct Instance {
    Matrix4x4 Model;
    ColorState Color{1, 1, 1, 1};
};

int GetWidth();
int GetHeight();

// NoLoop stoppt die Animations-Schleife.
void NoLoop();

// IsMouseDown gibt true zurueck, solange die linke Maustaste gedrueckt ist.
bool IsMouseDown();

// IsMouseUp gibt einmalig true zurueck, wenn die Maustaste losgelassen wurde.
bool IsMouseUp();

// IsKeyDown gibt true zurueck, solange die Taste gedrueckt ist (Dauerdruck,
// z. B. fuer laufende Steuerung pro Frame). key ist ein Key-Wert oder ein
// GLFW_KEY_*-Code.
bool IsKeyDown(int key);

// IsKeyPressed gibt einmalig true zurueck, wenn die Taste seit der letzten
// Abfrage gedrueckt wurde (konsumierend wie IsMouseUp, fuer Trigger).
bool IsKeyPressed(int key);

// SetProjection/SetView setzen die Kamera-Matrizen. Die Modell-Matrix wird
// NICHT mehr als globaler Zustand gehalten, sondern pro Zeichenaufruf als
// explizites Argument uebergeben (siehe model-Parameter der 3D-Primitive).
void SetProjection(const Matrix4x4& m);
void SetView(const Matrix4x4& m);

// GetPendingDrawCalls liefert die Anzahl der im aktuellen Batch noch nicht
// gezeichneten Draw-Calls. Fuer Tests/Diagnose gedacht (kein GL-Zugriff).
int GetPendingDrawCalls();

// GetPendingInstancedDrawCalls liefert analog die Anzahl der gesammelten
// Instancing-Draw-Calls (Fuer Tests/Diagnose, kein GL-Zugriff).
int GetPendingInstancedDrawCalls();

// SetBatchSort aktiviert das Sortieren/Umpacken der Pending-Batch, damit auch
// nicht-benachbarte Primitive mit gleichem Zustand zusammengefasst werden.
// ACHTUNG: aendert die Zeichenreihenfolge -> nur fuer opake Geometrie geeignet.
void SetBatchSort(bool on);

// CompactBatch sortiert (falls aktiviert) und fasst die Pending-Batch zusammen.
void CompactBatch();

// SetGradientCenter setzt das Zentrum fuer den Gradient-Effekt.
void SetGradientCenter(double cx, double cy, double cz, double radius);

// SetFog aktiviert Tiefen-Nebel.
void SetFog(double near, double far, double r, double g, double b, double a);

// SetLightDirection setzt die Lichtrichtung fuer das Flat-Shading in
// Kamerakoordinaten (zeigt vom Fragment ZUR Lichtquelle).
void SetLightDirection(double x, double y, double z);

// Init initialisiert Fenster, OpenGL-Kontext, Shader und Batch-Zustand.
// Wirft runtime_error bei Fehlern.
void Init(int w, int h);

// StartAnimation startet die Animations-Schleife.
void StartAnimation(const function<void()>& fnDraw);

// ---- Zeichenzustand ----
void FillColor(const ColorInput& color);
void FillColor(const ColorState& color);
void StrokeColor(const ColorInput& color);
void StrokeColor(const ColorState& color);
void StrokeWidth(double w);
void SetEffect(EffectMode effect);
void SetGradient(const ColorInput& color);
void SetGradient(const ColorState& color);

// Background loescht den gesamten Canvas inkl. Tiefenpuffer.
void Background(const ColorInput& color);
void Background(const ColorState& color);

// ---- 3D-Primitive ----
// Jeder Aufruf traegt seine Modell-Matrix explizit als letztes Argument
// (Default: Identity). Damit gibt es keinen globalen Modell-Zustand mehr.
void PointSize(double px);
void Point(double x, double y, double z,
           const Matrix4x4& model = IdentityMatrix());
void Line(double x1, double y1, double z1, double x2, double y2, double z2,
          const Matrix4x4& model = IdentityMatrix());
void Triangle(double x1, double y1, double z1,
              double x2, double y2, double z2,
              double x3, double y3, double z3,
              DrawStyle style, const Matrix4x4& model = IdentityMatrix());
void Shape(double x1, double y1, double z1,
           double x2, double y2, double z2,
           double x3, double y3, double z3,
           double x4, double y4, double z4,
           DrawStyle style, const Matrix4x4& model = IdentityMatrix());
void Rect(double x, double y, double w, double h, DrawStyle style, double z,
          const Matrix4x4& model = IdentityMatrix());
void Circle(double x, double y, double z, double radius,
            DrawStyle style, int segments,
            const Matrix4x4& model = IdentityMatrix());
void Polygon(const vector<double>& pts, DrawStyle style,
             const Matrix4x4& model = IdentityMatrix());

// =================================================================
// KAMERA- UND SZENEN-BUENDEL
//
// Reine Daten-Huellen plus Apply-Funktionen. Sie befuellen weiterhin die
// expliziten Setter; es entsteht KEIN globaler Modell-/Kamera-Zustand.
// =================================================================

// Camera buendelt die perspektivischen Parameter und die Blickrichtung.
struct Camera {
    Vec3 Pos{50, 100, 200};
    Vec3 Target{0, 0, 0};
    Vec3 Up{0, 1, 0};
    double FovY = 1.2;
    double Near = 0.1;
    double Far  = 1000;

    // Reine Mathe -> inline im Header.
    Matrix4x4 View() const { return LookAtMatrix(Pos, Target, Up); }
    Matrix4x4 Projection(double aspect) const {
        return PerspectiveMatrix(FovY, aspect, Near, Far);
    }
};

// Fog buendelt die Tiefen-Nebel-Parameter. Enabled == false -> kein Nebel.
struct Fog {
    bool Enabled = false;
    double Near = 0;
    double Far  = 1;
    ColorState Color{0, 0, 0, 0};
};

// Scene buendelt Kamera + Welt-Attribute (Licht, Nebel, Hintergrund).
// Farb-/Graustufenwerte sind hier als ColorState (0..1) hinterlegt.
struct Scene {
    Camera Cam;
    Vec3 SunDir{0.5, 1.0, 0.3};
    Fog sceneFog;
    ColorState Background{40 / 255.0, 40 / 255.0, 40 / 255.0, 1};
    bool BatchSort = false;
};

// ApplyCamera setzt View + Projection (Aspect aus GetWidth/GetHeight).
void ApplyCamera(const Camera& cam);

// BeginScene wendet die Szene an: Hintergrund, Nebel, Kamera und
// Sonnenrichtung (in Kamerakoordinaten). Pro Frame aufzurufen.
void BeginScene(const Scene& scene);
