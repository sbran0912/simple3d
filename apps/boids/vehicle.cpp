#include "vehicle.hpp"

#include <cmath>
#include <limits>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::acos;
using std::atan2;
using std::fabs;
using std::sin;
using std::sqrt;

// ====================================================================
// FLOATINGBOX
// ====================================================================

FloatingBox::FloatingBox(const Body& b, double phase, double spin)
    : body{b}, base{b.pos}, phase{phase}, spin{spin} {
    double maxR = 0.0;
    for (std::size_t i = 0; i + 2 < body.geometry.Triangles.size(); i += 9) {
        const double x = body.geometry.Triangles[i];
        const double y = body.geometry.Triangles[i + 1];
        const double z = body.geometry.Triangles[i + 2];
        const double l = sqrt(x * x + y * y + z * z);
        if (l > maxR) {
            maxR = l;
        }
    }
    radius = maxR;
}

void FloatingBox::update(double t) {
    body.pos = Vec3{
        base.x,
        base.y + sin(t + phase) * 6.0,
        base.z,
    };
    body.rotY += spin;
}

// ====================================================================
// VEHICLE
// ====================================================================

Vehicle::Vehicle(const Body& b) : body{b} {}
Vehicle::Vehicle(const Body& b, const Vec3& v) : body{b}, vel{v} {}

void Vehicle::alignToVelocity() {
    const double mag = sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
    if (mag < 0.0001) {
        return;
    }

    const double magXZ = sqrt(vel.x * vel.x + vel.z * vel.z);

    // vel.y/mag muss in [-1,1] liegen (Schutz vor NaN durch Float-Rundung).
    const double cosY = Constrain(vel.y / mag, -1.0, 1.0);
    body.rotX = acos(cosY);
    if (magXZ < 0.0001) {
        body.rotY = 0.0;
    } else {
        body.rotY = atan2(vel.x / magXZ, vel.z / magXZ);
    }
    body.rotZ = 0.0;

    heading = Vec3{vel.x / mag, vel.y / mag, vel.z / mag};
}

void Vehicle::applyForce(Vec3 force) {
    accel = accel.Add(force);
}

void Vehicle::update() {
    vel = vel.Add(accel);
    const double speed = vel.Length();
    vel = vel.Normalize().Scale(Constrain(speed, 0.5, 2.0));
    accel = Vec3{};
    body.pos = body.pos.Add(vel);
}

void Vehicle::seek(Vec3 target) {
    const Vec3 desired = target.Sub(body.pos).Limit(3.0);
    const Vec3 steer = desired.Sub(vel).Limit(2.0);
    applyForce(steer.Scale(0.2));
}

void Vehicle::align(const vector<Vehicle>& vehics) {
    constexpr double minDistance = 50.0;
    Vec3 sumVel{};
    int count = 0;

    for (const Vehicle& other : vehics) {
        const double distance = body.pos.DistanceTo(other.body.pos);
        if (distance > 0 && distance < minDistance) {
            sumVel = sumVel.Add(other.vel);
            count++;
        }
    }

    if (count > 0) {
        sumVel = sumVel.Scale(1.0 / count); // Durchschnittsgeschwindigkeit
        sumVel = sumVel.Mag(0.1);
        const Vec3 steer = sumVel.Sub(vel).Scale(0.1);
        applyForce(steer);
    }
}

void Vehicle::separate(const vector<Vehicle>& vehics) {
    constexpr double minDistance = 40.0;
    Vec3 diffSum{};
    int count = 0;

    for (const Vehicle& other : vehics) {
        const double distance = body.pos.DistanceTo(other.body.pos);
        if (distance > 0 && distance < minDistance) {
            diffSum = diffSum.Add(body.pos.Sub(other.body.pos).Normalize());
            count++;
        }
    }

    if (count > 0) {
        diffSum = diffSum.Scale(1.0 / count); // Durchschnitt
        diffSum = diffSum.Mag(0.1);
        const Vec3 steer = diffSum.Sub(vel).Scale(0.1);
        applyForce(steer);
    }
}

void Vehicle::cohesion(const vector<Vehicle>& vehics) {
    constexpr double minDistance = 50.0;
    Vec3 sumPos{};
    int count = 0;

    for (const Vehicle& other : vehics) {
        const double distance = body.pos.DistanceTo(other.body.pos);
        if (distance > 0 && distance < minDistance) {
            sumPos = sumPos.Add(other.body.pos);
            count++;
        }
    }

    if (count > 0) {
        sumPos = sumPos.Scale(1.0 / count); // Schwerpunkt der Nachbarn
        Vec3 desired = sumPos.Sub(body.pos); // Richtung zur Gruppenmitte
        desired = desired.Mag(0.1);
        const Vec3 steer = desired.Sub(vel).Scale(0.1);
        applyForce(steer);
    }
}

