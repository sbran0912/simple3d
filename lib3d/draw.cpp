#include "draw.hpp"

#include "mesh.hpp"
#include "renderer_internal.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::vector;

namespace {

// ApplyStyle uebernimmt die gesetzten Felder in den Low-Level-Zeichenzustand.
void ApplyStyle(const ColorInput& stroke, const ColorInput& fill,
                double lineWidth, double pointSize) {
    if (stroke.Set) {
        StrokeColor(stroke.Color);
    }
    if (fill.Set) {
        FillColor(fill.Color);
    }
    if (lineWidth != 0) {
        StrokeWidth(lineWidth);
    }
    if (pointSize != 0) {
        PointSize(pointSize);
    }
}

void ApplyStyle(const PrimitiveStyle& o) {
    ApplyStyle(o.stroke, o.fill, o.lineWidth, o.pointSize);
}

void ApplyStyle(const ShapeStyle& o) {
    ApplyStyle(o.stroke, o.fill, o.lineWidth, o.pointSize);
}

} // namespace

// ====================================================================
// MESHES
// ====================================================================

void DrawMesh(const Mesh& mesh, const ShapeStyle& style, const Matrix4x4& model) {
    ApplyStyle(style);
    if (style.mode == StyleFill || style.mode == StyleBoth) {
        SubmitTriangles(mesh.Triangles, model);
    }
    if (style.mode == StyleStroke || style.mode == StyleBoth) {
        SubmitLines(mesh.Lines, model);
    }
}

// ====================================================================
// GPU-INSTANCING
// ====================================================================

void DrawMeshInstanced(const Mesh& mesh, const ShapeStyle& style,
                       const vector<Instance>& instances) {
    ApplyStyle(style);
    SubmitInstanced(mesh.Triangles, mesh.Lines, style.mode, instances);
}

// ====================================================================
// IMMEDIATE-PRIMITIVES
// ====================================================================

void DrawPoint(double x, double y, double z, const PrimitiveStyle& style,
               const Matrix4x4& model) {
    ApplyStyle(style);
    Point(x, y, z, model);
}

void DrawLine(double x1, double y1, double z1, double x2, double y2, double z2,
              const PrimitiveStyle& style, const Matrix4x4& model) {
    ApplyStyle(style);
    Line(x1, y1, z1, x2, y2, z2, model);
}

void DrawTriangle(double x1, double y1, double z1,
                  double x2, double y2, double z2,
                  double x3, double y3, double z3,
                  const ShapeStyle& style, const Matrix4x4& model) {
    ApplyStyle(style);
    Triangle(x1, y1, z1, x2, y2, z2, x3, y3, z3, style.mode, model);
}

void DrawShape(double x1, double y1, double z1,
               double x2, double y2, double z2,
               double x3, double y3, double z3,
               double x4, double y4, double z4,
               const ShapeStyle& style, const Matrix4x4& model) {
    ApplyStyle(style);
    Shape(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4, style.mode, model);
}

void DrawRect(double x, double y, double w, double h, const ShapeStyle& style,
              const Matrix4x4& model) {
    ApplyStyle(style);
    Rect(x, y, w, h, style.mode, style.z, model);
}

void DrawCircle(double x, double y, double z, double radius, const ShapeStyle& style,
                const Matrix4x4& model) {
    ApplyStyle(style);
    int segments = style.segments;
    if (segments == 0) {
        segments = 64;
    }
    Circle(x, y, z, radius, style.mode, segments, model);
}

void DrawPolygon(const vector<double>& pts, const ShapeStyle& style,
                 const Matrix4x4& model) {
    ApplyStyle(style);
    Polygon(pts, style.mode, model);
}
