#include "lib3d.hpp"
#include "bodies.hpp"
#include "vehicle.hpp"

#include <exception>

using std::exception;
using std::printf;
namespace numbers = std::numbers;

int main() {
    Scene scene;

    const ShapeStyle gridStyle{
        .stroke = ColorInput("#777774"),
        .lineWidth = 1,
    };
    const ShapeStyle pyramidStyle{
        .stroke = ColorInput("#2f3a00"),
        .fill = ColorInput("#d3ff44"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };

    // Bodengitter EINMAL erzeugen und wiederverwenden.
    const Mesh gridMesh = MakeGridMesh(600, 24);
    // Vehicle
    const Mesh pyramidMesh = MakePyramidMesh(5, 10); // Pyramide: quadratische Basis bei y=0, Spitze bei y=height.
    const double radius = 100;     // Vehicle als Pyramide auf dem Kreisradius platzieren.
    Vehicle vehicle(MakeBody(pyramidMesh, Vec3{radius, 0, 0}));

    double angle = 0;

    /* Loop */
    auto draw = [&]() {
        BeginScene(scene);

        // Bodengitter (XZ-Ebene bei y=0) aus dem geteilten Mesh.
        DrawMesh(gridMesh, gridStyle);

        // Pyramide auf dem Kreis bewegen: konstante Winkelgeschwindigkeit.
        angle += 0.01;
        vehicle.body.pos = Vec3{radius * cos(angle), 0, radius * sin(angle)};
        // Tangential zur Kreisbahn zeigen (in Bewegungsrichtung).
        vehicle.body.RotY = angle + numbers::pi / 2;
        vehicle.vel = Vec3{-radius * sin(angle), 0, radius * cos(angle)}.Scale(0.01);
        vehicle.alignToVelocity();
        DrawMesh(vehicle.body.geometry, pyramidStyle, vehicle.body.ModelMatrix());
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
