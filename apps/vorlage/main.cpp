#include "lib3d.hpp"
#include "bodies.hpp"

#include <chrono>
#include <cstdio>
#include <exception>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
namespace chrono = std::chrono;
using std::exception;
using std::printf;

// ====================================================================
// Vorlage ohne Physik: Bodengitter und eine Kugel. Die Kugel wird ueber ein
// einmal erzeugtes Mesh (DrawMesh) gezeichnet, das Gitter ueber DrawLine.
// ====================================================================

int main() {
    const int screenW = 1200;
    const int screenH = 800;
    Scene scene;
    scene.Cam.Pos    = {50, 100, 200};
    scene.Cam.Target = {0, 0, 0};
    scene.Cam.Up     = {0, 1, 0};
    scene.Cam.FovY   = 1.2;
    scene.Cam.Near   = 0.1;
    scene.Cam.Far    = 1000;
    scene.SunDir     = {0.5, 1.0, 0.3};
    scene.Fog_       = {true, 100, 400, ColorState{0.25, 0.25, 0.25, 1}};

    const PrimitiveStyle gridStyle{
        .stroke = ColorInput("#777774"),
        .lineWidth = 1,
    };
    const ShapeStyle ballStyle{
        .stroke = ColorInput("#2f3a00"),
        .fill = ColorInput("#d3ff44"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };

    // Mesh EINMAL erzeugen und wiederverwenden (statt pro Frame).
    // Aufloesung: segments = 20 -> slices = 20, stacks = 10.
    const Mesh ballMesh = MakeSphereMesh(40, 40, 40);

    // Kugel als Body erstellen (wird sich drehen)
    Body ballBody = MakeBody(ballMesh, {0, 40, 0});

    const double gridSize = 600;
    const int gridCells = 24;

    double angle = 0;

    int fpsFrames = 0;
    auto fpsLastTime = chrono::steady_clock::now();

    auto updateFps = [&]() {
        fpsFrames++;
        const auto now = chrono::steady_clock::now();
        const auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - fpsLastTime).count();
        if (elapsed >= 500) {
            printf("FPS: %d\n", static_cast<int>(fpsFrames * 1000 / elapsed));
            fpsFrames = 0;
            fpsLastTime = now;
        }
    };

    /* Loop */
    auto draw = [&]() {
        updateFps();

        BeginScene(scene);

        // Bodengitter (XZ-Ebene bei y=0) aus einzelnen Linien.
        const double half = gridSize / 2;
        const double step = gridSize / gridCells;
        for (int i = 0; i <= gridCells; i++) {
            const double p = -half + i * step;
            DrawLine(p, 0, -half, p, 0, half, gridStyle);
            DrawLine(-half, 0, p, half, 0, p, gridStyle);
        }

        // Kugel rotieren lassen: Szene-Rotation * Verschiebung an (0,16,0).
        angle += 0.01;
        ballBody.RotY = angle;
        DrawMesh(ballBody.Geometry, ballStyle, ballBody.ModelMatrix());
    };

    /* Start */
    try {
        Init(screenW, screenH);
    } catch (const exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    SetBatchSort(true);
    StartAnimation(draw);
    return 0;
}
