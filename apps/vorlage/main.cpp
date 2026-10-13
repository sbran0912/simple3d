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
    Scene scene;

    const ShapeStyle gridStyle{
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

    // Bodengitter EINMAL erzeugen und wiederverwenden.
    const Mesh gridMesh = MakeGridMesh(600, 24);

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

        // Bodengitter (XZ-Ebene bei y=0) aus dem geteilten Mesh.
        DrawMesh(gridMesh, gridStyle);

        // Kugel rotieren lassen: Szene-Rotation * Verschiebung an (0,16,0).
        angle += 0.01;
        ballBody.rotY = angle;
        DrawMesh(ballBody.geometry, ballStyle, ballBody.ModelMatrix());
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
