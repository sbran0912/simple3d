#pragma once

#include <vector>

#include "lib3d.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::vector;

// ====================================================================
// BODIES - Flaechen/Normalen und Schnittabfragen (Raycasting)
//
// Separates Modul (ohne lib3d-Aenderungen). Ermittelt aus der Dreiecksliste
// eines Meshes die Flaechen inklusive Normalen (Faces) und bietet Strecken-/
// Strahl-Schnitttests darauf an. Das Modul zeichnet NICHT; das Rendern bleibt
// bei lib3d (DrawMesh).
// ====================================================================

// Face ist ein Dreieck eines Meshes samt (normierter) Flaechennormale.
struct Face {
    Vec3 A;
    Vec3 B;
    Vec3 C;
    Vec3 Normal;
};

// BuildFaces ermittelt aus der Dreiecksliste (9 Floats je Dreieck, xyz-Tripel)
// eines Meshes die Faces inklusive Normalen. Windungsreihenfolge CCW -> die
// Normale zeigt nach aussen.
vector<Face> BuildFaces(const Mesh& mesh);

// TransformFace(s) bringen Faces mit einer Modell-Matrix in den Weltraum.
Face TransformFace(const Face& face, const Matrix4x4& model);
vector<Face> TransformFaces(const vector<Face>& faces, const Matrix4x4& model);

// RayHit beschreibt den naechsten Treffer einer Strecke.
struct RayHit {
    bool isHit = false;
    double T = 0; // Parameter entlang der Strecke, 0..1
    Vec3 Point;   // Trefferpunkt
    Vec3 Normal;  // Face-Normale am Treffer
};

// RaycastSegment liefert den naechsten Schnitt der Strecke [p1,p2] mit den
// Faces. Ohne Treffer ist Hit == false.
RayHit RaycastSegment(const Vec3& p1, const Vec3& p2, const vector<Face>& faces);

// ClosestFaceHit beschreibt den naechsten Punkt auf einem Face zu einem
// Abfragepunkt. Distance ist der ungerichtete Abstand zu Point.
struct ClosestFaceHit {
    Vec3 Point;                // naechster Punkt auf dem Face
    Vec3 Normal;               // Flaechennormale (Aussenseite)
    double Distance = 0;       // Abstand |Point - p| (unsigned)
    double SignedDistance = 0; // >0 ausserhalb, <0 innerhalb (bzgl. Normal)
};

// ClosestPointToFace liefert den Punkt auf dem Face (Dreieck A,B,C), der einem
// Abfragepunkt p am naechsten liegt, samt Normale und Abstand.
ClosestFaceHit ClosestPointToFace(const Vec3& p, const Face& face);

// Body ist ein platzierbarer, kollisionsfaehiger Koerper: Mesh + lokale Faces
// + Transform. Darstellungsdaten (Farbe, Linienbreite) gehoeren nicht
// hierher, sondern in den ShapeStyle beim Zeichnen (DrawMesh).
struct Body {
    Mesh geometry;      // Zeichengeometrie (lokal)
    vector<Face> faces; // lokale Faces (inkl. Normalen)
    Vec3 pos;
    double rotX = 0;
    double rotY = 0;
    double rotZ = 0;

    // ModelMatrix ist die Modellmatrix (Translation * Rotation).
    Matrix4x4 ModelMatrix() const;

    // WorldFaces liefert die Faces in Weltkoordinaten.
    vector<Face> WorldFaces() const;
};

// MakeBody erzeugt einen Body aus einem Mesh. Die Faces werden automatisch
// aus der Geometrie ermittelt (Single Source of Truth). Farbe und
// Linienbreite kommen beim Zeichnen ueber den ShapeStyle (DrawMesh).
Body MakeBody(const Mesh& geometry, const Vec3& pos, double rotX = 0,
              double rotY = 0, double rotZ = 0);
