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
    void AlignToVelocity() {
        double mag = sqrt(vel.X * vel.X + vel.Y * vel.Y + vel.Z * vel.Z);
        if (mag < 0.0001) {
            return;
        }

        double magXZ = sqrt(vel.X * vel.X + vel.Z * vel.Z);

        // vel.Y/mag muss in [-1,1] liegen (Schutz vor NaN durch Float-Rundung)
        double cosY = Constrain(vel.Y / mag, -1.0, 1.0);
        body.RotX = acos(cosY);
        if (magXZ < 0.0001) {
            body.RotY = 0.0;
        } else {
            body.RotY = atan2(vel.X / magXZ, vel.Z / magXZ);
        }
        body.RotZ = 0.0;

        heading = Vec3{vel.X / mag, vel.Y / mag, vel.Z / mag};
    }
};