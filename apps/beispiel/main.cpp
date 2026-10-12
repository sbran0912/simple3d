#include "lib3d/lib3d.hpp"

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
    const int screenW = 1200;
    const int screenH = 800;

    const double fovY = 1.2;
    const double zNear = 0.1;
    const double zFar = 1000;

    const Vec3 camPos{50, 100, 200};
    const Vec3 camTarget{0, 0, 0};
    const Vec3 camUp{0, 1, 0};

    const Vec3 sunDir{0.5, 1.0, 0.3};

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
    const PrimitiveStyle gridStyle{
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

    // Bodengitter (fest, dient als Referenz fuer die Drehung).
    const double gridSize = 600;
    const int gridCells = 24;

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

        Background(ColorInput(40, 40, 40));
        SetFog(150, 500, 0.25, 0.25, 0.25, 1);

        const auto view = LookAtMatrix(camPos, camTarget, camUp);
        const auto proj = PerspectiveMatrix(
            fovY, static_cast<double>(GetWidth()) / GetHeight(), zNear, zFar);
        SetProjection(proj);
        SetView(view);

        const auto camLight = sunDir.TransformDir(view);
        SetLightDirection(camLight.X, camLight.Y, camLight.Z);

        // Gesamte Szene im Uhrzeigersinn drehen (von oben gesehen).
        angle -= 0.01;

        // Bodengitter (fest, XZ-Ebene bei y=0) als Referenz.
        const double half = gridSize / 2;
        const double step = gridSize / gridCells;
        for (int i = 0; i <= gridCells; i++) {
            const double p = -half + i * step;
            DrawLine(p, 0, -half, p, 0, half, gridStyle);
            DrawLine(-half, 0, p, half, 0, p, gridStyle);
        }

        // Szenen-Rotation als explizite Modell-Matrix; Position je Koerper dazu.
        const Matrix4x4 scene = RotateMatrix(0, angle, 0);
        DrawMesh(cubeMesh, cubeStyle,
                 MultMatrix(scene, TranslateMatrix(cubeX, cubeY, cubeZ)));
        DrawMesh(pyramidMesh, pyramidStyle,
                 MultMatrix(scene, TranslateMatrix(pyrX, pyrY, pyrZ)));
        DrawMesh(sphereMesh, sphereStyle,
                 MultMatrix(scene, TranslateMatrix(sphX, sphY, sphZ)));
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
