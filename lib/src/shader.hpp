#pragma once

#include "gl.hpp"

// Eingebauter GLSL-Code (Vertex/Fragment) des Standard-Renderers.
extern const char* const kVertSrc;
extern const char* const kFragSrc;

// CreateProgram verknuepft Vertex- und Fragment-Shader zu einem GL-Programm.
// Wirft runtime_error bei Compile-/Link-Fehlern.
GLuint CreateProgram(const char* vertSrc, const char* fragSrc);
