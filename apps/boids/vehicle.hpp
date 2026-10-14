#pragma once

#include <array>
#include <vector>
#include "bodies.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::array;
using std::vector;

// ====================================================================
// SCHWEBENDE HINDERNISBOX
//
// Ein FloatingBox ist ein Body mit Grundposition, sanfter Auf-und-Ab-
// Bewegung (Sinus) und langsamer Rotation um die Y-Achse. Der Radius ist
// die maximale Entfernung eines Vertices vom Ursprung (Bounding-Sphere-
// Radius) und dient als schneller Vorab-Test.
//
// Die Darstellung (Farbe, Linienbreite) erfolgt ueber den ShapeStyle beim
// Zeichnen (DrawMesh) — nicht im Body selbst.
//
// Definition: vehicle.cpp
// ====================================================================
struct FloatingBox {
    Body body;
    Vec3 base;
    double phase;
    double spin;
    double radius; // Bounding-Sphere-Radius (max. Vertex-Laenge)

    // FloatingBox erstellt eine schwebende Hindernis-Box aus einem Body
    // (analog zu Vehicle). phase: Phasenversatz des Schwebens,
    // spin: Rotationsgeschwindigkeit.
    FloatingBox(const Body& b, double phase, double spin);

    // update animiert die Box (Schweben + Rotation). t ist die Zeit in
    // Sekunden.
    void update(double t);
};

// ====================================================================
// VEHICLE (BOID)
//
// Ein Vehicle ist ein Body mit Geschwindigkeit/Beschleunigung samt den
// Flocking-Regeln (separate, align, cohesion) sowie Ausweichen vor den
// Hindernissen (avoidObstacles) und Reflexion an den Weltgrenzen
// (applyBoundary).
//
// Definition: vehicle.cpp
// ====================================================================
struct Vehicle {
    Body body;
    Vec3 vel{};
    Vec3 accel{};
    Vec3 heading{};
    double health = 1.0;
    array<double, 4> dna{};

    Vehicle(const Body& b);
    Vehicle(const Body& b, const Vec3& v);

    // alignToVelocity richtet den Body an der Geschwindigkeit aus.
    void alignToVelocity();

    // applyForce addiert eine Kraft zur Beschleunigung.
    void applyForce(Vec3 force);

    // update integriert die Bewegung und begrenzt die Geschwindigkeit.
    void update();

    // seek steuert das Fahrzeug in Richtung eines Ziels.
    void seek(Vec3 target);

    // align gleicht die eigene Geschwindigkeit an die der Nachbarn an
    // (Flocking-Regel: Alignment).
    void align(const vector<Vehicle>& vehics);

    // separate haelt Abstand zu den Nachbarn (Flocking-Regel: Separation).
    void separate(const vector<Vehicle>& vehics);

    // cohesion steuert in Richtung des Schwerpunkts der Nachbarn
    // (Flocking-Regel: Kohaesion).
    void cohesion(const vector<Vehicle>& vehics);

    // avoidObstacles laesst das Vehicle den schwebenden Boxen per Steering
    // ausweichen (siehe Detailkommentar in vehicle.cpp). margin ist die
    // letzte Absicherung, detect der Vorausschau-Radius.
    void avoidObstacles(const vector<FloatingBox>& obstacles, double margin,
                        double detect);

    // applyBoundary reflektiert die Geschwindigkeit an den Weltgrenzen.
    void applyBoundary();
};
