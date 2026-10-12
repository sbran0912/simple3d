#pragma once

#include <cmath>

#include "mat4.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::fabs;
using std::sqrt;

// ====================================================================
// TYPEN  (Port von lib-3d.ts: Vec3, Vec2, Matrix4x4)
// ====================================================================

// Vec3 ist ein 3D-Vektor.
struct Vec3 {
    double X = 0;
    double Y = 0;
    double Z = 0;

    Vec3() = default;
    Vec3(double x, double y, double z) : X(x), Y(y), Z(z) {}

    // ---- Arithmetik ----
    Vec3 Add(const Vec3& v) const { return {X + v.X, Y + v.Y, Z + v.Z}; }
    Vec3 Sub(const Vec3& v) const { return {X - v.X, Y - v.Y, Z - v.Z}; }
    Vec3 Scale(double s) const { return {X * s, Y * s, Z * s}; }

    // Mag skaliert auf eine bestimmte Ziel-Laenge. Der Nullvektor bleibt null.
    Vec3 Mag(double m) const {
        const double length = Length();
        if (length == 0) {
            return {0, 0, 0};
        }
        return Scale(m / length);
    }

    // Limit begrenzt die Laenge auf einen Maximalwert.
    Vec3 Limit(double maxLen) const {
        const double mSq = SquaredLength();
        if (mSq > maxLen * maxLen) {
            return Scale(maxLen / sqrt(mSq));
        }
        return Clone();
    }

    // Negate kehrt das Vorzeichen um (-this).
    Vec3 Negate() const { return {-X, -Y, -Z}; }

    // ---- Produkte & Betrag ----
    double Dot(const Vec3& v) const { return X * v.X + Y * v.Y + Z * v.Z; }

    Vec3 Cross(const Vec3& v) const {
        return {
            Y * v.Z - Z * v.Y,
            Z * v.X - X * v.Z,
            X * v.Y - Y * v.X,
        };
    }

    double SquaredLength() const { return X * X + Y * Y + Z * Z; }
    double Length() const { return sqrt(SquaredLength()); }
    double DistanceTo(const Vec3& v) const { return Sub(v).Length(); }

    // ---- Normalisierung & Interpolation ----
    Vec3 Normalize() const {
        const double length = Length();
        if (length == 0) {
            return {0, 0, 0};
        }
        return Scale(1 / length);
    }

    // Lerp ist die lineare Interpolation: this + (v - this) * t.
    Vec3 Lerp(const Vec3& v, double t) const { return Add(v.Sub(*this).Scale(t)); }

    // ---- Utility ----
    Vec3 Clone() const { return {X, Y, Z}; }

    bool Equals(const Vec3& v, double epsilon) const {
        return fabs(X - v.X) < epsilon &&
               fabs(Y - v.Y) < epsilon &&
               fabs(Z - v.Z) < epsilon;
    }

    // ---- Transformation ----
    Vec3 Transform(const Matrix4x4& m) const {
        return {
            m[0][0] * X + m[0][1] * Y + m[0][2] * Z + m[0][3],
            m[1][0] * X + m[1][1] * Y + m[1][2] * Z + m[1][3],
            m[2][0] * X + m[2][1] * Y + m[2][2] * Z + m[2][3],
        };
    }

    Vec3 TransformDir(const Matrix4x4& m) const {
        return {
            X * m[0][0] + Y * m[0][1] + Z * m[0][2],
            X * m[1][0] + Y * m[1][1] + Z * m[1][2],
            X * m[2][0] + Y * m[2][1] + Z * m[2][2],
        };
    }
};

// Vec2 ist ein 2D-Vektor mit Skalierungsfaktor (S) fuer z. B. Punktgroesse.
struct Vec2 {
    double X = 0;
    double Y = 0;
    double S = 0;
};
