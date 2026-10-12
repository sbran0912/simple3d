#pragma once

#include "vec.hpp"

// Project projiziert einen 3D-Punkt (im Kamerakoordinatensystem) auf den
// Bildschirm. Fuer 2D-Overlays gedacht.
Vec2 Project(double fov, const Vec3& v);

// Constrain begrenzt value auf den Bereich [lo, hi].
double Constrain(double value, double lo, double hi);

// Random liefert eine ganzzahlige Zufallszahl in [n1, n2).
int Random(int n1, int n2);

// RandomFloat liefert eine Gleitkomma-Zufallszahl in [lo, hi).
double RandomFloat(double lo, double hi);
