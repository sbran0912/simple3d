#include "renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>

#include "gl.hpp"
#include "shader.hpp"

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::array;
using std::cos;
using std::function;
namespace numbers = std::numbers;
using std::runtime_error;
using std::sin;
using std::size_t;
using std::vector;

namespace {

// =================================================================
// TYPEN & INTERNER ZUSTAND
// =================================================================

// DrawState ist der aktuelle Zeichenzustand (kein Stack).
struct DrawState {
    ColorState fill{1, 1, 1, 1};
    ColorState stroke{0, 0, 0, 1};
    double lineW = 1;
    EffectMode effect = EffectFlat;
    ColorState grad2{0, 0, 0, 1};
};

// DrawCmd ist ein gesammelter Zeichenbefehl fuer eine Immediate-Primitive.
struct DrawCmd {
    GLenum mode = 0; // GL-Modus (GL_POINTS/LINES/TRIANGLES/LINE_LOOP)
    int first = 0;   // Start-Index in batchVerts (Vertex-Nummer)
    int count = 0;   // Anzahl Vertices
    ColorState col;
    EffectMode effect = EffectFlat;
    ColorState grad2;
    double pointSize = 0;
    double lineW = 0;
    array<double, 3> center{0, 0, 0};
    double radius = 0;
    array<float, 16> view{};
    array<float, 16> proj{};
    int lit = 0; // 1 = gefuellte Flaeche -> beleuchten
};

// InstCmd ist ein gesammelter GPU-Instancing-Zeichenbefehl. first/count zeigen
// auf die Basis-Geometrie (instGeom), instFirst/instCount auf die Instanzen
// (instData).
struct InstCmd {
    GLenum mode = 0;
    int first = 0;
    int count = 0;
    int instFirst = 0;
    int instCount = 0;
    EffectMode effect = EffectFlat;
    ColorState grad2;
    double pointSize = 0;
    double lineW = 0;
    array<double, 3> center{0, 0, 0};
    double radius = 0;
    array<float, 16> view{};
    array<float, 16> proj{};
    int lit = 0;
};

constexpr array<float, 16> Identity16() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

// Fenster / GL
GLFWwindow* window = nullptr;
GLuint prog = 0;
GLuint vao = 0;
GLuint vbo = 0;
GLuint vboGeom = 0; // Basis-Geometrie der Instanzen
GLuint vboInst = 0; // Instanz-Daten (Modell-Matrix + Farbe)

int frameW = 0;
int frameH = 0;

// Shader-Locations
GLint locPos = -1;
GLint locColorAttr = -1;
GLint locInstModel = -1; // belegt 4 aufeinanderfolgende Locations
GLint locInstColor = -1;
GLint locInstanced = -1;
GLint locView = -1;
GLint locProjection = -1;
GLint locPointSize = -1;
GLint locMode = -1;
GLint locColor = -1;
GLint locColor2 = -1;
GLint locTime = -1;
GLint locCenter = -1;
GLint locRadius = -1;
GLint locFogNear = -1;
GLint locFogFar = -1;
GLint locFogColor = -1;
GLint locLighted = -1;
GLint locLightDir = -1;

// Animation
bool looping = true;
double startTime = 0;

// Maus
double mouseX = 0;
double mouseY = 0;
int mouseStatus = 0;

// Aktueller Zeichenzustand.
DrawState state{};

// Batching: batchVerts ist interleaved (x,y,z,r,g,b,a = 7 floats/Vertex).
vector<float> batchVerts;
vector<DrawCmd> batchCmds;
int batchCap = 0; // aktuell allozierte Byte-Groesse des VBO

// Optionale Sortierung/Umpacken der Batch (siehe SetBatchSort).
bool batchSort = false;

// Instancing: eigene Puffer fuer Basis-Geometrie und Instanz-Daten.
vector<float> instGeom; // interleaved xyz+rgbа (7 floats/Vertex)
vector<float> instData; // 16 (Modell, column-major) + 4 (Farbe) = 20 floats/Instanz
vector<InstCmd> instCmds;
int geomCap = 0;
int instCap = 0;

// Zuletzt gesetzte Uniform-Werte (fuer den Batch-Snapshot pro Befehl).
array<float, 16> viewUniform = Identity16();
array<float, 16> projUniform = Identity16();
double pointSizeVal = 4.0;
array<double, 3> gradCenter{0, 0, 0};
double gradRadius = 1.0;

// =================================================================
// HILFSFUNKTIONEN (intern)
// =================================================================

// FlattenMatrix wandelt eine row-major Matrix4x4 in ein column-major
// Float32-Array fuer OpenGL.
array<float, 16> FlattenMatrix(const Matrix4x4& m) {
    return {
        static_cast<float>(m[0][0]), static_cast<float>(m[1][0]),
        static_cast<float>(m[2][0]), static_cast<float>(m[3][0]),
        static_cast<float>(m[0][1]), static_cast<float>(m[1][1]),
        static_cast<float>(m[2][1]), static_cast<float>(m[3][1]),
        static_cast<float>(m[0][2]), static_cast<float>(m[1][2]),
        static_cast<float>(m[2][2]), static_cast<float>(m[3][2]),
        static_cast<float>(m[0][3]), static_cast<float>(m[1][3]),
        static_cast<float>(m[2][3]), static_cast<float>(m[3][3]),
    };
}

GLint EffectToMode(EffectMode effect) {
    switch (effect) {
    case EffectGradient:
        return 1;
    case EffectPulse:
        return 2;
    default:
        return 0;
    }
}

bool ArrEq(const array<float, 16>& a, const array<float, 16>& b) {
    for (size_t i = 0; i < 16; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

// CanMerge prueft, ob zwei aufeinanderfolgende Befehle identischen Zustand
// haben und zu einem DrawArrays-Aufruf zusammengefasst werden koennen.
// LINE_LOOP wird nie zusammengefasst, weil ein Loop sich selbst schliesst.
bool CanMerge(const DrawCmd& a, const DrawCmd& b) {
    return a.mode == b.mode &&
           a.mode != GL_LINE_LOOP &&
           ColEq(a.col, b.col) &&
           a.effect == b.effect &&
           ColEq(a.grad2, b.grad2) &&
           a.pointSize == b.pointSize &&
           a.lineW == b.lineW &&
           a.center[0] == b.center[0] &&
           a.center[1] == b.center[1] &&
           a.center[2] == b.center[2] &&
           a.radius == b.radius &&
           ArrEq(a.view, b.view) &&
           ArrEq(a.proj, b.proj) &&
           a.lit == b.lit;
}

// CmdKeyLess ist eine Sortier-Ordnung ueber den (fuer das Merging relevanten)
// Zustand eines Kommandos. view/proj sind pro Frame konstant und werden daher
// nicht verglichen.
bool CmdKeyLess(const DrawCmd& a, const DrawCmd& b) {
    if (a.mode != b.mode) return a.mode < b.mode;
    if (a.lit != b.lit) return a.lit < b.lit;
    if (a.effect != b.effect) return a.effect < b.effect;
    if (a.lineW != b.lineW) return a.lineW < b.lineW;
    if (a.pointSize != b.pointSize) return a.pointSize < b.pointSize;
    if (a.radius != b.radius) return a.radius < b.radius;
    if (a.center[0] != b.center[0]) return a.center[0] < b.center[0];
    if (a.center[1] != b.center[1]) return a.center[1] < b.center[1];
    if (a.center[2] != b.center[2]) return a.center[2] < b.center[2];
    if (a.col.R != b.col.R) return a.col.R < b.col.R;
    if (a.col.G != b.col.G) return a.col.G < b.col.G;
    if (a.col.B != b.col.B) return a.col.B < b.col.B;
    if (a.col.A != b.col.A) return a.col.A < b.col.A;
    if (a.grad2.R != b.grad2.R) return a.grad2.R < b.grad2.R;
    if (a.grad2.G != b.grad2.G) return a.grad2.G < b.grad2.G;
    if (a.grad2.B != b.grad2.B) return a.grad2.B < b.grad2.B;
    if (a.grad2.A != b.grad2.A) return a.grad2.A < b.grad2.A;
    return false;
}

// MergeBlock haengt den Vertex-Block von src an dst an und legt (bei gleichem
// Zustand wie der letzte Befehl) die Kommandos zusammen.
void MergeBlock(vector<float>& dstData, vector<DrawCmd>& dstCmds, const DrawCmd& src,
                const vector<float>& srcData) {
    DrawCmd c = src;
    const float* begin = srcData.data() + static_cast<size_t>(c.first) * 7;
    const float* end = begin + static_cast<size_t>(c.count) * 7;
    c.first = static_cast<int>(dstData.size() / 7);
    dstData.insert(dstData.end(), begin, end);
    if (!dstCmds.empty() && CanMerge(dstCmds.back(), c)) {
        dstCmds.back().count += c.count;
    } else {
        dstCmds.push_back(c);
    }
}

// =================================================================
// BATCHING - SAMMELN & ZEICHNEN (Kern des Renderers)
// =================================================================

// Submit sammelt eine Immediate-Primitive im Frame-Batch. Es wird noch nichts
// an die GPU geschickt; das passiert erst beim FlushBatch(). Die Modell-Matrix
// wird hier in die Vertices gebacken und die Farbe pro Vertex abgelegt.
void Submit(GLenum mode, const vector<float>& verts, bool useStroke,
            const Matrix4x4& model) {
    const ColorState col = useStroke ? state.stroke : state.fill;
    const int lit = useStroke ? 0 : 1; // gefuellte Flaechen werden beleuchtet
    const bool flat = state.effect == EffectFlat;

    // Flache Primitive tragen ihre Farbe pro Vertex (uColor bleibt weiss);
    // Gradient/Pulse nutzen uColor (Vertex-Farbe bleibt weiss).
    const ColorState vertexCol = flat ? col : ColorState{1, 1, 1, 1};
    const ColorState uniformCol = flat ? ColorState{1, 1, 1, 1} : col;

    DrawCmd cmd;
    cmd.mode = mode;
    cmd.first = static_cast<int>(batchVerts.size() / 7);
    cmd.count = static_cast<int>(verts.size() / 3);
    cmd.col = uniformCol;
    cmd.effect = state.effect;
    cmd.grad2 = state.grad2;
    cmd.pointSize = pointSizeVal;
    cmd.lineW = state.lineW;
    cmd.center = gradCenter;
    cmd.radius = gradRadius;
    cmd.view = viewUniform;
    cmd.proj = projUniform;
    cmd.lit = lit;

    // Vertices mit der Modell-Matrix backen und mit Farbe interleaven.
    const Matrix4x4& m = model;
    const size_t n = verts.size() / 3;
    batchVerts.reserve(batchVerts.size() + n * 7);
    for (size_t i = 0; i < n; i++) {
        const double x = verts[i * 3 + 0];
        const double y = verts[i * 3 + 1];
        const double z = verts[i * 3 + 2];
        batchVerts.push_back(
            static_cast<float>(m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3]));
        batchVerts.push_back(
            static_cast<float>(m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3]));
        batchVerts.push_back(
            static_cast<float>(m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3]));
        batchVerts.push_back(static_cast<float>(vertexCol.R));
        batchVerts.push_back(static_cast<float>(vertexCol.G));
        batchVerts.push_back(static_cast<float>(vertexCol.B));
        batchVerts.push_back(static_cast<float>(vertexCol.A));
    }

