#include "bodies.hpp"

#include <cmath>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::fabs;
using std::size_t;

// ====================================================================
// FACES - Ermittlung aus der Mesh-Dreiecksliste
// ====================================================================

vector<Face> BuildFaces(const Mesh& mesh) {
    const vector<float>& t = mesh.Triangles;
    vector<Face> faces;
    faces.reserve(t.size() / 9);

    for (size_t i = 0; i + 8 < t.size(); i += 9) {
        Face f;
        f.A = Vec3{t[i + 0], t[i + 1], t[i + 2]};
        f.B = Vec3{t[i + 3], t[i + 4], t[i + 5]};
        f.C = Vec3{t[i + 6], t[i + 7], t[i + 8]};
        f.Normal = f.B.Sub(f.A).Cross(f.C.Sub(f.A)).Normalize();
        faces.push_back(f);
    }
    return faces;
}

Face TransformFace(const Face& face, const Matrix4x4& model) {
    Face f;
    f.A = face.A.Transform(model);
    f.B = face.B.Transform(model);
    f.C = face.C.Transform(model);
    f.Normal = face.Normal.TransformDir(model).Normalize();
    return f;
}

vector<Face> TransformFaces(const vector<Face>& faces, const Matrix4x4& model) {
    vector<Face> out;
    out.reserve(faces.size());
    for (const Face& f : faces) {
        out.push_back(TransformFace(f, model));
    }
    return out;
}

// ====================================================================
// RAYCASTING
// ====================================================================

// RayTriangle ist der Moeller-Trumbore-Schnitt einer Strecke [p1,p2] mit einem
// Dreieck. Liefert true und t (Parameter 0..1) bei einem Treffer.
namespace {

bool RayTriangle(const Vec3& p1, const Vec3& dir,
                 const Vec3& a, const Vec3& b, const Vec3& c, double& t) {
    const Vec3 e1 = b.Sub(a);
    const Vec3 e2 = c.Sub(a);
    const Vec3 p = dir.Cross(e2);
    const double det = e1.Dot(p);
    if (fabs(det) < 1e-12) {
        return false; // Strecke parallel zur Face
    }
    const double inv = 1.0 / det;
    const Vec3 tvec = p1.Sub(a);
    const double u = tvec.Dot(p) * inv;
    if (u < 0.0 || u > 1.0) {
        return false;
    }
    const Vec3 q = tvec.Cross(e1);
    const double v = dir.Dot(q) * inv;
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }
    t = e2.Dot(q) * inv;
    return t >= 0.0 && t <= 1.0;
}

// ClosestPointOnTriangle ermittelt den naechsten Punkt auf dem Dreieck (a,b,c)
// zu p. Es kommen nur drei Faelle in Frage: naechste Ecke, naechste Kante oder
// ein innerer Punkt. Vorzeichen der Skalarprodukte waehlen den Fall
// (Ericson, Real-Time Collision Detection, 5.1.5).
Vec3 ClosestPointOnTriangle(const Vec3& p, const Vec3& a, const Vec3& b,
                            const Vec3& c) {
    const Vec3 ab = b.Sub(a);
    const Vec3 ac = c.Sub(a);
    const Vec3 ap = p.Sub(a);
    const double d1 = ab.Dot(ap);
    const double d2 = ac.Dot(ap);
    if (d1 <= 0 && d2 <= 0) {
        return a; // Ecke A
    }

    const Vec3 bp = p.Sub(b);
    const double d3 = ab.Dot(bp);
    const double d4 = ac.Dot(bp);
    if (d3 >= 0 && d4 <= d3) {
        return b; // Ecke B
    }

    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) {
        return a.Add(ab.Scale(d1 / (d1 - d3))); // Kante AB
    }

    const Vec3 cp = p.Sub(c);
    const double d5 = ab.Dot(cp);
    const double d6 = ac.Dot(cp);
    if (d6 >= 0 && d5 <= d6) {
        return c; // Ecke C
    }

    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) {
        return a.Add(ac.Scale(d2 / (d2 - d6))); // Kante AC
    }

    const double va = d3 * d6 - d5 * d4;
    if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0) {
        const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b.Add(c.Sub(b).Scale(w)); // Kante BC
    }

    // Inneres: baryzentrische Koordinaten (u, v, w), u = 1 - v - w.
    const double denom = 1.0 / (va + vb + vc);
    const double v = vb * denom;
    const double w = vc * denom;
    return a.Add(ab.Scale(v)).Add(ac.Scale(w));
}

} // namespace

RayHit RaycastSegment(const Vec3& p1, const Vec3& p2, const vector<Face>& faces) {
    RayHit best;
    const Vec3 dir = p2.Sub(p1);
    if (dir.SquaredLength() <= 0) {
        return best;
    }

    double bestT = 1.0;
    for (const Face& f : faces) {
        double t = 0;
        if (!RayTriangle(p1, dir, f.A, f.B, f.C, t)) {
            continue;
        }
        if (t > bestT) {
            continue;
        }
        bestT = t;
        best.isHit = true;
        best.T = t;
        best.Point = p1.Add(dir.Scale(t));
        best.Normal = f.Normal;
    }
    return best;
}

// ====================================================================
// NAECHSTER PUNKT
// ====================================================================

ClosestFaceHit ClosestPointToFace(const Vec3& p, const Face& face) {
    ClosestFaceHit hit;
    hit.Point = ClosestPointOnTriangle(p, face.A, face.B, face.C);
    hit.Normal = face.Normal;
    hit.Distance = p.DistanceTo(hit.Point);
    hit.SignedDistance = p.Sub(hit.Point).Dot(face.Normal);
    return hit;
}

// ====================================================================
// BODY
// ====================================================================

Matrix4x4 Body::ModelMatrix() const {
    const Matrix4x4 t = TranslateMatrix(pos.x, pos.y, pos.z);
    if (rotX == 0 && rotY == 0 && rotZ == 0) {
        return t;
    }
    return MultMatrix(t, RotateMatrix(rotX, rotY, rotZ));
}

vector<Face> Body::WorldFaces() const {
    return TransformFaces(faces, ModelMatrix());
}

Body MakeBody(const Mesh& geometry, const Vec3& pos, double rotX,
              double rotY, double rotZ) {
    Body b;
    b.geometry = geometry;
    b.faces = BuildFaces(b.geometry);
    b.pos = pos;
    b.rotX = rotX;
    b.rotY = rotY;
    b.rotZ = rotZ;
    return b;
}