// avoidObstacles laesst das Vehicle den schwebenden Boxen natuerlich
// ausweichen.
//
// Der Abstand wird mit den vorhandenen Bibliotheksfunktionen ermittelt:
// WorldFaces() liefert die Flaechen des Hindernisses in Weltkoordinaten,
// ClosestPointToFace() den naechsten Punkt auf einer Flaeche samt
// vorzeichenbehaftetem Abstand (SignedDistance > 0 ausserhalb, < 0
// innerhalb). Der betragsmassig kleinste SignedDistance ueber alle Flaechen
// ist damit der Abstand zur naechsten Oberflaeche.
//
// Statt erst bei Eindringen hart zu reagieren, weicht das Vehicle bereits
// innerhalb eines grossen Vorausschau-Radius (detect) per STEERING aus —
// analog zu seek(), nur vom Hindernis WEG:
//
//   1. desired  = Richtung weg von der Oberflaeche, begrenzt auf die
//                 Maximalgeschwindigkeit (Zielgeschwindigkeit).
//   2. steer    = desired - vel, begrenzt auf eine Maximal-Kraft.
//   3. applyForce(steer * Gewicht) — das Gewicht steigt mit der Naehe
//                 (near), ist aber insgesamt klein.
//
// Weil die Kraft begrenzt und schwach ist, dreht das Boid sanft und
// kontinuierlich ab, statt ruckartig die Richtung zu wechseln.
//
// margin ist nur noch die letzte Absicherung: dringt ein Boid trotz
// Steering in die Box ein, wird es minimal herausgeschoben, damit es nicht
// verschwindet.
void Vehicle::avoidObstacles(const vector<FloatingBox>& obstacles,
                             double margin, double detect) {
    for (const FloatingBox& o : obstacles) {
        // Schneller Vorab-Test (Bounding Sphere): spart das Erzeugen der
        // Weltflaechen fuer weit entfernte Hindernisse. Der Radius umfasst
        // den ganzen Vorausschau-Bereich.
        if (body.pos.DistanceTo(o.body.pos) > o.radius + detect) {
            continue;
        }

        const vector<Face> faces = o.body.WorldFaces();
        if (faces.empty()) {
            continue;
        }

        // Naechste Flaeche ueber den betragsmaessig kleinsten
        // vorzeichenbehafteten Abstand bestimmen. Aussen ist das die
        // naechste Oberflaeche, innen die Flaeche, die zum Herauschieben am
        // naechsten liegt.
        double signedDist = 0.0;
        Vec3 closest{};
        Vec3 normal{};
        double bestAbs = std::numeric_limits<double>::infinity();
        for (const Face& f : faces) {
            const ClosestFaceHit hit = ClosestPointToFace(body.pos, f);
            const double absDist = fabs(hit.SignedDistance);
            if (absDist < bestAbs) {
                bestAbs = absDist;
                signedDist = hit.SignedDistance;
                closest = hit.Point;
                normal = hit.Normal;
            }
        }

        // ---- Weiches, vorausschauendes Ausweichen ----
        // away zeigt von der naechsten Oberflaeche weg. Ausserhalb ist das
        // der Vektor zum naechsten Punkt; innerhalb (oder genau auf der
        // Flaeche) faellt man auf die Aussen-Normale zurueck.
        Vec3 away = body.pos.Sub(closest);
        if (signedDist < 0.0 || away.Length() < 1e-6) {
            away = normal;
        }
        away = away.Normalize();

        // near = 1 direkt an der Oberflaeche, 0 am Rand des
        // Vorausschau-Bereichs.
        const double dist = Constrain(signedDist, 0.0, detect);
        const double near = Constrain(1.0 - dist / detect, 0.0, 1.0);

        if (near > 0.0) {
            constexpr double maxSpeed = 2.0; // Zielgeschwindigkeit
            constexpr double maxForce = 2.0; // Grenze der Steering-Kraft
            constexpr double weight = 0.02;  // Einfluss (klein = sanfter)

            // 1. Zielgeschwindigkeit: weg vom Hindernis, auf maxSpeed
            //    begrenzt (wie desired in seek()).
            const Vec3 desired = away.Mag(maxSpeed);

            // 2. Steering-Kraft = gewuenschte - aktuelle Geschwindigkeit,
            //    auf maxForce begrenzt.
            const Vec3 steer = desired.Sub(vel).Limit(maxForce);

            // 3. Nur schwach einwirken, mit der Naehe gewichtet -> sanftes,
            //    kontinuierliches Abdrehen.
            applyForce(steer.Scale(weight * near));
        }

        // ---- Letzte Absicherung gegen Eindringen ----
        // Zum Testen deaktiviert (false): die Boids duerfen dann in das
        // Hindernis eintauchen. Auf true setzen, um die harte Rueckstellung
        // wieder zu aktivieren.
        constexpr bool hardSafety = false;
        if (hardSafety && signedDist < margin) {
            body.pos = body.pos.Add(normal.Scale(margin - signedDist));

            // Geschwindigkeit UND Beschleunigung ins Hindernis hinein
            // streichen (Gleitbewegung), damit das Boid nicht sofort wieder
            // hineingedrueckt wird.
            const double intoVel = vel.Dot(normal);
            if (intoVel < 0.0) {
                vel = vel.Sub(normal.Scale(intoVel));
            }
            const double intoAccel = accel.Dot(normal);
            if (intoAccel < 0.0) {
                accel = accel.Sub(normal.Scale(intoAccel));
            }
        }
    }
}

void Vehicle::applyBoundary() {
    constexpr double minX = -400.0;
    constexpr double maxX = 400.0;
    constexpr double minY = -400.0;
    constexpr double maxY = 400.0;
    constexpr double minZ = -400.0;
    constexpr double maxZ = 400.0;

    if (body.pos.x < minX || body.pos.x > maxX) {
        vel.x *= -1.0;
    }
    if (body.pos.y < minY || body.pos.y > maxY) {
        vel.y *= -1.0;
    }
    if (body.pos.z < minZ || body.pos.z > maxZ) {
        vel.z *= -1.0;
    }
}
