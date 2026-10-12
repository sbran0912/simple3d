#pragma once

#include <vector>

#include "renderer.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::vector;

// Interne Batch-Sammel-Helfer (in renderer.cpp implementiert). Sie nehmen
// fertige Float-Arrays an (x,y,z-Tripel) und eine Modell-Matrix, die beim
// Sammeln in die Vertices gebacken wird. Werden von der Zeichen-Fassade
// (draw.cpp, z. B. DrawMesh) benutzt. Flaechen werden beleuchtet, Linien nicht.
void SubmitTriangles(const vector<float>& verts, const Matrix4x4& model);
void SubmitLines(const vector<float>& verts, const Matrix4x4& model);

// SubmitInstanced sammelt einen GPU-Instancing-Zeichenauftrag: lokale
// Mesh-Dreiecke/-Linien plus eine Liste von Instanzen (Modell-Matrix + Farbe).
void SubmitInstanced(const vector<float>& triangles, const vector<float>& lines,
                     DrawStyle style, const vector<Instance>& instances);
