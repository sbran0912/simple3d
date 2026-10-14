#include "lib3d.hpp"
#include "bodies.hpp"
#include "vehicle.hpp"

#include <exception>
#include <vector>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::exception;
using std::fprintf;
using std::vector;

// ====================================================================
// Boids-Schwarm
//
// 80 Vehicles starten mit zufaelligen Positionen und Geschwindigkeiten.
// Pro Frame wirken die drei Flocking-Regeln (separate, align, cohesion)
// als Steering-Kraefte; applyBoundary reflektiert die Geschwindigkeit an
// den Weltgrenzen, damit der Schwarm im Raum bleibt. Sieben grosse,
// verschiedenfarbige Hindernis-Boxen an festen (deterministischen)
// Positionen werden ueber avoidObstacles umflogen.
// ====================================================================

int main() {
    Scene scene;
    scene.Cam.Pos    = {0, 320, 520};
    scene.Cam.Target = {0, 0, 0};
    scene.Cam.Up     = {0, 1, 0};
    scene.Cam.FovY   = 1.2;
    scene.Cam.Near   = 0.1;
    scene.Cam.Far    = 2000;
    scene.SunDir     = {0.5, 1.0, 0.3};
    scene.sceneFog   = {true, 400, 1400, ColorState{0.25, 0.25, 0.25, 1}};

    const ShapeStyle gridStyle{
        .stroke = ColorInput("#777774"),
        .lineWidth = 1,
    };

    // Bodengitter EINMAL erzeugen und wiederverwenden.
    const Mesh gridMesh = MakeGridMesh(600, 24);
    // Vehicle: Pyramide (quadratische Basis bei y=0, Spitze bei y=height).
    const Mesh pyramidMesh = MakePyramidMesh(5, 10);
    // Hindernis-Box (um den Ursprung zentriert), deutlich groesser als die
    // Vehicles.
    const Mesh boxMesh = MakeCubeMesh(120, 120, 120);

    constexpr int obstacleCount = 7;

    // Jede Hindernis-Box bekommt eine eigene Fuellfarbe.
    const ShapeStyle obstacleStyles[obstacleCount] = {
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#3a6ea5"), .lineWidth = 2, .mode = StyleBoth},
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#c0392b"), .lineWidth = 2, .mode = StyleBoth},
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#27ae60"), .lineWidth = 2, .mode = StyleBoth},
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#8e44ad"), .lineWidth = 2, .mode = StyleBoth},
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#e67e22"), .lineWidth = 2, .mode = StyleBoth},
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#16a085"), .lineWidth = 2, .mode = StyleBoth},
        {.stroke = ColorInput("#101010"), .fill = ColorInput("#d4a017"), .lineWidth = 2, .mode = StyleBoth},
    };

    // Schwebende Hindernisse an FESTEN, deterministischen Positionen. Sie
    // liegen auf einem groben 3D-Raster, in dem sich zwei Positionen in
    // mindestens einer Achse um mindestens 240 unterscheiden. Damit ist der Abstand
    // zweier Mittelpunkte stets groesser als der doppelte Bounding-Sphere-
    // Radius der Box (2*(60*sqrt(3)+6) ~ 230) — die Boxen ueberlagern sich
    // also nie, auch bei Rotation und Schweben nicht.
    const Vec3 obstaclePositions[obstacleCount] = {
        {-160.0, -120.0, -240.0},
        {   0.0,  120.0, -240.0},
        { 160.0, -120.0, -240.0},
        {-240.0,  120.0,    0.0},
        { 240.0,  120.0,    0.0},
        {-240.0, -120.0,  240.0},
        {   0.0,  -40.0,  240.0},
    };

    vector<FloatingBox> obstacles;
    obstacles.reserve(obstacleCount);
    for (int i = 0; i < obstacleCount; i++) {
        obstacles.push_back(
            FloatingBox(MakeBody(boxMesh, obstaclePositions[i]), i * 1.3, 0.01));
    }

    constexpr int vehicleCount = 80;
    // Spawn-Bereich liegt innerhalb der Weltgrenzen aus applyBoundary.
    constexpr double spawnX = 180.0;
    constexpr double spawnY = 110.0;
    constexpr double spawnZ = 110.0;

    // Farbe der Vehicles (Render-Info, gehoert nicht in den Body).
    const ShapeStyle vehicleStyle{
        .stroke = ColorInput("#7a3b00"),
        .fill = ColorInput("#ff8800"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };

    // Vehicles starten mit zufaelliger Position und zufaelliger
    // Geschwindigkeit.
    vector<Vehicle> vehicles;
    vehicles.reserve(vehicleCount);
    for (int i = 0; i < vehicleCount; i++) {
        const Vec3 pos{
            RandomFloat(-spawnX, spawnX),
            RandomFloat(-spawnY, spawnY),
            RandomFloat(-spawnZ, spawnZ),
        };
        const Vec3 velocity = Vec3{
            RandomFloat(-1.0, 1.0),
            RandomFloat(-1.0, 1.0),
            RandomFloat(-1.0, 1.0),
        }.Mag(1.0);

        Vehicle v{MakeBody(pyramidMesh, pos), velocity};
        v.alignToVelocity();
        vehicles.push_back(v);
    }

    double time = 0;

    /* Loop */
    auto draw = [&]() {
        BeginScene(scene);

        time += 0.02;

        // Bodengitter (XZ-Ebene bei y=0) aus dem geteilten Mesh.
        DrawMesh(gridMesh, gridStyle);

        // Hindernisse animieren (Schweben + Rotation).
        for (FloatingBox& o : obstacles) {
            o.update(time);
        }

        // Flocking: erst die Kraefte sammeln, dann integrieren.
        const Vec3 center = scene.Cam.Target;
        for (Vehicle& v : vehicles) {
            // Bei gedrueckter Maustaste wirkt zusaetzlich eine Kraft zum
            // Szenenzentrum (scene.Cam.Target) auf jedes Boid.
            if (IsMouseDown()) {
                const Vec3 toCenter = center.Sub(v.body.pos).Normalize();
                v.applyForce(toCenter.Scale(0.05));
            }
            v.separate(vehicles);
            v.align(vehicles);
            v.cohesion(vehicles);
            // margin = harte Sicherheitsdistanz, detect = Vorausschau-Radius
            v.avoidObstacles(obstacles, 6.0, 30.0);
            v.applyBoundary();
            v.update();
            v.alignToVelocity();
        }

        for (int i = 0; i < obstacleCount; i++) {
            const FloatingBox& o = obstacles[i];
            DrawMesh(o.body.geometry, obstacleStyles[i], o.body.ModelMatrix());
        }
        for (const Vehicle& v : vehicles) {
            DrawMesh(v.body.geometry, vehicleStyle, v.body.ModelMatrix());
        }
    };

    /* Start */
    try {
        Init();
    } catch (const exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    SetBatchSort(true);
    StartAnimation(draw);
    return 0;
}
