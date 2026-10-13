#include "mesh.hpp"

#include <cmath>
#include <numbers>

#include "vec.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::cos;
namespace numbers = std::numbers;
using std::sin;

namespace {

void AddTri(vector<float>& out, const Vec3& a, const Vec3& b, const Vec3& c) {
    out.push_back(static_cast<float>(a.X));
    out.push_back(static_cast<float>(a.Y));
    out.push_back(static_cast<float>(a.Z));
    out.push_back(static_cast<float>(b.X));
    out.push_back(static_cast<float>(b.Y));
    out.push_back(static_cast<float>(b.Z));
    out.push_back(static_cast<float>(c.X));
    out.push_back(static_cast<float>(c.Y));
    out.push_back(static_cast<float>(c.Z));
}

// AddQuad trianguliert ein konvexes Viereck als Fächer (a,b,c) + (a,c,d).
void AddQuad(vector<float>& out, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
    AddTri(out, a, b, c);
    AddTri(out, a, c, d);
}

void AddLine(vector<float>& out, const Vec3& a, const Vec3& b) {
    out.push_back(static_cast<float>(a.X));
    out.push_back(static_cast<float>(a.Y));
    out.push_back(static_cast<float>(a.Z));
    out.push_back(static_cast<float>(b.X));
    out.push_back(static_cast<float>(b.Y));
    out.push_back(static_cast<float>(b.Z));
}

} // namespace

Mesh MakeCubeMesh(double w, double h, double d) {
    const double hw = w / 2;
    const double hh = h / 2;
    const double hd = d / 2;

    const Vec3 v[8] = {
        {-hw, -hh, -hd}, {hw, -hh, -hd}, {hw, hh, -hd}, {-hw, hh, -hd},
        {-hw, -hh, hd},  {hw, -hh, hd},  {hw, hh, hd},  {-hw, hh, hd},
    };

    const int faces[6][4] = {
        {0, 3, 2, 1}, // vorne
        {4, 5, 6, 7}, // hinten
        {0, 4, 7, 3}, // links
        {1, 2, 6, 5}, // rechts
        {0, 1, 5, 4}, // unten
        {3, 7, 6, 2}, // oben
    };
    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // vorne
        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // hinten
        {0, 4}, {1, 5}, {2, 6}, {3, 7}, // Verbindungen
    };

    Mesh m;
    for (const auto& f : faces) {
        AddQuad(m.Triangles, v[f[0]], v[f[1]], v[f[2]], v[f[3]]);
    }
    for (const auto& e : edges) {
        AddLine(m.Lines, v[e[0]], v[e[1]]);
    }
    return m;
}

Mesh MakePyramidMesh(double base, double height) {
    const double hb = base / 2;

    const Vec3 v[5] = {
        {-hb, 0, -hb}, {hb, 0, -hb}, {hb, 0, hb}, {-hb, 0, hb}, {0, height, 0},
    };

    const int sides[4][3] = {
        {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4},
    };
    const int edges[8][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Basis
        {0, 4}, {1, 4}, {2, 4}, {3, 4}, // Seitenkanten
    };

    Mesh m;
    for (const auto& t : sides) {
        AddTri(m.Triangles, v[t[0]], v[t[1]], v[t[2]]);
    }
    AddQuad(m.Triangles, v[3], v[2], v[1], v[0]); // Basis (CCW von unten)
    for (const auto& e : edges) {
        AddLine(m.Lines, v[e[0]], v[e[1]]);
    }
    return m;
}

Mesh MakeSphereMesh(double radius, int slices, int stacks) {
    if (slices < 3) {
        slices = 3;
    }
    if (stacks < 2) {
        stacks = 2;
    }

    const double tau = 2 * numbers::pi;
    const auto point = [&](int i, int j) -> Vec3 {
        const double theta = (static_cast<double>(i) / stacks) * numbers::pi;
        const double phi = (static_cast<double>(j) / slices) * tau;
        const double r = radius * sin(theta);
        return {r * cos(phi), radius * cos(theta), r * sin(phi)};
    };

    Mesh m;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            const Vec3 a = point(i, j);
            const Vec3 b = point(i, j + 1);
            const Vec3 c = point(i + 1, j + 1);
            const Vec3 d = point(i + 1, j);
            if (i == 0) {
                AddTri(m.Triangles, a, c, d); // Nordpol
            } else if (i == stacks - 1) {
                AddTri(m.Triangles, a, b, c); // Suedpol
            } else {
                AddQuad(m.Triangles, a, b, c, d);
            }
        }
    }

    // Meridiane (Pol zu Pol).
    for (int j = 0; j < slices; j++) {
        for (int i = 0; i < stacks; i++) {
            AddLine(m.Lines, point(i, j), point(i + 1, j));
        }
    }
    // Breitenringe (ohne die zusammenfallenden Pol-Ringe).
    for (int i = 1; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            AddLine(m.Lines, point(i, j), point(i, j + 1));
        }
    }
    return m;
}

Mesh MakeGridMesh(double size, int cells) {
    if (cells < 1) {
        cells = 1;
    }

    const double half = size / 2;
    const double step = size / cells;

    Mesh m;
    for (int i = 0; i <= cells; i++) {
        const double p = -half + i * step;
        // Linie parallel zur Z-Achse und Linie parallel zur X-Achse.
        AddLine(m.Lines, Vec3{p, 0, -half}, Vec3{p, 0, half});
        AddLine(m.Lines, Vec3{-half, 0, p}, Vec3{half, 0, p});
    }
    return m;
}