    // Aufeinanderfolgende Befehle mit identischem Zustand zusammenfassen.
    if (!batchCmds.empty() && CanMerge(batchCmds.back(), cmd)) {
        batchCmds.back().count += cmd.count;
        return;
    }
    batchCmds.push_back(cmd);
}

// FlushInstanced zeichnet alle gesammelten Instancing-Auftraege. Basis-
// Geometrie und Instanz-Daten liegen in eigenen, wiederverwendeten VBOs.
void FlushInstanced() {
    if (instCmds.empty()) {
        return;
    }

    glBindVertexArray(vao);

    // Basis-Geometrie hochladen.
    glBindBuffer(GL_ARRAY_BUFFER, vboGeom);
    const int geomBytes = static_cast<int>(instGeom.size() * sizeof(float));
    if (geomBytes > geomCap) {
        glBufferData(GL_ARRAY_BUFFER, geomBytes, instGeom.data(), GL_DYNAMIC_DRAW);
        geomCap = geomBytes;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, geomBytes, instGeom.data());
    }
    const GLsizei gStride = 7 * static_cast<GLsizei>(sizeof(float));
    glEnableVertexAttribArray(static_cast<GLuint>(locPos));
    glEnableVertexAttribArray(static_cast<GLuint>(locColorAttr));
    glVertexAttribPointer(static_cast<GLuint>(locPos), 3, GL_FLOAT, GL_FALSE, gStride, nullptr);
    glVertexAttribPointer(static_cast<GLuint>(locColorAttr), 4, GL_FLOAT, GL_FALSE, gStride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glVertexAttribDivisor(static_cast<GLuint>(locPos), 0);
    glVertexAttribDivisor(static_cast<GLuint>(locColorAttr), 0);

    // Instanz-Daten hochladen (20 floats/Instanz).
    glBindBuffer(GL_ARRAY_BUFFER, vboInst);
    const int instBytes = static_cast<int>(instData.size() * sizeof(float));
    if (instBytes > instCap) {
        glBufferData(GL_ARRAY_BUFFER, instBytes, instData.data(), GL_DYNAMIC_DRAW);
        instCap = instBytes;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instBytes, instData.data());
    }
    const GLsizei iStride = 20 * static_cast<GLsizei>(sizeof(float));
    for (int k = 0; k < 4; k++) {
        const GLuint loc = static_cast<GLuint>(locInstModel + k);
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, iStride,
                              reinterpret_cast<const void*>(k * 4 * sizeof(float)));
        glVertexAttribDivisor(loc, 1);
    }
    glEnableVertexAttribArray(static_cast<GLuint>(locInstColor));
    glVertexAttribPointer(static_cast<GLuint>(locInstColor), 4, GL_FLOAT, GL_FALSE, iStride,
                          reinterpret_cast<const void*>(16 * sizeof(float)));
    glVertexAttribDivisor(static_cast<GLuint>(locInstColor), 1);

    glUniform1i(locInstanced, 1);
    glUniform4f(locColor, 1, 1, 1, 1);
    for (const InstCmd& c : instCmds) {
        glUniformMatrix4fv(locView, 1, GL_FALSE, c.view.data());
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, c.proj.data());
        glUniform1i(locMode, EffectToMode(c.effect));
        glUniform1i(locLighted, c.lit);
        glUniform4f(locColor2, static_cast<float>(c.grad2.R), static_cast<float>(c.grad2.G),
                    static_cast<float>(c.grad2.B), static_cast<float>(c.grad2.A));
        glUniform1f(locPointSize, static_cast<float>(c.pointSize));
        glUniform3f(locCenter, static_cast<float>(c.center[0]),
                    static_cast<float>(c.center[1]), static_cast<float>(c.center[2]));
        glUniform1f(locRadius, static_cast<float>(c.radius));
        glLineWidth(static_cast<float>(c.lineW));
        glDrawArraysInstanced(c.mode, c.first, c.count, c.instCount);
    }
    glUniform1i(locInstanced, 0);

    // Divisoren/Attribute zuruecksetzen, damit normale Draws sauber bleiben.
    for (int k = 0; k < 4; k++) {
        glVertexAttribDivisor(static_cast<GLuint>(locInstModel + k), 0);
        glDisableVertexAttribArray(static_cast<GLuint>(locInstModel + k));
    }
    glVertexAttribDivisor(static_cast<GLuint>(locInstColor), 0);
    glDisableVertexAttribArray(static_cast<GLuint>(locInstColor));

    instGeom.clear();
    instData.clear();
    instCmds.clear();
}

