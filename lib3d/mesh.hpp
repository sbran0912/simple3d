#pragma once

#include <vector>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::vector;

// ====================================================================
// MESH - reine Zeichen-Geometrie (keine Physik)
//
// Triangles sind xyz-Tripel einer Dreiecksliste, Lines xyz-Tripel einer
// Linienliste (Endpunktpaare). Die Koordinaten sind lokal (Objektraum); die
// Platzierung passiert beim Zeichnen ueber die Modell-Matrix.
// ====================================================================
struct Mesh {
    vector<float> Triangles;
    vector<float> Lines;
};

// MakeCubeMesh erzeugt einen achsenparallelen Wuerfel um den Ursprung.
Mesh MakeCubeMesh(double w, double h, double d);

// MakePyramidMesh erzeugt eine Pyramide mit quadratischer Basis. Die Basis
// liegt bei y=0 (Mittelpunkt im Ursprung), die Spitze bei y=height.
Mesh MakePyramidMesh(double base, double height);

// MakeSphereMesh erzeugt eine Kugel um den Ursprung.
Mesh MakeSphereMesh(double radius, int slices, int stacks);
