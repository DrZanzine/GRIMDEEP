// =============================================================================
//  ui.h — mini interface 2D : rectangles + police bitmap 5x7 (sans dépendance)
// =============================================================================
#pragma once
#include <GL/glew.h>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>

struct UI {
    GLuint prog = 0, vao = 0, vbo = 0; std::vector<float> v; int W = 1, H = 1;

    static const char* glyph(char c) {
        switch (c) {
            case 'A': return "01110 10001 10001 11111 10001 10001 10001";
            case 'B': return "11110 10001 10001 11110 10001 10001 11110";
            case 'C': return "01110 10001 10000 10000 10000 10001 01110";
            case 'D': return "11110 10001 10001 10001 10001 10001 11110";
            case 'E': return "11111 10000 10000 11110 10000 10000 11111";
            case 'F': return "11111 10000 10000 11110 10000 10000 10000";
            case 'G': return "01110 10001 10000 10111 10001 10001 01111";
            case 'H': return "10001 10001 10001 11111 10001 10001 10001";
            case 'I': return "01110 00100 00100 00100 00100 00100 01110";
            case 'J': return "00111 00010 00010 00010 00010 10010 01100";
            case 'K': return "10001 10010 10100 11000 10100 10010 10001";
            case 'L': return "10000 10000 10000 10000 10000 10000 11111";
            case 'M': return "10001 11011 10101 10101 10001 10001 10001";
            case 'N': return "10001 11001 10101 10011 10001 10001 10001";
            case 'O': return "01110 10001 10001 10001 10001 10001 01110";
            case 'P': return "11110 10001 10001 11110 10000 10000 10000";
            case 'Q': return "01110 10001 10001 10001 10101 10010 01101";
            case 'R': return "11110 10001 10001 11110 10100 10010 10001";
            case 'S': return "01111 10000 10000 01110 00001 00001 11110";
            case 'T': return "11111 00100 00100 00100 00100 00100 00100";
            case 'U': return "10001 10001 10001 10001 10001 10001 01110";
            case 'V': return "10001 10001 10001 10001 10001 01010 00100";
            case 'W': return "10001 10001 10001 10101 10101 11011 10001";
            case 'X': return "10001 10001 01010 00100 01010 10001 10001";
            case 'Y': return "10001 10001 01010 00100 00100 00100 00100";
            case 'Z': return "11111 00001 00010 00100 01000 10000 11111";
            case '0': return "01110 10001 10011 10101 11001 10001 01110";
            case '1': return "00100 01100 00100 00100 00100 00100 01110";
            case '2': return "01110 10001 00001 00010 00100 01000 11111";
            case '3': return "11110 00001 00001 01110 00001 00001 11110";
            case '4': return "00010 00110 01010 10010 11111 00010 00010";
            case '5': return "11111 10000 11110 00001 00001 10001 01110";
            case '6': return "00110 01000 10000 11110 10001 10001 01110";
            case '7': return "11111 00001 00010 00100 01000 01000 01000";
            case '8': return "01110 10001 10001 01110 10001 10001 01110";
            case '9': return "01110 10001 10001 01111 00001 00010 01100";
            case '.': return "00000 00000 00000 00000 00000 01100 01100";
            case ':': return "00000 01100 01100 00000 01100 01100 00000";
            case '/': return "00001 00010 00010 00100 01000 01000 10000";
            case '!': return "00100 00100 00100 00100 00100 00000 00100";
            case '-': return "00000 00000 00000 11111 00000 00000 00000";
            case ',': return "00000 00000 00000 00000 01100 00100 01000";
            case '\'': return "00100 00100 01000 00000 00000 00000 00000";
            case '?': return "01110 10001 00001 00010 00100 00000 00100";
            case '%': return "11001 11010 00010 00100 01000 01011 10011";
            case '+': return "00000 00100 00100 11111 00100 00100 00000";
            case '(': return "00010 00100 01000 01000 01000 00100 00010";
            case ')': return "01000 00100 00010 00010 00010 00100 01000";
            case '<': return "00010 00100 01000 10000 01000 00100 00010";
            case '>': return "01000 00100 00010 00001 00010 00100 01000";
            case '=': return "00000 00000 11111 00000 11111 00000 00000";
            case '_': return "00000 00000 00000 00000 00000 00000 11111";
            case '*': return "00000 10101 01110 11111 01110 10101 00000";
            case '"': return "01010 01010 01010 00000 00000 00000 00000";
            case '#': return "01010 11111 01010 01010 11111 01010 00000";
            default: return nullptr;
        }
    }