// FlushBatch zeichnet alle gesammelten Primitives des Frames mit einem
// einzigen wiederverwendbaren VBO.
void FlushBatch() {
    if (batchCmds.empty() && instCmds.empty()) {
        return;
    }

    CompactBatch();

    glUniform1i(locInstanced, 0);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    const int byteLen = static_cast<int>(batchVerts.size() * sizeof(float));
    if (byteLen > batchCap) {
        glBufferData(GL_ARRAY_BUFFER, byteLen, batchVerts.data(), GL_DYNAMIC_DRAW);
        batchCap = byteLen;
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, byteLen, batchVerts.data());
    }

    glEnableVertexAttribArray(static_cast<GLuint>(locPos));
    glEnableVertexAttribArray(static_cast<GLuint>(locColorAttr));
    const GLsizei stride = 7 * static_cast<GLsizei>(sizeof(float));
    glVertexAttribPointer(static_cast<GLuint>(locPos), 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glVertexAttribPointer(static_cast<GLuint>(locColorAttr), 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));

    for (const DrawCmd& c : batchCmds) {
        glUniformMatrix4fv(locView, 1, GL_FALSE, c.view.data());
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, c.proj.data());
        glUniform1i(locMode, EffectToMode(c.effect));
        glUniform1i(locLighted, c.lit);
        glUniform4f(locColor, static_cast<float>(c.col.R), static_cast<float>(c.col.G),
                    static_cast<float>(c.col.B), static_cast<float>(c.col.A));
        glUniform4f(locColor2, static_cast<float>(c.grad2.R), static_cast<float>(c.grad2.G),
                    static_cast<float>(c.grad2.B), static_cast<float>(c.grad2.A));
        glUniform1f(locPointSize, static_cast<float>(c.pointSize));
        glUniform3f(locCenter, static_cast<float>(c.center[0]),
                    static_cast<float>(c.center[1]), static_cast<float>(c.center[2]));
        glUniform1f(locRadius, static_cast<float>(c.radius));
        glLineWidth(static_cast<float>(c.lineW));
        glDrawArrays(c.mode, c.first, c.count);
    }

    batchVerts.clear();
    batchCmds.clear();

    FlushInstanced();
}

