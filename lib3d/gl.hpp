#pragma once

// Zentraler OpenGL-/GLFW-Include. GL_GLEXT_PROTOTYPES aktiviert die
// Core-Profil-Prototypen aus <GL/glext.h>, sodass die modernen GL-Funktionen
// direkt aus libGL gelinkt werden koennen (kein Loader wie GLEW/GLAD noetig).
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
