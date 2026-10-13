#pragma once

#include <array>
#include "bodies.hpp"

using std::array;
using std::sqrt;
using std:: acos;
using std::atan2;

struct Vehicle {
    Body body;
    Vec3 vel{};
    Vec3 accel{};
    Vec3 heading{};
    double health = 1.0;
    array<double, 4> dna{};

    Vehicle(const Body& b) : body{b} {}

    // AlignToVelocity richtet den Body an der Geschwindigkeit aus.
    void alignToVelocity() {
        double mag = sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        if (mag < 0.0001) {
            return;
        }

        double magXZ = sqrt(vel.x * vel.x + vel.z * vel.z);

        // vel.Y/mag muss in [-1,1] liegen (Schutz vor NaN durch Float-Rundung)
        double cosY = Constrain(vel.y / mag, -1.0, 1.0);
        body.rotX = acos(cosY);
        if (magXZ < 0.0001) {
            body.rotY = 0.0;
        } else {
            body.rotY = atan2(vel.x / magXZ, vel.z / magXZ);
        }
        body.rotZ = 0.0;

        heading = Vec3{vel.x / mag, vel.y / mag, vel.z / mag};
    }

    // ApplyForce addiert eine Kraft zur Beschleunigung.
    void applyForce(Vec3 force) {
	    accel = accel.Add(force);
    }

    // Update integriert die Bewegung und begrenzt die Geschwindigkeit.
    void Update() {
        vel = vel.Add(accel);
        double speed = vel.Length();
        vel = vel.Normalize().Scale(Constrain(speed, 0.5, 2.0));
        accel = Vec3{};
        body.pos = body.pos.Add(vel);
    }
};