// =================================================================
// MAUS / FENSTER (intern)
// =================================================================

void OnCursorPos(GLFWwindow* /*win*/, double x, double y) {
    mouseX = x - frameW / 2.0;
    mouseY = -(y - frameH / 2.0);
}

void OnMouseButton(GLFWwindow* /*win*/, int button, int action, int /*mods*/) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }
    if (action == GLFW_PRESS) {
        mouseStatus = 1;
    } else if (action == GLFW_RELEASE) {
        mouseStatus = 2;
    }
}

void OnFramebufferSize(GLFWwindow* /*win*/, int width, int height) {
    frameW = width;
    frameH = height;
    glViewport(0, 0, width, height);
}

} // namespace

// =================================================================
// BATCH-SAMMEL-HELFER (siehe renderer_internal.hpp)
// =================================================================

void SubmitTriangles(const vector<float>& verts, const Matrix4x4& model) {
    Submit(GL_TRIANGLES, verts, false, model);
}

void SubmitLines(const vector<float>& verts, const Matrix4x4& model) {
    Submit(GL_LINES, verts, true, model);
}

void SubmitInstanced(const vector<float>& triangles, const vector<float>& lines,
                     DrawStyle style, const vector<Instance>& instances) {
    if (instances.empty()) {
        return;
    }

    // Instanz-Daten: Modell-Matrix (column-major) + Farbe.
    const int instFirst = static_cast<int>(instData.size() / 20);
    instData.reserve(instData.size() + instances.size() * 20);
    for (const Instance& in : instances) {
        const Matrix4x4& m = in.Model;
        instData.push_back(static_cast<float>(m[0][0]));
        instData.push_back(static_cast<float>(m[1][0]));
        instData.push_back(static_cast<float>(m[2][0]));
        instData.push_back(static_cast<float>(m[3][0]));
        instData.push_back(static_cast<float>(m[0][1]));
        instData.push_back(static_cast<float>(m[1][1]));
        instData.push_back(static_cast<float>(m[2][1]));
        instData.push_back(static_cast<float>(m[3][1]));
        instData.push_back(static_cast<float>(m[0][2]));
        instData.push_back(static_cast<float>(m[1][2]));
        instData.push_back(static_cast<float>(m[2][2]));
        instData.push_back(static_cast<float>(m[3][2]));
        instData.push_back(static_cast<float>(m[0][3]));
        instData.push_back(static_cast<float>(m[1][3]));
        instData.push_back(static_cast<float>(m[2][3]));
        instData.push_back(static_cast<float>(m[3][3]));
        instData.push_back(static_cast<float>(in.Color.R));
        instData.push_back(static_cast<float>(in.Color.G));
        instData.push_back(static_cast<float>(in.Color.B));
        instData.push_back(static_cast<float>(in.Color.A));
    }
    const int instCount = static_cast<int>(instances.size());

    // Basis-Geometrie anhaengen (Farbe weiss; pro Instanz ueberschrieben).
    const auto appendGeom = [](vector<float>& dst, const vector<float>& src) {
        for (size_t i = 0; i + 2 < src.size(); i += 3) {
            dst.push_back(src[i]);
            dst.push_back(src[i + 1]);
            dst.push_back(src[i + 2]);
            dst.push_back(1);
            dst.push_back(1);
            dst.push_back(1);
            dst.push_back(1);
        }
    };

    if ((style == StyleFill || style == StyleBoth) && !triangles.empty()) {
        InstCmd c;
        c.mode = GL_TRIANGLES;
        c.first = static_cast<int>(instGeom.size() / 7);
        c.count = static_cast<int>(triangles.size() / 3);
        appendGeom(instGeom, triangles);
        c.instFirst = instFirst;
        c.instCount = instCount;
        c.effect = state.effect;
        c.grad2 = state.grad2;
        c.pointSize = pointSizeVal;
        c.lineW = state.lineW;
        c.center = gradCenter;
        c.radius = gradRadius;
        c.view = viewUniform;
        c.proj = projUniform;
        c.lit = 1;
        instCmds.push_back(c);
    }
    if ((style == StyleStroke || style == StyleBoth) && !lines.empty()) {
        InstCmd c;
        c.mode = GL_LINES;
        c.first = static_cast<int>(instGeom.size() / 7);
        c.count = static_cast<int>(lines.size() / 3);
        appendGeom(instGeom, lines);
        c.instFirst = instFirst;
        c.instCount = instCount;
        c.effect = state.effect;
        c.grad2 = state.grad2;
        c.pointSize = pointSizeVal;
        c.lineW = state.lineW;
        c.center = gradCenter;
        c.radius = gradRadius;
        c.view = viewUniform;
        c.proj = projUniform;
        c.lit = 0;
        instCmds.push_back(c);
    }
}

