#include "lib3d.hpp"
#include "bodies.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <numbers>
#include <vector>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
namespace chrono = std::chrono;
namespace numbers = std::numbers; // fuer numbers::pi
using std::cos;
using std::exception;
using std::fprintf;
using std::printf;
using std::sin;
using std::size_t;
using std::sqrt;
using std::vector;

// ====================================================================
// Rayleigh-Beispiel (Port von simple3d/apps/ray):
//
// Vier Wuerfel (rot/cyan) und eine Pyramide (weiss) stehen in der Szene.
// Aus dem Ursprung laeuft ein rotierender Lichtkegel aus 20 Strahlen; jeder
// Strahl endet am naechsten Face-Treffer (Raycasting ueber das bodies-Modul).
// ====================================================================

// ShiftMeshY verschiebt ein Mesh entlang Y. Die lib3d-Pyramide liegt mit der
// Basis bei y=0; die Referenz-Pyramide ist vertikal zentriert, daher hier um
// -height/2 verschieben.
static Mesh ShiftMeshY(const Mesh& mesh, double dy) {
    Mesh out = mesh;
    const float d = static_cast<float>(dy);
    for (size_t i = 1; i < out.Triangles.size(); i += 3) {
        out.Triangles[i] += d;
    }
    for (size_t i = 1; i < out.Lines.size(); i += 3) {
        out.Lines[i] += d;
    }
    return out;
}

// BodyStyle liefert den Zeichen-Stil eines Bodies. Farbe und Linienbreite
// sind Render-Information und werden daher nicht im Body gespeichert,
// sondern hier in den ShapeStyle gelegt (an DrawMesh uebergeben).
static ShapeStyle BodyStyle(const ColorState& color, double lineWidth) {
    ShapeStyle s;
    s.stroke.Set = true;
    s.stroke.Color = color;
    s.fill.Set = true;
    s.fill.Color = color;
    s.lineWidth = lineWidth;
    s.mode = StyleBoth;
    return s;
}

// SceneBody paart einen Body mit seinen Darstellungsdaten.
struct SceneBody {
    Body Object;
    ColorState Color;
    double LineWidth = 1;
};

int main() {
    const int coneLines = 20;
    const double camHeight = 140;

    Scene scene;
    scene.sceneFog       = {true, 100, 600, ColorState{0.25, 0.25, 0.25, 1}};

    const double camRadius = sqrt(80 * 80 + 360 * 360); // Kamera weiter entfernt
    double timeAccum = 0;
    double coneRotY = 0;
    int frameCount = 0;
    double fpsLast = 0;

    // Geometrie: Wuerfel wie lib3d; Pyramide vertikal zentriert (wie Referenz).
    Mesh boxMesh = MakeCubeMesh(100, 80, 60);
    Mesh pyrMesh = ShiftMeshY(MakePyramidMesh(90, 120), -60);

    const ColorState red = ParseHex("#ff0000");
    const ColorState cyan = ParseHex("#00ffff");
    const ColorState white = ParseHex("#ffffff");

    vector<SceneBody> bodies{
        {MakeBody(boxMesh, {150, 0, 50}, 0, 0, 0), red, 2},
        {MakeBody(boxMesh, {0, 0, 100}, 0, 0, 0), cyan, 2},
        {MakeBody(boxMesh, {-150, 0, -100}, 0, 0, 0), red, 2},
        {MakeBody(boxMesh, {-200, 0, 30}, numbers::pi / 2, 0, 0), cyan, 2},
        {MakeBody(pyrMesh, {100, 0, -100}, 0, 0, 0), white, 1}, // Default: weiss, 1
    };

    /* ----
    Render-Loop -  draw wird als Lamda-Funktion in StartAnimation übergeben
    Variablen aus umgebenden Scope werden als Referenz übegeben [&]
    -----*/
    auto draw = [&]() {
        timeAccum += 0.02;
        frameCount++;
        const double now =
            chrono::duration<double>(chrono::steady_clock::now().time_since_epoch())
                .count();
        if (now - fpsLast >= 2.0) {
            printf("FPS: %.1f\n", frameCount / (now - fpsLast));
            frameCount = 0;
            fpsLast = now;
        }

        const double camAngle = timeAccum * 0.15;
        
        scene.Cam.Pos = {
            sin(camAngle) * camRadius,
            camHeight,
            cos(camAngle) * camRadius,
        };

        BeginScene(scene);

        // Alle Bodies zeichnen (Mesh einmal erzeugt, Modell-Matrix explizit,
        // Linienbreite kommt aus dem ShapeStyle).
        for (const auto& b : bodies) {
            DrawMesh(b.Object.geometry, BodyStyle(b.Color, b.LineWidth),
                     b.Object.ModelMatrix());
        }

        // Welt-Faces aller Bodies (pro Frame neu, wie in der Referenz).
        vector<Face> faces;
        for (const auto& b : bodies) {
            const vector<Face> wf = b.Object.WorldFaces();
            faces.insert(faces.end(), wf.begin(), wf.end());
        }

        //Faces debuggen
        for (const auto& f : faces) {
            printf("Face: A=(%.2f, %.2f, %.2f) B=(%.2f, %.2f, %.2f) C=(%.2f, %.2f, %.2f) Normal=(%.2f, %.2f, %.2f)\n",
           f.A.x, f.A.y, f.A.z,
           f.B.x, f.B.y, f.B.z,
           f.C.x, f.C.y, f.C.z,
           f.Normal.x, f.Normal.y, f.Normal.z);
        }

        // Lichtkegel rotieren.
        coneRotY += 0.01;
        const auto coneRot = RotateMatrix(0, coneRotY, 0);

        // --- Rays ---
        const Vec3 apex{};
        const double coneLen = 600.0;
        const double coneAngle = numbers::pi / 60;
        const double coneR = coneLen * sin(coneAngle);
        const double coneZ = coneLen * cos(coneAngle);

        for (int i = 0; i < coneLines; i++) {
            const double a = (2.0 * numbers::pi * i) / coneLines;
            const Vec3 end{
                apex.x + cos(a) * coneR,
                apex.y + sin(a) * coneR,
                apex.z + coneZ,
            };
            const Vec3 rotatedEnd = RotateAround(end, apex, coneRot);

            const RayHit hit = RaycastSegment(apex, rotatedEnd, faces);
            const Vec3 endpoint = hit.isHit ? hit.Point : rotatedEnd;

            DrawLine(apex.x, apex.y, apex.z, endpoint.x, endpoint.y, endpoint.z,
                     PrimitiveStyle{.stroke = ColorInput("#ff8800"), .lineWidth = 1});
            DrawPoint(endpoint.x, endpoint.y, endpoint.z,
                      PrimitiveStyle{.stroke = ColorInput("#ff0000"), .pointSize = 5});
        }
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
