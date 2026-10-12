#pragma once

#include <functional>
#include <vector>

#include "color.hpp"
#include "mat4.hpp"

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
