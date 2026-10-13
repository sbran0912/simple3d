#pragma once

#include <vector>

#include "mesh.hpp"
#include "renderer.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::vector;

// ====================================================================
// ZEICHEN-FASSADE (Port von lib-render.ts)
//
// Fassade fuer das Zeichnen. Die Szene ruft ausschliesslich
// Draw*() auf; Farbe/Linienbreite/Punktgroesse kommen als Options-Objekt mit.
// ====================================================================

// PrimitiveStyle sind Zeichen-Optionen. Nicht gesetzte Felder (ColorInput.Set
// == false bzw. 0) ueberschreiben den aktuellen Low-Level-Zustand nicht.
struct PrimitiveStyle {
    ColorInput stroke; // Linienfarbe (unbeleuchtet)
    ColorInput fill;   // Fuellfarbe  (beleuchtet)
    double lineWidth = 0;
    double pointSize = 0;
};

// ShapeStyle erweitert PrimitiveStyle um den Zeichenmodus (und optionale
// Parameter fuer Rect/Circle).
struct ShapeStyle {
    ColorInput stroke;
    ColorInput fill;
    double lineWidth = 0;
    double pointSize = 0;
    DrawStyle mode = StyleStroke;
    double z = 0;
    int segments = 0;
};

// ---- Meshes ----
// DrawMesh zeichnet ein Mesh in lokalen Koordinaten und wendet dabei die
// uebergebene Modell-Matrix an (Default: Identity). Fill beleuchtet, Lines
// als Drahtgitter. Die Geometrie entsteht ausserhalb ueber Make*Mesh und
// kann so auch fuer Kollision/Raycasting wiederverwendet werden.
void DrawMesh(const Mesh& mesh, const ShapeStyle& style,
              const Matrix4x4& model = IdentityMatrix());

// ---- GPU-Instancing ----
// Zeichnet dieselbe Geometrie mehrfach in einem Draw-Call. Jede Instance
// bringt ihre eigene Modell-Matrix und Farbe mit; style bestimmt Fill/Stroke
// und die Linienbreite. (Farbe aus style wird ignoriert.)
void DrawMeshInstanced(const Mesh& mesh, const ShapeStyle& style,
                       const vector<Instance>& instances);

// ---- Immediate-Primitives ----
void DrawPoint(double x, double y, double z, const PrimitiveStyle& style,
               const Matrix4x4& model = IdentityMatrix());
void DrawLine(double x1, double y1, double z1, double x2, double y2, double z2,
              const PrimitiveStyle& style, const Matrix4x4& model = IdentityMatrix());
void DrawTriangle(double x1, double y1, double z1,
                  double x2, double y2, double z2,
                  double x3, double y3, double z3,
                  const ShapeStyle& style, const Matrix4x4& model = IdentityMatrix());
void DrawShape(double x1, double y1, double z1,
               double x2, double y2, double z2,
               double x3, double y3, double z3,
               double x4, double y4, double z4,
               const ShapeStyle& style, const Matrix4x4& model = IdentityMatrix());
void DrawRect(double x, double y, double w, double h, const ShapeStyle& style,
              const Matrix4x4& model = IdentityMatrix());
void DrawCircle(double x, double y, double z, double radius, const ShapeStyle& style,
                const Matrix4x4& model = IdentityMatrix());
void DrawPolygon(const vector<double>& pts, const ShapeStyle& style,
                 const Matrix4x4& model = IdentityMatrix());
