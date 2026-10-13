#include "shader.hpp"

#include <stdexcept>
#include <string>

// Importiert werden nur die tatsaechlich verwendeten std-Namen.
using std::runtime_error;
using std::size_t;
using std::string;

// =================================================================
// INTERNER GLSL-CODE  (Port von lib-wgl.ts)
//
// Unterschied zum WebGL-Original: Desktop OpenGL 3.3 Core. dFdx/dFdy sind
// im Core-Profil immer verfuegbar, es gibt keine Praezisions-Qualifier mehr.
// =================================================================

// Vertex-Shader: 3D-Transformation ueber View- und Projection-Matrix.
// Modus 0 (uInstanced == 0): aPos ist bereits Weltkoordinate (Modell-Matrix
//   wurde auf der CPU gebacken), Farbe kommt aus aColor.
// Modus 1 (uInstanced == 1): aPos ist lokale Mesh-Geometrie, die pro Instanz
//   mit aInstModel transformiert wird; Farbe kommt aus aInstColor.
const char* const kVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in mat4 aInstModel;
layout(location = 6) in vec4 aInstColor;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uPointSize;
uniform int uInstanced;

out vec3 vCamPos;
out vec4 vColor;

void main() {
    vec4 world;
    if (uInstanced == 1) {
        world = aInstModel * vec4(aPos, 1.0);
        vColor = aInstColor;
    } else {
        world = vec4(aPos, 1.0);
        vColor = aColor;
    }
    vec4 camPos = uView * world;
    vCamPos = camPos.xyz;
    gl_Position = uProjection * camPos;
    gl_PointSize = uPointSize;
}
)";

// Fragment-Shader:
// uMode  0 = flat      - einfache Farbe uColor
// uMode  1 = gradient  - radialer Verlauf (vom Zentrum in Kamera-Koordinaten)
// uMode  2 = pulse     - flat mit sinusfoermiger Helligkeitspulsation
// uLighted == 1 -> Flat-Shading (Beleuchtung).
const char* const kFragSrc = R"(#version 330 core

in vec3 vCamPos;
in vec4 vColor;
out vec4 fragColor;

uniform int   uMode;
uniform vec4  uColor;
uniform vec4  uColor2;
uniform float uTime;
uniform vec3  uShapeCenter;
uniform float uShapeRadius;
uniform float uFogNear;
uniform float uFogFar;
uniform vec4  uFogColor;
uniform int   uLighted;
uniform vec3  uLightDir;

void main() {
  vec3 base = uColor.rgb;

  if (uMode == 0) {
    // flat - Basis bleibt uColor (fuer flache Primitive ist uColor weiss,
    // die eigentliche Farbe steckt in vColor)
  } else if (uMode == 1) {
    float d = distance(vCamPos, uShapeCenter);
    float t = clamp(d / max(uShapeRadius, 1.0), 0.0, 1.0);
    base = mix(uColor, uColor2, t).rgb;
  } else if (uMode == 2) {
    float brightness = 0.6 + 0.4 * sin(uTime * 3.0);
    base = uColor.rgb * brightness;
  }

  base *= vColor.rgb;

  if (uLighted == 1) {
    vec3 n = normalize(cross(dFdy(vCamPos), dFdx(vCamPos)));
    float diff = max(dot(n, normalize(uLightDir)), 0.0);
    base *= 0.25 + 0.75 * diff;
  }

  float depth = vCamPos.z;
  float fog_t = clamp((depth - uFogNear) / (uFogFar - uFogNear), 0.0, 1.0);
  fragColor = mix(vec4(base, uColor.a * vColor.a), uFogColor, fog_t);
}
)";

namespace {

// GetShaderInfoLog liest das Info-Log eines Shaders.
string GetShaderInfoLog(GLuint shader) {
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength <= 0) {
        return "";
    }
    string buf(static_cast<size_t>(logLength), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, buf.data());
    return buf;
}

// GetProgramInfoLog liest das Info-Log eines Programms.
string GetProgramInfoLog(GLuint program) {
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength <= 0) {
        return "";
    }
    string buf(static_cast<size_t>(logLength), '\0');
    glGetProgramInfoLog(program, logLength, nullptr, buf.data());
    return buf;
}

// CompileShader kompiliert einen einzelnen GLSL-Shader.
GLuint CompileShader(GLenum type, const char* src) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        throw runtime_error("shader-fehler: " + GetShaderInfoLog(shader));
    }
    return shader;
}

} // namespace

GLuint CreateProgram(const char* vertSrc, const char* fragSrc) {
    const GLuint vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
    const GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);

    const GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        throw runtime_error("programm-fehler: " + GetProgramInfoLog(program));
    }
    return program;
}