// =================================================================
// OEFFENTLICHE API - Setup & Matrizen
// =================================================================

int GetWidth() { return frameW; }
int GetHeight() { return frameH; }

void NoLoop() { looping = false; }

bool IsMouseDown() { return mouseStatus == 1; }

bool IsMouseUp() {
    if (mouseStatus == 2) {
        mouseStatus = 0;
        return true;
    }
    return false;
}

void SetProjection(const Matrix4x4& m) {
    projUniform = FlattenMatrix(m);
    glUniformMatrix4fv(locProjection, 1, GL_FALSE, projUniform.data());
}

void SetView(const Matrix4x4& m) {
    viewUniform = FlattenMatrix(m);
    glUniformMatrix4fv(locView, 1, GL_FALSE, viewUniform.data());
}

int GetPendingDrawCalls() { return static_cast<int>(batchCmds.size()); }

int GetPendingInstancedDrawCalls() { return static_cast<int>(instCmds.size()); }

void SetBatchSort(bool on) { batchSort = on; }

void CompactBatch() {
    if (!batchSort || batchCmds.size() < 2) {
        return;
    }

    vector<int> order(batchCmds.size());
    for (size_t i = 0; i < order.size(); i++) {
        order[i] = static_cast<int>(i);
    }
    std::stable_sort(order.begin(), order.end(), [](int x, int y) {
        return CmdKeyLess(batchCmds[x], batchCmds[y]);
    });

    vector<float> newData;
    newData.reserve(batchVerts.size());
    vector<DrawCmd> newCmds;
    for (int idx : order) {
        MergeBlock(newData, newCmds, batchCmds[idx], batchVerts);
    }
    batchVerts.swap(newData);
    batchCmds.swap(newCmds);
}