    void init() {
        const char* vs = "#version 430 core\nlayout(location=0) in vec2 p; layout(location=1) in vec4 c; uniform vec2 uRes; out vec4 vc;\n"
                         "void main(){ vec2 n=p/uRes*2.0-1.0; gl_Position=vec4(n.x,-n.y,0.0,1.0); vc=c; }\n";
        const char* fs = "#version 430 core\nin vec4 vc; out vec4 fc; void main(){ fc=vc; }\n";
        auto comp = [](GLenum t, const char* s) { GLuint h = glCreateShader(t); glShaderSource(h, 1, &s, nullptr); glCompileShader(h); return h; };
        GLuint a = comp(GL_VERTEX_SHADER, vs), b = comp(GL_FRAGMENT_SHADER, fs);
        prog = glCreateProgram(); glAttachShader(prog, a); glAttachShader(prog, b); glLinkProgram(prog); glDeleteShader(a); glDeleteShader(b);
        glGenVertexArrays(1, &vao); glBindVertexArray(vao); glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 24, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 24, (void*)8);
    }
    void begin(int w, int h) { v.clear(); W = w; H = h; }
    void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
        float q[36] = {x, y, r, g, b, a, x + w, y, r, g, b, a, x + w, y + h, r, g, b, a, x, y, r, g, b, a, x + w, y + h, r, g, b, a, x, y + h, r, g, b, a};
        v.insert(v.end(), q, q + 36);
    }
    // rectangle à dégradé (couleurs aux 4 coins : haut-gauche, haut-droit, bas-droit, bas-gauche)
    void rectGrad(float x, float y, float w, float h, const float* c00, const float* c10, const float* c11, const float* c01) {
        float q[36] = {x, y, c00[0], c00[1], c00[2], c00[3], x + w, y, c10[0], c10[1], c10[2], c10[3], x + w, y + h, c11[0], c11[1], c11[2], c11[3],
                       x, y, c00[0], c00[1], c00[2], c00[3], x + w, y + h, c11[0], c11[1], c11[2], c11[3], x, y + h, c01[0], c01[1], c01[2], c01[3]};
        v.insert(v.end(), q, q + 36);
    }
    void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float r, float g, float b, float a) {
        float q[36] = {x0, y0, r, g, b, a, x1, y1, r, g, b, a, x2, y2, r, g, b, a, x0, y0, r, g, b, a, x2, y2, r, g, b, a, x3, y3, r, g, b, a};
        v.insert(v.end(), q, q + 36);
    }
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a) {
        float q[18] = {x0, y0, r, g, b, a, x1, y1, r, g, b, a, x2, y2, r, g, b, a}; v.insert(v.end(), q, q + 18);
    }
    void line(float x0, float y0, float x1, float y1, float th, float r, float g, float b, float a) {
        float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy); if (l < 1e-4f) return;
        float nx = -dy / l * th * 0.5f, ny = dx / l * th * 0.5f;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, r, g, b, a);
    }
    void disc(float cx, float cy, float rad, float r, float g, float b, float a, int seg = 16) {
        for (int i = 0; i < seg; i++) {
            float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
            tri(cx, cy, cx + std::cos(a0) * rad, cy + std::sin(a0) * rad, cx + std::cos(a1) * rad, cy + std::sin(a1) * rad, r, g, b, a);
        }
    }
    // coeur en pixels : fill = 0 vide, 1 moitié, 2 plein
    void heart(float x, float y, float sc, int fill, float alpha = 1.0f) {
        static const char* H[7] = {"0110110", "1111111", "1111111", "1111111", "0111110", "0011100", "0001000"};
        for (int r = 0; r < 7; r++) for (int c = 0; c < 7; c++) {
            if (H[r][c] != '1') continue;
            bool full = fill == 2 || (fill == 1 && c < 3.5f);
            float px = x + c * sc, py = y + r * sc;
            rect(px - sc * 0.25f, py - sc * 0.25f, sc * 1.5f, sc * 1.5f, 0.05f, 0.0f, 0.0f, 0.8f * alpha);                           // contour
        }
        for (int r = 0; r < 7; r++) for (int c = 0; c < 7; c++) {
            if (H[r][c] != '1') continue;
            bool full = fill == 2 || (fill == 1 && c < 3.5f);
            float px = x + c * sc, py = y + r * sc;
            if (full) { bool hl = (r == 1 && (c == 1 || c == 5)) || (r == 2 && c == 1); rect(px, py, sc, sc, hl ? 1.0f : 0.86f, hl ? 0.55f : 0.08f, hl ? 0.55f : 0.1f, alpha); }
            else rect(px, py, sc, sc, 0.18f, 0.05f, 0.06f, 0.75f * alpha);
        }
    }
    float textWidth(const std::string& s, float sc) { return (float)s.size() * 6.0f * sc; }
    void text(float x, float y, float sc, const std::string& s, float r, float g, float b, float a) {
        for (char ch : s) {
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
            const char* gl = glyph(ch);
            if (gl) {
                for (int row = 0; row < 7; row++) for (int col = 0; col < 5; col++)
                    if (gl[row * 6 + col] == '1') rect(x + col * sc, y + row * sc, sc, sc, r, g, b, a);
            }
            x += 6.0f * sc;
        }
    }
    // dessin limité à un rectangle (coordonnées écran, origine en haut à gauche)
    // texte à dégradé vertical (couleur haute -> basse)
    void textGrad(float x, float y, float sc, const std::string& s, const float* top, const float* bot, float a) {
        for (char ch : s) {
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
            const char* gl = glyph(ch);
            if (gl) for (int row = 0; row < 7; row++) {
                float t = row / 6.0f, r = top[0] + (bot[0] - top[0]) * t, g = top[1] + (bot[1] - top[1]) * t, b = top[2] + (bot[2] - top[2]) * t;
                for (int col = 0; col < 5; col++) if (gl[row * 6 + col] == '1') rect(x + col * sc, y + row * sc, sc, sc, r, g, b, a);
            }
            x += 6.0f * sc;
        }
    }
    // halo lumineux autour d'un texte (copies décalées, peu opaques)
    void textGlow(float x, float y, float sc, const std::string& s, float r, float g, float b, float spread, float a) {
        for (int k = 1; k <= 3; k++) for (int d = 0; d < 8; d++) {
            float ang = d * 0.7853982f, o = spread * k * 0.5f;
            text(x + std::cos(ang) * o, y + std::sin(ang) * o, sc, s, r, g, b, a / (1.0f + k * 0.6f));
        }
    }
    void flushClip(float x, float y, float w, float h) {
        glEnable(GL_SCISSOR_TEST); glScissor((GLint)x, (GLint)(H - y - h), (GLsizei)w, (GLsizei)h);
        flush(); glDisable(GL_SCISSOR_TEST);
    }
    void flush() {
        if (v.empty()) return;
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(prog); glUniform2f(glGetUniformLocation(prog, "uRes"), (float)W, (float)H);
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6)); glDisable(GL_BLEND); v.clear();
    }
};
