#include "lib3d.hpp"
#include "bodies.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
namespace numbers = std::numbers; // fuer numbers::pi
using std::fabs;
using std::printf;
using std::vector;

static int g_fails = 0;

#define EXPECT(cond, msg)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL: %s\n", msg);                                                        \
            g_fails++;                                                                             \
        }                                                                                          \
    } while (0)

static bool nearly(double a, double b, double eps) { return fabs(a - b) < eps; }

static void TestVec3CrossAndDot() {
    const Vec3 x{1, 0, 0};
    const Vec3 y{0, 1, 0};
    EXPECT(x.Cross(y).Equals(Vec3{0, 0, 1}, 1e-12), "cross = (0,0,1)");
    EXPECT(x.Dot(y) == 0, "dot = 0");
}

static void TestVec3Normalize() {
    const auto got = Vec3{3, 0, 4}.Normalize();
    EXPECT(nearly(got.Length(), 1, 1e-12) && got.Equals(Vec3{0.6, 0, 0.8}, 1e-12),
           "normalize = (0.6,0,0.8)");
    const auto zero = Vec3{}.Normalize();
    EXPECT(zero.Equals(Vec3{}, 1e-12), "zero vector normalize = zero");
}

static void TestLookAtCameraMapsToOrigin() {
    const Vec3 cam{0, 0, -100};
    const auto view = LookAtMatrix(cam, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    const auto got = cam.Transform(view);
    EXPECT(got.Equals(Vec3{0, 0, 0}, 1e-9), "Kamera -> Ursprung");
    const auto ahead = Vec3{0, 0, -90}.Transform(view);
    EXPECT(nearly(ahead.z, 10, 1e-9), "Punkt vor Kamera z = 10");
}

static void TestPerspectiveClipspace() {
    const auto proj = PerspectiveMatrix(numbers::pi / 4, 1, 0.1, 1000);
    const double near = 0.1;
    const auto v = Vec3{0, 0, near}.Transform(proj);
    EXPECT(nearly(v.z / near, -1, 1e-9), "near-Ebene z_clip/w = -1");
}

static void TestRotateMatrixY() {
    // +X um -90 Grad um Y drehen -> +Z (Richtung der Szenen-Rotation).
    const auto rot = RotateMatrix(0, -numbers::pi / 2, 0);
    const auto got = Vec3{1, 0, 0}.Transform(rot);
    EXPECT(got.Equals(Vec3{0, 0, 1}, 1e-9), "RotateY(-90) x -> z");
}

static void TestMeshGeometry() {
    const auto cube = MakeCubeMesh(2, 2, 2);
    EXPECT(cube.Triangles.size() == 6 * 2 * 3 * 3, "Wuerfel: 6 Flaechen = 36 Dreiecke");
    EXPECT(cube.Lines.size() == 12 * 2 * 3, "Wuerfel: 12 Kanten");

    const auto pyr = MakePyramidMesh(2, 3);
    EXPECT(pyr.Triangles.size() == (4 * 3 + 2 * 3) * 3, "Pyramide: 4 Seiten + Basis");
    EXPECT(pyr.Lines.size() == 8 * 2 * 3, "Pyramide: 8 Kanten");

    const auto grid = MakeGridMesh(6, 3);
    EXPECT(grid.Triangles.empty(), "Gitter: keine Dreiecke");
    EXPECT(grid.Lines.size() == 2 * (3 + 1) * 2 * 3, "Gitter: 2*(cells+1) Linien");

    const auto gridMin = MakeGridMesh(6, 0);
    EXPECT(gridMin.Lines.size() == 2 * 2 * 2 * 3, "Gitter: cells < 1 -> 1 Zelle");
}

static void TestBatchMerging() {
    // Ohne Init sammelt Submit nur (kein GL-Zugriff). Die Batch ist hier leer.

    // Phase 2: Sortierung fasst auch nicht-benachbarte Befehle zusammen.
    SetBatchSort(true);
    StrokeColor(ColorInput(200, 200, 200));
    StrokeWidth(1);
    Line(0, 0, 0, 1, 1, 1);
    StrokeWidth(9);
    Line(0, 0, 0, 1, 1, 1);
    StrokeWidth(1);
    Line(0, 0, 0, 1, 1, 1);
    StrokeWidth(9);
    Line(0, 0, 0, 1, 1, 1);
    EXPECT(GetPendingDrawCalls() == 4, "w1/w9 im Wechsel -> 4 Draw-Calls");
    CompactBatch();
    EXPECT(GetPendingDrawCalls() == 2, "sortiert -> 2 Draw-Calls");
    SetBatchSort(false);

    // 50 identische Linien -> ein einziger Draw-Call.
    StrokeColor(ColorInput(123, 45, 67));
    StrokeWidth(1);
    const int b1 = GetPendingDrawCalls();
    for (int i = 0; i < 50; i++) {
        Line(0, 0, 0, 1, 1, 1);
    }
    EXPECT(GetPendingDrawCalls() - b1 == 1, "50 gleiche Linien -> 1 Draw-Call");

    // Unterschiedliche Farben (flat) werden trotzdem gebuendelt, weil die
    // Farbe pro Vertex uebertragen wird -> keine neuen Draw-Calls.
    const int b2 = GetPendingDrawCalls();
    DrawLine(0, 0, 0, 1, 0, 0,
                    PrimitiveStyle{.stroke = ColorInput("#00ff00")});
    DrawLine(0, 0, 0, 1, 0, 0,
                    PrimitiveStyle{.stroke = ColorInput("#0000ff")});
    EXPECT(GetPendingDrawCalls() - b2 == 0, "Farbe (flat) bleibt ein Draw-Call");

    // Eine echte Zustandsaenderung (Linienbreite) trennt die Draw-Calls.
    const int b2b = GetPendingDrawCalls();
    DrawLine(0, 0, 0, 1, 0, 0,
                    PrimitiveStyle{.stroke = ColorInput("#00ff00"),
                                          .lineWidth = 5});
    EXPECT(GetPendingDrawCalls() - b2b == 1, "Linienbreite trennt Draw-Calls");

    // Ein Wuerfel -> 1 Fill-Call + 1 Line-Call.
    const ShapeStyle cube{
        .stroke = ColorInput("#111111"),
        .fill = ColorInput("#dddddd"),
        .lineWidth = 2,
        .mode = StyleBoth,
    };
    const int b3 = GetPendingDrawCalls();
    DrawMesh(MakeCubeMesh(10, 10, 10), cube);
    EXPECT(GetPendingDrawCalls() - b3 == 2, "Wuerfel -> 1 Fill + 1 Line");
}

static void TestInstancing() {
    // 10 Instanzen eines Wuerfels -> 1 Fill- + 1 Line-Instancing-Draw-Call.
    const ShapeStyle style{
        .stroke = ColorInput("#000000"),
        .fill = ColorInput("#ffffff"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    vector<Instance> instances;
    for (int i = 0; i < 10; i++) {
        Instance in;
        in.Model = TranslateMatrix(i * 5.0, 0, 0);
        in.Color = ColorState{1, 0, 0, 1};
        instances.push_back(in);
    }
    const int b = GetPendingInstancedDrawCalls();
    DrawMeshInstanced(MakeCubeMesh(2, 2, 2), style, instances);
    EXPECT(GetPendingInstancedDrawCalls() - b == 2,
           "10 Instanzen -> 1 Fill + 1 Line (instanced)");
}

static void TestParseColor() {
    const auto hex = ParseColor("#ff0000");
    EXPECT(nearly(hex.R, 1, 1e-12) && hex.G == 0 && hex.B == 0 && hex.A == 1, "hex rot");
    const auto gray = ParseColor(128);
    EXPECT(nearly(gray.R, 128.0 / 255, 1e-12) && nearly(gray.G, 128.0 / 255, 1e-12), "grau");
    const auto rgba = ParseColor(51, 255, 51, 64);
    EXPECT(nearly(rgba.G, 1, 1e-12) && nearly(rgba.A, 64.0 / 255, 1e-12), "rgba");
}

static void TestClosestPointToFace() {
    Face f;
    f.A = Vec3{0, 0, 0};
    f.B = Vec3{10, 0, 0};
    f.C = Vec3{0, 0, 10};
    f.Normal = Vec3{0, 1, 0};

    // Innerer Punkt: Projektion senkrecht nach unten in die Flaechenebene.
    const auto inside = ClosestPointToFace(Vec3{2, 5, 2}, f);
    EXPECT(inside.Point.Equals(Vec3{2, 0, 2}, 1e-9), "innen -> Punkt in der Ebene");
    EXPECT(nearly(inside.Distance, 5, 1e-9), "innen -> Abstand 5");
    EXPECT(nearly(inside.SignedDistance, 5, 1e-9), "aussen -> SignedDistance +5");
    EXPECT(inside.Normal.Equals(Vec3{0, 1, 0}, 1e-12), "Normale uebernommen");

    // Unterhalb der Ebene -> negativer gerichteter Abstand.
    const auto below = ClosestPointToFace(Vec3{2, -5, 2}, f);
    EXPECT(below.Point.Equals(Vec3{2, 0, 2}, 1e-9), "unterhalb -> Punkt in der Ebene");
    EXPECT(nearly(below.SignedDistance, -5, 1e-9), "innen -> SignedDistance -5");

    // Neben der Kante BC (x+z > 10): naechster Punkt auf der Kante.
    const auto edge = ClosestPointToFace(Vec3{6, 0, 6}, f);
    EXPECT(edge.Point.Equals(Vec3{5, 0, 5}, 1e-9), "Kante BC -> Mittelpunkt");
    EXPECT(nearly(edge.Distance, sqrt(2), 1e-9), "Kante BC -> Abstand sqrt(2)");

    // Neben der Ecke A: naechster Punkt ist die Ecke A.
    const auto corner = ClosestPointToFace(Vec3{-4, 0, -4}, f);
    EXPECT(corner.Point.Equals(Vec3{0, 0, 0}, 1e-9), "Ecke A");
    EXPECT(nearly(corner.Distance, sqrt(32), 1e-9), "Ecke A -> Abstand");

    // Senkrecht neben der Kante AC (x < 0).
    const auto ac = ClosestPointToFace(Vec3{-3, 0, 5}, f);
    EXPECT(ac.Point.Equals(Vec3{0, 0, 5}, 1e-9), "Kante AC");
    EXPECT(nearly(ac.Distance, 3, 1e-9), "Kante AC -> Abstand 3");
}

// Smoke-Test: DrawMesh sammelt nur in den Batch (kein GL-Zugriff) und darf
// ohne initialisierten Kontext nicht abstuerzen.
static void TestMeshShapes() {
    const ShapeStyle style{
        .stroke = ColorInput("#000000"),
        .fill = ColorInput("#ffffff"),
        .lineWidth = 1,
        .mode = StyleBoth,
    };
    DrawMesh(MakeCubeMesh(10, 10, 10), style);
    DrawMesh(MakePyramidMesh(10, 15), style);
    DrawMesh(MakeSphereMesh(5, 12, 6), style);
    EXPECT(true, "DrawMesh aufrufbar");
}

// Ruft einen Testblock auf und gibt aus, welcher Bestandteil geprueft wird.
static void RunTest(const char* name, void (*fn)()) {
    printf("== %s ==\n", name);
    const int before = g_fails;
    fn();
    if (g_fails == before) {
        printf("   ok\n");
    }
}

int main() {
    RunTest("Vec3: Kreuzprodukt & Skalarprodukt", TestVec3CrossAndDot);
    RunTest("Vec3: Normalisierung", TestVec3Normalize);
    RunTest("Kamera: LookAt -> Ursprung", TestLookAtCameraMapsToOrigin);
    RunTest("Projektion: Near-Ebene (Clip-Space)", TestPerspectiveClipspace);
    RunTest("Matrix: Rotation um Y", TestRotateMatrixY);
    RunTest("Meshes: Wuerfel/Pyramide (Dreiecke & Kanten)", TestMeshGeometry);
    RunTest("Batching: Zusammenfassen von Draw-Calls", TestBatchMerging);
    RunTest("Instancing: 10 Instanzen -> Draw-Calls", TestInstancing);
    RunTest("Farben: ParseColor (hex/grau/rgba)", TestParseColor);
    RunTest("Bodies: ClosestPointToFace", TestClosestPointToFace);
    RunTest("Meshes: DrawMesh-Smoke-Test", TestMeshShapes);

    if (g_fails == 0) {
        printf("Alle Tests bestanden.\n");
        return 0;
    }
    printf("%d Test(s) fehlgeschlagen.\n", g_fails);
    return 1;
}
