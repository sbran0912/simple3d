#include "lib3d.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
namespace chrono = std::chrono;
using std::exception;
using std::printf;
using std::vector;

// ====================================================================
// Beispiel: GPU-Instancing.
//
// Ein Raster aus Wuerfeln und ein Ring aus Kugeln werden mit je EINEM
// Instancing-Draw-Call gezeichnet (statt pro Objekt). Jede Instance traegt
// ihre eigene Modell-Matrix und Farbe. Die gesamte Szene dreht sich - von
// oben (+Y) gesehen - im Uhrzeigersinn.
// ====================================================================

int main() {
    Scene scene;
    scene.Cam.Pos    = {120, 240, 460};
    scene.Cam.Target = {0, 0, 0};
    scene.Cam.Up     = {0, 1, 0};
    scene.Cam.FovY   = 1.2;
    scene.Cam.Near   = 0.1;
    scene.Cam.Far    = 2000;
    scene.SunDir     = {0.5, 1.0, 0.3};
    scene.sceneFog       = {true, 400, 1200, ColorState{0.25, 0.25, 0.25, 1}};

    const ShapeStyle cubeStyle{
        .stroke = ColorInput("#101010"),
        .fill = ColorInput("#ffffff"), // wird pro Instance ueberschrieben
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    const ShapeStyle sphereStyle{
        .stroke = ColorInput("#101010"),
        .fill = ColorInput("#ffffff"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    const ShapeStyle gridStyle{
        .stroke = ColorInput("#777774"),
        .lineWidth = 1,
    };

    // --- Wuerfel-Raster ---
    const int gridN = 14;
    const double gridSpacing = 24;
    const double cubeSize = 12;
    const double gridOffset = -(gridN - 1) * gridSpacing / 2;

    vector<Vec3> cubePos;
    vector<ColorState> cubeCol;
    cubePos.reserve(static_cast<size_t>(gridN * gridN));
    cubeCol.reserve(static_cast<size_t>(gridN * gridN));
    for (int ix = 0; ix < gridN; ix++) {
        for (int iz = 0; iz < gridN; iz++) {
            const double px = gridOffset + ix * gridSpacing;
            const double pz = gridOffset + iz * gridSpacing;
            cubePos.push_back(Vec3{px, cubeSize / 2, pz});
            const double t = static_cast<double>(ix) / (gridN - 1);
            cubeCol.push_back(ColorState{0.25 + 0.6 * t, 0.45, 0.9 - 0.6 * t, 1});
        }
    }

    // --- Kugel-Ring ---
    const int ringCount = 28;
    const double ringRadius = 240;
    const double sphereRadius = 11;
    const double sphereY = 34;

    vector<Vec3> ringPos;
    vector<ColorState> ringCol;
    for (int i = 0; i < ringCount; i++) {
        const double a = (static_cast<double>(i) / ringCount) * 2.0 * 3.14159265358979;
        ringPos.push_back(Vec3{ringRadius * std::cos(a), sphereY, ringRadius * std::sin(a)});
        const double t = static_cast<double>(i) / ringCount;
        ringCol.push_back(ColorState{0.9 - 0.5 * t, 0.8 * t + 0.2, 0.35, 1});
    }

    // Meshes EINMAL erzeugen und ueber alle Instanzen teilen. Render- und
    // (spaetere) Kollisionsgeometrie kommen damit aus derselben Quelle.
    // Aufloesung der Kugel: slices = 12, stacks = 6.
    const Mesh cubeMesh = MakeCubeMesh(cubeSize, cubeSize, cubeSize);
    const Mesh sphereMesh = MakeSphereMesh(sphereRadius, 12, 6);

    // Bodengitter EINMAL erzeugen und wiederverwenden.
    const Mesh gridMesh = MakeGridMesh(800, 32);

    double angle = 0;
    vector<Instance> cubeInstances(cubePos.size());
    vector<Instance> ringInstances(ringPos.size());

    int fpsFrames = 0;
    auto fpsLastTime = chrono::steady_clock::now();
    auto updateFps = [&]() {
        fpsFrames++;
        const auto now = chrono::steady_clock::now();
        const auto elapsed =
            chrono::duration_cast<chrono::milliseconds>(now - fpsLastTime).count();
        if (elapsed >= 500) {
            printf("FPS: %d  Instanzen: %zu\n",
                   static_cast<int>(fpsFrames * 1000 / elapsed),
                   cubeInstances.size() + ringInstances.size());
            fpsFrames = 0;
            fpsLastTime = now;
        }
    };

    /* Loop */
    auto draw = [&]() {
        updateFps();

        BeginScene(scene);

        // Szene im Uhrzeigersinn drehen (von oben gesehen).
        angle -= 0.01;

        // Bodengitter (fest) aus dem geteilten Mesh.
        DrawMesh(gridMesh, gridStyle);

        // Instanz-Matrizen aktualisieren: Szene-Rotation * Platzierung * Spin.
        for (size_t i = 0; i < cubePos.size(); i++) {
            const Matrix4x4 place = TranslateMatrix(cubePos[i].x, cubePos[i].y, cubePos[i].z);
            const Matrix4x4 spin = RotateMatrix(0, angle * 2.0, 0);
            cubeInstances[i].Model =
                MultMatrix(MultMatrix(RotateMatrix(0, angle, 0), place), spin);
            cubeInstances[i].Color = cubeCol[i];
        }
        for (size_t i = 0; i < ringPos.size(); i++) {
            const Matrix4x4 place = TranslateMatrix(ringPos[i].x, ringPos[i].y, ringPos[i].z);
            ringInstances[i].Model = MultMatrix(RotateMatrix(0, angle, 0), place);
            ringInstances[i].Color = ringCol[i];
        }

        // Je EIN Instancing-Draw-Call fuer alle Wuerfel bzw. Kugeln. Es werden
        // die geteilten Meshes gezeichnet (identisch zur Kollisionsgeometrie).
        DrawMeshInstanced(cubeMesh, cubeStyle, cubeInstances);
        DrawMeshInstanced(sphereMesh, sphereStyle, ringInstances);
    };

    /* Start */
    try {
        Init();
    } catch (const exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    StartAnimation(draw);
    return 0;
}
