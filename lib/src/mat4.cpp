#include "mat4.hpp"

#include <cmath>

#include "vec.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::cos;
using std::sin;
using std::tan;

Matrix4x4 TranslateMatrix(double dx, double dy, double dz) {
    return Matrix4x4{{
        {1, 0, 0, dx},
        {0, 1, 0, dy},
        {0, 0, 1, dz},
        {0, 0, 0, 1},
    }};
}

Matrix4x4 RotateMatrix(double ax, double ay, double az) {
    const Matrix4x4 rx{{
        {1, 0, 0, 0},
        {0, cos(ax), -sin(ax), 0},
        {0, sin(ax), cos(ax), 0},
        {0, 0, 0, 1},
    }};
    const Matrix4x4 ry{{
        {cos(ay), 0, sin(ay), 0},
        {0, 1, 0, 0},
        {-sin(ay), 0, cos(ay), 0},
        {0, 0, 0, 1},
    }};
    const Matrix4x4 rz{{
        {cos(az), -sin(az), 0, 0},
        {sin(az), cos(az), 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    }};
    return MultMatrix(rz, MultMatrix(ry, rx));
}

Matrix4x4 MultMatrix(const Matrix4x4& a, const Matrix4x4& b) {
    Matrix4x4 result{};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}

Matrix4x4 IdentityMatrix() {
    return Matrix4x4{{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    }};
}

Matrix4x4 ViewWorldMatrix(const Matrix4x4& view, const Matrix4x4& world) {
    return MultMatrix(view, world);
}

Matrix4x4 LookAtMatrix(const Vec3& cameraPos, const Vec3& target, const Vec3& up) {
    const Vec3 forward = target.Sub(cameraPos).Normalize();
    const Vec3 right = forward.Cross(up).Normalize();
    const Vec3 realUp = right.Cross(forward);

    return Matrix4x4{{
        {right.x, right.y, right.z, -right.Dot(cameraPos)},
        {realUp.x, realUp.y, realUp.z, -realUp.Dot(cameraPos)},
        {forward.x, forward.y, forward.z, -forward.Dot(cameraPos)},
        {0, 0, 0, 1},
    }};
}

Vec3 WorldToCamera(const Vec3& point, const Matrix4x4& view, const Matrix4x4& world) {
    return point.Transform(MultMatrix(view, world));
}

Vec3 RotateAround(const Vec3& point, const Vec3& pivot, const Matrix4x4& rotation) {
    const Vec3 rel = point.Sub(pivot);
    const Vec3 rotated = rel.Transform(rotation);
    return rotated.Add(pivot);
}

Matrix4x4 PerspectiveMatrix(double fovY, double aspect, double near, double far) {
    const double f = 1.0 / tan(fovY / 2);
    return Matrix4x4{{
        {f / aspect, 0, 0, 0},
        {0, f, 0, 0},
        {0, 0, (far + near) / (far - near), -2 * near * far / (far - near)},
        {0, 0, 1, 0},
    }};
}
