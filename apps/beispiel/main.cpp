#include "lib3d.hpp"

#include <chrono>
#include <cstdio>
#include <exception>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
namespace chrono = std::chrono;
using std::exception;
using std::printf;

// ====================================================================
// Beispiel ohne Physik: Wuerfel, Pyramide und Kugel.
//
// Alle Koerper werden ueber einmal erzeugte Meshes (DrawMesh) gezeichnet.
// Die gesamte Szene dreht sich starr um die Y-Achse - von oben (+Y) gesehen
// im Uhrzeigersinn, also mit fallendem Winkel (angle -= speed).
// ====================================================================

int main() {
    Scene scene;
    scene.Cam.Pos    = {50, 100, 200};
    scene.Cam.Target = {0, 0, 0};
    scene.Cam.Up     = {0, 1, 0};
    scene.Cam.FovY   = 1.2;
    scene.Cam.Near   = 0.1;
    scene.Cam.Far    = 1000;
    scene.SunDir     = {0.5, 1.0, 0.3};
    scene.sceneFog       = {true, 150, 500, ColorState{0.25, 0.25, 0.25, 1}};

    const ShapeStyle cubeStyle{
        .stroke = ColorInput("#1b1b1b"),
        .fill = ColorInput("#d3ff44"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    const ShapeStyle pyramidStyle{
        .stroke = ColorInput("#2a1400"),
        .fill = ColorInput("#ff8844"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    const ShapeStyle sphereStyle{
        .stroke = ColorInput("#06263f"),
        .fill = ColorInput("#44aaff"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    const ShapeStyle gridStyle{
        .stroke = ColorInput("#777774"),
        .lineWidth = 1,
    };

    // Feste Positionen (lokal, Weltkoordinaten vor der Szenen-Rotation).
    const double cubeX = -45, cubeY = 15, cubeZ = 0, cubeSize = 30;
    const double pyrX = 45, pyrY = 0, pyrZ = 0, pyrBase = 34, pyrHeight = 48;
    const double sphX = 0, sphY = 18, sphZ = -45, sphRadius = 16;

    // Meshes EINMAL erzeugen und im Frame teilen. Render- und (spaetere)
    // Kollisionsgeometrie kommen damit aus derselben Quelle.
    // Kugel-Aufloesung: slices = 24, stacks = 12.
    const Mesh cubeMesh = MakeCubeMesh(cubeSize, cubeSize, cubeSize);
    const Mesh pyramidMesh = MakePyramidMesh(pyrBase, pyrHeight);
    const Mesh sphereMesh = MakeSphereMesh(sphRadius, 24, 12);

    // Bodengitter (fest, dient als Referenz fuer die Drehung). EINMAL erzeugen.
    const Mesh gridMesh = MakeGridMesh(600, 24);

    double angle = 0;

    int fpsFrames = 0;
    auto fpsLastTime = chrono::steady_clock::now();

    auto updateFps = [&]() {
        fpsFrames++;
        const auto now = chrono::steady_clock::now();
        const auto elapsed =
            chrono::duration_cast<chrono::milliseconds>(now - fpsLastTime).count();
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

        // Gesamte Szene im Uhrzeigersinn drehen (von oben gesehen).
        angle -= 0.01;

        // Bodengitter (fest, XZ-Ebene bei y=0) als Referenz.
        DrawMesh(gridMesh, gridStyle);

        // Szenen-Rotation als explizite Modell-Matrix; Position je Koerper dazu.
        const Matrix4x4 sceneRot = RotateMatrix(0, angle, 0);
        DrawMesh(cubeMesh, cubeStyle,
                 MultMatrix(sceneRot, TranslateMatrix(cubeX, cubeY, cubeZ)));
        DrawMesh(pyramidMesh, pyramidStyle,
                 MultMatrix(sceneRot, TranslateMatrix(pyrX, pyrY, pyrZ)));
        DrawMesh(sphereMesh, sphereStyle,
                 MultMatrix(sceneRot, TranslateMatrix(sphX, sphY, sphZ)));
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