void SetGradientCenter(double cx, double cy, double cz, double radius) {
    gradCenter = {cx, cy, cz};
    gradRadius = radius;
    glUniform3f(locCenter, static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
    glUniform1f(locRadius, static_cast<float>(radius));
}

void SetFog(double near, double far, double r, double g, double b, double a) {
    glUniform1f(locFogNear, static_cast<float>(near));
    glUniform1f(locFogFar, static_cast<float>(far));
    glUniform4f(locFogColor, static_cast<float>(r), static_cast<float>(g),
                static_cast<float>(b), static_cast<float>(a));
}

void SetLightDirection(double x, double y, double z) {
    glUniform3f(locLightDir, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

void Init(int w, int h) {
    if (!glfwInit()) {
        throw runtime_error("glfwInit fehlgeschlagen");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window = glfwCreateWindow(w, h, "3D Renderer", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw runtime_error("Fenster konnte nicht erstellt werden");
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    prog = CreateProgram(kVertSrc, kFragSrc);
    glUseProgram(prog);

    locPos = glGetAttribLocation(prog, "aPos");
    locColorAttr = glGetAttribLocation(prog, "aColor");
    locInstModel = glGetAttribLocation(prog, "aInstModel");
    locInstColor = glGetAttribLocation(prog, "aInstColor");
    locInstanced = glGetUniformLocation(prog, "uInstanced");
    locView = glGetUniformLocation(prog, "uView");
    locProjection = glGetUniformLocation(prog, "uProjection");
    locPointSize = glGetUniformLocation(prog, "uPointSize");
    locMode = glGetUniformLocation(prog, "uMode");
    locColor = glGetUniformLocation(prog, "uColor");
    locColor2 = glGetUniformLocation(prog, "uColor2");
    locTime = glGetUniformLocation(prog, "uTime");
    locCenter = glGetUniformLocation(prog, "uShapeCenter");
    locRadius = glGetUniformLocation(prog, "uShapeRadius");
    locFogNear = glGetUniformLocation(prog, "uFogNear");
    locFogFar = glGetUniformLocation(prog, "uFogFar");
    locFogColor = glGetUniformLocation(prog, "uFogColor");
    locLighted = glGetUniformLocation(prog, "uLighted");
    locLightDir = glGetUniformLocation(prog, "uLightDir");

    // VAO/VBO anlegen (OpenGL 3.3 Core benoetigt ein VAO).
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &vboGeom);
    glGenBuffers(1, &vboInst);

    glUniform1f(locPointSize, 4.0f);
    glUniform3f(locCenter, 0, 0, 0);
    glUniform1f(locRadius, 1);
    glUniform1i(locLighted, 0);
    glUniform1i(locInstanced, 0);
    glUniform3f(locLightDir, 0, 0, -1); // Headlight: Kamera blickt +Z

    const array<float, 16> ident = Identity16();
    glUniformMatrix4fv(locProjection, 1, GL_FALSE, ident.data());
    glUniformMatrix4fv(locView, 1, GL_FALSE, ident.data());

    // Fog-Defaults (kein Nebel)
    glUniform1f(locFogNear, 0);
    glUniform1f(locFogFar, 1);
    glUniform4f(locFogColor, 0, 0, 0, 0);

    // Depth-Testing + Alpha-Blending.
    // LEQUAL (statt LESS): koplanare Kanten bleiben ueber ihren eigenen
    // Flaechen sichtbar.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    frameW = w;
    frameH = h;
    glViewport(0, 0, w, h);
    startTime = glfwGetTime();

    // Batch-Zustand initialisieren.
    state = DrawState{};
    viewUniform = Identity16();
    projUniform = Identity16();
    batchSort = false;
    geomCap = 0;
    instCap = 0;
    instGeom.clear();
    instData.clear();
    instCmds.clear();
    pointSizeVal = 4.0;
    gradCenter = {0, 0, 0};
    gradRadius = 1;
    batchCap = 0;
    batchVerts.clear();
    batchCmds.clear();

    // Eingabe-Callbacks.
    glfwSetCursorPosCallback(window, OnCursorPos);
    glfwSetMouseButtonCallback(window, OnMouseButton);
    glfwSetFramebufferSizeCallback(window, OnFramebufferSize);
}

void StartAnimation(const function<void()>& fnDraw) {
    looping = true;
    while (looping && !glfwWindowShouldClose(window)) {
        // BeginFrame
        const double t = glfwGetTime() - startTime;
        glUniform1f(locTime, static_cast<float>(t));

        fnDraw();

        // EndFrame: alle gesammelten Primitives des Frames zeichnen
        FlushBatch();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}

// =================================================================
// OEFFENTLICHE API - Zeichenzustand
// =================================================================

void FillColor(const ColorState& color) { state.fill = color; }
void FillColor(const ColorInput& color) { FillColor(color.Color); }

void StrokeColor(const ColorState& color) { state.stroke = color; }
void StrokeColor(const ColorInput& color) { StrokeColor(color.Color); }

void StrokeWidth(double w) { state.lineW = w; }

void SetEffect(EffectMode effect) { state.effect = effect; }

void SetGradient(const ColorState& color) { state.grad2 = color; }
void SetGradient(const ColorInput& color) { SetGradient(color.Color); }

void Background(const ColorState& color) {
    FlushBatch();
    glClearColor(static_cast<float>(color.R), static_cast<float>(color.G),
                 static_cast<float>(color.B), static_cast<float>(color.A));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Background(const ColorInput& color) { Background(color.Color); }

// =================================================================
// OEFFENTLICHE API - 3D-Primitive
// =================================================================

void PointSize(double px) {
    pointSizeVal = px;
    glUniform1f(locPointSize, static_cast<float>(px));
}

void Point(double x, double y, double z, const Matrix4x4& model) {
    Submit(GL_POINTS, {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)},
           true, model);
}

void Line(double x1, double y1, double z1, double x2, double y2, double z2,
          const Matrix4x4& model) {
    Submit(GL_LINES,
           {static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1),
            static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2)},
           true, model);
}

void Triangle(double x1, double y1, double z1,
              double x2, double y2, double z2,
              double x3, double y3, double z3,
              DrawStyle style, const Matrix4x4& model) {
    const vector<float> pts{
        static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1),
        static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2),
        static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
    };

    if (style == StyleFill || style == StyleBoth) {
        Submit(GL_TRIANGLES, pts, false, model);
    }
    if (style == StyleStroke || style == StyleBoth) {
        Submit(GL_LINES,
               {static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1),
                static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2),
                static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2),
                static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
                static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
                static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1)},
               true, model);
    }
}

