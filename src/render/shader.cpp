// =============================================================================
//  render/shader.cpp — lecture des fichiers GLSL et compilation
// =============================================================================
#include "render/shader.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "core/paths.h"

namespace {

/// Lit le fichier `assets/shaders/<file>` en entier. Quitte si le fichier est introuvable.
std::string readShaderFile(const char* file) {
    const std::string path = assetPath(std::string("shaders/") + file);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "Shader introuvable : %s\n(lancez le jeu depuis la racine du projet, qui contient assets/)\n", path.c_str());
        std::exit(1);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// Compile un étage de shader ; affiche le journal et quitte en cas d'erreur.
GLuint compileShader(GLenum type, const char* file) {
    const std::string src = readShaderFile(file);
    const char* p = src.c_str();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, 4096, nullptr, log);
        std::fprintf(stderr, "Erreur shader (%s):\n%s\n", file, log);
        std::exit(1);
    }
    return s;
}

}  // namespace

GLuint loadProgram(const char* vsFile, const char* fsFile) {
    GLuint p = glCreateProgram();
    GLuint a = compileShader(GL_VERTEX_SHADER, vsFile), b = compileShader(GL_FRAGMENT_SHADER, fsFile);
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, 4096, nullptr, log);
        std::fprintf(stderr, "Erreur link (%s + %s):\n%s\n", vsFile, fsFile, log);
        std::exit(1);
    }
    glDeleteShader(a);
    glDeleteShader(b);
    return p;
}
