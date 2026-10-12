#pragma once

#include <array>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::array;

struct Vec3;

// Matrix4x4 ist eine 4x4-Matrix in Zeilen-Darstellung (row-major), wie im
// TypeScript-Original number[][]. Für OpenGL wird sie in ein flaches
// column-major Float32-Array gewandelt (siehe FlattenMatrix).
using Matrix4x4 = array<array<double, 4>, 4>;

// TranslateMatrix erzeugt eine 4x4-Translationsmatrix.
Matrix4x4 TranslateMatrix(double dx, double dy, double dz);

// RotateMatrix erzeugt eine kombinierte 4x4-Rotationsmatrix (Ry * Rx) * Rz.
Matrix4x4 RotateMatrix(double ax, double ay, double az);

// MultMatrix multipliziert zwei 4x4-Matrizen (a * b).
Matrix4x4 MultMatrix(const Matrix4x4& a, const Matrix4x4& b);

// IdentityMatrix liefert die 4x4-Identitaetsmatrix.
Matrix4x4 IdentityMatrix();

// ViewWorldMatrix kombiniert View- und World-Matrix (view * world).
Matrix4x4 ViewWorldMatrix(const Matrix4x4& view, const Matrix4x4& world);

// LookAtMatrix erzeugt eine 4x4-View-Matrix. Die Kamera blickt in
// +Z-Richtung (kompatibel mit PerspectiveMatrix).
Matrix4x4 LookAtMatrix(const Vec3& cameraPos, const Vec3& target, const Vec3& up);

// WorldToCamera transformiert einen Weltpunkt in Kamerakoordinaten.
Vec3 WorldToCamera(const Vec3& point, const Matrix4x4& view, const Matrix4x4& world);

// RotateAround rotiert einen Punkt um ein Pivot (Drehpunkt).
Vec3 RotateAround(const Vec3& point, const Vec3& pivot, const Matrix4x4& rotation);

// PerspectiveMatrix erzeugt eine perspektivische Projektionsmatrix.
// KONVENTION: Kamera blickt in +Z-Richtung.
Matrix4x4 PerspectiveMatrix(double fovY, double aspect, double near, double far);