void Shape(double x1, double y1, double z1,
           double x2, double y2, double z2,
           double x3, double y3, double z3,
           double x4, double y4, double z4,
           DrawStyle style, const Matrix4x4& model) {
    if (style == StyleFill || style == StyleBoth) {
        Submit(GL_TRIANGLES,
               {static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1),
                static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2),
                static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
                static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1),
                static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
                static_cast<float>(x4), static_cast<float>(y4), static_cast<float>(z4)},
               false, model);
    }
    if (style == StyleStroke || style == StyleBoth) {
        Submit(GL_LINES,
               {static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1),
                static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2),
                static_cast<float>(x2), static_cast<float>(y2), static_cast<float>(z2),
                static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
                static_cast<float>(x3), static_cast<float>(y3), static_cast<float>(z3),
                static_cast<float>(x4), static_cast<float>(y4), static_cast<float>(z4),
                static_cast<float>(x4), static_cast<float>(y4), static_cast<float>(z4),
                static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(z1)},
               true, model);
    }
}

void Rect(double x, double y, double w, double h, DrawStyle style, double z,
          const Matrix4x4& model) {
    const double hw = w / 2;
    const double hh = h / 2;
    Shape(x - hw, y - hh, z, x + hw, y - hh, z, x + hw, y + hh, z, x - hw, y + hh, z, style,
          model);
}

void Circle(double x, double y, double z, double radius, DrawStyle style, int segments,
            const Matrix4x4& model) {
    const double tau = 2 * numbers::pi;

    if (style == StyleFill || style == StyleBoth) {
        vector<float> fillVerts;
        fillVerts.reserve(static_cast<size_t>(segments) * 9);
        for (int i = 0; i < segments; i++) {
            const double a0 = (static_cast<double>(i) / segments) * tau;
            const double a1 = (static_cast<double>(i + 1) / segments) * tau;
            fillVerts.push_back(static_cast<float>(x));
            fillVerts.push_back(static_cast<float>(y));
            fillVerts.push_back(static_cast<float>(z));
            fillVerts.push_back(static_cast<float>(x + cos(a0) * radius));
            fillVerts.push_back(static_cast<float>(y + sin(a0) * radius));
            fillVerts.push_back(static_cast<float>(z));
            fillVerts.push_back(static_cast<float>(x + cos(a1) * radius));
            fillVerts.push_back(static_cast<float>(y + sin(a1) * radius));
            fillVerts.push_back(static_cast<float>(z));
        }
        Submit(GL_TRIANGLES, fillVerts, false, model);
    }

    if (style == StyleStroke || style == StyleBoth) {
        vector<float> strokeVerts;
        strokeVerts.reserve(static_cast<size_t>(segments) * 3);
        for (int i = 0; i < segments; i++) {
            const double a = (static_cast<double>(i) / segments) * tau;
            strokeVerts.push_back(static_cast<float>(x + cos(a) * radius));
            strokeVerts.push_back(static_cast<float>(y + sin(a) * radius));
            strokeVerts.push_back(static_cast<float>(z));
        }
        Submit(GL_LINE_LOOP, strokeVerts, true, model);
    }
}

void Polygon(const vector<double>& pts, DrawStyle style, const Matrix4x4& model) {
    if (pts.size() < 4) {
        return;
    }
    const int n = static_cast<int>(pts.size() / 3);

    if (style == StyleFill || style == StyleBoth) {
        vector<float> fillVerts;
        for (int i = 1; i < n - 1; i++) {
            fillVerts.push_back(static_cast<float>(pts[0]));
            fillVerts.push_back(static_cast<float>(pts[1]));
            fillVerts.push_back(static_cast<float>(pts[2]));
            fillVerts.push_back(static_cast<float>(pts[i * 3]));
            fillVerts.push_back(static_cast<float>(pts[i * 3 + 1]));
            fillVerts.push_back(static_cast<float>(pts[i * 3 + 2]));
            fillVerts.push_back(static_cast<float>(pts[i * 3 + 3]));
            fillVerts.push_back(static_cast<float>(pts[i * 3 + 4]));
            fillVerts.push_back(static_cast<float>(pts[i * 3 + 5]));
        }
        Submit(GL_TRIANGLES, fillVerts, false, model);
    }
    if (style == StyleStroke || style == StyleBoth) {
        vector<float> strokeVerts(pts.size());
        for (size_t i = 0; i < pts.size(); i++) {
            strokeVerts[i] = static_cast<float>(pts[i]);
        }
        Submit(GL_LINE_LOOP, strokeVerts, true, model);
    }
}
