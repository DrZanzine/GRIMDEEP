// =============================================================================
//  CAVE TUNNEL — réseau de galeries procédural éclairé à la torche (OpenGL 4.3)
//
//  - Galerie principale sinueuse (sans auto-intersection) + galeries latérales
//    en culs-de-sac, section déformée par du bruit fBm / crêtes
//  - Soutènements en planches brutes, coffrage, échelles, planches tombées
//    -> chaque planche est un objet : collision joueur, destructible à la pioche,
//       débris physiques, effondrement des cadres
//  - Torches murales avec support en bois (planche de fixation, manche, étoupe,
//    brides), lumières vacillantes + ombres omnidirectionnelles
//  - Toiles d'araignée, vraies flaques d'eau, trous de rats et rats qui passent
//  - Roche à facettes / strates, HDR + MSAA, bloom, étalonnage, grain
//
//  Contrôles : ZQSD / WASD, souris, Maj = courir, clic gauche = coup de pioche,
//  F = vol libre, R = nouvelle grotte, F12 = capture PNG, Echap = quitter.
// =============================================================================
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <tuple>
#include <string>
#include <vector>

#include "audio.h"
#include "ui.h"

using glm::vec2; using glm::vec3; using glm::vec4; using glm::mat4; using glm::mat3;

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

static const float PI = 3.14159265358979f;

// ----------------------------------------------------------------------------
//  Bruit / aléa
// ----------------------------------------------------------------------------
static inline uint32_t H32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}
static inline float H1(int x, int y, int z, uint32_t s) {
    uint32_t h = H32((uint32_t)x * 374761393U + (uint32_t)y * 668265263U + (uint32_t)z * 2147483629U + s * 1274126177U);
    h = H32(h ^ (h >> 13));
    return (h & 0xFFFFFF) / 16777216.0f;
}
static inline float fadef(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float mixf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smooth(float a, float b, float x) {
    float t = std::min(std::max((x - a) / (b - a), 0.0f), 1.0f); return t * t * (3 - 2 * t);
}
static float vn3(float x, float y, float z, uint32_t s) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y), zi = (int)std::floor(z);
    float fx = fadef(x - xi), fy = fadef(y - yi), fz = fadef(z - zi);
    float c000 = H1(xi, yi, zi, s), c100 = H1(xi + 1, yi, zi, s), c010 = H1(xi, yi + 1, zi, s), c110 = H1(xi + 1, yi + 1, zi, s);
    float c001 = H1(xi, yi, zi + 1, s), c101 = H1(xi + 1, yi, zi + 1, s), c011 = H1(xi, yi + 1, zi + 1, s), c111 = H1(xi + 1, yi + 1, zi + 1, s);
    return mixf(mixf(mixf(c000, c100, fx), mixf(c010, c110, fx), fy), mixf(mixf(c001, c101, fx), mixf(c011, c111, fx), fy), fz);
}
static float fbm3(vec3 p, int oct, uint32_t s) {
    float a = 0.5f, sum = 0, norm = 0;
    for (int o = 0; o < oct; o++) { sum += a * vn3(p.x, p.y, p.z, s + o * 17); norm += a; p *= 2.03f; a *= 0.5f; }
    return sum / norm;
}
static float n1(float x, uint32_t s) {
    int xi = (int)std::floor(x); float f = x - xi; f = f * f * (3 - 2 * f);
    return mixf(H1(xi, 0, 0, s), H1(xi + 1, 0, 0, s), f);
}

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL) { for (int i = 0; i < 4; i++) next(); }
    uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    float f() { return (next() >> 40) / 16777216.0f; }
    float r(float a, float b) { return a + (b - a) * f(); }
    bool chance(float p) { return f() < p; }
};

// ----------------------------------------------------------------------------
//  Textures procédurales (tileables)
// ----------------------------------------------------------------------------
static float pn2(float x, float y, int px, int py, uint32_t s) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = fadef(x - xi), fy = fadef(y - yi);
    auto h = [&](int a, int b) { a = ((a % px) + px) % px; b = ((b % py) + py) % py; return H1(a, b, 0, s); };
    return mixf(mixf(h(xi, yi), h(xi + 1, yi), fx), mixf(h(xi, yi + 1), h(xi + 1, yi + 1), fx), fy);
}
static float pfbm(float u, float v, int base, int oct, uint32_t s) {
    float a = 0.5f, sum = 0, norm = 0; int p = base;
    for (int o = 0; o < oct; o++) { sum += a * pn2(u * p, v * p, p, p, s + o * 31); norm += a; a *= 0.5f; p *= 2; }
    return sum / norm;
}
struct WCell { float F1, F2, dx, dy; int id; };
static WCell worleyCell(float u, float v, int C, uint32_t s) {
    float x = u * C, y = v * C; int cx = (int)std::floor(x), cy = (int)std::floor(y);
    WCell w; w.F1 = w.F2 = 9; w.dx = w.dy = 0; w.id = 0;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int gx = cx + dx, gy = cy + dy; int wx = ((gx % C) + C) % C, wy = ((gy % C) + C) % C;
        float fx = gx + H1(wx, wy, 1, s), fy = gy + H1(wx, wy, 2, s);
        float d = std::sqrt((fx - x) * (fx - x) + (fy - y) * (fy - y));
        if (d < w.F1) { w.F2 = w.F1; w.F1 = d; w.dx = x - fx; w.dy = y - fy; w.id = wy * C + wx; }
        else if (d < w.F2) w.F2 = d;
    }
    return w;
}
static uint8_t toB(float x) { return (uint8_t)(std::min(std::max(x, 0.0f), 1.0f) * 255.0f + 0.5f); }
static uint8_t enc(float lin) { return toB(std::pow(std::min(std::max(lin, 0.0f), 1.0f), 1.0f / 2.2f)); }

static void heightToNormal(int N, const std::vector<float>& H, float strength, std::vector<uint8_t>& nrm) {
    nrm.resize((size_t)N * N * 4);
    auto h = [&](int x, int y) { return H[(size_t)((y + N) % N) * N + ((x + N) % N)]; };
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float dx = (h(x + 1, y) - h(x - 1, y)) * strength, dy = (h(x, y + 1) - h(x, y - 1)) * strength;
        vec3 n = glm::normalize(vec3(-dx, -dy, 1.0f));
        size_t o = ((size_t)y * N + x) * 4;
        nrm[o] = toB(n.x * 0.5f + 0.5f); nrm[o + 1] = toB(n.y * 0.5f + 0.5f); nrm[o + 2] = toB(n.z * 0.5f + 0.5f); nrm[o + 3] = 255;
    }
}

// Roche : gros blocs en marches, facettes inclinées, fractures, grain et paillettes minérales
static void genRockTex(int N, std::vector<uint8_t>& col, std::vector<uint8_t>& nrm) {
    std::vector<float> H((size_t)N * N); col.resize((size_t)N * N * 4);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        float u = (i + 0.5f) / N, v = (j + 0.5f) / N;
        float wob = pfbm(u, v, 5, 3, 61) - 0.5f, wob2 = pfbm(u, v, 5, 3, 62) - 0.5f;
        float uu = u + wob * 0.07f, vv = v + wob2 * 0.07f;
        WCell A = worleyCell(uu, vv, 4, 5), B = worleyCell(uu, vv, 10, 9);
        float tiltA = (A.dx * (H1(A.id, 1, 0, 31) - 0.5f) + A.dy * (H1(A.id, 2, 0, 31) - 0.5f)) * 0.9f;
        float tiltB = (B.dx * (H1(B.id, 1, 0, 33) - 0.5f) + B.dy * (H1(B.id, 2, 0, 33) - 0.5f)) * 0.9f;
        float stepA = H1(A.id, 3, 0, 31), stepB = H1(B.id, 3, 0, 33);
        float f = pfbm(u, v, 4, 6, 11), f2 = pfbm(u, v, 3, 4, 23), f3 = pfbm(u, v, 5, 3, 41);
        float ridge = 1.0f - std::fabs(pfbm(u, v, 3, 5, 71) * 2.0f - 1.0f);
        float grain = pfbm(u, v, 48, 3, 83);
        float crackA = 1.0f - smooth(0.0f, 0.028f, A.F2 - A.F1), crackB = 1.0f - smooth(0.0f, 0.025f, B.F2 - B.F1);
        float h = 0.28f * (stepA * 0.6f + tiltA) + 0.14f * (stepB * 0.5f + tiltB) + 0.22f * f + 0.12f * ridge + 0.14f * grain - 0.06f * crackA - 0.04f * crackB;
        H[(size_t)j * N + i] = h;
        float tint = H1(A.id, 5, 0, 37);
        vec3 gray(0.25f, 0.25f, 0.265f), ochre(0.27f, 0.235f, 0.19f), blue(0.19f, 0.21f, 0.25f);
        vec3 a = tint < 0.5f ? glm::mix(gray, ochre, tint * 2.0f * 0.7f) : glm::mix(gray, blue, (tint - 0.5f) * 2.0f);
        a *= 0.75f + 0.5f * H1(B.id, 6, 0, 39);
        a = glm::mix(a, a * vec3(0.7f, 0.74f, 0.8f), smooth(0.5f, 0.8f, f3));
        a *= 0.45f + 0.9f * (0.5f * f2 + 0.5f * h * 2.0f);
        a *= 1.0f - 0.65f * crackA; a *= 1.0f - 0.35f * crackB;
        float fleck = smooth(0.88f, 0.92f, pn2(u * 160, v * 160, 160, 160, 91));
        a += vec3(0.07f, 0.07f, 0.075f) * fleck;
        size_t o = ((size_t)j * N + i) * 4;
        col[o] = enc(a.x); col[o + 1] = enc(a.y); col[o + 2] = enc(a.z); col[o + 3] = toB(0.12f + 0.4f * f2 + 0.6f * fleck);
    }
    heightToNormal(N, H, 7.0f, nrm);
}

static void genWoodTex(int N, std::vector<uint8_t>& col, std::vector<uint8_t>& nrm) {
    std::vector<float> H((size_t)N * N); col.resize((size_t)N * N * 4);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
        float u = (i + 0.5f) / N, v = (j + 0.5f) / N;   // u : le long du fil
        float warp = pn2(u * 2, v * 6, 2, 6, 7) * 1.8f + pn2(u * 4, v * 12, 4, 12, 9) * 0.5f;
        float ring = 0.5f + 0.5f * std::sin(2 * PI * (v * 9.0f + warp));
        ring = std::pow(ring, 1.6f);
        float streak = pn2(u * 6, v * 96, 6, 96, 13) * 0.6f + pn2(u * 12, v * 200, 12, 200, 17) * 0.4f;
        float crackN = pn2(u * 3, v * 40, 3, 40, 21);
        float crack = smooth(0.46f, 0.5f, crackN) * (1.0f - smooth(0.5f, 0.54f, crackN));
        float t = 0.45f * ring + 0.55f * streak;
        vec3 dark(0.15f, 0.10f, 0.065f), light(0.40f, 0.30f, 0.20f), gray(0.30f, 0.27f, 0.24f);
        vec3 a = glm::mix(dark, light, t);
        a = glm::mix(a, gray, 0.2f + 0.35f * pn2(u * 3, v * 3, 3, 3, 33));
        a *= 1.0f - 0.75f * crack;
        H[(size_t)j * N + i] = 0.35f * ring + 0.35f * streak - 0.5f * crack;
        size_t o = ((size_t)j * N + i) * 4;
        col[o] = enc(a.x); col[o + 1] = enc(a.y); col[o + 2] = enc(a.z); col[o + 3] = toB(0.3f + 0.3f * streak);
    }
    heightToNormal(N, H, 14.0f, nrm);
}

static GLuint makeTex(int N, const std::vector<uint8_t>& px, bool srgb) {
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f);
    return t;
}

// ----------------------------------------------------------------------------
//  Mesh
// ----------------------------------------------------------------------------
struct Vertex { vec3 p, n; vec2 uv; float mat, seed, biome; };
struct Chunk { uint32_t first, count; vec3 c; float r; };

// Une planche = boîte orientée, destructible, avec collision
struct Board {
    vec3 c, dl, dw, dt; float hl = 0, hw = 0, ht = 0; uint32_t i0 = 0;
    bool alive = true; int hp = 1, frame = -1, role = 0, side = 0, tube = 0;   // role : 1 poteau, 2 linteau, 3 contrefiche
    uint32_t n = 36;      // nombre d'indices de la géométrie visible (0 = invisible, portée par un modèle)
    int group = -1;       // échelle à laquelle appartient la planche (tout l'ensemble s'effondre)
    int wall = -1;        // éboulis auquel appartient le bloc de pierre (role 7) ; side : bit0 = touche le plafond, bit1 = posé au sol
};

struct Mesh {
    std::vector<Vertex> v; std::vector<uint32_t> idx; std::vector<Chunk> chunks; std::vector<Board> boards;
    bool track = false;
    int geom = 0;         // géométrie des planches : 0 boîte, 1 modèle de planche (plank.obj), 2 aucune (collision seule)
    GLuint vao = 0, vbo = 0, ebo = 0;
    size_t cIdx0 = 0, cVert0 = 0;
    void beginChunk() { cIdx0 = idx.size(); cVert0 = v.size(); }
    void endChunk() { endChunk(cVert0, v.size()); }
    void endChunk(size_t v0, size_t v1) {
        if (idx.size() == cIdx0 || v1 <= v0) return;
        vec3 mn(1e9f), mx(-1e9f);
        for (size_t i = v0; i < v1; i++) { mn = glm::min(mn, v[i].p); mx = glm::max(mx, v[i].p); }
        Chunk c; c.first = (uint32_t)cIdx0; c.count = (uint32_t)(idx.size() - cIdx0);
        c.c = (mn + mx) * 0.5f; c.r = glm::length(mx - mn) * 0.5f; chunks.push_back(c);
    }
    void ensure() {
        if (vao) return;
        glGenVertexArrays(1, &vao); glBindVertexArray(vao);
        glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glGenBuffers(1, &ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)24);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)32);
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)40);
    }
    void upload(GLenum usage = GL_STATIC_DRAW) {
        ensure(); glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(), usage);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), usage);
    }
    void clearCpu() { v.clear(); idx.clear(); chunks.clear(); boards.clear(); }
    void killIndices(uint32_t i0, uint32_t n) {            // supprime des triangles (indices dégénérés)
        for (uint32_t k = 0; k < n; k++) idx[i0 + k] = 0;
        std::vector<uint32_t> z(n, 0);
        glBindVertexArray(vao); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)i0 * 4, n * 4, z.data());
    }
    void destroy() {
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (ebo) glDeleteBuffers(1, &ebo);
        vao = vbo = ebo = 0;
    }
    void draw(const std::function<bool(const Chunk&)>& keep) const {
        if (!vao) return;
        glBindVertexArray(vao);
        for (const Chunk& c : chunks) if (keep(c)) glDrawElements(GL_TRIANGLES, c.count, GL_UNSIGNED_INT, (void*)((size_t)c.first * 4));
    }
    void drawAll() const {
        if (!vao || idx.empty()) return;
        glBindVertexArray(vao); glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, nullptr);
    }
};

// ----------------------------------------------------------------------------
//  Modèles OBJ (dossier models/, générés par models/generate_models.py)
//    - groupes `g` = parties animables, `# pivot nom x y z` = leur articulation
//    - matériaux : certains noms sont associés aux matériaux procéduraux du jeu (bois, acier…),
//      les autres utilisent la couleur / l'émission du .mtl (shader : vMat >= 21)
// ----------------------------------------------------------------------------
struct ModelData {
    std::vector<Vertex> v; std::vector<int> vg;                 // sommets (matériau dans Vertex::mat) et groupe de chaque sommet
    std::vector<uint32_t> idx;
    std::vector<std::string> gname; std::vector<vec3> gpivot;
    bool ok = false;
    int group(const std::string& n) const { for (size_t i = 0; i < gname.size(); i++) if (gname[i] == n) return (int)i; return -1; }
};
static std::vector<vec4> gMdlA, gMdlE;                          // matériaux génériques : (Kd, Ns) et (Ke, brillance)
static const int MDL_MAXMAT = 48;
static ModelData gBarricadeMdl, gAxeMdl, gLadderMdl, gPlankMdl, gMummyMdl, gSpiderMdl, gGolemMdl;

static bool loadModel(const std::string& name, ModelData& md) {
    std::string base = "models/" + name;
    FILE* fm = std::fopen((base + ".mtl").c_str(), "r"); FILE* fo = std::fopen((base + ".obj").c_str(), "r");
    if (!fm || !fo) { if (fm) std::fclose(fm); if (fo) std::fclose(fo); std::printf("Modele introuvable : %s (formes procedurales utilisees)\n", base.c_str()); return false; }
    struct MtlDef { vec3 kd = vec3(0.5f), ke = vec3(0), ks = vec3(0.05f); float ns = 10; };
    std::vector<std::pair<std::string, MtlDef>> mtls; char line[512];
    while (std::fgets(line, sizeof line, fm)) {
        char key[32], nm[128]; float a, b, c;
        if (std::sscanf(line, "newmtl %127s", nm) == 1) mtls.push_back({nm, MtlDef()});
        else if (!mtls.empty() && std::sscanf(line, "%31s %f %f %f", key, &a, &b, &c) == 4) {
            MtlDef& d = mtls.back().second;
            if (!std::strcmp(key, "Kd")) d.kd = vec3(a, b, c); else if (!std::strcmp(key, "Ke")) d.ke = vec3(a, b, c); else if (!std::strcmp(key, "Ks")) d.ks = vec3(a, b, c);
        } else if (!mtls.empty() && std::sscanf(line, "Ns %f", &a) == 1) mtls.back().second.ns = a;
    }
    std::fclose(fm);
    // noms de matériaux associés aux matériaux procéduraux du jeu
    static const std::pair<const char*, float> GAME[] = {{"wood", 1.0f}, {"steel", 5.0f}, {"steel_dark", 5.0f}, {"nail", 5.0f}, {"leather", 14.0f}, {"chitin", 15.0f}, {"chitin_dark", 15.0f}, {"fang", 10.0f}};
    std::vector<float> matId;
    for (auto& m : mtls) {
        float id = -1;
        for (auto& g : GAME) if (m.first == g.first) id = g.second;
        if (id < 0) {
            if ((int)gMdlA.size() >= MDL_MAXMAT) id = 5.0f;
            else { gMdlA.push_back(vec4(m.second.kd, m.second.ns)); gMdlE.push_back(vec4(m.second.ke, m.second.ks.x)); id = 21.0f + (float)gMdlA.size() - 1.0f; }
        }
        matId.push_back(id);
    }
    std::vector<vec3> P, N; std::vector<vec2> T; int cg = 0, cm = 0;
    md.gname.assign(1, "body"); md.gpivot.assign(1, vec3(0));
    std::vector<std::pair<std::string, vec3>> pivots;
    struct Key { int g, m, a, b, c; bool operator<(const Key& o) const { return std::tie(g, m, a, b, c) < std::tie(o.g, o.m, o.a, o.b, o.c); } };
    std::map<Key, uint32_t> cache;
    while (std::fgets(line, sizeof line, fo)) {
        char nm[128]; float a, b, c;
        if (line[0] == 'v' && line[1] == ' ') { std::sscanf(line, "v %f %f %f", &a, &b, &c); P.push_back(vec3(a, b, c)); }
        else if (line[0] == 'v' && line[1] == 't') { std::sscanf(line, "vt %f %f", &a, &b); T.push_back(vec2(a, b)); }
        else if (line[0] == 'v' && line[1] == 'n') { std::sscanf(line, "vn %f %f %f", &a, &b, &c); N.push_back(vec3(a, b, c)); }
        else if (std::sscanf(line, "# pivot %127s %f %f %f", nm, &a, &b, &c) == 4) pivots.push_back({nm, vec3(a, b, c)});
        else if (line[0] == 'g' && std::sscanf(line, "g %127s", nm) == 1) {
            cg = md.group(nm); if (cg < 0) { md.gname.push_back(nm); md.gpivot.push_back(vec3(0)); cg = (int)md.gname.size() - 1; }
        } else if (std::sscanf(line, "usemtl %127s", nm) == 1) { cm = 0; for (size_t i = 0; i < mtls.size(); i++) if (mtls[i].first == nm) cm = (int)i; }
        else if (line[0] == 'f' && line[1] == ' ') {
            std::vector<uint32_t> ids; char* tok = std::strtok(line + 2, " \t\r\n");
            while (tok) {
                int vi = 0, ti = 0, ni = 0; std::sscanf(tok, "%d/%d/%d", &vi, &ti, &ni);
                Key k{cg, cm, vi, ti, ni}; auto it = cache.find(k);
                if (it == cache.end()) {
                    Vertex vx; vx.p = P[vi - 1]; vx.n = N[ni - 1]; vx.uv = T[ti - 1]; vx.mat = matId[cm]; vx.seed = 0; vx.biome = 0;
                    md.v.push_back(vx); md.vg.push_back(cg); it = cache.emplace(k, (uint32_t)md.v.size() - 1).first;
                }
                ids.push_back(it->second); tok = std::strtok(nullptr, " \t\r\n");
            }
            for (size_t k = 1; k + 1 < ids.size(); k++) { md.idx.push_back(ids[0]); md.idx.push_back(ids[k]); md.idx.push_back(ids[k + 1]); }
        }
    }
    std::fclose(fo);
    for (auto& pv : pivots) { int g = md.group(pv.first); if (g >= 0) md.gpivot[g] = pv.second; }
    md.ok = !md.idx.empty();
    std::printf("Modele %-18s %6zu sommets %6zu triangles %zu groupes\n", name.c_str(), md.v.size(), md.idx.size() / 3, md.gname.size());
    return md.ok;
}

// Ajoute le modèle au maillage : W = placement, gx = matrice optionnelle par groupe (animation), uvOff = décalage de texture
static void addModelXf(Mesh& m, const ModelData& md, const mat4& W, float seed, vec2 uvOff = vec2(0), const std::vector<mat4>* gx = nullptr) {
    if (!md.ok) return;
    size_t ng = md.gname.size(); std::vector<mat4> M(ng, W); std::vector<mat3> NM(ng);
    for (size_t g = 0; g < ng; g++) { if (gx) M[g] = W * (*gx)[g]; NM[g] = glm::transpose(glm::inverse(mat3(M[g]))); }
    uint32_t base = (uint32_t)m.v.size();
    for (size_t i = 0; i < md.v.size(); i++) {
        const Vertex& s = md.v[i]; int g = md.vg[i]; Vertex o = s;
        o.p = vec3(M[g] * vec4(s.p, 1.0f)); o.n = glm::normalize(NM[g] * s.n); o.uv = s.uv + uvOff; o.seed = seed;
        m.v.push_back(o);
    }
    for (uint32_t k : md.idx) m.idx.push_back(base + k);
}

// Planche du jeu (boîte orientée) remplacée par plank.obj : x = longueur, y = épaisseur (face clouée), z = largeur
static void addPlankModel(Mesh& m, vec3 c, vec3 dl, vec3 dt, float hl, float hw, float ht, float seed) {
    if (!gPlankMdl.ok) return;
    vec3 z = glm::normalize(glm::cross(dl, dt));
    mat4 R(1.0f); R[0] = vec4(dl * (hl / 0.8f), 0); R[1] = vec4(dt * (ht / 0.0175f), 0); R[2] = vec4(z * (hw / 0.11f), 0); R[3] = vec4(c, 1);
    addModelXf(m, gPlankMdl, R * glm::translate(mat4(1.0f), vec3(0, -0.0175f, 0)), seed, vec2(seed * 7.13f, seed * 3.71f));
}

// Grille (rows x cols, colonnes bouclées) avec normales lisses orientées vers/loin de `ref`
static void addGrid(Mesh& m, const std::vector<vec3>& P, const std::vector<vec3>& ref, int rows, int cols,
                    float refSign, float mat, float seed) {
    uint32_t base = (uint32_t)m.v.size();
    std::vector<vec3> N(P.size(), vec3(0));
    auto id = [&](int r, int c) { return r * cols + (c % cols); };
    for (int r = 0; r < rows - 1; r++) for (int c = 0; c < cols; c++) {
        int a = id(r, c), b = id(r, c + 1), cc = id(r + 1, c + 1), d = id(r + 1, c);
        vec3 n1 = glm::cross(P[b] - P[a], P[cc] - P[a]), n2 = glm::cross(P[cc] - P[a], P[d] - P[a]);
        N[a] += n1 + n2; N[b] += n1; N[cc] += n1 + n2; N[d] += n2;
        m.idx.push_back(base + a); m.idx.push_back(base + b); m.idx.push_back(base + cc);
        m.idx.push_back(base + a); m.idx.push_back(base + cc); m.idx.push_back(base + d);
    }
    for (size_t i = 0; i < P.size(); i++) {
        vec3 n = glm::normalize(N[i] + vec3(1e-6f));
        if (glm::dot(n, ref[i] - P[i]) * refSign < 0) n = -n;
        m.v.push_back({P[i], n, vec2(0), mat, seed});
    }
}

static void addEllipsoid(Mesh& m, vec3 c, vec3 ax, vec3 ay, vec3 az, vec3 rad, float mat, float seed) {
    const int rows = 7, cols = 10; std::vector<vec3> P, ref;
    for (int r = 0; r < rows; r++) {
        float phi = PI * r / (rows - 1);
        for (int k = 0; k < cols; k++) {
            float a = 2 * PI * k / cols;
            P.push_back(c + ax * (rad.x * std::sin(phi) * std::cos(a)) + ay * (rad.y * std::cos(phi)) + az * (rad.z * std::sin(phi) * std::sin(a)));
            ref.push_back(c);
        }
    }
    addGrid(m, P, ref, rows, cols, -1.0f, mat, seed);
}

// Boîte à 8 coins quelconques (bit0 = x, bit1 = y, bit2 = z)
static void addHull8(Mesh& m, const vec3* corner, float mat, float seed) {
    vec3 c(0); for (int k = 0; k < 8; k++) c += corner[k]; c *= 0.125f;
    for (int a = 0; a < 3; a++) for (int s = -1; s <= 1; s += 2) {
        int b = (a + 1) % 3, cc = (a + 2) % 3;
        const int cb[4] = {-1, 1, 1, -1}, ccs[4] = {-1, -1, 1, 1};
        vec3 p[4];
        for (int q = 0; q < 4; q++) {
            int sg[3]; sg[a] = s; sg[b] = cb[q]; sg[cc] = ccs[q];
            p[q] = corner[(sg[0] > 0 ? 1 : 0) | (sg[1] > 0 ? 2 : 0) | (sg[2] > 0 ? 4 : 0)];
        }
        vec3 n = glm::normalize(glm::cross(p[1] - p[0], p[3] - p[0]) + vec3(1e-7f));
        vec3 fc = (p[0] + p[1] + p[2] + p[3]) * 0.25f;
        if (glm::dot(n, fc - c) < 0) n = -n;
        uint32_t base = (uint32_t)m.v.size();
        for (int q = 0; q < 4; q++) m.v.push_back({p[q], n, vec2(p[q].x, p[q].y), mat, seed});
        m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

// Planche (boîte légèrement irrégulière, faces planes). Enregistrée comme Board si m.track
static void addBoardBox(Mesh& m, vec3 c, vec3 dl, vec3 dw, vec3 dt, float hl, float hw, float ht,
                        float mat, float seed, Rng& rng, float jit) {
    vec3 corner[8];
    for (int k = 0; k < 8; k++) {
        float sx = (k & 1) ? 1.f : -1.f, sy = (k & 2) ? 1.f : -1.f, sz = (k & 4) ? 1.f : -1.f;
        corner[k] = c + dl * (sx * hl) + dw * (sy * hw) + dt * (sz * ht) + vec3(rng.r(-jit, jit), rng.r(-jit, jit), rng.r(-jit, jit));
    }
    vec2 uvOff(seed * 7.13f, seed * 3.71f);
    for (int a = 0; a < 3; a++) for (int s = -1; s <= 1; s += 2) {
        int b = (a + 1) % 3, cc = (a + 2) % 3;
        const int cb[4] = {-1, 1, 1, -1}, ccs[4] = {-1, -1, 1, 1};
        vec3 p[4]; vec2 uv[4];
        for (int q = 0; q < 4; q++) {
            int sg[3]; sg[a] = s; sg[b] = cb[q]; sg[cc] = ccs[q];
            p[q] = corner[(sg[0] > 0 ? 1 : 0) | (sg[1] > 0 ? 2 : 0) | (sg[2] > 0 ? 4 : 0)];
            if (a == 0) uv[q] = vec2(sg[1] * hw, sg[2] * ht);
            else if (a == 1) uv[q] = vec2(sg[0] * hl, sg[2] * ht);
            else uv[q] = vec2(sg[0] * hl, sg[1] * hw);
            uv[q] += uvOff;
        }
        vec3 n = glm::normalize(glm::cross(p[1] - p[0], p[3] - p[0]));
        vec3 fc = (p[0] + p[1] + p[2] + p[3]) * 0.25f;
        if (glm::dot(n, fc - c) < 0) n = -n;
        uint32_t base = (uint32_t)m.v.size();
        for (int q = 0; q < 4; q++) m.v.push_back({p[q], n, uv[q], mat, seed});
        m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}
static void addPlankModel(Mesh& m, vec3 c, vec3 dl, vec3 dt, float hl, float hw, float ht, float seed);
static void addBoard(Mesh& m, vec3 c, vec3 dl, vec3 dw, vec3 dt, float hl, float hw, float ht,
                     float mat, float seed, Rng& rng, float jit = 0.012f) {
    uint32_t i0 = (uint32_t)m.idx.size();
    if (m.geom == 1 && (mat == 1.0f || mat == 120.0f)) {
        uint32_t v0 = (uint32_t)m.v.size(); addPlankModel(m, c, dl, dt, hl, hw, ht, seed);
        if (mat != 1.0f) for (size_t i = v0; i < m.v.size(); i++) if (m.v[i].mat == 1.0f) m.v[i].mat = mat;
    }
    else if (m.geom != 2) addBoardBox(m, c, dl, dw, dt, hl, hw, ht, mat, seed, rng, jit);
    if (m.track) { Board b; b.c = c; b.dl = dl; b.dw = dw; b.dt = dt; b.hl = hl; b.hw = hw; b.ht = ht; b.i0 = i0; b.n = (uint32_t)m.idx.size() - i0; m.boards.push_back(b); }
}
static bool addBoardBetween(Mesh& m, vec3 a, vec3 b, vec3 hint, float hw, float ht, float mat, Rng& rng,
                            float extend = 0.0f, float jit = 0.012f) {
    vec3 d = b - a; float len = glm::length(d); if (len < 1e-4f) return false;
    vec3 dl = d / len;
    vec3 t = hint - dl * glm::dot(hint, dl);
    if (glm::length(t) < 1e-3f) t = glm::cross(dl, vec3(0, 1, 0));
    if (glm::length(t) < 1e-3f) t = vec3(1, 0, 0);
    vec3 dt = glm::normalize(t), dw = glm::normalize(glm::cross(dt, dl));
    addBoard(m, (a + b) * 0.5f, dl, dw, dt, len * 0.5f + extend, hw, ht, mat, rng.f() * 10.0f, rng, jit);
    return true;
}

// ----------------------------------------------------------------------------
//  Réseau de galeries
// ----------------------------------------------------------------------------
struct Ring { vec3 c, T, R, U; float w, h, yaw, pitch; };
struct BiomeDef { const char* name; vec3 tint, amb, torch; float torchI, wet, moss; };
static const int HUB_BIOME = 6;
static const BiomeDef BIOMES[7] = {
    {"GALERIES BOISEES",  vec3(1.0f, 1.0f, 1.0f),    vec3(0.004f, 0.006f, 0.011f), vec3(1.00f, 0.50f, 0.17f), 4.2f, 0.50f, 0.0f},
    {"GROTTES HUMIDES",   vec3(0.72f, 0.95f, 0.92f), vec3(0.002f, 0.008f, 0.012f), vec3(0.30f, 0.78f, 1.00f), 3.4f, 0.90f, 1.0f},
    {"ANTRE DES CRISTAUX", vec3(0.90f, 0.82f, 1.05f), vec3(0.008f, 0.004f, 0.016f), vec3(0.72f, 0.32f, 1.00f), 2.6f, 0.30f, 0.0f},
    {"CATACOMBES",        vec3(1.12f, 1.08f, 0.92f), vec3(0.006f, 0.007f, 0.004f), vec3(0.78f, 0.95f, 0.35f), 2.5f, 0.20f, 0.0f},
    {"FORGE INFERNALE",   vec3(1.0f, 0.66f, 0.55f),  vec3(0.014f, 0.004f, 0.002f), vec3(1.00f, 0.28f, 0.07f), 2.9f, 0.10f, 0.0f},
    {"LA SORTIE",         vec3(0.95f, 0.97f, 1.0f),  vec3(0.020f, 0.025f, 0.030f), vec3(0.75f, 0.88f, 1.00f), 4.0f, 0.30f, 0.0f},
    {"LE CAMP DE BASE",   vec3(0.72f, 0.68f, 0.68f),  vec3(0.15f, 0.13f, 0.25f),    vec3(1.00f, 0.55f, 0.20f), 4.2f, 0.15f, 0.55f}};
struct Torch { vec3 pos; float phase; vec3 col; float inten; int biome; bool flame; bool needsDoor = false; };
struct Pickup { vec3 p; int kind; bool alive; float phase; };     // 0 = cle, 1 = fiole de soin
struct Spawn { int type; vec3 pos; int tube; };
struct Drip { vec3 tip; int tube, ring; float timer; };
struct Web { vec3 c; float r; uint32_t i0; bool alive; int frame; };
struct Frame { int tube = 0; std::vector<int> post[2], beam, brace[2], webs; bool fell[2] = {false, false}; };
struct HolePair { vec3 pA, nA, pB, nB; int tube, iA, iB; float xA, xB; };
struct Debris {
    vec3 p, v, ax, ay, az, h, w; float seed, sleepT = 0, age = 0, life = 0, unseen = 0, die = -1; bool sleep = false, stone = false; int tube = 0, ring = -1;
};

struct Tube {
    std::vector<Ring> rings; std::vector<vec2> loc; std::vector<vec3> P; std::vector<char> cut;
    int N = 0, M = 56; float ds = 0.3f; int vBase = 0; bool isMain = false; int parent = -1, junction = -1, side = 0;

    vec3 toWorld(int i, float x, float y, float t = 0) const {
        const Ring& r = rings[i]; return r.c + r.R * x + r.U * y + r.T * t;
    }
    bool wallRange(int i, float y, float& xl, float& xr) const {
        bool f = false; xl = 1e9f; xr = -1e9f;
        for (int k = 0; k < M; k++) {
            vec2 a = loc[(size_t)i * M + k], b = loc[(size_t)i * M + (k + 1) % M];
            if ((a.y - y) * (b.y - y) <= 0 && a.y != b.y) {
                float t = (y - a.y) / (b.y - a.y), x = a.x + t * (b.x - a.x);
                xl = std::min(xl, x); xr = std::max(xr, x); f = true;
            }
        }
        return f;
    }
    float vertRange(int i, float x, bool top) const {
        float best = top ? -1e9f : 1e9f;
        for (int k = 0; k < M; k++) {
            vec2 a = loc[(size_t)i * M + k], b = loc[(size_t)i * M + (k + 1) % M];
            if ((a.x - x) * (b.x - x) <= 0 && a.x != b.x) {
                float t = (x - a.x) / (b.x - a.x), y = a.y + t * (b.y - a.y);
                best = top ? std::max(best, y) : std::min(best, y);
            }
        }
        return best;
    }
    float floorY(int i, float x) const { float y = vertRange(i, x, false); return y > 1e8f ? -1.0f : y; }
    float ceilY(int i, float x) const { float y = vertRange(i, x, true); return y < -1e8f ? 2.0f : y; }
    float radiusAt(int i, float phi) const {
        vec2 d(std::cos(phi), std::sin(phi)); float best = 1.0f;
        for (int k = 0; k < M; k++) {
            vec2 a = loc[(size_t)i * M + k], b = loc[(size_t)i * M + (k + 1) % M], e = b - a;
            float den = d.x * e.y - d.y * e.x; if (std::fabs(den) < 1e-6f) continue;
            float t = (a.x * e.y - a.y * e.x) / den, s = (a.x * d.y - a.y * d.x) / den;
            if (t > 0 && s >= -0.001f && s <= 1.001f) best = t;
        }
        return best;
    }
    int locate(vec3 p, int hint) const {
        int lo = 0, hi = N - 1;
        if (hint >= 0) { lo = std::max(0, hint - 40); hi = std::min(N - 1, hint + 40); }
        int best = lo; float bd = 1e18f;
        for (int i = lo; i <= hi; i++) {
            vec3 d = p - rings[i].c;
            float score = (hint < 0) ? glm::dot(d, d) : std::fabs(glm::dot(d, rings[i].T));
            if (score < bd) { bd = score; best = i; }
        }
        return best;
    }
};

struct World {
    Mesh rock, wood, torchM, webM, waterM, debrisM, ratM;
    std::vector<Tube> tubes; std::vector<Torch> torches; std::vector<Frame> frames; std::vector<Web> webs;
    std::vector<HolePair> holes; std::vector<Debris> debris;
    uint32_t seed = 1; Rng drng{99}; std::vector<vec3> ashEvents;     // positions où émettre de la cendre (consommées par la boucle principale)
    int doorRing = 0; vec3 doorPos = vec3(0), exitPos = vec3(0); bool doorOpen = false; int keysNeeded = 0; std::vector<int> doorBoards;
    std::vector<Pickup> pickups; std::vector<Spawn> spawns; std::vector<Drip> drips;
    struct HubSolid { float x, z, r; };
    Mesh hubM; std::vector<HubSolid> hubSolids; float hubG = 0, hubFront = 0; vec3 hubSpawn = vec3(0);     // camp de base
    float hubGround(float x, float z) const;
    vec3 constrainHub(vec3 p, bool fly) const;
    bool inHub(vec3 p) const { return !hubM.v.empty() && p.z > hubFront + 0.25f; }
    struct LadderObj { uint32_t i0 = 0, n = 0; std::vector<int> boards; bool alive = true; int kind = 0; vec3 stand = vec3(0), look = vec3(0); };   // kind : 0 échelle, 1 barricade
    std::vector<LadderObj> ladders;      // chaque échelle = un modèle + des planches invisibles (collision) ; elle s'effondre d'un bloc
    float biomeF(int ti, int i) const;
    float biomeAtPos(vec3 p) const;
    void openDoor();
    int corrA = -1, corrB = -1;          // couloir lisse et droit des catacombes (anneaux de la galerie principale)
    float corrK(int ti, int i) const { if (ti != 0 || corrA < 0) return 0.0f; return smooth((float)corrA - 12.0f, (float)corrA, (float)i) * (1.0f - smooth((float)corrB, (float)corrB + 12.0f, (float)i)); }
    bool nearCorr(int ti, int i, int m) const { return ti == 0 && corrA >= 0 && i > corrA - 12 - m && i < corrB + 12 + m; }
    vec3 camPos = vec3(0), camFwd = vec3(0, 0, -1);   // caméra (disparition des gravats hors du champ)
    void boardsNear(vec3 pt, float rad, std::vector<int>& out) const;

    void destroy() { hubM.destroy(); rock.destroy(); wood.destroy(); torchM.destroy(); webM.destroy(); waterM.destroy(); debrisM.destroy(); ratM.destroy(); }
    void build(uint32_t sd);

    // --- interaction
    vec3 constrain(vec3 p, bool fly, std::vector<int>& hints) const;
    void collideBoards(vec3& p, bool fly) const;
    int rayBoard(vec3 o, vec3 d, float maxT, float& tHit) const;
    void hitBoard(int bi, vec3 hit, vec3 push);
    void breakBoard(int bi, vec3 hit, vec3 push, bool whole);
    void checkCollapse(int frameId);
    int clearWebs(vec3 o, vec3 d, float reach);
    void spawnPiece(const Board& b, float s0, float s1, vec3 vel);
    void spawnChips(vec3 at, vec3 push, int n, bool stone = false);
    void breakStone(int bi, vec3 hit, vec3 push);
    void settleStone(int wid);
    struct StoneWall { vec3 c = vec3(0); int tube = 0, ring = 0; std::vector<int> blocks; bool told = false; };
    std::vector<StoneWall> walls;        // éboulis de pierre qui obstruent la galerie : à abattre à la pioche
    void stepDebris(float dt);
    void buildDebrisMesh();

private:
    void genTube(Tube& tb, int Nmax, vec3 p0, float yaw0, float pitch0, bool isMain, Rng& rng, int parent, int pj, float branchW);
    int validLength(const std::vector<Ring>& rs, int parent, int pj) const;
    void computeSection(size_t ti);
    void genRock(Rng& rng);
    void genStalagmites(Rng& rng);
    void genWood(Rng& rng);
public:
    void genHub(Rng& rng);
private:
    void addFrame(int ti, int i, Rng& rng, bool doorway);
    void addLagging(int ti, int i, int j, int side, Rng& rng);
    void addTorch(int ti, int i, int side, Rng& rng);
    void addLadder(int ti, int i, int side, Rng& rng);
    bool addBarricade(int ti, int i, Rng& rng);
    void addStoneWall(int ti, int i, int layers, Rng& rng);
    void addPuddles(int ti, int count, Rng& rng);
    void addRatHoles(int ti, int i, int k, Rng& rng);
    void addCone(vec3 base, vec3 axis, float len, float r0, Rng& rng);
    void addCap(int ti, int ring, int dir, float mat = 0.0f);
    void assignBiomes(Mesh& m);
    void addLight(vec3 pos, vec3 col, float inten, int biome, bool flame);
    void addCrystal(vec3 base, vec3 axis, float len, float rad, float hue);
    void addDecor(int ti, Rng& rng);
    void addDoor(Rng& rng);
    void addDoorAndExit(Rng& rng);
    void placeEntities(Rng& rng);
    void addWeb(int fid, vec3 corner, vec3 A, vec3 B, float size);
    int  addB(int ti, int fid, int role, int side, vec3 a, vec3 b, vec3 hint, float hw, float ht, Rng& rng, float ext = 0.0f, float jit = 0.012f);
};

// ---------- génération des galeries -----------------------------------------
int World::validLength(const std::vector<Ring>& rs, int parent, int pj) const {
    int n = (int)rs.size();
    for (int i = 0; i < n; i++) {
        for (size_t t = 0; t < tubes.size(); t++) {
            const Tube& o = tubes[t];
            for (int j = 0; j < o.N; j++) {
                if ((int)t == parent && std::abs(j - pj) < 60 && i < 50) continue;
                vec3 d = rs[i].c - o.rings[j].c; float lim = rs[i].w + o.rings[j].w + 1.6f;
                if (std::fabs(d.x) < lim && std::fabs(d.z) < lim && std::fabs(d.y) < 6.0f && glm::dot(d, d) < lim * lim) return i;
            }
        }
        for (int j = 0; j + 70 < i; j += 2) {                 // auto-intersection
            vec3 d = rs[i].c - rs[j].c; float lim = rs[i].w + rs[j].w + 1.6f;
            if (glm::dot(d, d) < lim * lim && std::fabs(d.y) < 6.0f) return i;
        }
    }
    return n;
}

void World::genTube(Tube& tb, int Nmax, vec3 p0, float yaw0, float pitch0, bool isMain, Rng& rng, int parent, int pj, float branchW) {
    int bestLen = -1; std::vector<Ring> best;
    for (int attempt = 0; attempt < 40; attempt++) {
        float off = rng.r(0, 500); std::vector<Ring> rs(Nmax);
        float yaw = yaw0; vec3 pos = p0;
        for (int i = 0; i < Nmax; i++) {
            float sd = i * tb.ds;
            float amp = isMain ? 0.26f : 0.2f, spring = isMain ? 0.012f : 0.035f;
            float kc = isMain ? corrK(0, i) : 0.0f; bool frozen = isMain && corrA >= 0 && i >= corrA - 12 && i <= corrB;
            if (!frozen) yaw += tb.ds * (amp * (n1(sd * 0.05f + off, seed) * 2 - 1) - spring * (yaw - yaw0));     // couloir : cap figé (ligne droite)
            if (isMain) yaw = std::max(yaw0 - 1.15f, std::min(yaw0 + 1.15f, yaw));
            float pitch = (pitch0 + (isMain ? 0.12f : 0.05f) * (n1(sd * 0.03f + off * 0.7f, seed + 3) * 2 - 1)) * (1.0f - kc);
            vec3 T(std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
            vec3 R = glm::normalize(glm::cross(T, vec3(0, 1, 0))), U = glm::cross(R, T);
            Ring r; r.c = pos; r.T = T; r.R = R; r.U = U; r.yaw = yaw; r.pitch = pitch;
            if (isMain) {
                float ch = smooth(0.62f, 0.82f, n1(sd * 0.022f + off * 0.3f, seed + 5));
                r.w = 2.1f + 0.7f * (n1(sd * 0.07f + off, seed + 7) * 2 - 1) + 2.4f * ch;
                r.h = 3.0f + 0.5f * (n1(sd * 0.05f + off, seed + 9) * 2 - 1) + 1.6f * ch;
                r.w = mixf(r.w, 1.55f, kc); r.h = mixf(r.h, 3.4f, kc);                                // couloir étroit et haut
            } else {
                r.w = branchW + 0.3f * (n1(sd * 0.09f + off, seed + 7) * 2 - 1);
                r.h = 2.75f + 0.3f * (n1(sd * 0.06f + off, seed + 9) * 2 - 1);
            }
            rs[i] = r; pos += T * tb.ds;
        }
        int len = validLength(rs, parent, pj);
        if (len > bestLen) { bestLen = len; best = rs; }
        if (len >= Nmax) break;
    }
    best.resize(std::max(0, bestLen));
    tb.rings = best; tb.N = (int)best.size(); tb.isMain = isMain; tb.parent = parent; tb.junction = pj;
}

void World::computeSection(size_t ti) {
    {
        Tube& tb = tubes[ti]; int N = tb.N, M = tb.M;
        tb.loc.resize((size_t)N * M); tb.P.resize((size_t)N * M);
        uint32_t sd = seed + (uint32_t)ti * 131;
        for (int i = 0; i < N; i++) {
            const Ring& r = tb.rings[i]; float kc = corrK((int)ti, i);
            for (int k = 0; k < M; k++) {
                float th = 2 * PI * k / M, c = std::cos(th), s = std::sin(th);
                float px = std::copysign(std::pow(std::fabs(c), mixf(0.75f, 0.32f, kc)), c) * r.w;     // couloir : parois droites, voûte en ogive
                float py = std::copysign(std::pow(std::fabs(s), mixf(0.8f, 0.55f, kc)), s) * r.h;
                if (py < 0) py *= 0.34f;
                vec3 pw = r.c + r.R * px + r.U * py;
                float rg = 1.0f - std::fabs(2.0f * fbm3(pw * 0.7f, 3, sd + 21) - 1.0f);        // crêtes / fractures
                float d = (fbm3(pw * 0.33f, 4, sd) - 0.5f) * 1.0f + (rg - 0.5f) * 0.5f
                        + (fbm3(pw * 1.7f, 3, sd + 99) - 0.5f) * 0.38f + (fbm3(pw * 5.0f, 2, sd + 12) - 0.5f) * 0.12f;
                d += 0.05f * std::sin(pw.y * 5.0f + 3.0f * fbm3(pw * 0.5f, 2, sd + 7));
                if (py < 0) d *= 0.14f;                                                      // sol presque plat (marche fluide)
                d *= 1.0f - 0.95f * kc;
                if (kc > 0 && py > -0.2f) d -= kc * 0.14f * std::pow(std::max(0.0f, std::cos(i * tb.ds * PI / 2.4f)), 24.0f);   // nervures régulières, comme des côtes
                float r0 = std::sqrt(px * px + py * py), r1 = std::max(r0 + d, 0.8f);
                vec2 l(px * r1 / r0, py * r1 / r0);
                tb.loc[(size_t)i * M + k] = l; tb.P[(size_t)i * M + k] = r.c + r.R * l.x + r.U * l.y;
            }
        }
    }
}

void World::genRock(Rng&) {
    // 2) ouvertures aux jonctions : on retire les quads de chaque galerie qui sont dans l'autre
    for (auto& tb : tubes) tb.cut.assign((size_t)std::max(0, tb.N - 1) * tb.M, 0);
    for (size_t bi = 1; bi < tubes.size(); bi++) {
        Tube& br = tubes[bi]; Tube& mn = tubes[br.parent]; int M = br.M;
        auto insideTube = [&](const Tube& t, int r0, int r1, vec3 q, float tmin, float margin) {
            int best = r0; float bt = 1e9f;
            for (int i = r0; i <= r1; i++) { float tt = std::fabs(glm::dot(q - t.rings[i].c, t.rings[i].T)); if (tt < bt) { bt = tt; best = i; } }
            vec3 d = q - t.rings[best].c; float x = glm::dot(d, t.rings[best].R), y = glm::dot(d, t.rings[best].U), tt = glm::dot(d, t.rings[best].T);
            if (best == r0 && tt < tmin) return false;
            return std::sqrt(x * x + y * y) < t.radiusAt(best, std::atan2(y, x)) - margin;
        };
        int j = br.junction, lo = std::max(0, j - 14), hi = std::min(mn.N - 2, j + 14);
        for (int i = lo; i <= hi; i++) for (int k = 0; k < mn.M; k++) {
            int a = i * mn.M + k, b = i * mn.M + (k + 1) % mn.M, c = (i + 1) * mn.M + (k + 1) % mn.M, d = (i + 1) * mn.M + k;
            vec3 q = (mn.P[a] + mn.P[b] + mn.P[c] + mn.P[d]) * 0.25f;
            if (insideTube(br, 0, std::min(br.N - 1, 40), q, -0.3f, 0.22f)) mn.cut[(size_t)i * mn.M + k] = 1;
        }
        for (int i = 0; i < std::min(br.N - 1, 16); i++) for (int k = 0; k < M; k++) {
            int a = i * M + k, b = i * M + (k + 1) % M, c = (i + 1) * M + (k + 1) % M, d = (i + 1) * M + k;
            vec3 q = (br.P[a] + br.P[b] + br.P[c] + br.P[d]) * 0.25f;
            if (insideTube(mn, lo, hi + 1, q, -1e9f, 0.22f)) br.cut[(size_t)i * M + k] = 1;
        }
    }
    // 3) sommets, normales lisses, chunks
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti]; int N = tb.N, M = tb.M;
        tb.vBase = (int)rock.v.size();
        std::vector<vec3> Nn(tb.P.size(), vec3(0));
        for (int i = 0; i < N - 1; i++) for (int k = 0; k < M; k++) {
            int a = i * M + k, b = i * M + (k + 1) % M, c = (i + 1) * M + (k + 1) % M, d = (i + 1) * M + k;
            vec3 n1v = glm::cross(tb.P[b] - tb.P[a], tb.P[c] - tb.P[a]), n2v = glm::cross(tb.P[c] - tb.P[a], tb.P[d] - tb.P[a]);
            Nn[a] += n1v + n2v; Nn[b] += n1v; Nn[c] += n1v + n2v; Nn[d] += n2v;
        }
        for (size_t i = 0; i < tb.P.size(); i++) {
            vec3 n = glm::normalize(Nn[i] + vec3(1e-6f));
            if (glm::dot(n, tb.rings[i / M].c - tb.P[i]) < 0) n = -n;
            float kc = corrK((int)ti, (int)(i / M)); rock.v.push_back({tb.P[i], n, vec2(0), kc > 0.01f ? 0.2f : 0.0f, kc});
        }
        const int cr = 12;
        for (int i0 = 0; i0 < N - 1; i0 += cr) {
            int i1 = std::min(i0 + cr, N - 1);
            rock.beginChunk();
            for (int i = i0; i < i1; i++) for (int k = 0; k < M; k++) {
                if (tb.cut[(size_t)i * M + k]) continue;
                uint32_t a = tb.vBase + i * M + k, b = tb.vBase + i * M + (k + 1) % M, c = tb.vBase + (i + 1) * M + (k + 1) % M, d = tb.vBase + (i + 1) * M + k;
                rock.idx.insert(rock.idx.end(), {a, b, c, a, c, d});
            }
            rock.endChunk((size_t)tb.vBase + (size_t)i0 * M, (size_t)tb.vBase + (size_t)(i1 + 1) * M);
        }
        addCap((int)ti, N - 1, +1, tb.isMain ? 17.0f : 0.0f);
    }
}

void World::addCap(int ti, int ring, int dir, float mat) {
    Tube& tb = tubes[ti]; int M = tb.M; const Ring& r = tb.rings[ring];
    if (mat < 16.0f) {                                      // cul-de-sac : dôme de roche (même bruit que la galerie), sans jour
        const int RW = 8; std::vector<vec3> Pp, ref; float L = std::min(2.4f, r.w * 0.9f + 0.6f);
        for (int q = 0; q <= RW; q++) {
            float t = (float)q / RW, sc = std::cos(t * PI * 0.5f); vec3 c = r.c + r.T * (std::sin(t * PI * 0.5f) * L * dir);
            for (int k = 0; k < M; k++) {
                vec3 rel = tb.P[(size_t)ring * M + k] - r.c, p = c + rel * sc;
                if (q > 0 && q < RW) p += glm::normalize(rel + vec3(1e-4f)) * (fbm3(p * 0.6f, 3, seed + 31) - 0.5f) * 0.6f * std::sin(t * PI);
                Pp.push_back(p); ref.push_back(c);
            }
        }
        rock.beginChunk(); addGrid(rock, Pp, ref, RW + 1, M, 1.0f, 0.0f, 0.0f); rock.endChunk(); return;
    }
    vec3 apex = r.c + r.T * ((mat > 16.0f ? 3.5f : 2.0f) * dir) + vec3(0, mat > 16.0f ? 0.0f : -0.4f, 0);
    vec3 inward = -r.T * (float)dir;
    rock.beginChunk();
    uint32_t base = (uint32_t)rock.v.size();
    for (int k = 0; k < M; k++) rock.v.push_back({tb.P[(size_t)ring * M + k], inward, vec2(0), mat, 0.0f});
    rock.v.push_back({apex, inward, vec2(0), mat, 0.0f});
    for (int k = 0; k < M; k++) rock.idx.insert(rock.idx.end(), {base + (uint32_t)k, base + (uint32_t)((k + 1) % M), base + (uint32_t)M});
    rock.endChunk();
    for (int k = 0; k <= M; k++) rock.v[base + k].n = glm::normalize(inward + glm::normalize(rock.v[base + k].p - r.c) * -0.3f);
}

void World::addCone(vec3 base, vec3 axis, float len, float r0, Rng& rng) {
    const int rows = 9, cols = 10;
    vec3 e1 = glm::normalize(glm::cross(axis, std::fabs(axis.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 0, 1)));
    vec3 e2 = glm::cross(axis, e1);
    vec3 bend = vec3(rng.r(-1, 1), 0, rng.r(-1, 1)) * len * 0.08f;
    std::vector<vec3> P, ref; float sd = rng.f() * 50;
    for (int r = 0; r < rows; r++) {
        float t = r / float(rows - 1);
        float rad = r0 * std::pow(1 - t, 1.15f) + 0.004f;
        vec3 c = base + axis * (len * t) + bend * t * t;
        for (int k = 0; k < cols; k++) {
            float a = 2 * PI * k / cols;
            float nz = 0.75f + 0.5f * vn3(std::cos(a) * 2 + sd, t * 5, std::sin(a) * 2, seed + 55);
            P.push_back(c + (e1 * std::cos(a) + e2 * std::sin(a)) * rad * nz); ref.push_back(c);
        }
    }
    addGrid(rock, P, ref, rows, cols, -1.0f, 0.0f, 0.0f);
}

void World::genStalagmites(Rng& rng) {
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti]; int M = tb.M;
        int count = tb.isMain ? 150 : 22;
        rock.beginChunk(); size_t vstart = rock.v.size(); int inChunk = 0;
        for (int n = 0; n < count; n++) {
            int i = 8 + rng.next() % (tb.N - 16);
            if ((!tb.isMain && i < 16) || nearCorr((int)ti, i, 2)) continue;
            bool nearJ = false;
            for (size_t b = 1; b < tubes.size(); b++) if (tubes[b].parent == (int)ti && std::abs(i - tubes[b].junction) < 14) nearJ = true;
            if (nearJ) continue;
            bool ceil = rng.chance(0.62f);
            int k = ceil ? (int)(M * rng.r(0.08f, 0.42f)) : (int)(M * rng.r(0.58f, 0.92f));
            Vertex v = rock.v[(size_t)tb.vBase + (size_t)i * M + k];
            float len = ceil ? rng.r(0.25f, 1.5f) : rng.r(0.15f, 0.8f);
            vec3 axis = glm::normalize((ceil ? vec3(0, -1, 0) : vec3(0, 1, 0)) + v.n * 0.15f);
            addCone(v.p - v.n * 0.05f - axis * 0.04f, axis, len, len * rng.r(0.17f, 0.28f) + 0.04f, rng);
            if (ceil) drips.push_back(Drip{v.p + axis * (len - 0.1f), (int)ti, i, rng.r(0.0f, 10.0f)});
            if (++inChunk == 8) { rock.endChunk(vstart, rock.v.size()); rock.beginChunk(); vstart = rock.v.size(); inChunk = 0; }
        }
        rock.endChunk(vstart, rock.v.size());
    }
}

// ---------- bois ---------------------------------------------------------------
int World::addB(int ti, int fid, int role, int side, vec3 a, vec3 b, vec3 hint, float hw, float ht, Rng& rng, float ext, float jit) {
    size_t before = wood.boards.size();
    addBoardBetween(wood, a, b, hint, hw, ht, 1.0f, rng, ext, jit);
    if (wood.boards.size() == before) return -1;
    Board& bd = wood.boards.back(); bd.tube = ti; bd.frame = fid; bd.role = role; bd.side = side;
    bd.hp = (role == 1 || role == 2) ? 2 : 1;
    return (int)wood.boards.size() - 1;
}

void World::addWeb(int fid, vec3 corner, vec3 A, vec3 B, float size) {
    vec3 n = glm::normalize(glm::cross(A, B));
    uint32_t i0 = (uint32_t)webM.idx.size(); uint32_t base = (uint32_t)webM.v.size();
    webM.v.push_back({corner, n, vec2(0, 0), 3.0f, size});
    webM.v.push_back({corner + A * size, n, vec2(1, 0), 3.0f, size});
    webM.v.push_back({corner + B * size, n, vec2(0, 1), 3.0f, size});
    webM.idx.insert(webM.idx.end(), {base, base + 1, base + 2});
    Web w; w.c = corner + (A + B) * size * 0.33f; w.r = size * 0.6f; w.i0 = i0; w.alive = true; w.frame = fid; webs.push_back(w);
    if (fid >= 0) frames[fid].webs.push_back((int)webs.size() - 1);
}

void World::addFrame(int ti, int i, Rng& rng, bool doorway) {
    Tube& tb = tubes[ti]; const Ring& r = tb.rings[i];
    int fid = (int)frames.size(); frames.push_back(Frame()); frames[fid].tube = ti;
    float fy = tb.floorY(i, 0.0f), xl0, xr0;
    if (!tb.wallRange(i, fy + 0.7f, xl0, xr0)) return;
    float xl = xl0 + 0.18f, xr = xr0 - 0.18f;
    float fyl = tb.floorY(i, xl), fyr = tb.floorY(i, xr);
    float tyl = tb.ceilY(i, xl + 0.12f) - 0.2f, tyr = tb.ceilY(i, xr - 0.12f) - 0.2f;
    wood.beginChunk();
    for (int s = 0; s < 2; s++) {
        float x = s ? xr : xl, y0 = s ? fyr : fyl, y1 = s ? tyr : tyl, lean = rng.r(-0.05f, 0.05f);
        for (int b = 0; b < 2; b++) {
            float t = (b - 0.5f) * 0.095f + rng.r(-0.01f, 0.01f);
            vec3 a = tb.toWorld(i, x + rng.r(-0.02f, 0.02f), y0 - 0.1f, t), c = tb.toWorld(i, x + lean, y1 - 0.12f, t);
            int id = addB(ti, fid, 1, s, a, c, r.T, 0.15f, 0.045f, rng); if (id >= 0) frames[fid].post[s].push_back(id);
        }
    }
    for (int b = 0; b < 2; b++) {
        float t = (b - 0.5f) * 0.095f + rng.r(-0.01f, 0.01f), sh = b ? 0.12f : -0.06f;
        vec3 a = tb.toWorld(i, xl - 0.25f + sh, tyl, t), c = tb.toWorld(i, xr + 0.25f + sh, tyr, t);
        int id = addB(ti, fid, 2, 0, a, c, r.T, 0.14f, 0.045f, rng); if (id >= 0) frames[fid].beam.push_back(id);
    }
    for (int s = 0; s < 2; s++) {
        if (!doorway && !rng.chance(0.75f)) continue;
        float sg = s ? -1.f : 1.f, x = s ? xr : xl, ty = s ? tyr : tyl;
        vec3 a = tb.toWorld(i, x, ty - rng.r(0.9f, 1.3f), 0.14f), b = tb.toWorld(i, x + sg * rng.r(0.9f, 1.2f), ty - 0.1f, 0.14f);
        int id = addB(ti, fid, 3, s, a, b, r.T, 0.085f, 0.035f, rng); if (id >= 0) frames[fid].brace[s].push_back(id);
    }
    if (rng.chance(0.5f) && !addBarricade(ti, i, rng)) {         // barricade en travers (ou, sans modèle, une simple planche)
        float y = std::max(fyl, fyr) + rng.r(0.35f, 0.6f);
        addB(ti, fid, 0, 0, tb.toWorld(i, xl, y, -0.14f), tb.toWorld(i, xr, y + rng.r(-0.05f, 0.05f), -0.14f), r.T, 0.07f, 0.035f, rng);
    }
    wood.endChunk();
    // toiles d'araignée dans les coins haut du cadre
    for (int s = 0; s < 2; s++) if (rng.chance(0.6f)) {
        float x = s ? xr - 0.14f : xl + 0.14f, ty = s ? tyr : tyl; float sg = s ? -1.f : 1.f;
        vec3 corner = tb.toWorld(i, x, ty - 0.13f, 0.1f);
        vec3 A = glm::normalize(tb.toWorld(i, x + sg * 1.0f, ty - 0.13f, 0.1f) - corner), B = -r.U;
        addWeb(fid, corner, A, B, rng.r(0.5f, 1.2f));
    }
}

void World::addLagging(int ti, int i, int j, int side, Rng& rng) {
    Tube& tb = tubes[ti]; int mid = (i + j) / 2; const Ring& r = tb.rings[mid];
    float fy = tb.floorY(mid, 0.0f), topY = std::min(tb.ceilY(mid, 0.0f), 3.0f) - 0.9f;
    float y = fy + 0.3f + rng.r(0, 0.1f);
    wood.beginChunk(); wood.geom = 1;
    while (y < topY) {
        if (rng.chance(0.82f)) {
            float xl, xr; if (!tb.wallRange(mid, y, xl, xr)) break;
            float xw = side > 0 ? xr : xl, span = (j - i) * tb.ds * 0.5f - 0.3f;
            if (span < 0.5f) break;
            float t0 = rng.r(-span, span * 0.4f), t1 = std::min(t0 + rng.r(1.2f, 2.6f), span + 0.2f);
            vec3 a = tb.toWorld(mid, xw - side * rng.r(-0.06f, 0.03f), y, t0), b = tb.toWorld(mid, xw - side * rng.r(-0.06f, 0.03f), y + rng.r(-0.03f, 0.04f), t1);
            addB(ti, -1, 0, 0, a, b, r.R * (float)(-side), 0.13f, 0.03f, rng, 0.0f, 0.015f);
        }
        y += 0.265f;
    }
    wood.geom = 0; wood.endChunk();
}

void World::addLadder(int ti, int i, int side, Rng& rng) {
    Tube& tb = tubes[ti]; const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr;
    float yTop = std::min(tb.ceilY(i, 0.0f) - 0.5f, fy + 3.1f);
    if (!tb.wallRange(i, yTop, xl, xr)) return;
    float xwTop = side > 0 ? xr : xl; float xBase = xwTop - side * 0.95f;
    float yBase = tb.floorY(i, xBase) + 0.02f;
    vec3 dirLadder = glm::normalize(tb.toWorld(i, xwTop - side * 0.07f, yTop) - tb.toWorld(i, xBase, yBase));
    wood.beginChunk();
    LadderObj lad; wood.geom = 2;                               // planches invisibles : collision et destruction
    for (int s = 0; s < 2; s++) {
        float t = s ? 0.22f : -0.22f;
        lad.boards.push_back(addB(ti, -1, 0, 0, tb.toWorld(i, xBase, yBase, t), tb.toWorld(i, xwTop - side * 0.07f, yTop, t), r.T, 0.04f, 0.025f, rng, 0.0f, 0.006f));
    }
    int rungs = 9;
    for (int k = 0; k < rungs; k++) {
        float f = (k + 0.7f) / (rungs + 0.3f);
        float x = mixf(xBase, xwTop - side * 0.07f, f), y = mixf(yBase, yTop, f);
        lad.boards.push_back(addB(ti, -1, 0, 0, tb.toWorld(i, x - side * 0.02f, y, -0.24f), tb.toWorld(i, x - side * 0.02f, y, 0.24f), dirLadder, 0.035f, 0.02f, rng, 0.0f, 0.005f));
    }
    wood.geom = 0;
    lad.boards.erase(std::remove(lad.boards.begin(), lad.boards.end(), -1), lad.boards.end());
    if (gLadderMdl.ok) {
        vec3 b0 = tb.toWorld(i, xBase, yBase, 0.0f), b1 = tb.toWorld(i, xwTop - side * 0.07f, yTop, 0.0f);
        vec3 ey = glm::normalize(b1 - b0), ex = glm::normalize(r.T - ey * glm::dot(r.T, ey)), ez = glm::cross(ex, ey);
        float len = glm::length(b1 - b0);
        mat4 W(1.0f); W[0] = vec4(ex * (0.44f / 0.52f), 0); W[1] = vec4(ey * (len / 3.0f), 0); W[2] = vec4(ez, 0); W[3] = vec4(b0, 1);
        lad.i0 = (uint32_t)wood.idx.size(); addModelXf(wood, gLadderMdl, W, rng.f() * 10.0f, vec2(rng.f() * 5.0f, rng.f() * 5.0f));
        lad.n = (uint32_t)wood.idx.size() - lad.i0;
        lad.look = (b0 + b1) * 0.5f; lad.stand = tb.toWorld(i, 0.0f, tb.floorY(i, 0.0f) + 1.65f);
        int lid = (int)ladders.size();
        for (int id : lad.boards) wood.boards[id].group = lid;
        ladders.push_back(lad);
    } else {                                                    // pas de modèle : échelle en planches (boîtes)
        for (int id : lad.boards) {
            Board& b = wood.boards[id]; Rng z(id); b.i0 = (uint32_t)wood.idx.size();
            addBoardBox(wood, b.c, b.dl, b.dw, b.dt, b.hl, b.hw, b.ht, 1.0f, z.f() * 10.0f, z, 0.005f); b.n = 36;
        }
    }
    wood.endChunk();
}

// Barricade (models/planks_barricade.obj) barrant toute la galerie : modèle visible + planches invisibles pour la collision.
// Comme une échelle, elle s'effondre d'un bloc quand on la frappe (2 coups).
bool World::addBarricade(int ti, int i, Rng& rng) {
    if (!gBarricadeMdl.ok) return false;
    Tube& tb = tubes[ti]; const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr; if (!tb.wallRange(i, fy + 0.8f, xl, xr)) return false;
    float xm = (xl + xr) * 0.5f, Wd = xr - xl + 0.12f;
    float base = std::min(fy, std::min(tb.floorY(i, xl + 0.3f), tb.floorY(i, xr - 0.3f))) - 0.05f;
    float top = std::min(tb.ceilY(i, xm), std::min(tb.ceilY(i, xl + 0.3f), tb.ceilY(i, xr - 0.3f))) - 0.12f;
    float sx = Wd / 1.6f, sy = std::min(1.6f, std::max(0.8f, (top - base) / 2.0f));
    if (Wd < 1.0f || Wd > 6.0f) return false;
    vec3 o = tb.toWorld(i, xm, base, -0.14f), ex = r.R, ey = r.U, ez = glm::cross(ex, ey);
    LadderObj ob; ob.kind = 1; wood.geom = 2;
    for (float h : {0.3f, 0.77f, 1.24f, 1.71f}) {                       // planches horizontales
        vec3 c = o + ey * (h * sy) + ez * 0.045f;
        ob.boards.push_back(addB(ti, -1, 0, 0, c - ex * (Wd * 0.5f), c + ex * (Wd * 0.5f), ez, 0.1f * sy, 0.03f, rng, 0.0f, 0.0f));
    }
    for (float sg : {-1.0f, 1.0f}) {                                    // montants
        vec3 c = o + ex * (0.5f * sx * sg) + ez * 0.0175f;
        ob.boards.push_back(addB(ti, -1, 0, 0, c, c + ey * (2.0f * sy), ez, 0.1f * sx, 0.0175f, rng, 0.0f, 0.0f));
    }
    wood.geom = 0;
    ob.boards.erase(std::remove(ob.boards.begin(), ob.boards.end(), -1), ob.boards.end());
    mat4 W(1.0f); W[0] = vec4(ex * sx, 0); W[1] = vec4(ey * sy, 0); W[2] = vec4(ez, 0); W[3] = vec4(o, 1);
    ob.i0 = (uint32_t)wood.idx.size(); addModelXf(wood, gBarricadeMdl, W, rng.f() * 10.0f, vec2(rng.f() * 5.0f, rng.f() * 5.0f));
    ob.n = (uint32_t)wood.idx.size() - ob.i0;
    ob.look = o + ey * (1.0f * sy); ob.stand = tb.toWorld(i, xm, tb.floorY(i, xm) + 1.65f, -2.2f);
    int lid = (int)ladders.size();
    for (int id : ob.boards) { wood.boards[id].group = lid; wood.boards[id].hp = 2; }
    ladders.push_back(ob);
    return true;
}


// Éboulis : un mur de blocs de pierre (appareil en quinconce, plusieurs épaisseurs) qui remplit toute la section de la galerie.
// Chaque bloc est une Board (collision + dégâts) de rôle 7 ; il faut le frapper à la pioche (2 coups) pour se frayer un passage.
void World::addStoneWall(int ti, int i, int layers, Rng& rng) {
    Tube& tb = tubes[ti]; int wid = (int)walls.size(); StoneWall W; W.tube = ti; W.ring = i; W.c = tb.rings[i].c + tb.rings[i].U * 1.0f;
    wood.beginChunk(); wood.geom = 0; const float STEP = 0.74f;
    for (int l = 0; l < layers; l++) {
        float t0 = (l - (layers - 1) * 0.5f) * 0.78f; int ri = std::min(tb.N - 2, std::max(1, i + (int)std::lround(t0 / tb.ds))); const Ring& r = tb.rings[ri];
        float fy0 = tb.floorY(ri, 0.0f), xl0, xr0; if (!tb.wallRange(ri, fy0 + 0.8f, xl0, xr0)) continue;
        float base = std::min(fy0, std::min(tb.floorY(ri, xl0 + 0.4f), tb.floorY(ri, xr0 - 0.4f))) - 0.12f;
        for (int row = 0; row < 10; row++) {
            float y0 = base + row * STEP, y1 = y0 + STEP, yc = (y0 + y1) * 0.5f, xl, xr;
            if (!tb.wallRange(ri, yc, xl, xr)) break;
            float a, b2; if (tb.wallRange(ri, y0 + 0.05f, a, b2)) { xl = std::min(xl, a); xr = std::max(xr, b2); }
            if (tb.wallRange(ri, y1 - 0.05f, a, b2)) { xl = std::min(xl, a); xr = std::max(xr, b2); }
            float xm = (xl + xr) * 0.5f, ceilY = std::min(tb.ceilY(ri, xm), tb.ceilY(ri, xm + (xr - xl) * 0.25f));
            bool last = y1 > ceilY - 0.3f; if (last) y1 = ceilY + 0.25f;
            float hh = (y1 - y0) * 0.5f; yc = (y0 + y1) * 0.5f;
            float x = xl - 0.15f - (((row + l) & 1) ? 0.3f : 0.0f);                          // quinconce
            while (x < xr + 0.15f) {
                float w = rng.r(0.62f, 0.9f), x0 = std::max(x, xl - 0.15f), x1 = std::min(x + w, xr + 0.15f); x += w;
                if (x1 - x0 < 0.18f) continue;
                float xc = (x0 + x1) * 0.5f, phi = rng.r(-0.12f, 0.12f), cs = std::cos(phi), sn = std::sin(phi);
                vec3 dl = r.R * cs + r.T * sn, dt = -r.R * sn + r.T * cs, c = tb.toWorld(ri, xc, yc + rng.r(-0.03f, 0.03f), rng.r(-0.05f, 0.05f));
                size_t before = wood.boards.size();
                addBoard(wood, c, dl, r.U, dt, (x1 - x0) * 0.5f, hh + 0.02f, rng.r(0.36f, 0.46f), 0.4f, rng.f() * 10.0f, rng, 0.035f);
                if (wood.boards.size() == before) continue;
                Board& bd = wood.boards.back(); bd.tube = ti; bd.role = 7; bd.hp = 2; bd.wall = wid; bd.side = (last ? 1 : 0) | (row == 0 ? 2 : 0);
                W.blocks.push_back((int)wood.boards.size() - 1);
            }
            if (last) break;
        }
    }
    wood.endChunk();
    std::printf("  eboulis : galerie %d, %.0f m, %zu blocs\n", ti, i * tb.ds, W.blocks.size());
    if (!W.blocks.empty()) walls.push_back(W);
}

void World::breakStone(int bi, vec3 hit, vec3 push) {
    Board& b = wood.boards[bi]; if (!b.alive) return;
    b.alive = false; wood.killIndices(b.i0, b.n);
    vec3 pu = push; pu.y *= 0.3f;
    for (int k = 0; k < 4; k++) {                                  // 4 morceaux (octants) qui retombent
        float sx = (drng.chance(0.5f) ? 1.f : -1.f), sy = (drng.chance(0.5f) ? 1.f : -1.f), sz = (drng.chance(0.5f) ? 1.f : -1.f);
        Debris d; d.p = b.c + b.dl * (sx * b.hl * 0.5f) + b.dw * (sy * b.hw * 0.5f) + b.dt * (sz * b.ht * 0.5f);
        d.ax = b.dl; d.ay = b.dw; d.az = b.dt;
        d.h = vec3(b.hl * 0.5f, b.hw * 0.5f, b.ht * 0.5f) * drng.r(0.7f, 1.0f);
        d.v = pu * drng.r(1.0f, 2.5f) + glm::normalize(d.p - b.c + vec3(1e-3f)) * drng.r(0.8f, 2.0f) + vec3(0, drng.r(0.5f, 2.2f), 0);
        d.w = vec3(drng.r(-5, 5), drng.r(-5, 5), drng.r(-5, 5)); d.seed = drng.f() * 10; d.tube = b.tube; d.ring = -1; d.stone = true;
        if (debris.size() > 320) debris.erase(debris.begin());
        debris.push_back(d);
    }
    spawnChips(b.c, push, 9, true);
    if (b.wall >= 0) settleStone(b.wall);
}

// un bloc sans appui (ni sol, ni bloc dessous, ni plafond calé entre voisins) s'écroule
void World::settleStone(int wid) {
    StoneWall& W = walls[wid];
    for (int pass = 0; pass < 6; pass++) {
        std::vector<int> fall;
        for (int id : W.blocks) {
            const Board& b = wood.boards[id]; if (!b.alive || (b.side & 2)) continue;
            bool sup = false, left = false, right = false;
            for (int jd : W.blocks) {
                if (jd == id) continue; const Board& o = wood.boards[jd]; if (!o.alive) continue;
                vec3 d = o.c - b.c; float dy = d.y, dx = glm::dot(d, b.dl), dz = glm::dot(d, b.dt);
                if (std::fabs(dz) > (o.ht + b.ht) * 0.9f) continue;
                if (dy < 0 && dy > -(o.hw + b.hw) - 0.25f && std::fabs(dx) < (o.hl + b.hl) * 0.9f) sup = true;
                if (std::fabs(dy) < 0.25f && dx > 0 && dx < o.hl + b.hl + 0.3f) right = true;
                if (std::fabs(dy) < 0.25f && dx < 0 && dx > -(o.hl + b.hl + 0.3f)) left = true;
            }
            if (sup) continue;
            if ((b.side & 1) && left && right) continue;
            fall.push_back(id);
        }
        if (fall.empty()) break;
        for (int id : fall) {
            Board& b = wood.boards[id]; if (!b.alive) continue;
            b.alive = false; wood.killIndices(b.i0, b.n);
            Debris d; d.p = b.c; d.ax = b.dl; d.ay = b.dw; d.az = b.dt; d.h = vec3(b.hl, b.hw, b.ht) * 0.92f;
            d.v = vec3(drng.r(-0.6f, 0.6f), -0.3f, drng.r(-0.6f, 0.6f)); d.w = vec3(drng.r(-3, 3), drng.r(-3, 3), drng.r(-3, 3)); d.seed = drng.f() * 10;
            d.tube = b.tube; d.ring = -1; d.stone = true; if (debris.size() > 320) debris.erase(debris.begin()); debris.push_back(d);
        }
    }
}

void World::addTorch(int ti, int i, int side, Rng& rng) {
    Tube& tb = tubes[ti]; const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr, yb = fy + 1.7f;
    if (!tb.wallRange(i, yb, xl, xr)) return;
    float xw = side > 0 ? xr : xl;
    torchM.beginChunk();
    // planche de fixation clouée au mur
    addBoardBetween(torchM, tb.toWorld(i, xw - side * 0.035f, yb - 0.4f), tb.toWorld(i, xw - side * 0.035f, yb + 0.35f), r.R * (float)side, 0.075f, 0.022f, 1.0f, rng, 0.0f, 0.006f);
    // manche incliné
    vec3 base = tb.toWorld(i, xw - side * 0.095f, yb - 0.35f);
    vec3 dir = glm::normalize(r.R * (-side * 0.26f) + r.U * 0.96f + r.T * rng.r(-0.05f, 0.05f));
    vec3 top = base + dir * 1.15f;
    addBoardBetween(torchM, base, top, r.T, 0.04f, 0.04f, 1.0f, rng, 0.0f, 0.004f);
    // brides métalliques
    for (float f : {0.12f, 0.38f}) {
        vec3 p = base + dir * (1.15f * f);
        addBoardBetween(torchM, p - dir * 0.022f, p + dir * 0.022f, r.R, 0.06f, 0.06f, 5.0f, rng, 0.0f, 0.0f);
    }
    // étoupe goudronnée + goutte de poix
    addBoardBetween(torchM, top - dir * 0.22f, top + dir * 0.04f, r.T, 0.072f, 0.072f, 2.0f, rng, 0.0f, 0.01f);
    addBoardBetween(torchM, top - dir * 0.02f, top + dir * 0.07f, r.T, 0.055f, 0.055f, 2.0f, rng, 0.0f, 0.012f);
    torchM.endChunk();
    int bi = (int)std::floor(biomeF(ti, i)); bi = std::min(4, std::max(0, bi));
    addLight(top + dir * 0.13f + r.U * 0.04f, BIOMES[bi].torch, BIOMES[bi].torchI, bi, true);
}

void World::addPuddles(int ti, int count, Rng& rng) {
    Tube& tb = tubes[ti];
    for (int n = 0; n < count; n++) {
        int lim = tb.isMain ? doorRing - 8 : tb.N - 10;
        int i = 10 + rng.next() % std::max(1, lim - 10); const Ring& r = tb.rings[i];
        int bi = (int)std::floor(biomeF(ti, i) + 0.5f); if (bi >= 5 || nearCorr(ti, i, 3)) continue; float pmat = bi == 4 ? 10.0f : 4.0f;
        float fy = tb.floorY(i, 0.0f), xl, xr; if (!tb.wallRange(i, fy + 0.6f, xl, xr)) continue;
        float x0 = rng.r(xl + 0.9f, xr - 0.9f), rx = rng.r(0.5f, 1.4f) * (bi == 1 ? 1.35f : 1.0f), rz = rx * rng.r(0.9f, 2.2f);
        std::vector<float> fl;
        for (int q = 0; q < 12; q++) {
            float a = 2 * PI * q / 12; int ii = std::max(1, std::min(tb.N - 2, i + (int)std::lround(std::sin(a) * rz * 0.5f / tb.ds)));
            fl.push_back(tb.floorY(ii, x0 + std::cos(a) * rx * 0.8f));
        }
        std::sort(fl.begin(), fl.end());
        float level = fl[7] + 0.03f;
        const int seg = 22; waterM.beginChunk();
        uint32_t base = (uint32_t)waterM.v.size(); float sd = rng.f() * 20;
        vec3 cw = tb.toWorld(i, x0, level);
        waterM.v.push_back({cw, vec3(0, 1, 0), vec2(0, 0), pmat, sd});
        if (bi == 4 && rng.chance(0.4f)) addLight(cw + vec3(0, 0.35f, 0), vec3(1.0f, 0.32f, 0.06f), 1.2f, 4, false);
        for (int q = 0; q < seg; q++) {
            float a = 2 * PI * q / seg; float nz = 0.7f + 0.6f * vn3(std::cos(a) * 1.5f + sd, std::sin(a) * 1.5f, sd, seed + 3);
            vec3 p = cw + r.R * (std::cos(a) * rx * nz) + r.T * (std::sin(a) * rz * nz);
            p.y = cw.y;
            waterM.v.push_back({p, vec3(0, 1, 0), vec2(1, 0), pmat, sd});
        }
        for (int q = 0; q < seg; q++) waterM.idx.insert(waterM.idx.end(), {base, base + 1 + (uint32_t)q, base + 1 + (uint32_t)((q + 1) % seg)});
        waterM.endChunk();
    }
}

void World::addRatHoles(int ti, int i, int k, Rng& rng) {
    Tube& tb = tubes[ti]; int M = tb.M;
    auto makeHole = [&](int ring, int side, vec3& pOut, vec3& nOut, float& xOut) -> bool {
        float fy = tb.floorY(ring, 0.0f), xl, xr; float yy = fy + 0.28f;
        if (!tb.wallRange(ring, yy, xl, xr)) return false;
        float xw = side > 0 ? xr : xl; int bk = 0; float bd = 1e9f;
        for (int q = 0; q < M; q++) { vec2 l = tb.loc[(size_t)ring * M + q]; float d = (l.x - xw) * (l.x - xw) + (l.y - yy) * (l.y - yy); if (d < bd) { bd = d; bk = q; } }
        const Vertex& v = rock.v[(size_t)tb.vBase + (size_t)ring * M + bk];
        vec3 n = v.n, p = v.p; vec3 e1 = glm::normalize(glm::cross(n, vec3(0, 1, 0))), e2 = glm::cross(n, e1);
        pOut = p; nOut = n; xOut = tb.loc[(size_t)ring * M + bk].x;
        rock.beginChunk(); uint32_t base = (uint32_t)rock.v.size();
        rock.v.push_back({p + n * 0.012f, n, vec2(0), 7.0f, 0.0f});
        const int seg = 14;
        for (int q = 0; q < seg; q++) { float a = 2 * PI * q / seg; rock.v.push_back({p + n * 0.012f + e1 * (std::cos(a) * 0.1f) + e2 * (std::sin(a) * 0.085f), n, vec2(0), 7.0f, 0.0f}); }
        for (int q = 0; q < seg; q++) rock.idx.insert(rock.idx.end(), {base, base + 1 + (uint32_t)q, base + 1 + (uint32_t)((q + 1) % seg)});
        std::vector<vec3> P, ref; const int rows = 4, cols = 14;
        float rr[rows] = {0.24f, 0.17f, 0.125f, 0.1f}, oo[rows] = {-0.03f, 0.04f, 0.035f, 0.0f};
        for (int r2 = 0; r2 < rows; r2++) for (int c = 0; c < cols; c++) {
            float a = 2 * PI * c / cols, nz = 0.85f + 0.3f * H1(c, r2, ring, seed + 77);
            P.push_back(p + n * oo[r2] + e1 * (std::cos(a) * rr[r2] * nz) + e2 * (std::sin(a) * rr[r2] * 0.9f * nz)); ref.push_back(p + n * 2.0f);
        }
        addGrid(rock, P, ref, rows, cols, 1.0f, 0.0f, 0.0f);
        rock.endChunk(); return true;
    };
    int side = rng.chance(0.5f) ? 1 : -1; HolePair h; h.tube = ti; h.iA = i; h.iB = std::min(tb.N - 6, i + k);
    if (!makeHole(h.iA, side, h.pA, h.nA, h.xA)) return;
    if (!makeHole(h.iB, -side, h.pB, h.nB, h.xB)) return;
    holes.push_back(h);
}

void World::genWood(Rng& rng) {
    wood.track = true;
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti]; int tIdx = (int)ti;
        std::vector<int> stations; int i = tb.isMain ? 18 : 4; const int lim = tb.isMain ? doorRing - 10 : tb.N - 12;
        std::vector<char> doorway;
        auto junc = [&](int ring, int m) { for (size_t b = 1; b < tubes.size(); b++) if (tubes[b].parent == tIdx && std::abs(ring - tubes[b].junction) < m) return true; return false; };
        // éboulis : murs de blocs de pierre qui bouchent toute la galerie (aucun cadre, échelle ni trou de rat à proximité)
        std::vector<int> wallRings;
        if (tb.isMain) {
            static const float WF[5] = {0.12f, 0.30f, 0.48f, 0.65f, 0.81f};
            for (int w = 0; w < 5; w++) {                       // pour chaque jalon, le point le plus étroit dans une fenêtre de +-14 m
                int best = -1; float bw = 1e9f;
                for (int k = (int)(doorRing * WF[w]) - 45; k <= (int)(doorRing * WF[w]) + 45; k += 2) {
                    float xl, xr, fy = tb.floorY(k, 0.0f); if (k < 30 || junc(k, 40) || nearCorr(0, k, 20) || !tb.wallRange(k, fy + 0.8f, xl, xr)) continue;
                    if (xr - xl < bw) { bw = xr - xl; best = k; }
                }
                if (best >= 0 && bw < 8.5f) wallRings.push_back(best);
            }
        } else if (tb.N > 70) {
            for (int tries = 0; tries < 10; tries++) { int k = tb.N / 2 + (int)(rng.next() % 12) - 6; float xl, xr, fy = tb.floorY(k, 0.0f); if (!tb.wallRange(k, fy + 0.8f, xl, xr) || xr - xl > 5.0f) continue; wallRings.push_back(k); break; }
        }
        auto nearWall = [&](int ring) { for (int wr : wallRings) if (std::abs(ring - wr) < 16) return true; return false; };
        auto nearJunction = [&](int ring) { return junc(ring, 15) || nearWall(ring) || nearCorr(tIdx, ring, 4); };
        while (i < lim) {
            if (!nearJunction(i)) { stations.push_back(i); doorway.push_back(!tb.isMain && stations.size() == 1); }
            i += (int)(rng.r(4.8f, 8.5f) / tb.ds); if (!tb.isMain && stations.size() == 1) i = std::max(i, 20);
        }
        std::vector<char> has(stations.size(), 0);
        for (size_t s = 0; s < stations.size(); s++)
            if (doorway[s] || rng.chance(0.86f) || s < 2) { addFrame(tIdx, stations[s], rng, doorway[s] != 0); has[s] = 1; }
        for (size_t s = 0; s + 1 < stations.size(); s++)
            if (has[s] && has[s + 1] && rng.chance(0.5f) && !nearWall((stations[s] + stations[s + 1]) / 2) && !nearWall(stations[s]) && !nearWall(stations[s + 1]))
                for (int side = -1; side <= 1; side += 2) if (rng.chance(0.7f)) addLagging(tIdx, stations[s], stations[s + 1], side, rng);
        // torches
        int side = rng.chance(0.5f) ? 1 : -1;
        if (tb.isMain) { for (size_t s = 0; s < stations.size(); s += 2) { addTorch(tIdx, std::min(stations[s] + 2, tb.N - 2), side, rng); side = -side; } }
        else { addTorch(tIdx, std::min(tb.N - 2, 18), side, rng); if (tb.N > 70) addTorch(tIdx, tb.N - 10, -side, rng); }
        // échelles
        int ladders = tb.isMain ? 4 : 1;
        for (int n = 0; n < ladders; n++) {
            int k = tb.isMain ? 40 + rng.next() % std::max(1, doorRing - 110) : tb.N - 8;
            if (nearJunction(k)) continue;
            addLadder(tIdx, k, rng.chance(0.5f) ? 1 : -1, rng);
        }
        // planches appuyées ou tombées
        int debrisN = tb.isMain ? 40 : 8;
        for (int n = 0; n < debrisN; n++) {
            int k = 14 + rng.next() % std::max(1, lim - 14); const Ring& r = tb.rings[k]; if (nearJunction(k)) continue;
            float fy = tb.floorY(k, 0.0f), xl, xr; if (!tb.wallRange(k, fy + 0.5f, xl, xr)) continue;
            wood.beginChunk(); wood.geom = 1;
            if (rng.chance(0.5f)) {
                int sd = rng.chance(0.5f) ? 1 : -1; float xw = sd > 0 ? xr : xl;
                float len = rng.r(1.5f, 2.6f), lean = rng.r(0.35f, 0.6f);
                vec3 b = tb.toWorld(k, xw - sd * (lean + 0.12f), tb.floorY(k, xw - sd * lean) + 0.04f, rng.r(-0.3f, 0.3f));
                vec3 t = b + r.U * (len * 0.92f) + r.R * ((float)sd * (lean * 0.9f));
                addB(tIdx, -1, 0, 0, b, t, r.T, 0.11f, 0.03f, rng, 0.0f, 0.015f);
            } else {
                float x = rng.r(xl + 0.6f, xr - 0.6f), len = rng.r(0.8f, 2.2f);
                float ang = rng.r(0, PI); vec3 d = r.R * std::cos(ang) + r.T * std::sin(ang);
                vec3 c = tb.toWorld(k, x, tb.floorY(k, x) + 0.05f);
                addB(tIdx, -1, 0, 0, c - d * (len * 0.5f), c + d * (len * 0.5f) + r.U * rng.r(0.0f, 0.12f), r.U, 0.1f, 0.03f, rng, 0.0f, 0.015f);
            }
            wood.geom = 0; wood.endChunk();
        }
        // trous de rats par paires
        int pairs = tb.isMain ? 7 : 1;
        for (int n = 0; n < pairs; n++) {
            int k = 20 + rng.next() % std::max(1, lim - 50); if (nearJunction(k) || nearJunction(k + 30)) continue;
            addRatHoles(tIdx, k, 14 + rng.next() % 26, rng);
        }
        addPuddles(tIdx, tb.isMain ? 60 : 8, rng);
        addDecor(tIdx, rng);
        for (int wr : wallRings) addStoneWall(tIdx, wr, tb.isMain ? 3 : 2, rng);
    }
    addDoorAndExit(rng);
    placeEntities(rng);
    auto inWall = [&](vec3 p) { for (const StoneWall& w : walls) if (glm::length(p - w.c) < 3.4f) return true; return false; };
    auto inCorr = [&](vec3 p) { if (corrA < 0) return false; int r = tubes[0].locate(p, -1); return glm::length(p - tubes[0].rings[r].c) < 6.0f && nearCorr(0, r, 6); };
    spawns.erase(std::remove_if(spawns.begin(), spawns.end(), [&](const Spawn& sp) { return inWall(sp.pos) || inCorr(sp.pos); }), spawns.end());
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [&](const Pickup& pk) { return pk.kind == 1 && inCorr(pk.p); }), pickups.end());
    if (corrA >= 0) addLight(tubes[0].toWorld(std::min(tubes[0].N - 2, corrB + 7), 0.0f, tubes[0].floorY(std::min(tubes[0].N - 2, corrB + 7), 0.0f) + 2.1f), vec3(0.55f, 0.68f, 0.92f), 3.4f, 5, false);   // lumière froide au bout du couloir
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [&](const Pickup& pk) { return inWall(pk.p); }), pickups.end());
    // toiles supplémentaires accrochées au plafond / à la paroi
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti];
        for (int n = 0; n < (tb.isMain ? 14 : 3); n++) {
            int k = 14 + rng.next() % std::max(1, (tb.isMain ? doorRing - 8 : tb.N - 12) - 14); const Ring& r = tb.rings[k]; if (nearCorr((int)ti, k, 3)) continue;
            float xl, xr; float y = tb.ceilY(k, 0.0f) - 0.6f; if (!tb.wallRange(k, y, xl, xr)) continue;
            int sd = rng.chance(0.5f) ? 1 : -1; float xw = sd > 0 ? xr : xl;
            addWeb(-1, tb.toWorld(k, xw - sd * 0.12f, y, 0), r.T, -r.U, rng.r(0.7f, 1.4f));
        }
    }
}

// ---------- biomes, décors, porte de sortie, objets et ennemis -----------------------
float World::biomeF(int ti, int i) const {
    const Tube& tb = tubes[ti];
    if (!tb.isMain) return std::floor(biomeF(tb.parent, tb.junction) + 0.5f);
    if (doorRing <= 0) return 0.0f;
    if (i >= doorRing) return 5.0f;
    float u = 5.0f * (float)i / (float)doorRing, fl = std::floor(u);
    return fl + smooth(0.86f, 1.0f, u - fl);                 // plateaux + courtes transitions
}

float World::biomeAtPos(vec3 p) const {
    if (tubes.empty()) return 0.0f;
    if (inHub(p)) return (float)HUB_BIOME;
    int bt = 0, bi = 0; float bd = 1e18f;
    for (size_t t = 0; t < tubes.size(); t++)
        for (int i = 0; i < tubes[t].N; i += 4) { vec3 d = p - tubes[t].rings[i].c; float dd = glm::dot(d, d); if (dd < bd) { bd = dd; bt = (int)t; bi = i; } }
    return biomeF(bt, std::min(tubes[bt].N - 1, bi));
}

void World::assignBiomes(Mesh& m) { for (auto& v : m.v) v.biome = biomeAtPos(v.p); }

void World::addLight(vec3 pos, vec3 col, float inten, int biome, bool flame) {
    Torch t; t.pos = pos; t.phase = 0; t.col = col; t.inten = inten; t.biome = biome; t.flame = flame; torches.push_back(t);
}

void World::addCrystal(vec3 base, vec3 axis, float len, float rad, float hue) {
    vec3 e1 = glm::normalize(glm::cross(axis, std::fabs(axis.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 0, 1))), e2 = glm::cross(axis, e1);
    const int rows = 4, cols = 6; std::vector<vec3> P, ref;
    const float rr[rows] = {1.0f, 1.0f, 0.75f, 0.0f}, tt[rows] = {0.0f, 0.62f, 0.86f, 1.0f};
    for (int r = 0; r < rows; r++) {
        vec3 c = base + axis * (len * tt[r]);
        for (int k = 0; k < cols; k++) { float a = 2 * PI * k / cols + 0.3f; P.push_back(c + (e1 * std::cos(a) + e2 * std::sin(a)) * rad * rr[r]); ref.push_back(c); }
    }
    size_t v0 = rock.v.size(); addGrid(rock, P, ref, rows, cols, -1.0f, 9.0f, hue);
    for (int r = 0; r < rows; r++) for (int k = 0; k < cols; k++) rock.v[v0 + (size_t)r * cols + k].uv = vec2((float)k / cols, tt[r]);
}

// champignons luminescents, cristaux, ossements selon le biome
void World::addDecor(int ti, Rng& rng) {
    Tube& tb = tubes[ti]; int lim = tb.isMain ? doorRing - 8 : tb.N - 8; int count = tb.isMain ? 120 : 16;
    for (int n = 0; n < count; n++) {
        int i = 12 + rng.next() % std::max(1, lim - 12); int bi = (int)std::floor(biomeF(ti, i) + 0.5f);
        bool nearJ = false; for (size_t b = 1; b < tubes.size(); b++) if (tubes[b].parent == ti && std::abs(i - tubes[b].junction) < 14) nearJ = true;
        if (nearJ || nearCorr(ti, i, 3)) continue;
        const Ring& r = tb.rings[i]; float fy = tb.floorY(i, 0.0f), xl, xr; if (!tb.wallRange(i, fy + 0.5f, xl, xr)) continue;
        float x = rng.r(xl + 0.5f, xr - 0.5f); vec3 base = tb.toWorld(i, x, tb.floorY(i, x) - 0.02f);
        if (bi == 1 && rng.chance(0.6f)) {
            rock.beginChunk(); int cnt = 2 + rng.next() % 4;
            for (int k = 0; k < cnt; k++) {
                float h = rng.r(0.1f, 0.45f), rc = rng.r(0.05f, 0.14f); vec3 p = base + r.R * rng.r(-0.35f, 0.35f) + r.T * rng.r(-0.35f, 0.35f);
                addEllipsoid(rock, p + r.U * (h * 0.5f), r.R, r.U, r.T, vec3(rc * 0.25f, h * 0.5f, rc * 0.25f), 12.0f, 0.2f);
                addEllipsoid(rock, p + r.U * h, r.R, r.U, r.T, vec3(rc, rc * 0.5f, rc), 12.0f, 0.8f);
            }
            rock.endChunk();
        } else if (bi == 2 && rng.chance(0.55f)) {
            rock.beginChunk(); float hue = rng.f(); int cnt = 3 + rng.next() % 4;
            for (int k = 0; k < cnt; k++) {
                vec3 ax = glm::normalize(r.U + r.R * rng.r(-0.6f, 0.6f) + r.T * rng.r(-0.6f, 0.6f));
                addCrystal(base + r.R * rng.r(-0.3f, 0.3f) + r.T * rng.r(-0.3f, 0.3f), ax, rng.r(0.3f, 1.1f), rng.r(0.05f, 0.13f), hue);
            }
            rock.endChunk();
            if (rng.chance(0.4f)) addLight(base + r.U * 0.8f, hue < 0.5f ? vec3(0.7f, 0.3f, 1.0f) : vec3(0.25f, 0.7f, 1.0f), 1.6f, 2, false);
        } else if (bi == 3 && rng.chance(0.6f)) {
            rock.beginChunk();
            for (int k = 0; k < 4; k++) {
                float a = rng.r(0, 2 * PI); vec3 d = r.R * std::cos(a) + r.T * std::sin(a);
                addEllipsoid(rock, base + r.R * rng.r(-0.3f, 0.3f) + r.T * rng.r(-0.3f, 0.3f) + r.U * 0.03f, d, r.U, glm::cross(d, r.U), vec3(rng.r(0.12f, 0.26f), 0.02f, 0.02f), 11.0f, 0.0f);
            }
            if (rng.chance(0.6f)) {
                vec3 c = base + r.U * 0.1f;
                addEllipsoid(rock, c, r.R, r.U, r.T, vec3(0.09f, 0.08f, 0.1f), 11.0f, 0.0f);
                addEllipsoid(rock, c + r.R * 0.075f + r.U * 0.02f + r.T * 0.035f, r.R, r.U, r.T, vec3(0.03f), 7.0f, 0.0f);
                addEllipsoid(rock, c + r.R * 0.075f + r.U * 0.02f - r.T * 0.035f, r.R, r.U, r.T, vec3(0.03f), 7.0f, 0.0f);
            }
            rock.endChunk();
        }
    }
}

// porte massive en planches cerclées d'acier, indestructible : s'ouvre avec les clés
void World::addDoor(Rng& rng) {
    Tube& tb = tubes[0]; int i = doorRing; const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr; if (!tb.wallRange(i, fy + 1.2f, xl, xr)) return;
    float W = (xr - xl) - 0.04f; int n = (int)std::ceil(W / 0.26f); float pw = W / n;
    wood.beginChunk();
    for (int k = 0; k < n; k++) {
        float x = xl + 0.02f + (k + 0.5f) * pw, y0 = tb.floorY(i, x) - 0.1f, y1 = tb.ceilY(i, x) + 0.1f;
        int id = addB(0, -1, 9, 0, tb.toWorld(i, x, y0), tb.toWorld(i, x, y1), r.T, pw * 0.5f, 0.06f, rng, 0, 0.004f);
        if (id >= 0) { wood.boards[id].hp = 99999; doorBoards.push_back(id); }
    }
    for (float h : {0.6f, 2.0f}) {
        float y = fy + h, a, b2; if (!tb.wallRange(i, y, a, b2)) continue;
        size_t before = wood.boards.size();
        addBoardBetween(wood, tb.toWorld(i, a + 0.02f, y, -0.1f), tb.toWorld(i, b2 - 0.02f, y, -0.1f), r.T, 0.09f, 0.025f, 5.0f, rng, 0, 0.0f);
        if (wood.boards.size() > before) { Board& bd = wood.boards.back(); bd.role = 9; bd.hp = 99999; bd.tube = 0; doorBoards.push_back((int)wood.boards.size() - 1); }
    }
    wood.endChunk();
    doorPos = tb.toWorld(i, 0.0f, fy + 1.2f, -0.3f);
}

void World::addDoorAndExit(Rng& rng) {
    Tube& tb = tubes[0]; if (doorRing < 60 || tb.N - doorRing < 20) return;
    addFrame(0, doorRing - 3, rng, false);
    addTorch(0, doorRing - 6, 1, rng); addTorch(0, doorRing - 6, -1, rng);
    addDoor(rng);
    int ie = tb.N - 1;
    addLight(tb.toWorld(ie - 4, 0.0f, 1.2f), vec3(0.72f, 0.86f, 1.0f), 4.0f, 5, false); torches.back().needsDoor = true;
    addLight(tb.toWorld(doorRing + (ie - doorRing) * 2 / 3, 0.0f, 1.5f), vec3(0.7f, 0.85f, 1.0f), 2.0f, 5, false); torches.back().needsDoor = true;
    exitPos = tb.rings[ie].c;
}

void World::openDoor() {
    if (doorOpen) return; doorOpen = true;
    for (int id : doorBoards) { Board& b = wood.boards[id]; if (b.alive) { b.alive = false; wood.killIndices(b.i0, b.n); } }
}

void World::boardsNear(vec3 pt, float rad, std::vector<int>& out) const {
    for (size_t i = 0; i < wood.boards.size(); i++) {
        const Board& b = wood.boards[i]; if (!b.alive || b.role == 9) continue;
        if (glm::length(b.c - pt) > b.hl + rad + 0.3f) continue;
        vec3 d = pt - b.c; vec3 l(glm::dot(d, b.dl), glm::dot(d, b.dw), glm::dot(d, b.dt)); vec3 h(b.hl, b.hw, b.ht);
        if (glm::length(l - glm::clamp(l, -h, h)) < rad) out.push_back((int)i);
    }
}

static int enemyTypeFor(int bi, Rng& rng) {
    switch (bi) { case 1: return 1; case 2: return rng.chance(0.6f) ? 1 : 0; case 4: return rng.chance(0.45f) ? 2 : 0; default: return 0; }
}

// clés au fond des impasses (gardées), fioles de soin, ennemis par biome
void World::placeEntities(Rng& rng) {
    int keys = 0; Tube& mn = tubes[0];
    for (size_t b = 1; b < tubes.size() && keys < 3; b++) {
        Tube& tb = tubes[b]; int i = std::max(8, tb.N - 6);
        pickups.push_back(Pickup{tb.toWorld(i, 0.0f, tb.floorY(i, 0.0f) + 0.9f), 0, true, rng.f() * 6}); keys++;
        pickups.push_back(Pickup{tb.toWorld(i - 12, 0.6f, tb.floorY(i - 12, 0.6f) + 0.5f), 1, true, rng.f() * 6});
        int bi = (int)std::floor(biomeF((int)b, i) + 0.5f);
        for (int k = 0; k < 2; k++) { int ri = std::max(10, i - 7 - 6 * k); spawns.push_back(Spawn{enemyTypeFor(bi, rng), tb.toWorld(ri, rng.r(-0.4f, 0.4f), tb.floorY(ri, 0.0f) + 1.65f), (int)b}); }
    }
    while (keys < 3 && doorRing > 160) {
        int i = 80 + rng.next() % std::max(1, doorRing - 140); float x = rng.r(-0.8f, 0.8f); int bi = (int)std::floor(biomeF(0, i) + 0.5f);
        pickups.push_back(Pickup{mn.toWorld(i, x, mn.floorY(i, x) + 0.9f), 0, true, rng.f() * 6}); keys++;
        spawns.push_back(Spawn{enemyTypeFor(bi, rng), mn.toWorld(i + 8, 0.0f, mn.floorY(i + 8, 0.0f) + 1.65f), 0});
    }
    keysNeeded = keys;
    static const int cnt[5] = {3, 4, 4, 5, 4};
    for (int b = 0; b < 5 && doorRing > 160; b++) {
        int lo = (b == 0) ? 80 : b * doorRing / 5 + 10, hi = (b + 1) * doorRing / 5 - 10;
        for (int k = 0; k < cnt[b]; k++) {
            int i = lo + rng.next() % std::max(1, hi - lo); float x = rng.r(-0.7f, 0.7f);
            spawns.push_back(Spawn{enemyTypeFor(b, rng), mn.toWorld(i, x, mn.floorY(i, x) + 1.65f), 0});
        }
        for (int k = 0; k < 2; k++) {
            int i = lo + rng.next() % std::max(1, hi - lo); float x = rng.r(-0.9f, 0.9f);
            pickups.push_back(Pickup{mn.toWorld(i, x, mn.floorY(i, x) + 0.5f), 1, true, rng.f() * 6});
        }
    }
}


// ----------------------------------------------------------------------------
//  Camp de base (hub) : plaine, forêt, flanc de montagne percé par l'entrée de la mine,
//  matériel de mine (chariot sur rails, sacs, caisses, tonneaux), forge, feu de camp, cristaux géants.
//  Repère : la mine part vers -Z depuis l'anneau 0 de la galerie principale ; le camp est du côté +Z.
// ----------------------------------------------------------------------------
struct HubMats { float leaf[3] = {5, 5, 5}, pine = 5, bark = 5, sack = 5, canvas = 5, cloth = 5, crys[5] = {5, 5, 5, 5, 5}, flash = 5; };
static HubMats gHM;
static vec3 gCrysCol[5] = {vec3(1.0f, 0.25f, 0.9f), vec3(0.25f, 0.8f, 1.0f), vec3(1.0f, 0.65f, 0.15f), vec3(0.35f, 1.0f, 0.45f), vec3(0.62f, 0.35f, 1.0f)};

static float regMat(vec3 kd, float ns, vec3 ke, float gloss) {
    for (size_t i = 0; i < gMdlA.size(); i++)
        if (gMdlA[i] == vec4(kd, ns) && gMdlE[i] == vec4(ke, gloss)) return 21.0f + (float)i;
    if ((int)gMdlA.size() >= MDL_MAXMAT) return 5.0f;
    gMdlA.push_back(vec4(kd, ns)); gMdlE.push_back(vec4(ke, gloss)); return 21.0f + (float)gMdlA.size() - 1.0f;
}
static void registerHubMats() {
    gHM.leaf[0] = regMat(vec3(0.11f, 0.28f, 0.07f), 10, vec3(0), 0.05f); gHM.leaf[1] = regMat(vec3(0.19f, 0.34f, 0.09f), 10, vec3(0), 0.05f);
    gHM.leaf[2] = regMat(vec3(0.42f, 0.27f, 0.07f), 10, vec3(0), 0.05f); gHM.pine = regMat(vec3(0.05f, 0.17f, 0.09f), 10, vec3(0), 0.05f);
    gHM.bark = regMat(vec3(0.17f, 0.11f, 0.07f), 8, vec3(0), 0.03f); gHM.sack = regMat(vec3(0.50f, 0.40f, 0.26f), 6, vec3(0), 0.02f);
    gHM.canvas = regMat(vec3(0.62f, 0.56f, 0.44f), 6, vec3(0), 0.02f); gHM.cloth = regMat(vec3(0.45f, 0.10f, 0.08f), 6, vec3(0), 0.02f);
    for (int i = 0; i < 5; i++) gHM.crys[i] = regMat(gCrysCol[i] * 0.18f, 40, gCrysCol[i] * 0.85f, -1.0f);       // brillance -1 = ombrage « cristal »
    gHM.flash = regMat(vec3(0.9f, 0.55f, 0.45f), 8, vec3(3.2f, 1.1f, 0.7f), 0.1f);       // éclair blanc-rouge quand un ennemi est touché
}

// cylindre / cône tronqué lisse
static void addTubeM(Mesh& m, vec3 p0, vec3 p1, float r0, float r1, int seg, float mat, float seed, bool cap0 = true, bool cap1 = true) {
    vec3 ax = p1 - p0; float L = glm::length(ax); if (L < 1e-5f) return; ax /= L;
    vec3 t = glm::normalize(glm::cross(ax, std::fabs(ax.y) < 0.95f ? vec3(0, 1, 0) : vec3(1, 0, 0))), b = glm::cross(ax, t);
    float slope = (r0 - r1) / L; uint32_t base = (uint32_t)m.v.size();
    for (int i = 0; i <= seg; i++) {
        float a = 2 * PI * i / seg; vec3 d = t * std::cos(a) + b * std::sin(a), n = glm::normalize(d + ax * slope);
        m.v.push_back({p0 + d * r0, n, vec2((float)i / seg, 0), mat, seed, 0}); m.v.push_back({p1 + d * r1, n, vec2((float)i / seg, 1), mat, seed, 0});
    }
    for (int i = 0; i < seg; i++) { uint32_t a = base + 2 * i; m.idx.insert(m.idx.end(), {a, a + 1, a + 3, a, a + 3, a + 2}); }
    for (int c = 0; c < 2; c++) {
        float r = c ? r1 : r0; if ((c ? !cap1 : !cap0) || r < 1e-4f) continue;
        vec3 ctr = c ? p1 : p0, n = c ? ax : -ax; uint32_t cb = (uint32_t)m.v.size();
        m.v.push_back({ctr, n, vec2(0.5f), mat, seed, 0});
        for (int i = 0; i <= seg; i++) { float a = 2 * PI * i / seg; m.v.push_back({ctr + (t * std::cos(a) + b * std::sin(a)) * r, n, vec2(0.5f), mat, seed, 0}); }
        for (int i = 0; i < seg; i++) m.idx.insert(m.idx.end(), {cb, cb + 1 + (uint32_t)i, cb + 2 + (uint32_t)i});
    }
}

static void addPine(Mesh& m, vec3 base, float h, Rng& rng) {
    float seed = rng.f() * 10, tr = 0.16f + h * 0.016f; m.beginChunk();
    addTubeM(m, base - vec3(0, 0.4f, 0), base + vec3(0, h * 0.9f, 0), tr, tr * 0.2f, 7, gHM.bark, seed, true, false);
    for (int i = 0; i < 5; i++) {
        float f = i / 4.0f, y0 = h * (0.16f + 0.56f * f), rad = h * (0.27f - 0.2f * f) * rng.r(0.9f, 1.1f), len = h * (0.36f - 0.12f * f);
        addTubeM(m, base + vec3(rng.r(-0.1f, 0.1f), y0, rng.r(-0.1f, 0.1f)), base + vec3(0, y0 + len, 0), rad, 0.0f, 9, gHM.pine, seed + i, true, false);
    }
    m.endChunk();
}
static void addOak(Mesh& m, vec3 base, float h, Rng& rng) {
    float seed = rng.f() * 10, tr = 0.22f + h * 0.02f; m.beginChunk();
    addTubeM(m, base - vec3(0, 0.4f, 0), base + vec3(0.0f, h * 0.62f, 0.0f), tr, tr * 0.55f, 8, gHM.bark, seed, true, false);
    vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1); float lm = rng.f() < 0.25f ? 2.0f : (rng.f() < 0.5f ? 1.0f : 0.0f);
    for (int i = 0; i < 5; i++) {
        float a = 2 * PI * i / 5 + rng.f(); float rr = h * 0.2f * (i == 0 ? 0.0f : 1.0f);
        vec3 c = base + vec3(std::cos(a) * rr, h * (0.72f + 0.1f * (i % 2)), std::sin(a) * rr);
        float rad = h * rng.r(0.2f, 0.27f);
        addEllipsoid(m, c, X, Y, Z, vec3(rad, rad * 0.82f, rad), gHM.leaf[(int)(rng.f() * 2.0f + (lm == 2.0f ? 1.0f : 0.0f)) % 3], seed + i);
    }
    m.endChunk();
}

float World::hubGround(float x, float z) const {
    float zr = z - hubFront, dm = std::sqrt(x * x + (zr - 8.0f) * (zr - 8.0f));
    float flat = smooth(20.0f, 55.0f, dm);
    float n = fbm3(vec3(x * 0.035f, 0.0f, z * 0.035f), 4, seed + 77) - 0.5f;
    float edge = smooth(50.0f, 105.0f, std::fabs(x)) * 5.0f + smooth(104.0f, 146.0f, zr) * 7.0f;
    return hubG + n * 1.5f * flat + edge * (0.55f + (fbm3(vec3(x * 0.05f, 3.0f, z * 0.05f), 3, seed + 78)));
}

static float hubPathX(float z) { return 3.2f * std::sin(z * 0.05f) + 1.2f * std::sin(z * 0.13f); }       // identique au shader d'herbe

void World::genHub(Rng& rngIn) {
    (void)rngIn;
    hubM = Mesh(); hubSolids.clear();
    const Tube& tb = tubes[0]; const Ring& r0 = tb.rings[0]; const int M = tb.M;
    hubFront = r0.c.z; float fl0 = tb.floorY(0, 0.0f); hubG = tb.toWorld(0, 0.0f, fl0).y;
    Rng R(seed * 7u + 5u); vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    auto G = [&](float x, float zr) { return hubGround(x, hubFront + zr); };
    auto solid = [&](float x, float zr, float r) { hubSolids.push_back({x, hubFront + zr, r}); };
    auto box = [&](vec3 c, vec3 half, float ang, float mat) {            // boîte posée, tournée autour de Y
        vec3 dl(std::cos(ang), 0, std::sin(ang)), dt(-std::sin(ang), 0, std::cos(ang));
        addBoard(hubM, c, dl, Y, dt, half.x, half.y, half.z, mat, R.f() * 10, R, 0.004f);
    };
    auto sack = [&](float x, float zr, float s) {
        float y = G(x, zr); vec3 c(x, y + 0.28f * s, hubFront + zr);
        addEllipsoid(hubM, c, X, Y, Z, vec3(0.34f * s, 0.30f * s, 0.26f * s), gHM.sack, R.f());
        addEllipsoid(hubM, c + vec3(0, 0.3f * s, 0), X, Y, Z, vec3(0.11f * s, 0.08f * s, 0.11f * s), gHM.sack, R.f());
    };
    auto barrel = [&](float x, float zr) {
        float y = G(x, zr) - 0.05f; vec3 b(x, y, hubFront + zr); hubM.beginChunk();
        addTubeM(hubM, b, b + vec3(0, 0.95f, 0), 0.40f, 0.40f, 12, 1.0f, R.f() * 10);
        addTubeM(hubM, b + vec3(0, 0.3f, 0), b + vec3(0, 0.65f, 0), 0.435f, 0.435f, 12, 1.0f, R.f() * 10, false, false);
        for (float h : {0.12f, 0.5f, 0.84f}) addTubeM(hubM, b + vec3(0, h - 0.02f, 0), b + vec3(0, h + 0.02f, 0), 0.445f, 0.445f, 12, 5.0f, 0.0f, false, false);
        hubM.endChunk(); solid(x, zr, 0.5f);
    };
    auto crate = [&](float x, float zr, float yoff, float ang, float s) {
        float y = G(x, zr) - 0.05f + yoff; hubM.beginChunk();
        box(vec3(x, y + 0.45f * s, hubFront + zr), vec3(0.45f * s, 0.45f * s, 0.45f * s), ang, 1.0f);
        vec3 dl(std::cos(ang), 0, std::sin(ang));
        for (float o : {-0.3f, 0.3f}) box(vec3(x, y + 0.45f * s, hubFront + zr) + dl * (o * s), vec3(0.03f, 0.465f * s, 0.465f * s), ang, 5.0f);   // cerclage
        hubM.endChunk(); if (yoff < 0.01f) solid(x, zr, 0.7f * s);
    };
    auto torch = [&](vec3 base, float h) {
        hubM.beginChunk();
        addBoardBetween(hubM, base, base + vec3(0, h, 0), Z, 0.05f, 0.05f, 1.0f, R, 0.0f, 0.004f);
        addBoardBetween(hubM, base + vec3(0, h - 0.25f, 0), base + vec3(0, h + 0.1f, 0), Z, 0.08f, 0.08f, 2.0f, R, 0.0f, 0.01f);
        hubM.endChunk();
        addLight(base + vec3(0, h + 0.28f, 0), vec3(1.0f, 0.5f, 0.17f), 4.4f, HUB_BIOME, true);
    };

    // ---- 1) sol : grille 1,5 m, découpée au pied de la montagne, blocs de 12x12 cases pour le culling
    {
        const float X0 = -115, X1 = 115, Z0 = -56, Z1 = 146, st = 1.5f;
        int nx = (int)((X1 - X0) / st) + 1, nz = (int)((Z1 - Z0) / st) + 1; uint32_t base = (uint32_t)hubM.v.size();
        for (int iz = 0; iz < nz; iz++) for (int ix = 0; ix < nx; ix++) {
            float x = X0 + ix * st, z = hubFront + Z0 + iz * st, y = hubGround(x, z);
            float gx = (hubGround(x + 1, z) - hubGround(x - 1, z)) * 0.5f, gz = (hubGround(x, z + 1) - hubGround(x, z - 1)) * 0.5f;
            hubM.v.push_back({vec3(x, y, z), glm::normalize(vec3(-gx, 1, -gz)), vec2(0), 100.0f, 0, 0});
        }
        auto zEdge = [](float x) { float ax = std::fabs(x); return ax < 8.0f ? 0.0f : (ax < 25.0f ? -8.0f : -0.5f * (ax - 25.0f) - 8.0f); };
        const int B = 12;
        for (int bz = 0; bz < nz - 1; bz += B) for (int bx = 0; bx < nx - 1; bx += B) {
            uint32_t i0 = (uint32_t)hubM.idx.size();
            for (int iz = bz; iz < std::min(bz + B, nz - 1); iz++) for (int ix = bx; ix < std::min(bx + B, nx - 1); ix++) {
                float cx = X0 + (ix + 0.5f) * st, czr = Z0 + (iz + 0.5f) * st; if (czr < zEdge(cx)) continue;
                uint32_t a = base + iz * nx + ix, b = a + 1, c = a + nx + 1, d = a + nx;
                hubM.idx.insert(hubM.idx.end(), {a, b, c, a, c, d});
            }
            if (hubM.idx.size() == i0) continue;
            Chunk ch; ch.first = i0; ch.count = (uint32_t)hubM.idx.size() - i0;
            ch.c = vec3(X0 + (bx + B * 0.5f) * st, hubG, hubFront + Z0 + (bz + B * 0.5f) * st); ch.r = B * st * 0.75f + 6.0f; hubM.chunks.push_back(ch);
        }
    }

    // ---- 2) flanc de montagne : maillage polaire dont l'anneau intérieur est exactement l'ouverture de la galerie
    std::vector<vec3> cliff; const int K = M * 3, S = 36;
    {   // anneaux des galeries (repère de l'entrée) : la façade est repoussée / ramenée pour rester toujours devant elles
        struct RC { float x, y, d, R; }; std::vector<RC> rc; int nClamp = 0; float worstClamp = 0.0f;
        for (const Tube& t : tubes) for (int i = 0; i < t.N; i += 2) {
            vec3 dd = t.rings[i].c - r0.c; float d = glm::dot(dd, r0.T); if (d < 5.0f) continue;
            rc.push_back({glm::dot(dd, r0.R), glm::dot(dd, r0.U), d, std::max(t.rings[i].w, t.rings[i].h) + 2.2f});
        }
        cliff.resize((size_t)(S + 1) * K); std::vector<vec3> ref(cliff.size());
        for (int k = 0; k < K; k++) {
            int k0 = k / 3, k1 = (k0 + 1) % M; float f = (k % 3) / 3.0f;
            vec2 in = glm::mix(tb.loc[k0], tb.loc[k1], f); float th = 2 * PI * (k0 + f) / M, ct = std::cos(th), sn = std::sin(th);
            float ry = 34.0f + 70.0f * std::pow(std::max(0.0f, std::sin(3 * th + 1.0f)), 1.6f) + 46.0f * std::pow(std::max(0.0f, std::sin(5 * th + 2.2f)), 2.0f) + 26.0f * std::pow(std::max(0.0f, std::sin(9 * th + 0.4f)), 2.0f), rx = 114.0f + 6.0f * std::sin(5 * th);
            vec2 out(ct * rx, sn >= 0 ? sn * ry : sn * 7.0f);
            for (int s = 0; s <= S; s++) {
                float t = (float)s / S, e = std::pow(t, 1.45f); vec2 q = glm::mix(in, out, e);
                float yAbove = q.y - fl0, zoff = std::max(0.5f * std::max(0.0f, std::fabs(q.x) - 25.0f), 0.62f * std::max(0.0f, yAbove - 3.0f));
                vec3 wp = r0.c + r0.R * q.x + r0.U * q.y;
                float nz = (fbm3(wp * 0.045f, 4, seed + 501) - 0.5f) * 26.0f + (fbm3(wp * 0.17f, 3, seed + 502) - 0.5f) * 8.0f + (fbm3(wp * 0.6f, 2, seed + 503) - 0.5f) * 1.0f;
                nz = std::max(nz, -0.4f) * smooth(2.0f, 12.0f, yAbove) * smooth(0.0f, 0.2f, t);
                zoff *= smooth(0.0f, 0.1f, t);
                float off = zoff + nz, lim = 1e9f;
                for (const RC& c : rc) { float dx = q.x - c.x, dy = q.y - c.y, rr = c.R + 3.0f; if (dx * dx + dy * dy < rr * rr) lim = std::min(lim, c.d - c.R - 1.2f); }
                if (lim < off) { worstClamp = std::max(worstClamp, off - std::max(lim, 0.0f)); off = std::max(lim, 0.0f) * smooth(0.0f, 0.1f, t); nClamp++; }
                cliff[(size_t)s * K + k] = wp + r0.T * off; ref[(size_t)s * K + k] = r0.c + r0.T * 80.0f;
            }
        }
        std::printf("Montagne : %d sommets ramenes devant les galeries (max %.1f m)\n", nClamp, worstClamp);
        hubM.beginChunk(); addGrid(hubM, cliff, ref, S + 1, K, -1.0f, 0.0f, 0.0f);
        // chunk toujours visible (énorme)
        hubM.endChunk(); if (!hubM.chunks.empty()) { hubM.chunks.back().c = r0.c + r0.T * 20.0f; hubM.chunks.back().r = 400.0f; }
    }

    // ---- 3) montagnes lointaines (silhouettes dans la brume)
    {
        hubM.beginChunk();
        for (int i = 0; i < 16; i++) {
            float a = 2 * PI * i / 16 + R.r(-0.1f, 0.1f), rad = R.r(230, 280);
            vec3 c(std::cos(a) * rad, hubG - 14.0f, hubFront + 60.0f + std::sin(a) * rad); if (c.z < hubFront + 25.0f) continue;       // jamais derrière le front de la montagne (les galeries y passent)
            addEllipsoid(hubM, c, X, Y, Z, vec3(R.r(70, 110), R.r(45, 80), R.r(70, 110)), 0.0f, R.f());
        }
        hubM.endChunk(); if (!hubM.chunks.empty()) { hubM.chunks.back().c = vec3(0, hubG, hubFront + 60.0f); hubM.chunks.back().r = 600.0f; }
    }

    // ---- 4) entrée de la mine : cadre de soutènement, enseigne, torches, rails
    {
        float a1, a2; tb.wallRange(0, fl0 + 1.0f, a1, a2); float cy = tb.ceilY(0, 0.0f);
        float xl = a1 + 0.3f, xr = a2 - 0.3f; float tz = -0.9f;                       // (t négatif = côté camp)
        hubM.beginChunk();
        for (float xx : {xl, xr}) for (int b = 0; b < 2; b++)
            addBoardBetween(hubM, tb.toWorld(0, xx + (b - 0.5f) * 0.05f, fl0 - 0.2f, tz + b * 0.1f), tb.toWorld(0, xx, cy - 0.2f, tz + b * 0.1f), r0.T, 0.17f, 0.06f, 1.0f, R, 0.0f, 0.006f);
        for (int b = 0; b < 2; b++) addBoardBetween(hubM, tb.toWorld(0, xl - 0.45f, cy - 0.15f, tz + b * 0.1f), tb.toWorld(0, xr + 0.45f, cy - 0.15f, tz + b * 0.1f), r0.T, 0.17f, 0.07f, 1.0f, R, 0.0f, 0.006f);
        for (float sg : {-1.0f, 1.0f}) addBoardBetween(hubM, tb.toWorld(0, sg > 0 ? xr : xl, cy - 1.7f, tz - 0.1f), tb.toWorld(0, sg > 0 ? xr - 1.1f : xl + 1.1f, cy - 0.3f, tz - 0.1f), r0.T, 0.1f, 0.05f, 1.0f, R, 0.0f, 0.006f);
        // enseigne en planches cerclées
        vec3 sc = tb.toWorld(0, (xl + xr) * 0.5f, cy + 0.65f, tz - 0.25f);
        addBoardBetween(hubM, sc - r0.R * 1.7f, sc + r0.R * 1.7f, r0.T, 0.32f, 0.04f, 1.0f, R, 0.0f, 0.004f);
        for (float o : {-1.2f, 0.0f, 1.2f}) addBoardBetween(hubM, sc + r0.R * o - r0.U * 0.34f, sc + r0.R * o + r0.U * 0.34f, r0.T, 0.04f, 0.055f, 5.0f, R, 0.0f, 0.0f);
        hubM.endChunk();
        for (float xx : {xl - 0.9f, xr + 0.9f}) { vec3 b = tb.toWorld(0, xx, fl0 - 0.05f, -1.4f); torch(vec3(b.x, G(b.x, b.z - hubFront), b.z), 2.3f); solid(b.x, b.z - hubFront, 0.3f); }
        // rails + traverses jusqu'au seuil
        hubM.beginChunk();
        for (float xx : {-0.55f, 0.55f}) addBoardBetween(hubM, vec3(xx, G(xx, 1.0f) + 0.07f, hubFront + 0.6f), vec3(xx, G(xx, 30.0f) + 0.07f, hubFront + 30.0f), Y, 0.035f, 0.035f, 5.0f, R, 0.0f, 0.0f);
        for (float zr = 1.0f; zr < 30.0f; zr += 0.9f) box(vec3(0, G(0, zr) + 0.03f, hubFront + zr), vec3(0.95f, 0.04f, 0.09f), R.r(-0.05f, 0.05f), 1.0f);
        hubM.endChunk();
    }

    // ---- 5) matériel de mine
    // chariot sur rails chargé de minerai
    {
        float zc = 11.0f, y = G(0, zc); hubM.beginChunk();
        box(vec3(0, y + 0.42f, hubFront + zc), vec3(0.6f, 0.06f, 0.95f), 0.0f, 1.0f);
        for (float sx : {-1.0f, 1.0f}) box(vec3(sx * 0.62f, y + 0.74f, hubFront + zc), vec3(0.05f, 0.35f, 0.95f), 0.0f, 1.0f);
        for (float sz : {-1.0f, 1.0f}) box(vec3(0, y + 0.74f, hubFront + zc + sz * 0.97f), vec3(0.62f, 0.35f, 0.05f), 0.0f, 1.0f);
        for (float sz : {-0.55f, 0.0f, 0.55f}) box(vec3(0, y + 0.74f, hubFront + zc + sz), vec3(0.66f, 0.04f, 0.035f), 0.0f, 5.0f);
        for (float sx : {-1.0f, 1.0f}) for (float sz : {-0.6f, 0.6f}) addTubeM(hubM, vec3(sx * 0.58f, y + 0.29f, hubFront + zc + sz), vec3(sx * 0.7f, y + 0.29f, hubFront + zc + sz), 0.27f, 0.27f, 12, 5.0f, 0.0f);
        for (int i = 0; i < 7; i++) addEllipsoid(hubM, vec3(R.r(-0.4f, 0.4f), y + 1.05f + R.r(0, 0.12f), hubFront + zc + R.r(-0.7f, 0.7f)), X, Y, Z, vec3(R.r(0.18f, 0.3f)), 0.0f, R.f());
        for (int i = 0; i < 5; i++) addEllipsoid(hubM, vec3(R.r(-0.4f, 0.4f), y + 1.18f + R.r(0, 0.1f), hubFront + zc + R.r(-0.7f, 0.7f)), X, Y, Z, vec3(R.r(0.07f, 0.12f)), 18.0f, R.f());
        hubM.endChunk(); for (float dz : {-0.7f, 0.0f, 0.7f}) solid(0, zc + dz, 0.8f);
        // minerai répandu devant l'entrée
        hubM.beginChunk(); for (int i = 0; i < 9; i++) addEllipsoid(hubM, vec3(R.r(-2.5f, 2.5f), G(0, 4.0f) + 0.1f, hubFront + R.r(2.0f, 6.0f)), X, Y, Z, vec3(R.r(0.12f, 0.3f), 0.12f, R.r(0.12f, 0.3f)), 0.0f, R.f()); hubM.endChunk();
    }
    // forge sous auvent, à gauche
    {
        const float fx = -17.5f, fz = 14.0f; float y = G(fx, fz) - 0.15f; vec3 F(fx, y, hubFront + fz);
        hubM.beginChunk();
        box(F + vec3(0, 0.6f, 0), vec3(1.4f, 0.6f, 1.7f), 0.0f, 0.0f);                       // massif de pierre
        box(F + vec3(-1.15f, 2.3f, 0), vec3(0.35f, 1.7f, 1.7f), 0.0f, 0.0f);                   // dosseret
        box(F + vec3(-1.15f, 5.2f, 0), vec3(0.6f, 2.3f, 0.6f), 0.0f, 0.0f);                    // cheminée
        box(F + vec3(0.5f, 1.22f, 0), vec3(0.8f, 0.05f, 1.25f), 0.0f, 8.0f);                  // lit de braises
        for (float sz : {-1.3f, 1.3f}) box(F + vec3(0.55f, 1.35f, sz), vec3(0.8f, 0.14f, 0.2f), 0.0f, 0.0f);
        for (int i = 0; i < 6; i++) addEllipsoid(hubM, F + vec3(R.r(-0.2f, 1.0f), 1.28f, R.r(-1.0f, 1.0f)), X, Y, Z, vec3(R.r(0.12f, 0.2f), 0.1f, R.r(0.12f, 0.2f)), 20.0f, R.f());
        for (float sx : {-2.9f, 3.2f}) for (float sz : {-3.1f, 3.1f}) addBoardBetween(hubM, F + vec3(sx, -0.1f, sz), F + vec3(sx, 3.6f, sz), Z, 0.1f, 0.1f, 1.0f, R, 0.0f, 0.006f);   // auvent
        for (float sz : {-3.1f, 3.1f}) addBoardBetween(hubM, F + vec3(-2.9f, 3.65f, sz), F + vec3(3.2f, 3.65f, sz), Y, 0.1f, 0.08f, 1.0f, R, 0.0f, 0.004f);
        for (int i = 0; i < 9; i++) addBoardBetween(hubM, F + vec3(-3.0f, 3.78f + 0.02f * i, -3.3f + i * 0.8f), F + vec3(3.4f, 3.7f, -3.3f + i * 0.8f), Z, 0.0f + 0.4f, 0.03f, 1.0f, R, 0.0f, 0.004f);
        box(F + vec3(3.9f, 0.3f, -0.9f), vec3(0.42f, 0.4f, 0.42f), 0.0f, 1.0f);                // billot + enclume
        box(F + vec3(3.9f, 0.9f, -0.9f), vec3(0.55f, 0.16f, 0.2f), 0.0f, 5.0f); box(F + vec3(3.9f, 1.15f, -0.9f), vec3(0.3f, 0.1f, 0.2f), 0.0f, 5.0f);
        addTubeM(hubM, F + vec3(4.45f, 0.95f, -0.9f), F + vec3(5.0f, 0.9f, -0.9f), 0.12f, 0.02f, 8, 5.0f, 0.0f);
        for (int i = 0; i < 3; i++) {                                                          // râtelier d'outils
            addBoardBetween(hubM, F + vec3(-0.6f + i * 0.5f, 0.0f, -3.0f), F + vec3(-0.6f + i * 0.5f, 1.8f, -3.0f), X, 0.03f, 0.03f, 1.0f, R, 0.0f, 0.004f);
            box(F + vec3(-0.6f + i * 0.5f, 1.85f, -3.0f), vec3(0.03f, 0.05f, 0.34f), 0.0f, 5.0f);
        }
        hubM.endChunk();
        barrel(fx + 3.2f, fz + 2.6f); barrel(fx + 4.0f, fz + 2.2f); sack(fx + 1.0f, fz + 4.4f, 1.0f); sack(fx + 1.6f, fz + 4.6f, 0.9f); sack(fx + 1.3f, fz + 5.2f, 1.0f);
        solid(fx, fz - 1.3f, 1.5f); solid(fx, fz, 1.5f); solid(fx, fz + 1.3f, 1.5f); solid(fx + 3.9f, fz - 0.9f, 0.7f);
        addLight(F + vec3(0.6f, 1.9f, -0.5f), vec3(1.0f, 0.42f, 0.12f), 4.8f, HUB_BIOME, true);
        addLight(F + vec3(0.6f, 1.9f, 0.7f), vec3(1.0f, 0.48f, 0.14f), 4.4f, HUB_BIOME, true);
    }
    // caisses, tonneaux et sacs à droite
    crate(14.0f, 8.0f, 0.0f, 0.3f, 1.0f); crate(15.1f, 8.4f, 0.0f, -0.2f, 1.0f); crate(14.4f, 8.1f, 0.9f, 0.1f, 0.95f); crate(13.3f, 9.4f, 0.0f, 0.5f, 0.9f);
    barrel(17.0f, 8.6f); barrel(17.9f, 9.6f); barrel(16.4f, 10.0f); sack(12.0f, 10.8f, 1.0f); sack(12.7f, 11.2f, 1.1f); sack(11.7f, 11.6f, 0.9f); sack(12.4f, 10.3f, 0.9f);
    sack(-4.5f, 5.0f, 1.0f); sack(-5.2f, 5.5f, 0.9f); sack(-4.8f, 6.3f, 1.1f); solid(-4.8f, 5.6f, 1.2f);
    // feu de camp, rondins, tente
    {
        const float cx = 11.0f, cz = 26.0f; float y = G(cx, cz); hubM.beginChunk();
        for (int i = 0; i < 9; i++) { float a = 2 * PI * i / 9; addEllipsoid(hubM, vec3(cx + std::cos(a) * 0.85f, y + 0.12f, hubFront + cz + std::sin(a) * 0.85f), X, Y, Z, vec3(0.28f, 0.2f, 0.26f), 0.0f, R.f()); }
        for (int i = 0; i < 4; i++) { float a = PI * i / 4; addBoardBetween(hubM, vec3(cx - std::cos(a) * 0.55f, y + 0.12f, hubFront + cz - std::sin(a) * 0.55f), vec3(cx + std::cos(a) * 0.55f, y + 0.3f, hubFront + cz + std::sin(a) * 0.55f), Y, 0.07f, 0.07f, 2.0f, R, 0.0f, 0.01f); }
        for (int i = 0; i < 3; i++) { float a = 0.5f + i * 2.1f; vec3 c(cx + std::cos(a) * 2.6f, y + 0.26f, hubFront + cz + std::sin(a) * 2.6f); addTubeM(hubM, c - vec3(-std::sin(a), 0, std::cos(a)) * 0.7f, c + vec3(-std::sin(a), 0, std::cos(a)) * 0.7f, 0.24f, 0.24f, 8, gHM.bark, 0.0f); }
        hubM.endChunk(); solid(cx, cz, 1.0f);
        addLight(vec3(cx, y + 0.9f, hubFront + cz), vec3(1.0f, 0.5f, 0.16f), 4.4f, HUB_BIOME, true);
        const float tx = 21.0f, tzr = 22.0f; float ty = G(tx, tzr) - 0.05f; hubM.beginChunk();
        for (float sg : {-1.0f, 1.0f}) {
            addBoardBetween(hubM, vec3(tx, ty + 2.3f, hubFront + tzr), vec3(tx + sg * 1.9f, ty, hubFront + tzr), Z, 1.9f, 0.02f, gHM.canvas, R, 0.0f, 0.0f);
            addBoardBetween(hubM, vec3(tx + sg * 1.9f, ty, hubFront + tzr - 1.9f), vec3(tx + sg * 1.9f, ty + 0.04f, hubFront + tzr + 1.9f), Y, 0.05f, 0.05f, 1.0f, R, 0.0f, 0.0f);
        }
        addBoardBetween(hubM, vec3(tx, ty + 2.35f, hubFront + tzr - 2.0f), vec3(tx, ty + 2.35f, hubFront + tzr + 2.0f), X, 0.06f, 0.06f, 1.0f, R, 0.0f, 0.0f);
        addBoardBetween(hubM, vec3(tx, ty + 2.3f, hubFront + tzr + 1.9f), vec3(tx, ty + 0.1f, hubFront + tzr + 1.9f), X, 0.9f, 0.015f, gHM.cloth, R, 0.0f, 0.0f);
        hubM.endChunk(); solid(tx, tzr, 2.1f);
    }
    // lanternes le long du chemin + panneau
    for (int i = 0; i < 4; i++) {
        float zr = 34.0f + i * 20.0f, x = hubPathX(hubFront + zr) + (i % 2 ? 3.4f : -3.4f), y = G(x, zr); hubM.beginChunk();
        addBoardBetween(hubM, vec3(x, y - 0.1f, hubFront + zr), vec3(x, y + 2.9f, hubFront + zr), Z, 0.07f, 0.07f, 1.0f, R, 0.0f, 0.004f);
        addBoardBetween(hubM, vec3(x, y + 2.8f, hubFront + zr), vec3(x + (i % 2 ? -0.5f : 0.5f), y + 2.8f, hubFront + zr), Z, 0.04f, 0.04f, 1.0f, R, 0.0f, 0.0f);
        addEllipsoid(hubM, vec3(x + (i % 2 ? -0.5f : 0.5f), y + 2.55f, hubFront + zr), X, Y, Z, vec3(0.13f, 0.2f, 0.13f), 13.0f, 0.0f);
        hubM.endChunk(); solid(x, zr, 0.3f);
        addLight(vec3(x + (i % 2 ? -0.5f : 0.5f), y + 2.55f, hubFront + zr), vec3(1.0f, 0.74f, 0.42f), 3.4f, HUB_BIOME, false);
    }
    {
        float zr = 90.0f, x = hubPathX(hubFront + zr) + 2.6f, y = G(x, zr); hubM.beginChunk();
        addBoardBetween(hubM, vec3(x, y - 0.1f, hubFront + zr), vec3(x, y + 2.4f, hubFront + zr), Z, 0.07f, 0.07f, 1.0f, R, 0.0f, 0.004f);
        box(vec3(x - 0.4f, y + 2.1f, hubFront + zr), vec3(0.75f, 0.14f, 0.025f), 0.0f, 1.0f); box(vec3(x + 0.3f, y + 1.7f, hubFront + zr), vec3(0.6f, 0.14f, 0.025f), 0.0f, 1.0f);
        hubM.endChunk(); solid(x, zr, 0.25f);
    }

    // ---- 6) rochers au pied de la montagne et semés dans la plaine
    hubM.beginChunk();
    for (int i = 0; i < 46; i++) {
        float x = R.r(-60.0f, 60.0f), zr = R.f() < 0.6f ? R.r(2.0f, 22.0f) : R.r(10.0f, 100.0f); if (std::fabs(x) < 5.0f && zr < 30.0f) continue; if (std::fabs(x - hubPathX(hubFront + zr)) < 2.5f) continue;
        float rad = R.r(0.5f, 2.2f); addEllipsoid(hubM, vec3(x, G(x, zr) + rad * 0.25f, hubFront + zr), X, Y, Z, vec3(rad, rad * R.r(0.5f, 0.8f), rad * R.r(0.8f, 1.2f)), 0.0f, R.f()); solid(x, zr, rad * 0.8f);
    }
    hubM.endChunk();

    // ---- 7) arbres : clairsemés dans la plaine, forêt dense au-delà des murs invisibles
    for (int i = 0; i < 900; i++) {
        float x = R.r(-112.0f, 112.0f), zr = R.r(4.0f, 142.0f);
        bool play = std::fabs(x) < 58.0f && zr < 112.0f;
        if (play && R.f() > 0.07f) continue;                                                              // plaine presque dégagée
        if (std::fabs(x - hubPathX(hubFront + zr)) < 4.5f && zr > 20.0f) continue;
        if (std::sqrt(x * x + (zr - 14.0f) * (zr - 14.0f)) < 28.0f && play) continue;
        if (zr < 14.0f && std::fabs(x) < 40.0f) continue;
        float h = R.r(6.0f, 15.0f), y = G(x, zr); vec3 b(x, y, hubFront + zr);
        if (R.f() < 0.6f) addPine(hubM, b, h, R); else addOak(hubM, b, h * 0.8f, R);
        if (play) solid(x, zr, 0.55f);
    }

    // ---- 8) cristaux géants dans la montagne (et deux au sol), colorés et lumineux
    {
        auto crystalCluster = [&](vec3 at, vec3 n, int hue, int count, float scale) {
            hubM.beginChunk();
            for (int i = 0; i < count; i++) {
                vec3 d = glm::normalize(n * 0.7f + vec3(R.r(-0.7f, 0.7f), R.r(0.1f, 0.8f), R.r(-0.7f, 0.7f)) + vec3(0, 0.3f, 0));
                float len = R.r(9.0f, 28.0f) * scale, rad = R.r(1.0f, 3.0f) * scale; int hv = (hue + (R.f() < 0.25f ? 1 : 0)) % 5;
                vec3 b = at - n * 1.5f + vec3(R.r(-1.2f, 1.2f), R.r(-0.6f, 0.6f), R.r(-1.2f, 1.2f)) * scale;
                vec3 mid = b + d * (len * 0.72f), tip = b + d * len;
                size_t v0 = hubM.v.size(); addTubeM(hubM, b, mid, rad, rad * 0.9f, 6, gHM.crys[hv], R.f(), false, false); size_t v1 = hubM.v.size();
                addTubeM(hubM, mid, tip, rad * 0.9f, 0.0f, 6, gHM.crys[hv], R.f(), true, false);
                for (size_t q = v0; q < v1; q++) hubM.v[q].uv.y *= 0.72f;
                for (size_t q = v1; q < hubM.v.size(); q++) hubM.v[q].uv.y = std::min(1.0f, 0.72f + 0.28f * hubM.v[q].uv.y);
            }
            hubM.endChunk();
            addLight(at + n * 4.0f, gCrysCol[hue] * 1.2f, 5.0f, 7 + hue, false);
        };
        for (int c = 0; c < 9; c++) {
            for (int tries = 0; tries < 50; tries++) {
                int s = 7 + (int)(R.f() * 20), k = 3 + (int)(R.f() * (K - 6)); vec3 wp = cliff[(size_t)s * K + k];
                vec3 n = glm::normalize(glm::cross(cliff[(size_t)(s + 1) * K + k] - cliff[(size_t)(s - 1) * K + k], cliff[(size_t)s * K + (k + 1) % K] - cliff[(size_t)s * K + (k + K - 1) % K]));
                if (glm::dot(n, wp - (r0.c + r0.T * 80.0f)) < 0) n = -n;
                float h = wp.y - hubG; if (h < 10.0f || h > 60.0f || std::fabs(wp.x) < 7.0f || n.z < 0.15f) continue;
                crystalCluster(wp, n, c % 5, 4 + (int)(R.f() * 4), 1.0f); break;
            }
        }
        for (float sx : {-30.0f, 33.0f}) { float zr = 9.0f; crystalCluster(vec3(sx, G(sx, zr), hubFront + zr), vec3(0, 0.6f, 0.4f), sx < 0 ? 4 : 1, 5, 0.85f); solid(sx, zr, 3.0f); }
    }

    for (auto& v : hubM.v) v.biome = (float)HUB_BIOME;
    hubSpawn = vec3(hubPathX(hubFront + 100.0f), G(hubPathX(hubFront + 100.0f), 100.0f) + 1.65f, hubFront + 100.0f);
    std::printf("Camp de base : %zu sommets, %zu obstacles\n", hubM.v.size(), hubSolids.size());
}

vec3 World::constrainHub(vec3 p, bool fly) const {
    const float HW = 58.0f, HD = 112.0f; const Tube& tb = tubes[0]; const Ring& r0 = tb.rings[0];
    vec3 q = p; q.x = std::min(std::max(q.x, -HW), HW); q.z = std::min(q.z, hubFront + HD);
    float zMin = hubFront + 0.3f, lo = 0, hi = 0; bool corridor = false;
    { float fl = tb.floorY(0, 0.0f), a1, a2, b1, b2; if (tb.wallRange(0, fl + 1.65f, a1, a2) && tb.wallRange(0, fl + 0.5f, b1, b2)) { lo = std::max(a1, b1) + 0.45f; hi = std::min(a2, b2) - 0.45f; corridor = lo < hi; } }
    float xr = glm::dot(q - r0.c, r0.R);
    if (!corridor || xr < lo - 0.05f || xr > hi + 0.05f) q.z = std::max(q.z, zMin);
    else if (q.z < zMin + 1.2f) q.x += r0.R.x * (std::min(std::max(xr, lo), hi) - xr);
    for (const HubSolid& s : hubSolids) {
        float dx = q.x - s.x, dz = q.z - s.z, rr = s.r + 0.35f, d2 = dx * dx + dz * dz;
        if (d2 < rr * rr) { float d = std::sqrt(std::max(d2, 1e-8f)); q.x = s.x + dx / d * rr; q.z = s.z + dz / d * rr; }
    }
    float g = hubGround(q.x, q.z); q.y = fly ? std::max(q.y, g + 0.6f) : g + 1.65f;
    return q;
}

void World::build(uint32_t sd) {
    destroy(); seed = sd; hubM = Mesh(); rock = Mesh(); wood = Mesh(); torchM = Mesh(); webM = Mesh(); waterM = Mesh(); debrisM = Mesh(); ratM = Mesh();
    torches.clear(); frames.clear(); webs.clear(); holes.clear(); debris.clear(); tubes.clear();
    pickups.clear(); spawns.clear(); drips.clear(); doorBoards.clear(); ladders.clear(); walls.clear(); doorOpen = false; keysNeeded = 0; doorRing = 0;
    Rng rng(sd);
    tubes.push_back(Tube());
    corrA = (int)(900 * 0.92f * 0.635f); corrB = corrA + 92;          // ~28 m dans les catacombes
    genTube(tubes[0], 900, vec3(0), 0.0f, 0.0f, true, rng, -1, -1, 0);
    doorRing = (int)(tubes[0].N * 0.92f);
    if (corrB + 40 > doorRing || corrA < 5 * doorRing * 3 / 25) corrA = corrB = -1;
    for (int i = doorRing; i < tubes[0].N; i++) { float k = smooth((float)doorRing, (float)doorRing + 25.0f, (float)i); tubes[0].rings[i].w *= 1.0f + 0.5f * k; tubes[0].rings[i].h *= 1.0f + 0.3f * k; }
    computeSection(0);
    // galeries latérales en culs-de-sac
    int wantBranches = 4;
    for (int attempt = 0; attempt < 14 && (int)tubes.size() - 1 < wantBranches; attempt++) {
        Tube& mn = tubes[0]; if (mn.N < 300) break;
        int j = 70 + rng.next() % std::max(1, doorRing - 150);
        bool clash = false; for (size_t b = 1; b < tubes.size(); b++) if (std::abs(tubes[b].junction - j) < 110) clash = true;
        if (clash) continue;
        int side = rng.chance(0.5f) ? 1 : -1; const Ring& r = mn.rings[j];
        float yawB = r.yaw + side * (PI * 0.5f - 0.3f);
        float wB = rng.r(1.5f, 1.9f), hB = 2.75f;
        float fy = mn.floorY(j, 0.0f), xl, xr; if (!mn.wallRange(j, fy + 1.0f, xl, xr)) continue;
        float xw = side > 0 ? xr : xl;
        vec3 W = mn.toWorld(j, xw, fy + 0.34f * hB + 0.02f);
        vec3 dB(std::sin(yawB), 0, -std::cos(yawB));
        Tube tb; tb.side = side;
        genTube(tb, 50 + (int)(rng.next() % 60), W - dB * 1.0f, yawB, 0.0f, false, rng, 0, j, wB);
        if (tb.N < 45) continue;
        tb.junction = j; tubes.push_back(tb); computeSection(tubes.size() - 1);
    }
    genRock(rng); genStalagmites(rng); genWood(rng); genHub(rng);
    assignBiomes(rock); assignBiomes(wood); assignBiomes(torchM); assignBiomes(webM); assignBiomes(waterM);
    rock.upload(); wood.upload(); torchM.upload(); webM.upload(); waterM.upload(); hubM.upload();
    for (size_t b = 1; b < tubes.size(); b++)
        std::printf("  galerie %zu : jonction a %.0f m (%s), longueur %.0f m\n", b, tubes[b].junction * tubes[0].ds, tubes[b].side > 0 ? "droite" : "gauche", tubes[b].N * tubes[b].ds);
    for (auto& h : holes) std::printf("  trous de rats : galerie %d, %.0f m -> %.0f m\n", h.tube, h.iA * tubes[h.tube].ds, h.iB * tubes[h.tube].ds);
    std::printf("Grotte #%u : %zu galeries, %zu torches, %zu planches, %zu toiles, %zu trous de rats\n",
                sd, tubes.size(), torches.size(), wood.boards.size(), webs.size(), holes.size() * 2);
}

// ---------- interactions ---------------------------------------------------------
static bool pushSphereOut(vec3& p, float r, const Board& b, bool horizontalOnly) {
    vec3 d = p - b.c; vec3 l(glm::dot(d, b.dl), glm::dot(d, b.dw), glm::dot(d, b.dt));
    vec3 h(b.hl, b.hw, b.ht); vec3 q = glm::clamp(l, -h, h); vec3 dd = l - q; float d2 = glm::dot(dd, dd);
    if (d2 >= r * r) return false;
    vec3 n;
    if (d2 < 1e-9f) {                                     // centre dans la boîte : sortir par la face la plus proche
        vec3 pen = h - glm::abs(l); int a = 0; if (pen.y < pen.x) a = 1; if (pen.z < pen[a]) a = 2;
        vec3 nl(0); nl[a] = l[a] >= 0 ? 1.f : -1.f; n = b.dl * nl.x + b.dw * nl.y + b.dt * nl.z;
        float push = pen[a] + r; if (horizontalOnly) n.y = 0;
        if (glm::length(n) < 1e-4f) return false;
        p += glm::normalize(n) * push; return true;
    }
    float dist = std::sqrt(d2); vec3 nl = dd / dist; n = b.dl * nl.x + b.dw * nl.y + b.dt * nl.z;
    if (horizontalOnly) { n.y = 0; if (glm::length(n) < 1e-4f) return false; n = glm::normalize(n); }
    p += n * (r - dist); return true;
}

void World::collideBoards(vec3& p, bool fly) const {
    for (int iter = 0; iter < 2; iter++)
        for (const Board& b : wood.boards) {
            if (!b.alive) continue;
            if (glm::length(b.c - p) > b.hl + 2.2f) continue;
            if (fly) pushSphereOut(p, 0.35f, b, false);
            else {
                for (float off : {1.2f, 0.65f, 0.05f}) {
                    vec3 q = p - vec3(0, off, 0); pushSphereOut(q, 0.3f, b, true); p.x = q.x; p.z = q.z;
                }
            }
        }
}

vec3 World::constrain(vec3 p, bool fly, std::vector<int>& hints) const {
    if (inHub(p)) return constrainHub(p, fly);
    struct C { int k, ring; float x, y, t, lo, hi, fl; bool in; float disp; };
    std::vector<C> cs;
    if (hints.size() < tubes.size()) hints.resize(tubes.size(), -1);
    for (size_t k = 0; k < tubes.size(); k++) {
        const Tube& tb = tubes[k]; if (tb.N == 0) continue;
        int ring = tb.locate(p, hints[k]); vec3 d = p - tb.rings[ring].c;
        if (glm::dot(d, d) > 49.0f) { hints[k] = -1; continue; }
        hints[k] = ring; const Ring& r = tb.rings[ring];
        C c; c.k = (int)k; c.ring = ring; c.x = glm::dot(d, r.R); c.y = glm::dot(d, r.U); c.t = glm::dot(d, r.T);
        float tmin = (ring == 0) ? (tb.isMain ? -0.4f : -1.6f) : -1e9f, tmax = (ring == tb.N - 1) ? 0.4f : 1e9f;
        bool tOK = c.t >= tmin && c.t <= tmax;
        c.fl = tb.floorY(ring, c.x); c.lo = c.hi = 0; c.disp = 0; c.in = false;
        if (!fly) {
            float a1, a2, b1, b2;
            if (!tb.wallRange(ring, c.fl + 1.65f, a1, a2) || !tb.wallRange(ring, c.fl + 0.5f, b1, b2)) continue;
            c.lo = std::max(a1, b1) + 0.45f; c.hi = std::min(a2, b2) - 0.45f;
            if (c.lo > c.hi) { float m = (c.lo + c.hi) * 0.5f; c.lo = c.hi = m; c.in = std::fabs(c.x - m) < 0.15f && tOK; }
            else c.in = c.x >= c.lo && c.x <= c.hi && tOK;
            c.disp = std::fabs(c.x - std::min(std::max(c.x, c.lo), c.hi)) + (tOK ? 0 : 1.0f);
        } else {
            float len = std::sqrt(c.x * c.x + c.y * c.y), rad = tb.radiusAt(ring, std::atan2(c.y, c.x)) - 0.45f;
            c.in = len <= rad && tOK; c.disp = std::max(0.0f, len - rad) + (tOK ? 0 : 1.0f);
        }
        cs.push_back(c);
    }
    if (cs.empty()) return p;
    const C* pick = nullptr;
    for (const C& c : cs) if (c.in && (!pick || std::fabs(c.t) < std::fabs(pick->t))) pick = &c;
    if (!pick) for (const C& c : cs) if (!pick || c.disp < pick->disp) pick = &c;
    const Tube& tb = tubes[pick->k]; float x = pick->x, y = pick->y, t = pick->t;
    if (!pick->in) {
        if (!fly) x = std::min(std::max(x, pick->lo), pick->hi);
        else { float len = std::sqrt(x * x + y * y), lim = tb.radiusAt(pick->ring, std::atan2(y, x)) - 0.45f; if (len > lim && len > 1e-4f) { x *= lim / len; y *= lim / len; } }
        float tmin = (pick->ring == 0) ? (tb.isMain ? -0.4f : -1.6f) : -1e9f, tmax = (pick->ring == tb.N - 1) ? 0.4f : 1e9f;
        t = std::min(std::max(t, tmin), tmax);
    }
    if (!fly) {
        y = tb.floorY(pick->ring, x) + 1.65f;
        const Ring& r = tb.rings[pick->ring]; vec3 Th(r.T.x, 0, r.T.z); float cp = glm::length(Th);
        if (cp > 1e-3f) {
            Th /= cp; float tn = (glm::dot(p - r.c, Th) + r.T.y * y) / cp;
            float tmin = (pick->ring == 0) ? (tb.isMain ? -0.4f : -1.6f) : -1e9f, tmax = (pick->ring == tb.N - 1) ? 0.4f : 1e9f;
            t = std::min(std::max(tn, tmin), tmax);
        }
    }
    return tb.toWorld(pick->ring, x, y, t);
}

int World::rayBoard(vec3 o, vec3 d, float maxT, float& tHit) const {
    int best = -1; tHit = maxT;
    for (size_t i = 0; i < wood.boards.size(); i++) {
        const Board& b = wood.boards[i]; if (!b.alive || b.role == 9) continue;
        if (glm::length(b.c - o) > maxT + b.hl + 0.5f) continue;
        vec3 oo = o - b.c; vec3 ol(glm::dot(oo, b.dl), glm::dot(oo, b.dw), glm::dot(oo, b.dt)), dl(glm::dot(d, b.dl), glm::dot(d, b.dw), glm::dot(d, b.dt));
        vec3 h = vec3(b.hl, b.hw, b.ht) + vec3(0.03f);
        float t0 = 0, t1 = tHit; bool ok = true;
        for (int a = 0; a < 3 && ok; a++) {
            if (std::fabs(dl[a]) < 1e-6f) { if (std::fabs(ol[a]) > h[a]) ok = false; }
            else { float ta = (-h[a] - ol[a]) / dl[a], tb2 = (h[a] - ol[a]) / dl[a]; if (ta > tb2) std::swap(ta, tb2); t0 = std::max(t0, ta); t1 = std::min(t1, tb2); if (t0 > t1) ok = false; }
        }
        if (ok && t0 < tHit) { tHit = t0; best = (int)i; }
    }
    return best;
}

void World::spawnPiece(const Board& b, float s0, float s1, vec3 vel) {
    if (std::fabs(s1 - s0) < 0.06f) return;
    Debris d; d.p = b.c + b.dl * ((s0 + s1) * 0.5f); d.ax = b.dl; d.ay = b.dw; d.az = b.dt; d.h = vec3(std::fabs(s1 - s0) * 0.5f, b.hw, b.ht);
    d.v = vel + vec3(drng.r(-1, 1), drng.r(0, 1.5f), drng.r(-1, 1)); d.w = vec3(drng.r(-6, 6), drng.r(-6, 6), drng.r(-6, 6));
    d.seed = drng.f() * 10; d.tube = b.tube; d.ring = -1;
    if (debris.size() > 320) debris.erase(debris.begin());
    debris.push_back(d);
}
void World::spawnChips(vec3 at, vec3 push, int n, bool stone) {
    int bt = 0; float bd = 1e9f;
    for (size_t t = 0; t < tubes.size(); t++) { int r = tubes[t].locate(at, -1); float dd = glm::length(tubes[t].rings[r].c - at); if (dd < bd) { bd = dd; bt = (int)t; } }
    for (int k = 0; k < n; k++) {
        Debris d; d.p = at + vec3(drng.r(-0.05f, 0.05f), drng.r(-0.05f, 0.05f), drng.r(-0.05f, 0.05f));
        vec3 a = glm::normalize(vec3(drng.r(-1, 1), drng.r(-1, 1), drng.r(-1, 1)) + vec3(0.01f));
        vec3 b = glm::normalize(glm::cross(a, vec3(0.3f, 0.8f, 0.5f))), c = glm::cross(a, b);
        d.ax = a; d.ay = b; d.az = c; d.h = vec3(drng.r(0.03f, 0.1f), drng.r(0.01f, 0.03f), drng.r(0.008f, 0.02f));
        d.v = push * drng.r(1.0f, 3.5f) + vec3(drng.r(-1.5f, 1.5f), drng.r(0.5f, 2.5f), drng.r(-1.5f, 1.5f));
        d.w = vec3(drng.r(-12, 12), drng.r(-12, 12), drng.r(-12, 12)); d.seed = drng.f() * 10;
        d.tube = bt; d.ring = -1;
        if (stone) { d.stone = true; d.h = vec3(drng.r(0.04f, 0.1f), drng.r(0.03f, 0.07f), drng.r(0.03f, 0.08f)); d.v *= 1.3f; }
        if (debris.size() > 320) debris.erase(debris.begin());
        debris.push_back(d);
    }
}

void World::breakBoard(int bi, vec3 hit, vec3 push, bool whole) {
    Board& b = wood.boards[bi]; if (!b.alive) return;
    b.alive = false; wood.killIndices(b.i0, b.n);
    if (b.group >= 0 && ladders[b.group].alive) {                // l'échelle entière s'effondre
        LadderObj& L = ladders[b.group]; L.alive = false; wood.killIndices(L.i0, L.n);
        for (int id : L.boards) if (id != bi && wood.boards[id].alive) breakBoard(id, wood.boards[id].c, vec3(0), true);
    }
    if (whole) spawnPiece(b, -b.hl, b.hl, push * 0.3f);
    else {
        float s = std::min(std::max(glm::dot(hit - b.c, b.dl), -b.hl * 0.6f), b.hl * 0.6f);
        spawnPiece(b, -b.hl, s, push * 3.0f - b.dl * 1.5f); spawnPiece(b, s, b.hl, push * 3.0f + b.dl * 1.5f);
        spawnChips(hit, push, 6);
    }
    if (b.frame >= 0) checkCollapse(b.frame);
}

void World::hitBoard(int bi, vec3 hit, vec3 push) {
    Board& b = wood.boards[bi]; b.hp--;
    if (b.role == 7) { if (b.hp <= 0) breakStone(bi, hit, push); else spawnChips(hit, push, 5, true); return; }
    if (b.hp <= 0) breakBoard(bi, hit, push, false); else spawnChips(hit, push, 4);
}

void World::checkCollapse(int fid) {
    Frame& f = frames[fid];
    for (int s = 0; s < 2; s++) {
        if (f.fell[s]) continue;
        bool any = false; for (int id : f.post[s]) if (wood.boards[id].alive) any = true;
        if (any) continue;
        f.fell[s] = true;
        for (int id : f.brace[s]) if (wood.boards[id].alive) breakBoard(id, wood.boards[id].c, vec3(0), true);
        for (int id : f.beam) if (wood.boards[id].alive) breakBoard(id, wood.boards[id].c, vec3(0.0f, -0.5f, 0.0f), true);
        for (int id : f.brace[1 - s]) if (wood.boards[id].alive) breakBoard(id, wood.boards[id].c, vec3(0), true);
        for (int w : f.webs) if (webs[w].alive) { webs[w].alive = false; webM.killIndices(webs[w].i0, 3); }
    }
}

int World::clearWebs(vec3 o, vec3 d, float reach) {
    int n = 0;
    for (auto& w : webs) {
        if (!w.alive) continue; vec3 oc = w.c - o; float t = glm::dot(oc, d); if (t < 0 || t > reach + 0.6f) continue;
        float dist = glm::length(oc - d * t); if (dist < w.r + 0.55f) { w.alive = false; webM.killIndices(w.i0, 3); n++; }
    }
    return n;
}

// Vie d'un débris de bois : intact (0-9 s), il noircit et braise (9-13 s), puis se réduit en cendres (13-17 s) et disparaît.
static const float WOOD_CHAR = 9.0f, WOOD_ASH = 13.0f, WOOD_GONE = 17.0f;

void World::stepDebris(float dt) {
    for (Debris& d : debris) {
        d.life += dt;
        if (d.stone) {                                       // pierre : au bout de 10 s, dès qu'on ne la regarde plus, elle s'effrite en poussière
            vec3 tc = d.p - camPos; float dl = glm::length(tc); bool seen = dl < 45.0f && glm::dot(tc / std::max(dl, 1e-3f), camFwd) > 0.42f;
            d.unseen = seen ? 0.0f : d.unseen + dt;
            if (d.die < 0 && d.life > 10.0f && d.unseen > 0.3f) { d.die = 0; d.sleep = false; }
            if (d.die >= 0) { d.die += dt; if (drng.chance(std::min(1.0f, dt * 10.0f))) ashEvents.push_back(d.p + vec3(drng.r(-1, 1), drng.r(0, 1), drng.r(-1, 1)) * d.h.x); }
        }
        if (!d.stone && d.life > WOOD_ASH && d.life < WOOD_GONE && drng.chance(std::min(1.0f, dt * 16.0f)))
            ashEvents.push_back(d.p + vec3(drng.r(-d.h.x, d.h.x) * 0.6f, drng.r(-0.05f, 0.1f), drng.r(-0.1f, 0.1f)));
    }
    debris.erase(std::remove_if(debris.begin(), debris.end(), [&](const Debris& d) {
        if (d.stone) return d.die > 1.4f || d.life > 90.0f;
        if (d.life < WOOD_GONE) return false;
        for (int k = 0; k < 4; k++) ashEvents.push_back(d.p); return true; }), debris.end());
    for (Debris& d : debris) {
        if (d.die >= 0) { d.p.y -= 0.25f * dt; continue; }    // s'enfonce en s'effritant
        if (d.sleep) continue; d.age += dt;
        d.v.y -= 9.8f * dt; d.p += d.v * dt;
        float wl = glm::length(d.w);
        if (wl > 1e-4f) { mat3 R = mat3(glm::rotate(mat4(1), wl * dt, d.w / wl)); d.ax = R * d.ax; d.ay = R * d.ay; d.az = R * d.az; }
        const Tube& tb = tubes[d.tube]; d.ring = tb.locate(d.p, d.ring); const Ring& r = tb.rings[d.ring];
        float maxPen = 0;
        for (int k = 0; k < 8; k++) {
            vec3 cp = d.p + d.ax * ((k & 1 ? 1.f : -1.f) * d.h.x) + d.ay * ((k & 2 ? 1.f : -1.f) * d.h.y) + d.az * ((k & 4 ? 1.f : -1.f) * d.h.z);
            vec3 dd = cp - r.c; float pen = tb.floorY(d.ring, glm::dot(dd, r.R)) - glm::dot(dd, r.U); maxPen = std::max(maxPen, pen);
        }
        bool ground = false;
        if (maxPen > 0) {
            d.p += r.U * maxPen; float vn = glm::dot(d.v, r.U); if (vn < 0) d.v -= r.U * (vn * 1.25f);
            d.v -= (d.v - r.U * glm::dot(d.v, r.U)) * std::min(1.0f, 5.0f * dt); d.w *= std::max(0.0f, 1.0f - 4.0f * dt); ground = true;
        }
        vec3 dd = d.p - r.c; float x = glm::dot(dd, r.R), y = glm::dot(dd, r.U), xl, xr;
        if (tb.wallRange(d.ring, y, xl, xr)) {
            float m = 0.12f;
            if (x < xl + m) { d.p += r.R * (xl + m - x); float vr = glm::dot(d.v, r.R); if (vr < 0) d.v -= r.R * (vr * 1.3f); }
            if (x > xr - m) { d.p -= r.R * (x - (xr - m)); float vr = glm::dot(d.v, r.R); if (vr > 0) d.v -= r.R * (vr * 1.3f); }
        }
        if (ground && glm::length(d.v) < 0.5f) d.sleepT += dt; else d.sleepT = 0;
        if (d.sleepT > 0.12f) {
            d.v = vec3(0); d.w = vec3(0);
            vec3* ax[3] = {&d.ax, &d.ay, &d.az}; int a = 0; float best = 0;
            for (int q = 0; q < 3; q++) { float v = std::fabs(glm::dot(*ax[q], r.U)); if (v > best) { best = v; a = q; } }
            vec3 target = glm::dot(*ax[a], r.U) >= 0 ? r.U : -r.U; vec3 axis = glm::cross(*ax[a], target); float sl = glm::length(axis);
            if (sl > 1e-4f) {
                float ang = std::asin(std::min(1.0f, sl)) * std::min(1.0f, 10.0f * dt); mat3 R = mat3(glm::rotate(mat4(1), ang, axis / sl));
                d.ax = R * d.ax; d.ay = R * d.ay; d.az = R * d.az;
            }
            if (sl < 0.02f && d.sleepT > 0.4f) d.sleep = true;
        }
        if (d.age > 30) d.sleep = true;
    }
}

void World::buildDebrisMesh() {
    debrisM.clearCpu(); Rng z(1);
    for (const Debris& d : debris) {
        if (d.stone) {                                                                      // pierre : ne brûle pas, rétrécit en fin de vie
            float ks = d.die < 0 ? 1.0f : std::max(0.03f, 1.0f - d.die / 1.4f); ks = ks * (2.0f - ks); debrisM.geom = 0;
            addBoard(debrisM, d.p, d.ax, d.ay, d.az, d.h.x * ks, d.h.y * ks, d.h.z * ks, 0.4f, d.seed, z, 0.012f); continue;
        }
        debrisM.geom = (d.h.z <= 0.036f && d.h.y >= 0.065f && d.h.x >= 0.1f) ? 1 : 0;    // morceaux de planche : modèle ; éclats et poteaux : boîtes
        float k = d.life < WOOD_ASH ? 1.0f : std::max(0.03f, 1.0f - (d.life - WOOD_ASH) / (WOOD_GONE - WOOD_ASH));    // rétrécit en cendres
        k = k * (2.0f - k);
        addBoard(debrisM, d.p, d.ax, d.ay, d.az, d.h.x * k, d.h.y * k, d.h.z * k, d.life < WOOD_CHAR ? 1.0f : 120.0f,
                 d.life < WOOD_CHAR ? d.seed : 20.0f + 10.0f * std::min(1.0f, std::max(0.0f, 1.0f - (d.life - WOOD_CHAR) / (WOOD_ASH - WOOD_CHAR))), z, 0.0f);   // noirci (braises qui s'éteignent) ensuite
    }
    debrisM.geom = 0;
    debrisM.upload(GL_STREAM_DRAW);
}

// ----------------------------------------------------------------------------
//  Hache (modèle vue subjective)
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
//  Rats
// ----------------------------------------------------------------------------
struct Rat { std::vector<vec3> pts; std::vector<float> cum; float s = 0, speed = 3.2f, phase = 0; bool alive = true; };

static vec3 ratPos(const Rat& r, float s, vec3& fwd) {
    size_t k = 1; while (k + 1 < r.cum.size() && r.cum[k] < s) k++;
    float seg = std::max(1e-4f, r.cum[k] - r.cum[k - 1]), t = std::min(1.0f, std::max(0.0f, (s - r.cum[k - 1]) / seg));
    fwd = glm::normalize(r.pts[k] - r.pts[k - 1] + vec3(1e-6f)); return glm::mix(r.pts[k - 1], r.pts[k], t);
}

static void buildRat(Mesh& m, vec3 pos, vec3 fwd, float phase) {
    vec3 up(0, 1, 0); vec3 ax = glm::normalize(vec3(fwd.x, 0, fwd.z) + vec3(1e-6f)), az = glm::normalize(glm::cross(ax, up)), ay = up;
    float sc = 1.2f; pos += ay * (0.012f * std::sin(phase * 2.0f));
    auto E = [&](vec3 c, vec3 rad, float mat, float seed) { addEllipsoid(m, pos + (ax * c.x + ay * c.y + az * c.z) * sc, ax, ay, az, rad * sc, mat, seed); };
    E(vec3(0.0f, 0.055f, 0), vec3(0.11f, 0.052f, 0.048f), 6, 0); E(vec3(-0.07f, 0.058f, 0), vec3(0.07f, 0.058f, 0.054f), 6, 0);
    E(vec3(0.125f, 0.052f, 0), vec3(0.058f, 0.04f, 0.034f), 6, 0); E(vec3(0.18f, 0.044f, 0), vec3(0.042f, 0.024f, 0.02f), 6, 0.6f);
    E(vec3(0.112f, 0.092f, 0.03f), vec3(0.014f, 0.02f, 0.008f), 6, 0.6f); E(vec3(0.112f, 0.092f, -0.03f), vec3(0.014f, 0.02f, 0.008f), 6, 0.6f);
    E(vec3(0.15f, 0.066f, 0.024f), vec3(0.007f, 0.007f, 0.007f), 8, 0); E(vec3(0.15f, 0.066f, -0.024f), vec3(0.007f, 0.007f, 0.007f), 8, 0);
    for (int k = 0; k < 7; k++) {
        float f = k / 6.0f; E(vec3(-0.13f - 0.045f * k, 0.04f - 0.003f * k, std::sin(phase * 1.5f - k * 0.9f) * 0.03f * f), vec3(0.03f, 0.006f, 0.006f), 6, 1.0f);
    }
    for (int k = 0; k < 4; k++) {
        float front = (k < 2) ? 0.07f : -0.07f, side = (k & 1) ? 0.04f : -0.04f, sw = std::sin(phase * 2.6f + k * 1.7f);
        E(vec3(front + sw * 0.035f, 0.022f + std::max(0.0f, std::cos(phase * 2.6f + k * 1.7f)) * 0.012f, side), vec3(0.022f, 0.026f, 0.011f), 6, 0.6f);
    }
}

// ----------------------------------------------------------------------------
//  Shaders
// ----------------------------------------------------------------------------
static const char* SCENE_VS = R"(#version 430 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aN; layout(location=2) in vec2 aUV; layout(location=3) in vec2 aMS; layout(location=4) in float aBiome;
uniform mat4 uVP; uniform mat4 uModel;
out vec3 vPos; out vec3 vN; out vec2 vUV; flat out float vMat; out float vSeed; out float vBiome; out vec3 vLocal;
void main(){ vec4 w=uModel*vec4(aPos,1.0); vPos=w.xyz; vLocal=aPos; vN=mat3(uModel)*aN; vUV=aUV; vMat=aMS.x; vSeed=aMS.y; vBiome=aBiome; gl_Position=uVP*w; }
)";

static const char* SCENE_FS = R"(#version 430 core
in vec3 vPos; in vec3 vN; in vec2 vUV; flat in float vMat; in float vSeed; in float vBiome; in vec3 vLocal;
out vec4 fragColor;
uniform float uGhost;                               // dissolution de « la Dame » (0..1)
uniform vec3 uCam; uniform float uTime;
uniform sampler2D uRockCol, uRockNrm, uWoodCol, uWoodNrm;
uniform samplerCubeArray uShadow;
const int MAXL=32;
uniform int uNL;
uniform vec3 uLPos[MAXL]; uniform vec3 uLCol[MAXL]; uniform int uLSh[MAXL];
uniform float uLR;
const float PI=3.14159265;
uniform vec3 uTint[7]; uniform vec3 uAmbB[7]; uniform float uWetB[7]; uniform float uMossB[7];
uniform vec3 uSunDir; uniform vec3 uSunCol; uniform vec3 uHaze; uniform vec2 uHubC; uniform float uHubG;
uniform vec4 uMdlA[48]; uniform vec4 uMdlE[48];     // matériaux des modèles OBJ : (Kd, Ns) et (Ke, brillance)

float h31(vec3 p){ p=fract(p*0.1031); p+=dot(p,p.zyx+31.32); return fract((p.x+p.y)*p.z); }
float vn(vec3 x){ vec3 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(mix(h31(i),h31(i+vec3(1,0,0)),f.x),mix(h31(i+vec3(0,1,0)),h31(i+vec3(1,1,0)),f.x),f.y),
             mix(mix(h31(i+vec3(0,0,1)),h31(i+vec3(1,0,1)),f.x),mix(h31(i+vec3(0,1,1)),h31(i+vec3(1,1,1)),f.x),f.y),f.z); }
float ign(vec2 p){ return fract(52.9829189*fract(dot(p,vec2(0.06711056,0.00583715)))); }

vec4 tri(sampler2D t, vec3 p, vec3 w, float sc){
  return texture(t,p.zy*sc)*w.x + texture(t,p.xz*sc)*w.y + texture(t,p.xy*sc)*w.z; }
vec3 triN(sampler2D t, vec3 p, vec3 n, vec3 w, float sc){
  vec3 tx=texture(t,p.zy*sc).xyz*2.0-1.0, ty=texture(t,p.xz*sc).xyz*2.0-1.0, tz=texture(t,p.xy*sc).xyz*2.0-1.0;
  tx=vec3(tx.xy+n.zy, abs(tx.z)*n.x);
  ty=vec3(ty.xy+n.xz, abs(ty.z)*n.y);
  tz=vec3(tz.xy+n.xy, abs(tz.z)*n.z);
  return normalize(tx.zyx*w.x + ty.xzy*w.y + tz.xyz*w.z); }

vec2 ripples(vec2 p, float t){
  vec2 g=floor(p/1.5), acc=vec2(0.0);
  for(int j=-1;j<=1;j++) for(int i=-1;i<=1;i++){
    vec2 c=g+vec2(i,j);
    float hh=h31(vec3(c,7.0)); vec2 ctr=(c+vec2(h31(vec3(c,1.0)),h31(vec3(c,2.0))))*1.5;
    float per=2.5+hh*3.0, ph=fract(t/per+hh);
    float r=length(p-ctr), rr=ph*0.7;
    float w=exp(-pow((r-rr)*16.0,2.0))*(1.0-ph);
    acc+=(p-ctr)/max(r,1e-3)*w*sin((r-rr)*45.0);
  } return acc; }

// cristal : facettes nettes, cœur lumineux qui s'intensifie vers la pointe, bandes de croissance, fêlures internes,
// lueur qui dérive lentement, liseré de Fresnel et paillettes scintillantes
void crystalShade(vec3 col, vec3 P, vec3 V, float dP, inout vec3 albedo, inout vec3 emis, inout vec3 N, inout float gloss, inout float shin){
  vec3 fn=normalize(cross(dFdx(P),dFdy(P))); if(dot(fn,V)<0.0) fn=-fn;
  float along=clamp(vUV.y,0.0,1.0), fres=pow(1.0-max(dot(fn,V),0.0),2.5);
  float crack=1.0-smoothstep(0.0,0.07,abs(vn(P*2.2+vec3(along*2.0,0.0,0.0))*2.0-1.0));
  float inner=0.35+0.65*vn(P*1.4+vec3(0.0,uTime*0.12,0.0)), bands=0.5+0.5*sin(along*22.0+vn(P*3.0)*5.0), core=pow(along,1.4);
  albedo=col*0.04;
  emis=col*(0.18+0.95*core)*(0.55+0.55*inner)*(0.82+0.3*bands)+mix(col,vec3(1.0),0.5)*crack*0.7*(0.4+core)+mix(col,vec3(1.0),0.7)*fres*1.3;
  float gl=h31(floor(P*38.0)); emis+=vec3(2.0)*step(0.992,gl)*(0.6+0.4*sin(uTime*5.0+gl*40.0))*(1.0-smoothstep(10.0,40.0,dP));
  N=fn; gloss=1.6; shin=220.0;
}

float shadowAt(int slot, vec3 P, vec3 Ng, vec3 lp){
  vec3 dir=(P+Ng*0.05)-lp; float dist=length(dir); vec3 d=dir/dist;
  vec3 t1=normalize(cross(d,abs(d.y)<0.9?vec3(0,1,0):vec3(1,0,0))), t2=cross(d,t1);
  float rot=ign(gl_FragCoord.xy)*6.2831853, bias=0.05+0.022*dist, lit=0.0;
  for(int n=0;n<8;n++){
    float r=sqrt((float(n)+0.5)/8.0), a=float(n)*2.39996+rot;
    vec3 off=(t1*cos(a)+t2*sin(a))*r*0.02;
    lit+= (dist-bias > texture(uShadow, vec4(d+off,float(slot))).r) ? 0.0 : 1.0;
  } return lit/8.0; }

void main(){
  vec3 P=vPos; vec3 Ng=normalize(vN); vec3 V=normalize(uCam-P);
  vec3 rd=P-uCam; float dP=length(rd); vec3 vd=rd/dP;
  if(dot(Ng,V)<0.0) Ng=-Ng;
  float bfv=clamp(vBiome,0.0,6.0); int bi0=int(floor(bfv)); int bi1=min(bi0+1,6); float bt=fract(bfv); float hubW=smoothstep(5.5,6.0,bfv);
  vec3 tint=mix(uTint[bi0],uTint[bi1],bt); vec3 ambB=mix(uAmbB[bi0],uAmbB[bi1],bt); float wetB=mix(uWetB[bi0],uWetB[bi1],bt); float mossB=mix(uMossB[bi0],uMossB[bi1],bt);

  // ---------- eau : reflets spéculaires des torches, absorption
  if(vMat>3.5 && vMat<4.5){
    vec2 rg=ripples(P.xz,uTime)*0.045+vec2(vn(vec3(P.xz*7.0,uTime*0.35))-0.5,vn(vec3(P.zx*7.0+7.0,uTime*0.35))-0.5)*0.012;
    vec3 Nw=normalize(vec3(rg.x,1.0,rg.y));
    float ndv=max(dot(Nw,V),0.0); float fres=0.03+0.97*pow(1.0-ndv,5.0);
    vec3 refl=vec3(0.0035,0.0035,0.004)*fres*3.0;
    for(int i=0;i<uNL;i++){
      vec3 Lv=uLPos[i]-P; float d2=dot(Lv,Lv); if(d2>=uLR*uLR) continue;
      float d=sqrt(d2); vec3 L=Lv/d; float win=1.0-d2/(uLR*uLR); win*=win;
      vec3 H=normalize(L+V); float nh=max(dot(Nw,H),0.0);
      float s=pow(nh,1400.0)*22.0+pow(nh,70.0)*0.7;
      vec3 c=uLCol[i]*(win/(0.25+d2*0.26))*s*(0.25+fres);
      if(uLSh[i]>=0 && s>0.01) c*=shadowAt(uLSh[i],P,vec3(0,1,0),uLPos[i]);
      refl+=c;
    }
    float edge=1.0-smoothstep(0.65,1.0,vUV.x);
    fragColor=vec4(refl*edge*exp(-dP*0.012), 0.66*edge);
    return;
  }

  if(vMat>9.5 && vMat<10.5){                    // lave : émissive, croûte sombre
    float fl=vn(vec3(P.x*1.3,uTime*0.04,P.z*1.3))*0.6+vn(vec3(P.x*4.0,0.0,P.z*4.0+uTime*0.05))*0.4;
    float crack=smoothstep(0.38,0.62,fl);
    vec3 lav=mix(vec3(0.04,0.006,0.0),vec3(1.0,0.26,0.04),crack)*(0.9+0.2*vn(vec3(P.xz*9.0,uTime*0.1)));
    fragColor=vec4(lav*exp(-dP*0.012),1.0); return;
  }
  vec3 albedo=vec3(0.5), N=Ng, emis=vec3(0.0); float gloss=0.2, shin=16.0, alphaOut=1.0; bool thin=false;
  float flick=0.8+0.2*vn(vec3(uTime*9.0,vSeed,1.0));

  if(vMat<0.5){
    vec3 w=pow(abs(Ng),vec3(6.0)); w/=dot(w,vec3(1.0));
    vec4 c0=tri(uRockCol,P,w,0.27), c1=tri(uRockCol,P,w,1.1); float farK=smoothstep(25.0,110.0,dP);
    float fb=smoothstep(26.0,80.0,dP)*hubW;       // montagne vue de loin : motifs 6 fois plus grands (plus de motif d'écailles répétitif)
    if(fb>0.001){ c0=mix(c0,tri(uRockCol,P,w,0.045),fb); c1=mix(c1,tri(uRockCol,P,w,0.18),fb); }
    albedo=c0.rgb*(0.55+0.9*mix(c1.rgb,vec3(0.5),farK*0.5)); gloss=c0.a*(1.0-0.85*farK);
    { vec3 fn=normalize(cross(dFdx(P),dFdy(P))); if(dot(fn,Ng)<0.0) fn=-fn; Ng=normalize(mix(Ng,fn,0.35)); w=pow(abs(Ng),vec3(6.0)); w/=dot(w,vec3(1.0)); }
    float strata=vn(vec3(P.x*0.08,P.y*4.5,P.z*0.08));
    albedo*=mix(0.75,1.2,strata); albedo*=tint; albedo*=0.82+0.36*vn(P*0.045+vec3(3.0,8.0,1.0));
    if(hubW>0.01){                                // montagne du camp : herbe et mousse au pied, neige sur les hauteurs
      float hy=P.y-uHubG, nh=vn(P*0.35)-0.5;
      float gr=(1.0-smoothstep(1.5,9.0,hy+nh*6.0))*smoothstep(0.25,0.65,Ng.y);
      albedo=mix(albedo,vec3(0.10,0.22,0.06)*(0.7+0.6*vn(P*3.0)),gr*hubW*0.85);
      float sn=smoothstep(38.0,62.0,hy+nh*14.0)*smoothstep(0.32,0.62,Ng.y);
      albedo=mix(albedo,vec3(0.78,0.80,0.92),sn*hubW*0.9); gloss*=1.0-0.6*sn*hubW;
    }
    float mossM=mossB*smoothstep(0.42,0.62,vn(P*0.8+vec3(5.0,2.0,9.0)))*(0.35+0.65*smoothstep(-0.3,0.9,Ng.y));
    albedo=mix(albedo,vec3(0.06,0.14,0.05)*(0.6+0.8*vn(P*6.0)),mossM*0.85);
    vec3 n0=triN(uRockNrm,P,Ng,w,0.27), n1=triN(uRockNrm,P,n0,w,1.1);
    if(fb>0.001){ vec3 m0=triN(uRockNrm,P,Ng,w,0.045), m1=triN(uRockNrm,P,m0,w,0.18); n0=normalize(mix(n0,m0,fb)); n1=normalize(mix(n1,m1,fb)); }
    N=normalize(mix(n0,n1,0.55)); N=normalize(mix(N,Ng,smoothstep(35.0,150.0,dP)*0.8));
    float fl=smoothstep(0.6,0.85,Ng.y);
    albedo=mix(albedo,albedo*vec3(1.0,0.92,0.8)*0.85,fl);
    if(vMat>0.3){ float bs=h31(vec3(vSeed*3.1,1.7,2.3)); albedo*=vec3(1.18,1.08,0.95)*(0.8+0.5*bs); gloss*=0.7; }
    float wm=vn(P*0.21+vec3(3.0,1.0,7.0))*0.65+vn(P*0.7)*0.35;
    float wthr=0.66-0.32*(wetB-0.5); float wet=smoothstep(wthr-0.08,wthr+0.08,wm);
    albedo*=mix(1.0,0.5,wet);
    shin=mix(9.0,80.0,wet); gloss=mix(gloss*0.18,0.8,wet);
    if(vMat>0.1 && vMat<0.3){                     // couloir des catacombes : pierre lisse, sombre, coulures verticales luisantes
      float kc=clamp(vSeed,0.0,1.0), streak=smoothstep(0.5,0.9,vn(vec3(P.x*6.0,P.y*0.35,P.z*6.0)));
      vec3 cAlb=vec3(0.085,0.09,0.088)*(0.8+0.35*vn(P*vec3(0.5,0.05,0.5)))*(1.0-0.45*streak);
      albedo=mix(albedo,cAlb,kc); gloss=mix(gloss,0.45+0.5*streak,kc); shin=mix(shin,70.0,kc); N=normalize(mix(N,Ng,kc*0.85));
    }
  } else if(vMat<2.5){
    vec2 uv=vUV*vec2(0.8,1.9);
    vec4 wc=texture(uWoodCol,uv); vec3 tn=texture(uWoodNrm,uv).xyz*2.0-1.0;
    vec3 dp1=dFdx(P), dp2=dFdy(P); vec2 du1=dFdx(uv), du2=dFdy(uv);
    vec3 d2p=cross(dp2,Ng), d1p=cross(Ng,dp1);
    vec3 T=d2p*du1.x+d1p*du2.x, B=d2p*du1.y+d1p*du2.y;
    float im=inversesqrt(max(max(dot(T,T),dot(B,B)),1e-12));
    N=normalize(mat3(T*im,B*im,Ng)*tn);
    albedo=wc.rgb*mix(1.0,0.75,vn(P*5.0)); gloss=wc.a; shin=22.0;
    if(vMat>1.5){ float g=smoothstep(0.1,0.9,vn(P*30.0+uTime*0.5)); albedo*=0.05; gloss=0.02; emis=vec3(1.0,0.3,0.06)*g*0.35*flick; }
  } else if(vMat<3.5){                       // toile d'araignée
    vec2 p=vUV; float r=length(p); float ang=atan(p.y,p.x)/1.5707963; float fa=ang*5.0;
    float aa=fwidth(r)*0.8+1e-4, wuv=0.0045/max(vSeed,0.2);
    float sp=abs(fract(fa+0.5)-0.5)/5.0*1.5707963*r;
    float rr=r+0.045*sin(fract(fa)*3.1416);
    float dr=abs(fract(rr*9.0+0.5)-0.5)/9.0;
    float spokes=(1.0-smoothstep(wuv,wuv+aa,sp))*step(0.02,r)*step(vn(vec3(p*4.0+3.0,vSeed)),0.9);
    float rings=(1.0-smoothstep(wuv*0.8,wuv*0.8+aa,dr))*step(0.1,r)*step(vn(vec3(p*7.0,vSeed*3.0)),0.72);
    float cov=max(spokes,rings)*(1.0-smoothstep(0.88,1.0,p.x+p.y));
    if(cov<0.02) discard;
    albedo=vec3(0.8,0.8,0.82); gloss=1.0; shin=60.0; alphaOut=cov; thin=true;
  } else if(vMat<5.5){                       // acier
    albedo=vec3(0.22,0.23,0.26)*(0.8+0.4*vn(P*40.0)); gloss=0.3; shin=60.0;
  } else if(vMat<6.5){                       // fourrure
    float tail=step(0.5,vSeed)*step(vSeed,1.5);
    albedo=mix(vec3(0.20,0.16,0.13)*(0.7+0.6*vn(P*70.0)), vec3(0.34,0.2,0.18), tail+step(0.5,vSeed)*step(vSeed,0.8));
    gloss=0.25; shin=28.0;
  } else if(vMat<7.5){                       // intérieur d'un trou
    albedo=vec3(0.0); gloss=0.0; shin=8.0;
  } else if(vMat<8.5){ albedo=vec3(0.0); emis=vec3(0.9,0.22,0.1)*0.7; gloss=1.0; shin=200.0;
  } else if(vMat<9.5){                       // cristal luminescent
    vec3 cc=mix(vec3(0.65,0.2,1.0),vec3(0.2,0.7,1.0),step(0.5,vSeed));
    crystalShade(cc*1.3,P,V,dP,albedo,emis,N,gloss,shin);
  } else if(vMat<11.5){                      // os
    albedo=vec3(0.62,0.58,0.47)*(0.75+0.35*vn(P*25.0)); gloss=0.25; shin=26.0;
  } else if(vMat<12.5){                      // champignon luminescent
    if(vSeed>0.5){ albedo=vec3(0.02,0.08,0.1); emis=vec3(0.1,0.9,1.0)*(0.9+0.4*vn(P*20.0)); gloss=0.5; shin=40.0; } else { albedo=vec3(0.55,0.6,0.58); gloss=0.2; shin=20.0; }
  } else if(vMat<13.5){                      // verre de lanterne
    albedo=vec3(0.0); emis=vec3(1.0,0.78,0.5)*2.4; gloss=0.0;
  } else if(vMat<14.5){                      // étoffe sombre
    albedo=vec3(0.07,0.06,0.06)*(0.6+0.8*vn(P*18.0)); gloss=0.08; shin=10.0;
  } else if(vMat<15.5){                      // chitine
    albedo=vec3(0.035,0.03,0.03); gloss=1.0; shin=90.0;
  } else if(vMat<16.5){                      // golem : roche et veines de lave
    float cr=smoothstep(0.52,0.62,vn(P*5.0)); albedo=vec3(0.1,0.085,0.075); emis=vec3(1.2,0.28,0.04)*cr; gloss=0.2; shin=20.0;
  } else if(vMat<17.5){                      // ciel de la sortie
    albedo=vec3(0.0); emis=vec3(0.85,0.95,1.2)*1.5; gloss=0.0;
  } else if(vMat<18.5){                      // or
    albedo=vec3(0.9,0.62,0.12); emis=vec3(0.22,0.14,0.02); gloss=1.5; shin=60.0;
  } else if(vMat<19.5){                      // fiole
    albedo=vec3(0.1,0.0,0.0); emis=vec3(1.0,0.08,0.1)*1.2; gloss=1.0; shin=80.0;
  } else if(vMat>89.5 && vMat<95.0){        // « la Dame » : peau, robe, cheveux, yeux, orbites
    vec3 L=vLocal;
    if(uGhost>0.0 && vn(L*13.0)*0.8+vn(L*41.0)*0.2 < uGhost*1.15-0.08) discard;
    if(vMat<90.5){                               // peau marbrée, veines violacées, crasse, plaies, pores
      float mott=vn(L*7.0), vein=(1.0-smoothstep(0.0,0.07,abs(vn(L*vec3(11.0,5.0,11.0))*2.0-1.0)))*smoothstep(0.35,0.7,vn(L*3.0+5.0));
      float vein2=1.0-smoothstep(0.0,0.05,abs(vn(L*vec3(23.0,9.0,23.0)+3.0)*2.0-1.0));
      albedo=vec3(0.50,0.52,0.56)*(0.72+0.4*mott);
      albedo=mix(albedo,vec3(0.17,0.08,0.20),clamp(vein*0.75+vein2*0.35,0.0,1.0));
      albedo=mix(albedo,vec3(0.24,0.22,0.19),smoothstep(0.55,0.85,vn(L*2.4+9.0))*0.55);
      albedo=mix(albedo,vec3(0.30,0.04,0.04),smoothstep(0.78,0.9,vn(L*4.0+21.0))*0.6);
      vec3 bump=vec3(vn(L*95.0),vn(L*95.0+3.1),vn(L*95.0+7.7))-0.5; N=normalize(N+bump*0.32*(1.0-smoothstep(4.0,12.0,dP)));
      gloss=0.4; shin=45.0;
    } else if(vMat<91.5){                        // robe : toile sale, déchirée, trouée, tachée
      float hem=vUV.y, tear=0.04+0.24*vn(vec3(vUV.x*11.0,1.0,2.0))+0.12*vn(vec3(vUV.x*37.0,3.0,1.0));
      if(hem<tear) discard;
      if(hem<1.5 && vn(L*vec3(8.0,5.0,8.0)+11.0)>0.8) discard;
      float weave=mix(1.0,0.5+0.5*sin(vUV.x*1400.0)*sin(hem*700.0),1.0-smoothstep(2.0,8.0,dP));
      albedo=vec3(0.66,0.64,0.58)*(0.78+0.3*vn(L*vec3(26.0,3.0,26.0)))*(0.9+0.1*weave);
      float dirt=smoothstep(0.5,0.0,hem-tear)*0.75+smoothstep(0.55,0.78,vn(L*3.0))*0.5;
      albedo=mix(albedo,vec3(0.16,0.12,0.09),clamp(dirt,0.0,0.85));
      albedo=mix(albedo,vec3(0.22,0.02,0.02),smoothstep(0.72,0.8,vn(L*4.5+9.0))*0.85);
      gloss=0.05; shin=8.0; thin=true;
    } else if(vMat<92.5){ albedo=vec3(0.010,0.009,0.011); gloss=0.7; shin=70.0;
    } else if(vMat<93.5){ albedo=vec3(0.0); emis=vec3(5.0,0.22,0.1)*(0.85+0.15*sin(uTime*3.3)); gloss=0.0;
    } else { albedo=vec3(0.008,0.0,0.0); emis=vec3(0.06,0.0,0.0); gloss=0.3; shin=30.0; }
  } else if(vMat>119.5){                     // bois calciné : noir, quelques braises qui s'éteignent (vSeed = 20 + intensité*10)
    float amount=clamp((vSeed-20.0)/10.0,0.0,1.0); float g=smoothstep(0.58,0.9,vn(P*22.0+vec3(0.0,uTime*0.25,0.0)));
    albedo=vec3(0.028,0.022,0.018)*(0.6+0.8*vn(P*25.0)); gloss=0.05; shin=8.0; emis=vec3(1.0,0.26,0.05)*g*amount*0.9*flick;
  } else if(vMat>99.5){                      // herbe du camp de base, chemin de terre
    vec3 g1=vec3(0.10,0.22,0.055), g2=vec3(0.23,0.36,0.10), g3=vec3(0.33,0.30,0.12), dirt=vec3(0.30,0.21,0.13);
    float n1=vn(vec3(P.xz*0.32,1.0)), n2=vn(vec3(P.xz*2.1,3.0)), n3=vn(vec3(P.xz*8.0,5.0));
    albedo=mix(mix(g1,g2,n1),g3,smoothstep(0.62,0.9,vn(vec3(P.xz*0.11,9.0)))*0.6)*(0.78+0.4*n2)*(0.86+0.28*n3);
    float pathX=3.2*sin(P.z*0.05)+1.2*sin(P.z*0.13);
    float path=1.0-smoothstep(1.3,2.7,abs(P.x-pathX)); path*=smoothstep(2.0,6.0,P.z-uHubC.y+60.0);
    float plaza=1.0-smoothstep(11.0,19.0,length(P.xz-uHubC));
    float dirtK=max(path*0.9,plaza)*(0.75+0.25*n2);
    albedo=mix(albedo,dirt*(0.8+0.5*n3),dirtK);
    N=normalize(Ng+vec3(n2-0.5,0.0,n3-0.5)*0.35); gloss=0.04; shin=6.0;
  } else if(vMat>20.5){                      // matériau de modèle OBJ (couleur / émission du .mtl)
    int mi=clamp(int(vMat-20.5),0,47); albedo=uMdlA[mi].rgb; shin=max(uMdlA[mi].a,4.0); emis=uMdlE[mi].rgb*0.6; gloss=uMdlE[mi].a;
    if(uMdlE[mi].a<-0.5) crystalShade(uMdlE[mi].rgb*0.8,P,V,dP,albedo,emis,N,gloss,shin);
  } else { albedo=vec3(0.0); emis=vec3(1.0,0.82,0.2)*2.2; gloss=0.0; }

  vec3 diffA=vec3(0), specA=vec3(0), indir=vec3(0), scat=vec3(0);
  for(int i=0;i<uNL;i++){
    vec3 lp=uLPos[i], lc=uLCol[i];
    vec3 oc=lp-uCam; float t0=dot(oc,vd); float a=sqrt(max(dot(oc,oc)-t0*t0,0.05));
    scat+=lc*((atan((dP-t0)/a)-atan(-t0/a))/a);
    vec3 Lv=lp-P; float d2=dot(Lv,Lv);
    if(d2>=uLR*uLR) continue;
    float d=sqrt(d2); vec3 L=Lv/d;
    float win=1.0-d2/(uLR*uLR); win*=win;
    vec3 c=lc*(win/(0.25+d2*0.26));
    indir+=c*0.025;
    float ng=dot(Ng,L); if(thin) ng=abs(ng);
    if(ng<=0.0) continue;
    float sh=1.0; if(uLSh[i]>=0) sh=shadowAt(uLSh[i],P,Ng,lp);
    if(sh<=0.0) continue;
    float ndl=dot(N,L); if(thin) ndl=abs(ndl); ndl=max(ndl,0.0);
    diffA+=c*ndl*sh;
    vec3 H=normalize(L+V); float nh=max(dot(N,H),0.0);
    float fres=0.04+0.96*pow(1.0-max(dot(H,V),0.0),5.0);
    specA+=c*sh*ndl*pow(nh,shin)*(shin+8.0)/(8.0*PI)*mix(0.15,1.0,fres)*gloss;
  }
  vec3 amb=ambB*(0.4+0.6*Ng.y);
  if(hubW>0.0){                               // soleil couchant (sans ombre portée) : lumière directionnelle chaude
    float sn=max(dot(N,uSunDir),0.0), wrap=max(dot(N,uSunDir)*0.5+0.5,0.0);
    diffA+=uSunCol*(sn*0.85+wrap*0.15)*hubW;
    vec3 Hs=normalize(uSunDir+V); specA+=uSunCol*pow(max(dot(N,Hs),0.0),shin)*(shin+8.0)/(8.0*PI)*gloss*0.35*sn*hubW;
  }
  vec3 col=albedo*(diffA+indir+amb)+specA+emis;
  float fogC=exp(-dP*0.012), fogH=exp(-dP*0.0026);
  col=mix(col*fogC+scat*0.0016, mix(uHaze,col,fogH)+scat*0.0011, hubW);
  fragColor=vec4(col,alphaOut);
}
)";

static const char* SHADOW_VS = R"(#version 430 core
layout(location=0) in vec3 aPos; uniform mat4 uVP; out vec3 vWP;
void main(){ vWP=aPos; gl_Position=uVP*vec4(aPos,1.0); }
)";
static const char* SHADOW_FS = R"(#version 430 core
in vec3 vWP; uniform vec3 uLP; out float outD;
void main(){ outD=length(vWP-uLP); }
)";

static const char* DUST_VS = R"(#version 430 core
uniform mat4 uVP; uniform vec3 uCam; uniform float uTime; uniform float uPx;
const int MAXL=32; uniform int uNL; uniform vec3 uLPos[MAXL]; uniform vec3 uLCol[MAXL]; uniform float uLR;
out float vB;
float h(float n){ return fract(sin(n*127.1)*43758.5453); }
void main(){
  float f=float(gl_VertexID);
  vec3 base=(vec3(h(f),h(f+11.3),h(f+23.7))*2.0-1.0)*20.0;
  vec3 vel=vec3(h(f+3.1)-0.5,(h(f+5.5)-0.5)*0.4-0.03,h(f+8.8)-0.5)*0.15;
  vec3 turb=vec3(sin(uTime*0.3+f),sin(uTime*0.25+f*1.7),cos(uTime*0.2+f*0.7))*0.35;
  vec3 rel=mod(base+vel*uTime+turb-uCam+20.0,40.0)-20.0;
  vec3 pos=uCam+rel;
  float s=0.0;
  for(int i=0;i<uNL;i++){ vec3 L=uLPos[i]-pos; float d2=dot(L,L); if(d2>=uLR*uLR) continue;
    float w=1.0-d2/(uLR*uLR); s+=w*w/(0.25+d2*0.18)*uLCol[i].r; }
  float tw=0.5+0.5*sin(uTime*(0.8+h(f+1.7))+f*5.0);
  vB=s*0.5*tw*(1.0-smoothstep(12.0,20.0,length(rel)));
  gl_Position=uVP*vec4(pos,1.0);
  gl_PointSize=clamp(uPx*0.012/max(gl_Position.w,0.05),1.0,4.0);
}
)";
static const char* DUST_FS = R"(#version 430 core
in float vB; out vec4 fc;
void main(){ float r=length(gl_PointCoord*2.0-1.0); if(r>1.0) discard; fc=vec4(vec3(1.0,0.72,0.45)*vB*(1.0-r),1.0); }
)";

static const char* PART_VS = R"(#version 430 core
layout(location=0) in vec4 aPS; layout(location=1) in vec4 aK;
uniform mat4 uVP; uniform float uPx; out vec4 vK;
void main(){ gl_Position=uVP*vec4(aPS.xyz,1.0); gl_PointSize=clamp(aPS.w*uPx/max(gl_Position.w,0.05),1.0,500.0); vK=aK; }
)";
static const char* PART_FS = R"(#version 430 core
in vec4 vK; uniform float uTime; uniform vec3 uFlame[13]; out vec4 fc;
const float PI_=3.14159265;
float h21(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float vn2(vec2 x){ vec2 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(h21(i),h21(i+vec2(1,0)),f.x),mix(h21(i+vec2(0,1)),h21(i+vec2(1,1)),f.x),f.y); }
void main(){
  float kind=vK.x, seed=vK.y, life=vK.z, inten=vK.w;
  vec2 q=vec2(gl_PointCoord.x*2.0-1.0, 1.0-gl_PointCoord.y*2.0);
  if(kind<0.5){
    float t=clamp((q.y+0.72)/1.6,0.0,1.0);
    float sway=(vn2(vec2(q.y*2.2-uTime*2.0+seed*10.0,seed*7.0))-0.5)*0.55*t + 0.1*sin(uTime*3.0+seed*20.0)*t;
    float width=0.40*sin(PI_*pow(t,0.45))*(1.0-0.25*t);
    float dx=abs(q.x-sway);
    float body=1.0-smoothstep(width*0.5,width,dx);
    float core=1.0-smoothstep(0.0,width*0.5,dx);
    body*=0.75+0.5*vn2(q*3.5+vec2(0.0,-uTime*3.0+seed*5.0));
    body*=step(0.001,width);
    int bi=clamp(int(floor(life+0.5)),0,12); vec3 mid=uFlame[bi]; vec3 hot=mix(mid,vec3(1.0),0.6); vec3 cool=mid*vec3(0.55,0.3,0.25);
    vec3 col=mix(mix(hot,mid,smoothstep(0.0,0.5,t)),cool,smoothstep(0.45,1.0,t));
    col=mix(col,hot*1.5,core*(1.0-t));
    fc=vec4(col*body*inten*1.7,1.0);
  } else if(kind<1.5){
    float r2=dot(q,q); if(r2>1.0) discard;
    int bi=clamp(int(floor(life+0.5)),0,12); fc=vec4(uFlame[bi]*exp(-r2*7.0)*inten*0.4,1.0);
  } else if(kind<2.5){
    float r=length(q); if(r>1.0) discard;
    float a=smoothstep(1.0,0.15,r);
    fc=vec4(mix(uFlame[clamp(int(inten+0.5),0,12)],vec3(1.0),0.25)*a*life*(0.6+0.8*h21(vec2(seed,floor(uTime*12.0))))*4.0,1.0);
  } else {
    float r=length(q); if(r>1.0) discard;
    float n=0.6+0.8*vn2(q*3.0+seed*7.0); float a=pow(1.0-r,2.0)*n;
    if(kind>5.5){ fc=vec4(vec3(0.34,0.33,0.33)*a*life*inten*1.3,1.0); }
    else if(kind>3.5){ vec2 qq=q*vec2(1.7,0.55); fc=vec4(vec3(0.7,0.85,1.0)*exp(-dot(qq,qq)*3.0)*inten,1.0); } else fc=vec4(vec3(0.62,0.5,0.38)*a*life*inten,1.0);
  }
}
)";

static const char* FS_QUAD_VS = R"(#version 430 core
out vec2 uv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.0-1.0,0.0,1.0); }
)";
static const char* BLOOM_DOWN_FS = R"(#version 430 core
in vec2 uv; out vec4 fc; uniform sampler2D uTex; uniform vec2 uTexel; uniform int uFirst;
vec3 T(float x,float y){ return texture(uTex,uv+vec2(x,y)*uTexel).rgb; }
void main(){
  vec3 A=T(-2,-2),B=T(0,-2),C=T(2,-2),D=T(-1,-1),E=T(1,-1),F=T(-2,0),G=T(0,0),H=T(2,0),I=T(-1,1),J=T(1,1),K=T(-2,2),L=T(0,2),M=T(2,2);
  vec3 c=(D+E+I+J)*0.125+(A+B+F+G)*0.03125+(B+C+G+H)*0.03125+(F+G+K+L)*0.03125+(G+H+L+M)*0.03125;
  if(uFirst==1){ float br=max(c.r,max(c.g,c.b)); c*=max(br-0.7,0.0)/max(br,1e-4); c=min(c,vec3(30.0)); }
  fc=vec4(c,1.0); }
)";
static const char* BLOOM_UP_FS = R"(#version 430 core
in vec2 uv; out vec4 fc; uniform sampler2D uTex; uniform vec2 uTexel;
vec3 T(float x,float y){ return texture(uTex,uv+vec2(x,y)*uTexel).rgb; }
void main(){
  vec3 c=T(-1,-1)+T(0,-1)*2.0+T(1,-1)+T(-1,0)*2.0+T(0,0)*4.0+T(1,0)*2.0+T(-1,1)+T(0,1)*2.0+T(1,1);
  fc=vec4(c/16.0,1.0); }
)";
static const char* COMPOSE_FS = R"(#version 430 core
in vec2 uv; out vec4 fc; uniform sampler2D uHdr, uBloom; uniform float uTime, uAspect, uBloomK;
float hash(vec2 p){ return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453); }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){
  vec3 c=texture(uHdr,uv).rgb+texture(uBloom,uv).rgb*uBloomK;
  vec2 d=(uv-0.5)*vec2(uAspect,1.0); c*=mix(1.0,smoothstep(1.15,0.25,length(d)),0.85);
  c=aces(c);
  float l=dot(c,vec3(0.299,0.587,0.114));
  c=mix(c,c*vec3(0.88,0.98,1.12),(1.0-smoothstep(0.0,0.5,l))*0.6);
  c=pow(c,vec3(1.0/2.2));
  c+=(hash(uv*vec2(1920.0,1080.0)+fract(uTime))-0.5)*0.02;
  fc=vec4(c,1.0); }
)";

static const char* SKY_FS = R"(#version 430 core
in vec2 uv; out vec4 fc;
uniform mat4 uInvVP; uniform vec3 uCam; uniform vec3 uSunDir; uniform float uAmt; uniform float uTime;
float h21(vec2 p){ p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y); }
float vn2(vec2 x){ vec2 i=floor(x), f=fract(x); f=f*f*(3.0-2.0*f);
  return mix(mix(h21(i),h21(i+vec2(1,0)),f.x),mix(h21(i+vec2(0,1)),h21(i+vec2(1,1)),f.x),f.y); }
float fbm(vec2 p){ float a=0.5,s=0.0; for(int i=0;i<5;i++){ s+=a*vn2(p); p=p*2.03+vec2(1.7,9.2); a*=0.5; } return s; }
void main(){
  vec2 nd=uv*2.0-1.0; vec4 w=uInvVP*vec4(nd,1.0,1.0); vec3 dir=normalize(w.xyz/w.w-uCam); float y=dir.y; vec3 sun=normalize(uSunDir);
  vec3 hor=vec3(0.80,0.30,0.22), mid=vec3(0.26,0.11,0.30), zen=vec3(0.010,0.018,0.085);
  float t=clamp(y,0.0,1.0);
  vec3 col=mix(hor,mid,smoothstep(0.0,0.26,t)); col=mix(col,zen,smoothstep(0.16,0.9,t));
  float sd=max(dot(dir,sun),0.0);
  col+=vec3(1.0,0.55,0.25)*pow(sd,5.0)*0.45+vec3(1.0,0.72,0.42)*pow(sd,48.0)*1.1+vec3(1.0,0.92,0.75)*smoothstep(0.9992,0.9998,sd)*14.0;
  if(y>0.02){                                   // étoiles qui scintillent doucement
    vec2 sp=dir.xz/(y+0.25)*70.0, ci=floor(sp), cf=fract(sp)-0.5; float hh=h21(ci);
    float stl=step(0.962,hh)*smoothstep(0.30,0.0,length(cf-(vec2(h21(ci+7.0),h21(ci+13.0))-0.5)*0.6));
    stl*=0.55+0.45*sin(uTime*(0.8+hh*3.0)+hh*60.0); col+=vec3(0.8,0.86,1.0)*stl*1.4*smoothstep(0.05,0.45,y);
  }
  if(y>0.0){
    vec2 cp=dir.xz/(y+0.18)*0.8+vec2(uTime*0.006,0.0); float cl=fbm(cp*1.5), cl2=fbm(cp*1.5+vec2(0.12,0.05)*3.0);
    float cov=smoothstep(0.50,0.80,cl); float edge=clamp((cl-cl2)*6.0+0.5,0.0,1.0);
    vec3 cc=mix(vec3(0.34,0.20,0.30),vec3(1.35,0.66,0.36),edge*(0.35+0.65*pow(sd,2.0)));
    col=mix(col,cc,cov*0.88*smoothstep(0.0,0.14,y));
  } else col=mix(hor*0.72,vec3(0.20,0.12,0.11),smoothstep(0.0,-0.3,y));
  fc=vec4(col*uAmt,1.0);
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[4096]; glGetShaderInfoLog(s, 4096, nullptr, log); std::fprintf(stderr, "Erreur shader:\n%s\n", log); std::exit(1); }
    return s;
}
static GLuint program(const char* vs, const char* fs) {
    GLuint p = glCreateProgram(); GLuint a = compile(GL_VERTEX_SHADER, vs), b = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, a); glAttachShader(p, b); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[4096]; glGetProgramInfoLog(p, 4096, nullptr, log); std::fprintf(stderr, "Erreur link:\n%s\n", log); std::exit(1); }
    glDeleteShader(a); glDeleteShader(b); return p;
}
static GLint U(GLuint p, const char* n) { return glGetUniformLocation(p, n); }

// ----------------------------------------------------------------------------
//  PNG (sans compression) pour les captures
// ----------------------------------------------------------------------------
static void writePNG(const char* path, const std::vector<uint8_t>& rgb, int w, int h) {
    static uint32_t tab[256]; static bool init = false;
    if (!init) { for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320U ^ (c >> 1) : c >> 1; tab[n] = c; } init = true; }
    auto crc = [&](const uint8_t* d, size_t n, uint32_t c = 0xFFFFFFFFU) { for (size_t i = 0; i < n; i++) c = tab[(c ^ d[i]) & 0xFF] ^ (c >> 8); return c; };
    std::vector<uint8_t> raw; raw.reserve((size_t)(w * 3 + 1) * h);
    for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3); }
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t o = 0; o < raw.size(); o += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - o); bool last = o + n >= raw.size();
        z.push_back(last ? 1 : 0); z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + o, raw.begin() + o + n);
    }
    uint32_t a = 1, b = 0; for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    uint32_t ad = (b << 16) | a; for (int i = 3; i >= 0; i--) z.push_back((ad >> (8 * i)) & 255);
    FILE* f = std::fopen(path, "wb"); if (!f) return;
    auto be32 = [&](uint32_t v) { uint8_t b4[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v}; std::fwrite(b4, 1, 4, f); };
    auto chunk = [&](const char* type, const std::vector<uint8_t>& d) {
        be32((uint32_t)d.size()); std::fwrite(type, 1, 4, f); if (!d.empty()) std::fwrite(d.data(), 1, d.size(), f);
        std::vector<uint8_t> t(type, type + 4); t.insert(t.end(), d.begin(), d.end()); be32(~crc(t.data(), t.size()));
    };
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr(13, 0);
    for (int i = 0; i < 4; i++) { ihdr[i] = (w >> (24 - 8 * i)) & 255; ihdr[4 + i] = (h >> (24 - 8 * i)) & 255; }
    ihdr[8] = 8; ihdr[9] = 2; chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", {}); std::fclose(f);
}

// ----------------------------------------------------------------------------
//  Cibles de rendu
// ----------------------------------------------------------------------------
static const int BL = 6;
struct Targets {
    int w = 0, h = 0;
    GLuint msFbo = 0, msColor = 0, msDepth = 0, resFbo = 0, resTex = 0;
    GLuint blTex[BL] = {}, blFbo[BL] = {}; int blW[BL] = {}, blH[BL] = {};
    void destroy() {
        if (!msFbo) return;
        glDeleteFramebuffers(1, &msFbo); glDeleteRenderbuffers(1, &msColor); glDeleteRenderbuffers(1, &msDepth);
        glDeleteFramebuffers(1, &resFbo); glDeleteTextures(1, &resTex);
        glDeleteFramebuffers(BL, blFbo); glDeleteTextures(BL, blTex); msFbo = 0;
    }
    void create(int W, int H) {
        destroy(); w = W; h = H;
        glGenFramebuffers(1, &msFbo); glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        glGenRenderbuffers(1, &msColor); glBindRenderbuffer(GL_RENDERBUFFER, msColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
        glGenRenderbuffers(1, &msDepth); glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT32F, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
        auto mk = [&](GLuint& tex, GLuint& fbo, int tw, int th) {
            glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tw, th, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        };
        mk(resTex, resFbo, W, H);
        for (int i = 0; i < BL; i++) { blW[i] = std::max(1, W >> (i + 1)); blH[i] = std::max(1, H >> (i + 1)); mk(blTex[i], blFbo[i], blW[i], blH[i]); }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

// ----------------------------------------------------------------------------
//  Programme principal
// ----------------------------------------------------------------------------
struct Part { vec3 p, v; float life, maxLife, seed, size, grow, inten; int kind; };

// ----------------------------------------------------------------------------
//  Ennemis
// ----------------------------------------------------------------------------
struct EnemyStat { float speed, hp, dmg, cd, reach, aggro, radius, centerH, knock, stun, wind; };
static const EnemyStat ES[3] = {
    {2.4f, 3, 12, 1.2f, 1.7f, 13.0f, 0.55f, 0.9f, 7.5f, 0.45f, 0.42f},    // rôdeur
    {3.6f, 2, 8, 0.9f, 1.4f, 12.0f, 0.7f, 0.5f, 9.0f, 0.38f, 0.28f},      // araignée
    {1.5f, 6, 25, 1.9f, 2.5f, 14.0f, 1.0f, 1.6f, 4.0f, 0.22f, 0.72f}};    // golem de lave
static const float ATK_STRIKE = 0.14f, ATK_RECOVER = 0.5f;
struct Enemy {
    int type = 0; vec3 pos = vec3(0), face = vec3(1, 0, 0); float hp = 1, cd = 0, anim = 0, hurt = 0; bool awake = false, alive = true; std::vector<int> hints;
    vec3 vel = vec3(0);                 // repoussement (m/s), s'éteint vite
    float stun = 0, hurtLean = 0;       // sonné (ne bouge ni n'attaque), recul visuel
    float atkT = -1; bool atkHit = false;   // attaque : armement (wind) -> frappe (ATK_STRIKE) -> récupération (ATK_RECOVER)
};
struct Drop { vec3 p; float vy; int tube, ring; float age; };
static float gEnemyT = 0;               // temps de jeu (tremblement des ennemis touchés)

// Ennemi à partir d'un modèle OBJ : placement + animation par groupes (rotation autour des pivots du modèle)
static float easeOutF(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
static float easeInF(float t) { return t * t; }
static const float ENEMY_SC[3] = {0.95f, 0.6f, 0.7f};
// poses d'attaque par type : bras levés / abattus, bascule du corps, recul puis fente
struct AtkCfg { float armUp, armSlam, pitchUp, pitchSlam, back, fwd; };
static const AtkCfg AC[3] = {{-1.85f, 0.75f, -0.30f, 0.45f, 0.20f, 0.45f}, {0.0f, 0.0f, -0.62f, 0.30f, 0.12f, 0.70f}, {-2.7f, -0.4f, -0.28f, 0.32f, 0.18f, 0.35f}};

static bool buildEnemyModel(Mesh& m, const Enemy& e) {
    const ModelData& md = e.type == 0 ? gMummyMdl : e.type == 1 ? gSpiderMdl : gGolemMdl;
    if (!md.ok) return false;
    const float sc = ENEMY_SC[e.type];
    vec3 up(0, 1, 0), f = glm::normalize(vec3(e.face.x, 0, e.face.z) + vec3(1e-5f)), x = glm::cross(up, f);
    float a = e.anim, bob = e.type == 0 ? 0.03f * std::fabs(std::sin(a)) : e.type == 2 ? 0.04f * std::fabs(std::sin(a)) : 0.0f;
    // phases de l'attaque : armement 0..1, frappe 0..1, récupération 0..1
    float wu = 0, st = 0, rc = 0;
    if (e.atkT >= 0) {
        float W = ES[e.type].wind, t = e.atkT;
        if (t < W) wu = t / W; else { wu = 1; if (t < W + ATK_STRIKE) st = (t - W) / ATK_STRIKE; else { st = 1; rc = std::min(1.0f, (t - W - ATK_STRIKE) / ATK_RECOVER); } }
    }
    const AtkCfg& ac = AC[e.type]; float up_ = easeOutF(wu), sl = easeInF(st), rec = easeOutF(rc);
    auto blend = [&](float vUp, float vSlam) { float v = vUp * up_; if (st > 0) v = mixf(vUp, vSlam, sl); if (rc > 0) v = vSlam * (1.0f - rec); return v; };
    bool atk = e.atkT >= 0; float h = e.hurtLean * e.hurtLean;
    float pitch = (e.type == 0 ? 0.07f : 0.0f) + (atk ? blend(ac.pitchUp, ac.pitchSlam) : 0.0f) - 0.55f * h;
    float fwd = (atk ? blend(-ac.back, ac.fwd) : 0.0f) - 0.14f * h, lift = e.type == 1 ? 0.28f * std::sin(PI * st) * (rc > 0 ? 0.0f : 1.0f) : 0.0f;
    float armA = atk ? blend(ac.armUp, ac.armSlam) : 0.0f; float armHurt = (e.type == 2 ? 0.7f : -0.9f) * h;
    float hip = (e.type == 0 ? 0.85f : e.type == 1 ? 0.3f : 1.35f);
    vec3 sh = (h > 0.01f) ? x * (std::sin(gEnemyT * 70.0f) * 0.05f * h) : vec3(0);
    mat4 W = glm::translate(mat4(1.0f), e.pos - vec3(0, 1.65f, 0) + up * (bob + lift) + sh) * mat4(mat3(x, up, f)) * glm::translate(mat4(1.0f), vec3(0, 0, fwd))
           * glm::translate(mat4(1.0f), vec3(0, hip, 0)) * glm::rotate(mat4(1.0f), pitch, vec3(1, 0, 0)) * glm::translate(mat4(1.0f), vec3(0, -hip, 0)) * glm::scale(mat4(1.0f), vec3(sc));
    std::vector<mat4> G(md.gname.size(), mat4(1.0f));
    float walkK = atk ? 0.15f : 1.0f;
    auto swing = [&](size_t g, float ang, vec3 axis) { vec3 p = md.gpivot[g]; G[g] = glm::translate(mat4(1.0f), p) * glm::rotate(mat4(1.0f), ang, axis) * glm::translate(mat4(1.0f), -p); };
    for (size_t g = 0; g < md.gname.size(); g++) {
        const std::string& n = md.gname[g]; float side = n.find("_p") != std::string::npos ? 1.0f : -1.0f;
        if (e.type == 1 && n.compare(0, 4, "leg_") == 0) {                          // araignée : balayage + levée de chaque patte
            int k = n.back() - '0'; float ph = a * 1.4f + (float)((k + (side > 0 ? 1 : 0)) & 1) * PI;
            float raise = (k == 0 && atk) ? blend(0.9f, 0.1f) : 0.0f;               // pattes avant dressées pendant l'attaque
            vec3 p = md.gpivot[g]; G[g] = glm::translate(mat4(1.0f), vec3(0, std::max(0.0f, std::cos(ph)) * 0.12f * walkK, 0)) * glm::translate(mat4(1.0f), p)
                * glm::rotate(mat4(1.0f), side * raise, vec3(0, 0, 1)) * glm::rotate(mat4(1.0f), std::sin(ph) * 0.22f * walkK, up) * glm::translate(mat4(1.0f), -p);
        } else if (n.compare(0, 4, "leg_") == 0) swing(g, side * std::sin(a) * (e.type == 0 ? 0.5f : 0.35f) * (atk ? 0.2f : 1.0f), vec3(1, 0, 0));
        else if (n.compare(0, 4, "arm_") == 0) swing(g, -side * std::sin(a) * (e.type == 0 ? 0.1f : 0.3f) * walkK + armA + armHurt, vec3(1, 0, 0));
    }
    size_t v0 = m.v.size();
    addModelXf(m, md, W, 0.0f, vec2(0), &G);
    if (e.hurt > 0.2f) for (size_t i = v0; i < m.v.size(); i++) m.v[i].mat = gHM.flash;      // éclair au moment de l'impact
    return true;
}

static void buildEnemy(Mesh& m, const Enemy& e) {
    if (buildEnemyModel(m, e)) return;
    vec3 up(0, 1, 0); vec3 f = glm::normalize(vec3(e.face.x, 0, e.face.z) + vec3(1e-5f)), rt = glm::normalize(glm::cross(f, up));
    vec3 feet = e.pos - vec3(0, 1.65f, 0); float a = e.anim; Rng z(1);
    auto E = [&](vec3 c, vec3 rad, float mat, float seed = 0.0f) { addEllipsoid(m, feet + f * c.x + up * c.y + rt * c.z, f, up, rt, rad, mat, seed); };
    auto P = [&](vec3 c) { return feet + f * c.x + up * c.y + rt * c.z; };
    if (e.type == 0) {                                         // rôdeur : silhouette voûtée aux yeux jaunes
        for (int s = -1; s <= 1; s += 2) {
            float sw = std::sin(a + (s > 0 ? 0.0f : PI)) * 0.25f;
            E(vec3(sw * 0.5f, 0.45f, s * 0.13f), vec3(0.09f, 0.45f, 0.09f), 14);
            E(vec3(0.25f - sw * 0.3f, 1.05f, s * 0.36f), vec3(0.09f, 0.4f, 0.07f), 14);
            E(vec3(0.45f, 0.95f, s * 0.34f), vec3(0.3f, 0.05f, 0.05f), 14);
            E(vec3(0.34f, 1.67f, s * 0.07f), vec3(0.03f, 0.025f, 0.025f), 20);
        }
        E(vec3(0.08f, 1.15f, 0), vec3(0.2f, 0.42f, 0.28f), 14); E(vec3(0.22f, 1.62f, 0), vec3(0.15f, 0.17f, 0.14f), 14);
    } else if (e.type == 1) {                                  // araignée géante
        E(vec3(0, 0.55f, 0), vec3(0.34f, 0.24f, 0.28f), 15); E(vec3(0.34f, 0.5f, 0), vec3(0.17f, 0.14f, 0.15f), 15);
        for (int s = -1; s <= 1; s += 2) {
            E(vec3(0.47f, 0.56f, s * 0.05f), vec3(0.03f), 20); E(vec3(0.45f, 0.5f, s * 0.1f), vec3(0.025f), 20);
        }
        for (int k = 0; k < 8; k++) {
            float side = (k & 1) ? 1.0f : -1.0f; int idx = k >> 1; float ph = a * 1.4f + k * 1.3f;
            vec3 hip(0.2f - 0.13f * idx, 0.55f, side * 0.22f), knee(0.3f - 0.14f * idx + std::sin(ph) * 0.05f, 1.0f, side * 0.6f);
            vec3 foot(0.3f - 0.18f * idx + std::sin(ph) * 0.14f, 0.02f + std::max(0.0f, std::cos(ph)) * 0.08f, side * 1.0f);
            addBoardBetween(m, P(hip), P(knee), up, 0.022f, 0.022f, 15, z, 0, 0); addBoardBetween(m, P(knee), P(foot), up, 0.018f, 0.018f, 15, z, 0, 0);
        }
    } else {                                                   // golem de lave
        for (int s = -1; s <= 1; s += 2) {
            float sw = std::sin(a + (s > 0 ? 0.0f : PI)) * 0.3f;
            addBoardBetween(m, P(vec3(sw * 0.4f, 0.0f, s * 0.38f)), P(vec3(sw * 0.2f, 1.1f, s * 0.38f)), up, 0.22f, 0.22f, 16, z, 0, 0);
            addBoardBetween(m, P(vec3(0.1f, 2.25f, s * 0.78f)), P(vec3(0.5f - sw * 0.5f, 1.05f, s * 0.95f)), up, 0.2f, 0.2f, 16, z, 0, 0);
            E(vec3(0.4f, 2.72f, s * 0.12f), vec3(0.06f), 20);
        }
        E(vec3(0, 1.75f, 0), vec3(0.5f, 0.75f, 0.65f), 16); E(vec3(0.1f, 2.65f, 0), vec3(0.3f, 0.3f, 0.3f), 16);
    }
}

// ----------------------------------------------------------------------------
//  Ragdoll : à la mort, le corps devient un solide qui bascule et rebondit (points de contact = coins du tronc),
//  chaque membre est une masse au bout d'une rotule (pendule de Verlet) qui pend et traîne au sol.
// ----------------------------------------------------------------------------
struct RagTpl { bool ok = false; vec3 cM = vec3(0), bmin = vec3(0), bmax = vec3(0); std::vector<vec3> pivot, cent; std::vector<float> len; std::vector<vec3> pts; };
static RagTpl gRagTpl[3];
static const ModelData& enemyModel(int type) { return type == 0 ? gMummyMdl : type == 1 ? gSpiderMdl : gGolemMdl; }
static void buildRagTpl(int type) {
    const ModelData& md = enemyModel(type); RagTpl& t = gRagTpl[type]; if (t.ok || !md.ok) return;
    size_t ng = md.gname.size(); t.pivot = md.gpivot; t.cent.assign(ng, vec3(0)); t.len.assign(ng, 0.0f); std::vector<int> cnt(ng, 0);
    t.bmin = vec3(1e9f); t.bmax = vec3(-1e9f);
    for (size_t i = 0; i < md.v.size(); i++) {
        int g = md.vg[i]; t.cent[g] += md.v[i].p; cnt[g]++;
        if (g == 0) { t.bmin = glm::min(t.bmin, md.v[i].p); t.bmax = glm::max(t.bmax, md.v[i].p); }
    }
    for (size_t g = 0; g < ng; g++) { if (cnt[g]) t.cent[g] /= (float)cnt[g]; t.len[g] = glm::length(t.cent[g] - t.pivot[g]); }
    t.cM = (t.bmin + t.bmax) * 0.5f;
    for (int k = 0; k < 8; k++) t.pts.push_back(vec3((k & 1) ? t.bmax.x : t.bmin.x, (k & 2) ? t.bmax.y : t.bmin.y, (k & 4) ? t.bmax.z : t.bmin.z));
    t.pts.push_back(vec3(t.cM.x, t.bmax.y, t.cM.z)); t.pts.push_back(vec3(t.cM.x, t.bmin.y, t.cM.z));
    t.ok = true;
}

struct Ragdoll {
    int type = 0; mat3 R = mat3(1.0f); vec3 C = vec3(0), v = vec3(0), w = vec3(0); float sc = 1, t = 0, rest = 0; bool asleep = false;
    struct Limb { int g; vec3 tip, prev; float L; vec3 restL; };
    std::vector<Limb> limbs; std::vector<int> hints;
};

static Ragdoll makeRagdoll(const Enemy& e, vec3 dir, float knock, Rng& rng) {
    Ragdoll r; r.type = e.type; buildRagTpl(e.type); const RagTpl& T = gRagTpl[e.type]; r.sc = ENEMY_SC[e.type];
    vec3 up(0, 1, 0), f = glm::normalize(vec3(e.face.x, 0, e.face.z) + vec3(1e-5f)), x = glm::cross(up, f);
    r.R = mat3(x, up, f); r.hints = e.hints;
    r.C = e.pos - vec3(0, 1.65f, 0) + r.R * (r.sc * T.cM);
    r.v = dir * (knock * 0.85f + 1.0f) + vec3(0, 1.3f, 0);                         // projeté en arrière par l'impact
    r.w = glm::cross(up, dir) * rng.r(2.4f, 3.4f);                                  // bascule vers l'arrière
    for (size_t g = 1; g < T.pivot.size(); g++) {
        Ragdoll::Limb l; l.g = (int)g; l.L = std::max(0.14f, r.sc * T.len[g]);
        vec3 d = T.cent[g] - T.pivot[g]; l.restL = glm::length(d) > 1e-4f ? glm::normalize(d) : vec3(0, -1, 0);
        l.tip = r.C + r.R * (r.sc * (T.cent[g] - T.cM)); l.prev = l.tip - r.v * 0.016f; r.limbs.push_back(l);
    }
    return r;
}

static void stepRagdoll(World& world, Ragdoll& r, float dt) {
    buildRagTpl(r.type); const RagTpl& T = gRagTpl[r.type]; if (!T.ok) return;
    r.t += dt; int sub = std::max(1, (int)std::ceil(dt / 0.012f)); float h = dt / sub; vec3 up(0, 1, 0);
    auto floorAt = [&](vec3 p) { return world.constrain(vec3(p.x, p.y, p.z), false, r.hints).y - 1.65f; };
    const float invI = 1.0f / (0.45f * std::pow(r.sc * (T.bmax.y - T.bmin.y) * 0.5f + 0.15f, 2.0f));
    for (int s = 0; s < sub; s++) {
        if (!r.asleep) {
            r.v.y -= 9.8f * h; r.C += r.v * h;
            float wl = glm::length(r.w); if (wl > 1e-4f) r.R = mat3(glm::rotate(mat4(1.0f), wl * h, r.w / wl)) * r.R;
            vec3 c0 = glm::normalize(r.R[0]), c1 = glm::normalize(r.R[1] - c0 * glm::dot(r.R[1], c0)), c2 = glm::cross(c0, c1); r.R = mat3(c0, c1, c2);
            // parois de la galerie / blocs de pierre
            { vec3 q = world.constrain(r.C, false, r.hints); vec3 d(q.x - r.C.x, 0, q.z - r.C.z);
              if (glm::length(d) > 1e-5f) { vec3 n = glm::normalize(d); r.C.x = q.x; r.C.z = q.z; float vn = glm::dot(r.v, n); if (vn < 0) r.v -= n * (vn * 1.4f); }
              vec3 pe(r.C.x, q.y, r.C.z), p0 = pe; world.collideBoards(pe, false); vec3 dd = pe - p0; dd.y = 0; if (glm::length(dd) > 1e-5f) { r.C.x = pe.x; r.C.z = pe.z; vec3 n = glm::normalize(dd); float vn = glm::dot(r.v, n); if (vn < 0) r.v -= n * (vn * 1.4f); } }
            // sol : impulsions aux points de contact (rebond amorti + frottement)
            float maxPen = 0; bool touch = false;
            for (const vec3& pl : T.pts) {
                vec3 rr = r.R * (r.sc * (pl - T.cM)), p = r.C + rr; float pen = floorAt(p) + 0.03f - p.y; if (pen <= 0) continue;
                maxPen = std::max(maxPen, pen); touch = true; vec3 vp = r.v + glm::cross(r.w, rr);
                if (vp.y < 0) {
                    vec3 rn = glm::cross(rr, up); float jn = -(1.0f + 0.22f) * vp.y / (1.0f + glm::dot(rn, rn) * invI);
                    r.v += up * jn; r.w += glm::cross(rr, up * jn) * invI;
                    vec3 vt = vp - up * vp.y; float vtl = glm::length(vt);
                    if (vtl > 1e-4f) { vec3 td = -vt / vtl; float jt = std::min(0.55f * jn, vtl / (1.0f + 0.5f * glm::dot(rn, rn) * invI)); r.v += td * jt; r.w += glm::cross(rr, td * jt) * invI; }
                }
            }
            r.C.y += maxPen * 0.85f;
            r.w *= std::max(0.0f, 1.0f - (touch ? 1.6f : 0.15f) * h); r.v.x *= std::max(0.0f, 1.0f - (touch ? 1.4f : 0.0f) * h); r.v.z *= std::max(0.0f, 1.0f - (touch ? 1.4f : 0.0f) * h);
            if (touch && glm::length(r.v) < 0.35f && glm::length(r.w) < 0.7f) r.rest += h; else r.rest = 0;
            if (r.rest > 0.5f) { r.asleep = true; r.v = vec3(0); r.w = vec3(0); }
        }
        // membres : pendules de Verlet accrochés à leur rotule
        for (auto& l : r.limbs) {
            vec3 Pw = r.C + r.R * (r.sc * (T.pivot[l.g] - T.cM));
            vec3 vel = (l.tip - l.prev) * 0.987f; l.prev = l.tip; l.tip += vel + vec3(0, -9.8f * h * h, 0);
            vec3 d = l.tip - Pw; float len = glm::length(d); if (len > 1e-4f) l.tip = Pw + d * (l.L / len);
            vec3 rw = glm::normalize(r.R * l.restL), dn = glm::normalize(l.tip - Pw); float cs = glm::dot(rw, dn);
            if (cs < -0.5f) { vec3 pr = dn - rw * cs; float pl = glm::length(pr); if (pl > 1e-4f) { dn = rw * -0.5f + (pr / pl) * 0.866f; l.tip = Pw + dn * l.L; } }
            float fl = floorAt(l.tip) + 0.07f;
            if (l.tip.y < fl) { l.tip.y = fl; l.prev.y = fl; l.prev.x += (l.tip.x - l.prev.x) * 0.45f; l.prev.z += (l.tip.z - l.prev.z) * 0.45f;
                d = l.tip - Pw; len = glm::length(d); if (len > 1e-4f) l.tip = Pw + d * (l.L / len); }
        }
    }
}

static void addRagdoll(Mesh& m, const Ragdoll& r, float k) {
    const ModelData& md = enemyModel(r.type); const RagTpl& T = gRagTpl[r.type]; if (!md.ok || !T.ok) return;
    std::vector<mat4> G(md.gname.size(), mat4(1.0f)); mat4 Rb(r.R); float sc = r.sc * k;
    G[0] = glm::translate(mat4(1.0f), r.C) * Rb * glm::scale(mat4(1.0f), vec3(sc)) * glm::translate(mat4(1.0f), -T.cM);
    for (const auto& l : r.limbs) {
        vec3 Pw = r.C + r.R * (sc * (T.pivot[l.g] - T.cM)), rw = glm::normalize(r.R * l.restL), dn = glm::normalize(l.tip - Pw);
        float c = std::min(1.0f, std::max(-1.0f, glm::dot(rw, dn))); vec3 ax = glm::cross(rw, dn); float al = glm::length(ax);
        if (al < 1e-5f) ax = glm::cross(rw, std::fabs(rw.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)); ax = glm::normalize(ax);
        mat4 Ra = glm::rotate(mat4(1.0f), std::acos(c), ax);
        G[l.g] = glm::translate(mat4(1.0f), Pw) * Ra * Rb * glm::scale(mat4(1.0f), vec3(sc)) * glm::translate(mat4(1.0f), -T.pivot[l.g]);
    }
    addModelXf(m, md, mat4(1.0f), 0.0f, vec2(0), &G);
}

// Pioche en vue subjective : manche de bois (poignée de cuir) le long de +Y, tête d'acier courbe en travers (axe X) dont les pointes retombent vers le bas
static void buildPickaxe(Mesh& m) {
    Rng r(5); vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1); const float TOP = 0.56f;
    addTubeM(m, vec3(0, -0.15f, 0), vec3(0, TOP + 0.02f, 0), 0.027f, 0.021f, 10, 1.0f, 0.3f);                 // manche
    addTubeM(m, vec3(0, -0.145f, 0), vec3(0, 0.19f, 0), 0.0305f, 0.0305f, 10, 14.0f, 0.0f);                   // poignée de cuir
    addTubeM(m, vec3(0, -0.165f, 0), vec3(0, -0.145f, 0), 0.036f, 0.036f, 10, 5.0f, 0.0f);                    // pommeau
    addTubeM(m, vec3(0, 0.19f, 0), vec3(0, 0.205f, 0), 0.035f, 0.035f, 10, 5.0f, 0.0f);                       // virole
    addTubeM(m, vec3(0, TOP - 0.085f, 0), vec3(0, TOP - 0.04f, 0), 0.033f, 0.033f, 10, 5.0f, 0.0f);          // frette sous la tête
    addBoard(m, vec3(0, TOP, 0), X, Y, Z, 0.058f, 0.06f, 0.046f, 5.0f, 0.1f, r, 0.0f);                         // œil de la tête
    for (int sgn = -1; sgn <= 1; sgn += 2) {                                                                  // deux pointes courbes
        vec3 prev(sgn * 0.052f, TOP + 0.006f, 0); float rprev = 0.043f; const int N = 9;
        for (int k = 1; k <= N; k++) {
            float f = (float)k / N, x = sgn * (0.052f + 0.31f * f), y = TOP + 0.006f - 0.14f * f * f, rad = mixf(0.043f, 0.005f, std::pow(f, 0.75f));
            vec3 p(x, y, 0); addTubeM(m, prev, p, rprev, rad, 8, 5.0f, 0.0f, k == 1 && false, k == N); prev = p; rprev = rad;
        }
    }
}

// ----------------------------------------------------------------------------
//  « La Dame » (easter egg) : grande silhouette décharnée, robe blanche déchirée, peau marbrée de veines, longs cheveux noirs,
//  yeux rouges. Maillage très détaillé construit une fois dans son repère : pieds à l'origine, regard vers +Z.
//  Matériaux : 90 peau, 91 robe, 92 cheveux, 93 yeux, 94 orbites / bouche / ongles.
// ----------------------------------------------------------------------------
static vec3 gWomanEye[2], gWomanHead;

// grille (colonnes bouclées) : normales à l'opposé de ref, uv et matériau par sommet
static void addSurf(Mesh& m, const std::vector<vec3>& P, const std::vector<vec3>& ref, const std::vector<vec2>& UV, const std::vector<float>& MT, int rows, int cols) {
    uint32_t base = (uint32_t)m.v.size(); std::vector<vec3> N(P.size(), vec3(0));
    auto id = [&](int r, int c) { return (uint32_t)(r * cols + (c % cols)); };
    for (int r = 0; r < rows - 1; r++) for (int c = 0; c < cols; c++) {
        uint32_t a = id(r, c), b = id(r, c + 1), cc = id(r + 1, c + 1), d = id(r + 1, c);
        vec3 n1 = glm::cross(P[b] - P[a], P[cc] - P[a]), n2 = glm::cross(P[cc] - P[a], P[d] - P[a]);
        N[a] += n1 + n2; N[b] += n1; N[cc] += n1 + n2; N[d] += n2;
        m.idx.insert(m.idx.end(), {base + a, base + b, base + cc, base + a, base + cc, base + d});
    }
    for (size_t i = 0; i < P.size(); i++) { vec3 n = glm::normalize(N[i] + vec3(1e-7f)); if (glm::dot(n, ref[i] - P[i]) > 0) n = -n; m.v.push_back({P[i], n, UV[i], MT[i], 0.0f, 0.0f}); }
}
static std::vector<vec3> splineCR(const std::vector<vec3>& cp, int per) {
    std::vector<vec3> out; int n = (int)cp.size();
    for (int i = 0; i < n - 1; i++) {
        vec3 p0 = cp[std::max(0, i - 1)], p1 = cp[i], p2 = cp[i + 1], p3 = cp[std::min(n - 1, i + 2)];
        for (int k = 0; k < per; k++) {
            float t = (float)k / per, t2 = t * t, t3 = t2 * t;
            out.push_back(0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
        }
    }
    out.push_back(cp.back()); return out;
}
// tube lisse le long d'un chemin (repères à rotation minimale), rayon fonction de l'abscisse 0..1
static void loftPath(Mesh& m, const std::vector<vec3>& path, const std::function<float(float)>& rad, int seg, float mat, float flat = 1.0f) {
    int n = (int)path.size(); if (n < 2) return; std::vector<vec3> P, ref; std::vector<vec2> UV; std::vector<float> MT;
    vec3 t0 = glm::normalize(path[1] - path[0] + vec3(1e-7f)), X = glm::normalize(glm::cross(t0, std::fabs(t0.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    for (int i = 0; i < n; i++) {
        vec3 t = glm::normalize(path[std::min(n - 1, i + 1)] - path[std::max(0, i - 1)] + vec3(1e-7f));
        X = glm::normalize(X - t * glm::dot(X, t) + vec3(1e-7f)); vec3 Y = glm::cross(t, X); float u = (float)i / (n - 1), r = rad(u);
        for (int k = 0; k < seg; k++) { float a = 2 * PI * k / seg; P.push_back(path[i] + (X * (std::cos(a) * flat) + Y * std::sin(a)) * r); ref.push_back(path[i]); UV.push_back(vec2((float)k / seg, u)); MT.push_back(mat); }
    }
    addSurf(m, P, ref, UV, MT, n, seg);
}
static void ellHi(Mesh& m, vec3 c, const mat3& B, vec3 r, float mat, int rows = 16, int cols = 24) {
    std::vector<vec3> P, ref; std::vector<vec2> UV; std::vector<float> MT;
    for (int i = 0; i < rows; i++) {
        float ph = PI * i / (rows - 1);
        for (int k = 0; k < cols; k++) { float a = 2 * PI * k / cols; P.push_back(c + B * vec3(r.x * std::sin(ph) * std::cos(a), r.y * std::cos(ph), r.z * std::sin(ph) * std::sin(a))); ref.push_back(c); UV.push_back(vec2((float)k / cols, (float)i / (rows - 1))); MT.push_back(mat); }
    }
    addSurf(m, P, ref, UV, MT, rows, cols);
}

static void buildWoman(Mesh& m) {
    Rng hr(1313); const mat3 I3(1.0f);
    auto prof = [](float y, float& rx, float& rz) {                         // silhouette : robe évasée, taille fine, côtes, épaules osseuses
        if (y < 1.42f) { float t = std::pow(y / 1.42f, 0.6f); rx = mixf(0.50f, 0.15f, t); rz = mixf(0.42f, 0.11f, t); }
        else if (y < 1.88f) { float t = smooth(1.42f, 1.88f, y); rx = mixf(0.15f, 0.18f, t); rz = mixf(0.11f, 0.125f, t); }
        else if (y < 2.01f) { float t = (y - 1.88f) / 0.13f; rx = mixf(0.18f, 0.24f, std::sqrt(t)); rz = mixf(0.125f, 0.10f, t); }
        else { float t = std::min(1.0f, (y - 2.01f) / 0.09f); rx = mixf(0.24f, 0.055f, std::sqrt(t)); rz = mixf(0.10f, 0.05f, std::sqrt(t)); }
    };
    auto bodyZ = [](float y) { return 0.05f * smooth(1.55f, 2.1f, y); };    // dos voûté, épaules jetées en avant
    {   // corps + robe
        const int rows = 120, cols = 80; std::vector<vec3> P, ref; std::vector<vec2> UV; std::vector<float> MT;
        for (int i = 0; i < rows; i++) {
            float y = 0.02f + 2.08f * (float)i / (rows - 1), rx, rz; prof(y, rx, rz); float zc = bodyZ(y);
            for (int k = 0; k < cols; k++) {
                float th = 2 * PI * k / cols, c = std::cos(th), sn = std::sin(th), f = 1.0f;
                if (y < 1.5f) { float w = std::pow(1.0f - y / 1.5f, 1.3f); f += w * (0.075f * std::sin(9 * th + 1.7f * std::sin(3 * th + y * 2.0f)) + 0.035f * std::sin(19 * th + y * 7.0f)) + 0.05f * w * (fbm3(vec3(c * 2.0f, y * 3.0f, sn * 2.0f), 3, 901) - 0.5f); }
                float neck = 1.86f - 0.13f * std::pow(std::max(0.0f, sn), 2.0f); bool skin = y > neck;
                if (skin && sn > 0.2f) f += 0.035f * std::pow(std::fabs(std::sin(y * 55.0f)), 6.0f) * sn;      // côtes / clavicules saillantes
                P.push_back(vec3(c * rx * f, y, zc + sn * rz * f)); ref.push_back(vec3(0, y, zc)); UV.push_back(vec2(th / (2 * PI), y)); MT.push_back(skin ? 90.0f : 91.0f);
            }
        }
        addSurf(m, P, ref, UV, MT, rows, cols);
    }
    for (int sd = -1; sd <= 1; sd += 2) {                                   // jambes maigres et pieds nus sous la robe
        float s = (float)sd;
        loftPath(m, splineCR({vec3(s * 0.09f, 1.05f, 0.0f), vec3(s * 0.10f, 0.55f, 0.02f), vec3(s * 0.10f, 0.09f, 0.0f)}, 10), [](float u) { return mixf(0.06f, 0.03f, u); }, 20, 90.0f);
        ellHi(m, vec3(s * 0.10f, 0.035f, 0.07f), I3, vec3(0.04f, 0.03f, 0.105f), 90.0f, 12, 18);
    }
    loftPath(m, splineCR({vec3(0, 2.02f, 0.03f), vec3(0, 2.18f, 0.05f), vec3(0, 2.33f, 0.075f)}, 10), [](float u) { return mixf(0.05f, 0.043f, u); }, 32, 90.0f, 0.88f);   // long cou

    // ---- tête penchée : crâne allongé, mâchoire étirée, orbites creuses, bouche béante
    mat4 Hm = glm::translate(mat4(1.0f), vec3(0, 2.40f, 0.075f)) * glm::rotate(mat4(1.0f), 0.32f, vec3(0, 0, 1)) * glm::rotate(mat4(1.0f), 0.12f, vec3(1, 0, 0));
    mat3 H3(Hm); auto HP = [&](vec3 l) { return vec3(Hm * vec4(l, 1.0f)); };
    const vec3 eyeL[2] = {vec3(-0.036f, 0.012f, 0.083f), vec3(0.036f, 0.012f, 0.083f)};
    {
        const int rows = 56, cols = 64; std::vector<vec3> P, ref; std::vector<vec2> UV; std::vector<float> MT;
        for (int i = 0; i < rows; i++) {
            float y = mixf(-0.215f, 0.145f, (float)i / (rows - 1)), rx, rz, zc = 0.0f;
            if (y >= -0.03f) { float e = (y + 0.03f) / 0.175f, q = std::sqrt(std::max(0.0f, 1.0f - e * e)); rx = 0.097f * q; rz = 0.112f * q; }
            else { float e = (-0.03f - y) / 0.185f; rx = mixf(0.097f, 0.012f, std::pow(e, 1.15f)); rz = mixf(0.112f, 0.022f, std::pow(e, 1.1f)); zc = 0.012f * e; }
            for (int k = 0; k < cols; k++) {
                float th = 2 * PI * k / cols, c = std::cos(th), sn = std::sin(th);
                vec3 p(c * rx, y, zc + sn * rz * (sn > 0 ? 0.93f : 1.0f)), rd = glm::normalize(vec3(c, 0.0f, sn) + vec3(1e-6f));
                for (const vec3& e : eyeL) { float d = glm::length(p - vec3(e.x, e.y, e.z - 0.01f)); p -= rd * 0.024f * std::exp(-(d / 0.03f) * (d / 0.03f)); }
                for (int sd = -1; sd <= 1; sd += 2) { float d = glm::length(p - vec3(sd * 0.055f, -0.07f, 0.07f)); p -= rd * 0.014f * std::exp(-(d / 0.035f) * (d / 0.035f)); }
                { float dx = p.x / 0.022f, dy = (p.y + 0.11f) / 0.055f; if (p.z > 0) p -= rd * 0.02f * std::exp(-(dx * dx + dy * dy)); }
                P.push_back(HP(p)); ref.push_back(HP(vec3(0, y, zc))); UV.push_back(vec2(th / (2 * PI), (float)i / (rows - 1))); MT.push_back(90.0f);
            }
        }
        addSurf(m, P, ref, UV, MT, rows, cols);
    }
    for (int e = 0; e < 2; e++) {
        ellHi(m, HP(eyeL[e] - vec3(0, 0, 0.007f)), H3, vec3(0.024f, 0.017f, 0.012f), 94.0f, 12, 18);       // orbite noire
        ellHi(m, HP(eyeL[e]), H3, vec3(0.0125f), 93.0f, 12, 16); gWomanEye[e] = HP(eyeL[e] + vec3(0, 0, 0.01f));   // œil rouge
    }
    ellHi(m, HP(vec3(0, -0.115f, 0.072f)), H3, vec3(0.017f, 0.05f, 0.014f), 94.0f, 14, 18);                 // bouche béante
    gWomanHead = HP(vec3(0));
    {   // calotte de cheveux (rentrée sous la peau devant le visage)
        const int rows = 30, cols = 56; std::vector<vec3> P, ref; std::vector<vec2> UV; std::vector<float> MT;
        for (int i = 0; i < rows; i++) {
            float ph = 1.95f * i / (rows - 1);
            for (int k = 0; k < cols; k++) {
                float th = 2 * PI * k / cols; vec3 p(0.104f * std::sin(ph) * std::cos(th), 0.152f * std::cos(ph) + 0.005f, 0.122f * std::sin(ph) * std::sin(th) - 0.012f);
                if (std::sin(th) > 0.2f && ph > 0.62f) p *= 0.82f;
                P.push_back(HP(p)); ref.push_back(HP(vec3(0, 0.005f, -0.012f))); UV.push_back(vec2(0)); MT.push_back(92.0f);
            }
        }
        addSurf(m, P, ref, UV, MT, rows, cols);
    }
    for (int n = 0; n < 230; n++) {                                         // longs cheveux noirs, mèches qui tombent jusqu'aux hanches
        float ph = hr.r(0.05f, 1.9f), th = hr.r(0.0f, 2 * PI), sn = std::sin(th), cs = std::cos(th);
        if (sn > 0.3f && ph > 0.55f && std::fabs(cs) < 0.6f) continue;      // le visage reste dégagé
        vec3 rl(0.104f * std::sin(ph) * cs, 0.152f * std::cos(ph) + 0.005f, 0.122f * std::sin(ph) * sn - 0.012f);
        vec3 root = HP(rl), out = glm::normalize(H3 * rl); float L = sn < 0.0f ? hr.r(0.95f, 1.5f) : hr.r(0.6f, 1.1f);
        std::vector<vec3> pts{root}; vec3 p = root;
        for (int j = 1; j <= 10; j++) {
            float f = (float)j / 10; p += out * (0.03f * (1.0f - f)) + vec3(hr.r(-0.014f, 0.014f), -L / 10, hr.r(-0.014f, 0.014f));
            if (p.y < 2.12f) { float rx, rz; prof(std::max(0.05f, p.y), rx, rz); float zc = bodyZ(p.y), dx = p.x / (rx + 0.03f), dz = (p.z - zc) / (rz + 0.03f), e2 = dx * dx + dz * dz; if (e2 < 1.0f) { float k = 1.0f / std::sqrt(std::max(e2, 1e-4f)); p.x *= k; p.z = zc + (p.z - zc) * k; } }
            vec3 hl = p - gWomanHead; if (glm::length(hl * vec3(1.0f / 0.11f, 1.0f / 0.16f, 1.0f / 0.13f)) < 1.0f) p = gWomanHead + hl * 1.08f;
            pts.push_back(p);
        }
        loftPath(m, splineCR(pts, 3), [](float u) { return mixf(0.0065f, 0.0022f, u); }, 5, 92.0f);
    }
    // ---- bras trop longs, mains crochues aux doigts interminables
    for (int sd = -1; sd <= 1; sd += 2) {
        float s = (float)sd; vec3 W(s * 0.31f, 0.80f, 0.12f);
        std::vector<vec3> arm = splineCR({vec3(s * 0.215f, 2.0f, 0.02f), vec3(s * 0.265f, 1.74f, 0.03f), vec3(s * 0.29f, 1.40f, 0.05f), vec3(s * 0.30f, 1.08f, 0.09f), W}, 10);
        loftPath(m, arm, [](float u) { return u < 0.5f ? mixf(0.047f, 0.029f, u / 0.5f) : u < 0.7f ? mixf(0.029f, 0.034f, (u - 0.5f) / 0.2f) : mixf(0.034f, 0.022f, (u - 0.7f) / 0.3f); }, 24, 90.0f);
        ellHi(m, vec3(s * 0.29f, 1.40f, 0.05f), I3, vec3(0.033f), 90.0f, 10, 14);                          // coude osseux
        vec3 K = W + vec3(s * 0.008f, -0.115f, 0.02f);
        loftPath(m, splineCR({W, W + vec3(s * 0.005f, -0.06f, 0.012f), K}, 4), [](float u) { return mixf(0.024f, 0.028f, u); }, 16, 90.0f, 0.55f);
        for (int k = 0; k < 4; k++) {
            float off = (k - 1.5f) * 0.0135f, len = (k == 1 || k == 2) ? 0.1f : 0.085f; vec3 b = K + vec3(0, 0, off);
            vec3 j1 = b + vec3(0, -len, 0.004f), j2 = j1 + vec3(s * 0.003f, -len * 0.85f, 0.016f), j3 = j2 + vec3(0, -len * 0.7f, 0.026f);
            loftPath(m, splineCR({b, j1, j2, j3}, 5), [](float u) { return mixf(0.0085f, 0.0052f, u); }, 10, 90.0f);
            ellHi(m, j3 + vec3(0, -0.006f, 0.003f), I3, vec3(0.0055f, 0.012f, 0.0055f), 94.0f, 8, 10);       // ongle noir
        }
        vec3 tb = W + vec3(-s * 0.012f, -0.05f, 0.03f), t1 = tb + vec3(-s * 0.004f, -0.055f, 0.02f), t2 = t1 + vec3(0, -0.05f, 0.022f);
        loftPath(m, splineCR({tb, t1, t2}, 5), [](float u) { return mixf(0.01f, 0.006f, u); }, 10, 90.0f);
        ellHi(m, t2 + vec3(0, -0.005f, 0.003f), I3, vec3(0.006f, 0.011f, 0.006f), 94.0f, 8, 10);
    }
    std::printf("La Dame : %zu sommets, %zu triangles\n", m.v.size(), m.idx.size() / 3);
}

static void buildLantern(Mesh& m) {
    Rng z(3); vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    addBoard(m, vec3(0, -0.11f, 0), X, Y, Z, 0.058f, 0.012f, 0.058f, 5.0f, 0.1f, z, 0);
    for (int sx = -1; sx <= 1; sx += 2) for (int sz = -1; sz <= 1; sz += 2) addBoard(m, vec3(sx * 0.052f, 0.0f, sz * 0.052f), Y, X, Z, 0.11f, 0.006f, 0.006f, 5.0f, 0.1f, z, 0);
    addBoard(m, vec3(0, 0.118f, 0), X, Y, Z, 0.07f, 0.012f, 0.07f, 5.0f, 0.1f, z, 0);
    addBoard(m, vec3(0, 0.15f, 0), X, Y, Z, 0.032f, 0.02f, 0.032f, 5.0f, 0.1f, z, 0);
    addBoardBetween(m, vec3(-0.045f, 0.168f, 0), vec3(0, 0.24f, 0), Z, 0.008f, 0.008f, 5.0f, z, 0, 0);
    addBoardBetween(m, vec3(0.045f, 0.168f, 0), vec3(0, 0.24f, 0), Z, 0.008f, 0.008f, 5.0f, z, 0, 0);
    addEllipsoid(m, vec3(0), X, Y, Z, vec3(0.044f, 0.09f, 0.044f), 13.0f, 0.0f);
}

// ----------------------------------------------------------------------------
//  Réglages (settings.ini) et plein écran
// ----------------------------------------------------------------------------
struct Settings { float master = 0.8f, music = 0.7f, sfx = 0.8f, sens = 1.0f; bool fullscreen = true; };
static void loadSettings(Settings& st) {
    FILE* f = std::fopen("settings.ini", "r"); if (!f) return; char k[64]; float v;
    while (std::fscanf(f, " %63[^=]=%f", k, &v) == 2) {
        std::string key = k;
        if (key == "master") st.master = v; else if (key == "music") st.music = v; else if (key == "sfx") st.sfx = v;
        else if (key == "sens") st.sens = v; else if (key == "fullscreen") st.fullscreen = v > 0.5f;
    }
    std::fclose(f);
    st.master = std::min(1.0f, std::max(0.0f, st.master)); st.music = std::min(1.0f, std::max(0.0f, st.music)); st.sfx = std::min(1.0f, std::max(0.0f, st.sfx));
    st.sens = std::min(2.5f, std::max(0.2f, st.sens));
}
static void saveSettings(const Settings& st) {
    FILE* f = std::fopen("settings.ini", "w"); if (!f) return;
    std::fprintf(f, "master=%.3f\nmusic=%.3f\nsfx=%.3f\nsens=%.3f\nfullscreen=%d\n", st.master, st.music, st.sfx, st.sens, st.fullscreen ? 1 : 0); std::fclose(f);
}
struct WinState { int x = 100, y = 100, w = 1600, h = 900; };
static void applyFullscreen(GLFWwindow* win, bool fs, WinState& ws) {
    bool isFs = glfwGetWindowMonitor(win) != nullptr;
    if (fs && !isFs) {
        glfwGetWindowPos(win, &ws.x, &ws.y); glfwGetWindowSize(win, &ws.w, &ws.h);
        GLFWmonitor* mon = glfwGetPrimaryMonitor(); const GLFWvidmode* vm = glfwGetVideoMode(mon);
        glfwSetWindowMonitor(win, mon, 0, 0, vm->width, vm->height, vm->refreshRate);
    } else if (!fs && isFs) glfwSetWindowMonitor(win, nullptr, ws.x, ws.y, ws.w, ws.h, 0);
    glfwSwapInterval(1);
}

int main(int argc, char** argv) {
    uint32_t seed = (uint32_t)std::chrono::system_clock::now().time_since_epoch().count() % 100000;
    int winW = 1600, winH = 900; bool startFly = false; float startAt = 3.0f, lookOff = 0.0f, lookPitch = 0.0f;
    std::string shot; int shotFrames = 25; float forceSwing = 0.22f; bool autoWalk = false, forceAxe = false, forceRat = false, forceBreak = false, ratShow = false, noAudio = false;
    int startBranch = 0, startBiome = -1, testEnemy = -1; bool optMenu = false, optPause = false, optSettings = false, optControls = false, optWindowed = false, startAtGiven = false; bool atLadder = false, wantBarricade = false, startHub = false; float hubX = 0, hubZ = 100; bool atDoor = false, atExit = false, noEnemies = false, openDoorTest = false, autoE = false, noclip = false, camGiven = false, god = false, corrTest = false; float dameDist = -1.0f; vec3 camPos(0), eyeOff(0); int testKeys = 0; float testHp = 100.0f;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) seed = (uint32_t)std::atoi(argv[++i]);
        else if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--at" && i + 1 < argc) { startAt = (float)std::atof(argv[++i]); startAtGiven = true; }
        else if (a == "--look" && i + 1 < argc) lookOff = (float)std::atof(argv[++i]);
        else if (a == "--pitch" && i + 1 < argc) lookPitch = (float)std::atof(argv[++i]);
        else if (a == "--w" && i + 1 < argc) winW = std::atoi(argv[++i]);
        else if (a == "--h" && i + 1 < argc) winH = std::atoi(argv[++i]);
        else if (a == "--frames" && i + 1 < argc) shotFrames = std::atoi(argv[++i]);
        else if (a == "--branch" && i + 1 < argc) startBranch = std::atoi(argv[++i]);
        else if (a == "--biome" && i + 1 < argc) startBiome = std::atoi(argv[++i]);
        else if (a == "--enemy" && i + 1 < argc) testEnemy = std::atoi(argv[++i]);
        else if (a == "--fly") startFly = true;
        else if (a == "--ladder") atLadder = true;
        else if (a == "--barricade") wantBarricade = true;
        else if (a == "--hub") startHub = true;
        else if (a == "--noclip") noclip = true;
        else if (a == "--god") god = true;
        else if (a == "--corr") corrTest = true;
        else if (a == "--dame" && i + 1 < argc) dameDist = (float)std::atof(argv[++i]);
        else if (a == "--eyeoff" && i + 3 < argc) { eyeOff = vec3((float)std::atof(argv[i + 1]), (float)std::atof(argv[i + 2]), (float)std::atof(argv[i + 3])); i += 3; }
        else if (a == "--campos" && i + 3 < argc) { camGiven = true; camPos = vec3((float)std::atof(argv[i + 1]), (float)std::atof(argv[i + 2]), (float)std::atof(argv[i + 3])); i += 3; }
        else if (a == "--menu") optMenu = true;
        else if (a == "--pause") optPause = true;
        else if (a == "--settings") optSettings = true;
        else if (a == "--controls") optControls = true;
        else if (a == "--windowed") optWindowed = true;
        else if (a == "--hubpos" && i + 2 < argc) { startHub = true; hubX = (float)std::atof(argv[++i]); hubZ = (float)std::atof(argv[++i]); }
        else if (a == "--axe") forceAxe = true;
        else if (a == "--swing" && i + 1 < argc) { forceAxe = true; forceSwing = (float)std::atof(argv[++i]); }
        else if (a == "--autowalk") autoWalk = true;
        else if (a == "--rat") forceRat = true;
        else if (a == "--ratshow") ratShow = true;
        else if (a == "--break") forceBreak = true;
        else if (a == "--door") atDoor = true;
        else if (a == "--exit") atExit = true;
        else if (a == "--noaudio") noAudio = true;
        else if (a == "--noenemies") noEnemies = true;
        else if (a == "--open") openDoorTest = true;
        else if (a == "--hp" && i + 1 < argc) testHp = (float)std::atof(argv[++i]);
        else if (a == "--autoe") autoE = true;
        else if (a == "--keys" && i + 1 < argc) testKeys = std::atoi(argv[++i]);
    }
    if (!glfwInit()) { std::fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    Settings cfg; loadSettings(cfg); if (optWindowed) cfg.fullscreen = false;
    WinState wst; wst.w = winW; wst.h = winH;
    GLFWmonitor* mon0 = glfwGetPrimaryMonitor(); const GLFWvidmode* vm0 = glfwGetVideoMode(mon0);
    GLFWwindow* win = nullptr;
    if (cfg.fullscreen && shot.empty()) {                       // plein écran au démarrage
        glfwWindowHint(GLFW_RED_BITS, vm0->redBits); glfwWindowHint(GLFW_GREEN_BITS, vm0->greenBits); glfwWindowHint(GLFW_BLUE_BITS, vm0->blueBits); glfwWindowHint(GLFW_REFRESH_RATE, vm0->refreshRate);
        win = glfwCreateWindow(vm0->width, vm0->height, "GRIMDEEP", mon0, nullptr);
    } else win = glfwCreateWindow(winW, winH, "GRIMDEEP", nullptr, nullptr);
    if (!win) { std::fprintf(stderr, "Impossible de creer la fenetre OpenGL 4.3\n"); return 1; }
    glfwMakeContextCurrent(win); glfwSwapInterval(shot.empty() ? 1 : 0);
    glewExperimental = GL_TRUE; if (glewInit() != GLEW_OK) { std::fprintf(stderr, "GLEW init failed\n"); return 1; }
    std::printf("GL: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    static aud::Engine audio;
    if (shot.empty() && !noAudio) { if (!audio.start()) std::printf("Audio indisponible\n"); }
    audio.setVolumes(cfg.master, cfg.music, cfg.sfx);

    GLuint progScene = program(SCENE_VS, SCENE_FS), progShadow = program(SHADOW_VS, SHADOW_FS);
    GLuint progDust = program(DUST_VS, DUST_FS), progPart = program(PART_VS, PART_FS);
    GLuint progSky = program(FS_QUAD_VS, SKY_FS);
    GLuint progDown = program(FS_QUAD_VS, BLOOM_DOWN_FS), progUp = program(FS_QUAD_VS, BLOOM_UP_FS), progComp = program(FS_QUAD_VS, COMPOSE_FS);
    UI ui; ui.init();

    GLuint texRockC, texRockN, texWoodC, texWoodN;
    { std::vector<uint8_t> c, n; genRockTex(512, c, n); texRockC = makeTex(512, c, true); texRockN = makeTex(512, n, false);
      genWoodTex(512, c, n); texWoodC = makeTex(512, c, true); texWoodN = makeTex(512, n, false); }

    const int SH = 4, SHRES = 384; const float SHFAR = 40.0f;
    GLuint shadowTex, shadowFbo, shadowDepth;
    glGenTextures(1, &shadowTex); glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowTex);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_R32F, SHRES, SHRES, 6 * SH);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &shadowDepth); glBindRenderbuffer(GL_RENDERBUFFER, shadowDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SHRES, SHRES);
    glGenFramebuffers(1, &shadowFbo); glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, shadowDepth);

    GLuint partVao, partVbo, dustVao;
    glGenVertexArrays(1, &partVao); glBindVertexArray(partVao); glGenBuffers(1, &partVbo); glBindBuffer(GL_ARRAY_BUFFER, partVbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glGenVertexArrays(1, &dustVao);
    GLuint quadVao; glGenVertexArrays(1, &quadVao);

    registerHubMats(); loadModel("planks_barricade", gBarricadeMdl); loadModel("axe", gAxeMdl); loadModel("ladder", gLadderMdl); loadModel("plank", gPlankMdl);
    loadModel("mummy", gMummyMdl); loadModel("giant_spider", gSpiderMdl); loadModel("lava_golem", gGolemMdl);
    glUseProgram(progScene);
    if (!gMdlA.empty()) { glUniform4fv(U(progScene, "uMdlA"), (GLsizei)gMdlA.size(), glm::value_ptr(gMdlA[0])); glUniform4fv(U(progScene, "uMdlE"), (GLsizei)gMdlE.size(), glm::value_ptr(gMdlE[0])); }
    Mesh axeM, lanternM, enemyM, propsM, womanM; buildPickaxe(axeM); axeM.upload(); buildWoman(womanM); womanM.upload(); buildLantern(lanternM); lanternM.upload();
    World world; world.build(seed);
    Targets tg;

    // palettes de biomes pour les shaders
    GLfloat palTint[21], palAmb[21], palWet[7], palMoss[7], palFlame[39] = {};
    for (int b = 0; b < 7; b++) for (int c = 0; c < 3; c++) { palTint[b * 3 + c] = BIOMES[b].tint[c]; palAmb[b * 3 + c] = BIOMES[b].amb[c]; palFlame[b * 3 + c] = BIOMES[b].torch[c]; }
    for (int b = 0; b < 5; b++) for (int c = 0; c < 3; c++) palFlame[(7 + b) * 3 + c] = gCrysCol[b][c];
    palFlame[36] = 1.0f; palFlame[37] = 0.05f; palFlame[38] = 0.03f;          // 12 : rouge des yeux de « la Dame »
    for (int b = 0; b < 7; b++) { palWet[b] = BIOMES[b].wet; palMoss[b] = BIOMES[b].moss; }

    // ---- état de jeu
    enum { PLAY, DEAD, WIN }; int state = PLAY;
    float hp = 100, hurtFlash = 0, playTime = 0; int keys = 0, kills = 0; std::string msg; float msgT = 0;
    std::vector<Enemy> enemies; std::vector<Drop> drops; std::vector<Part> parts; std::vector<Ragdoll> rags;
    vec3 pos(0); float yaw = 0, pitch = 0; bool fly = startFly; std::vector<int> hints; float walked = 0;
    int curBiome = -1; float biomeBanner = 0; Rng grng(4242), prng(777), drng2(31337), rrng(2024);

    auto placePlayer = [&](float meters) {
        const Tube* tb = &world.tubes[0]; int i = std::min(tb->N - 12, std::max(6, (int)(meters / tb->ds)));
        if (startBranch > 0 && startBranch < (int)world.tubes.size()) { tb = &world.tubes[startBranch]; i = std::min(tb->N - 8, std::max(6, (int)(meters / tb->ds))); }
        const Ring& r = tb->rings[i];
        pos = tb->toWorld(i, 0.0f, tb->floorY(i, 0.0f) + 1.65f); hints.assign(world.tubes.size(), -1);
        yaw = std::atan2(r.T.z, r.T.x) + glm::radians(lookOff); pitch = glm::radians(lookPitch);
    };
    // cartographie : anneaux de galerie découverts (+ bords gauche/droit en XZ) et cases du camp
    std::vector<std::vector<uint8_t>> mapSeen; std::vector<std::vector<vec4>> mapEdge; std::vector<uint8_t> hubSeen(80 * 60, 0);
    auto resetMap = [&]() {
        mapSeen.assign(world.tubes.size(), {}); mapEdge.assign(world.tubes.size(), {});
        for (size_t t = 0; t < world.tubes.size(); t++) { mapSeen[t].assign((size_t)world.tubes[t].N, 0); mapEdge[t].assign((size_t)world.tubes[t].N, vec4(0)); }
        std::fill(hubSeen.begin(), hubSeen.end(), 0);
    };
    resetMap();
    struct Haunt { int st[3] = {0, 0, 0}; int ev = -1; vec3 p = vec3(0), f = vec3(0, 0, 1); float dis = -1, lookT = 0; } hn; int hauntHint = -1;    // apparitions de « la Dame »
    auto placeHub = [&]() { hn = Haunt(); pos = world.hubSpawn; yaw = -PI * 0.5f; pitch = 0.0f; hints.assign(world.tubes.size(), -1); };
    auto initEntities = [&]() {
        enemies.clear();
        for (auto& sp : world.spawns) { Enemy e; e.type = sp.type; e.pos = sp.pos; e.hp = ES[sp.type].hp; e.hints.assign(world.tubes.size(), -1); enemies.push_back(e); }
        if (noEnemies) enemies.clear();
        if (testEnemy >= 0) { Enemy e; e.type = testEnemy % 3; vec3 fl = glm::normalize(vec3(std::cos(yaw), 0, std::sin(yaw))); e.pos = pos + fl * 2.3f; e.face = -fl; e.hp = ES[e.type].hp; e.hints.assign(world.tubes.size(), -1); enemies.push_back(e); }
    };
    auto restart = [&](bool newSeed) {
        if (newSeed) { seed = seed * 1664525u + 1013904223u; seed %= 100000; }
        world.build(seed); hints.clear(); parts.clear(); drops.clear(); rags.clear(); hp = 100; keys = 0; kills = 0; state = PLAY; playTime = 0; hurtFlash = 0; msg.clear();
        for (int b = 0; b < 7; b++) for (int c = 0; c < 3; c++) palFlame[b * 3 + c] = BIOMES[b].torch[c];
        placeHub(); initEntities(); curBiome = -1; resetMap();
    };
    float startMeters = startAt;
    if (startBiome >= 0) startMeters = (startBiome + 0.5f) * world.doorRing / 5.0f * 0.3f;
    if (atDoor) startMeters = (world.doorRing - 9) * 0.3f;
    if (atExit) startMeters = (world.tubes[0].N - 14) * 0.3f;
    bool tubeStart = startAtGiven || startBiome >= 0 || atDoor || atExit || startBranch > 0 || atLadder || wantBarricade;
    if (tubeStart) placePlayer(startMeters); else placeHub();
    if ((atLadder || wantBarricade) && !world.ladders.empty()) {                  // test : se place face à la première échelle
        const World::LadderObj* pL = &world.ladders[0];
        for (const auto& l : world.ladders) if (l.kind == (wantBarricade ? 1 : 0)) { pL = &l; break; }
        const auto& L = *pL; vec3 d = L.look - L.stand; pos = L.stand; yaw = std::atan2(d.z, d.x); pitch = 0.0f;
    }
    if (startHub) { pos = vec3(hubX, world.hubGround(hubX, world.hubFront + hubZ) + 1.65f, world.hubFront + hubZ); yaw = -PI * 0.5f + glm::radians(lookOff); pitch = glm::radians(lookPitch); }
    if (corrTest && world.corrA >= 0) { startBranch = 0; placePlayer((world.corrA - 30) * 0.3f); }
    if (camGiven) { pos = camPos; yaw = glm::radians(lookOff); pitch = glm::radians(lookPitch); fly = true; }
    initEntities(); keys = testKeys; hp = testHp; if (openDoorTest) world.openDoor();

    std::vector<Rat> rats; Mesh& ratM = world.ratM;
    float ratTimer = grng.r(14.0f, 30.0f);
    int slotLight[SH]; for (int s = 0; s < SH; s++) slotLight[s] = -1;
    double lastT = glfwGetTime(), fpsT = lastT; int fpsN = 0; double simT = 0;
    double mx0 = 0, my0 = 0; bool firstMouse = true;
    bool keyF = false, keyR = false, keyP = false, mouseL = false, keyE = false, keyEnter = false; int frame = 0; double tStart = glfwGetTime();
    float swingT = -1.0f; bool swingHit = false;
    vec3 pvel(0); float sprintAmt = 0, camShake = 0, camKick = 0, hitStop = 0, bobAmt = 0;      // déplacement lissé, effets de caméra
    float groundEye = -1e9f, jumpOff = 0, jumpV = 0; bool keySpace = false;                       // saut
    bool wallHint = false;
    // ---- écrans et menus
    enum Screen { SC_TITLE, SC_PLAY, SC_PAUSE, SC_SETTINGS, SC_CONTROLS };
    Screen screen = !shot.empty() ? SC_PLAY : SC_TITLE, backTo = SC_TITLE;
    if (optMenu) screen = SC_TITLE; if (optPause) screen = SC_PAUSE; if (optSettings) { screen = SC_SETTINGS; backTo = SC_TITLE; } if (optControls) { screen = SC_CONTROLS; backTo = SC_TITLE; }
    bool worldDirty = false, cursorFree = true, keyEsc = false, keyF11 = false, keyUp = false, keyDn = false, keyLf = false, keyRt = false, keyRet = false;
    double menuT = 0; float heartT = 0;
    struct MouseS { float x = 0, y = 0; bool down = false, click = false, moved = false; } ms;
    int btnCount = 0, menuSel = 0, menuCountPrev = 1, dragId = -1; bool uiEnter = false, uiLeft = false, uiRight = false;
    std::printf("ZQSD/WASD deplacement | Maj courir | clic gauche pioche | E interagir | F vol | R nouvelle grotte | F12 capture | Echap quitter\n");

    struct LV { vec3 p, c; float dist; int src; float inten; int biome; };
    std::vector<LV> lv;
    auto lightAt = [&](vec3 p) { float s = 0; for (auto& l : lv) { float d2 = glm::dot(l.p - p, l.p - p); if (d2 < 19.0f * 19.0f) { float w = 1 - d2 / (19.0f * 19.0f); s += w * w / (0.25f + d2 * 0.26f) * l.c.r; } } return std::min(s * 0.05f, 1.2f); };
    auto say = [&](const std::string& s) { msg = s; msgT = 3.5f; };
    auto puff = [&](vec3 at, int n, float strength) {
        for (int k = 0; k < n; k++) {
            Part q; q.p = at + vec3(prng.r(-0.3f, 0.3f), prng.r(-0.2f, 0.3f), prng.r(-0.3f, 0.3f)); q.v = vec3(prng.r(-0.5f, 0.5f), prng.r(0.0f, 0.6f), prng.r(-0.5f, 0.5f));
            q.maxLife = q.life = prng.r(1.0f, 2.2f); q.seed = prng.f() * 30; q.size = prng.r(0.35f, 0.7f); q.grow = 0.4f; q.kind = 3; q.inten = (lightAt(at) + 0.02f) * strength; parts.push_back(q);
        }
    };
    auto hurtPlayer = [&](float dmg) {
        if (state != PLAY || god) return; hp -= dmg; hurtFlash = 0.7f; audio.sfx(aud::SF_HURT, 0.9f);
        if (hp <= 0) { hp = 0; state = DEAD; audio.sfx(aud::SF_DIE, 1.0f); }
    };
    auto killEnemy = [&](Enemy& e, vec3 dir) {
        e.alive = false; kills++; audio.sfx(aud::SF_DIE, 0.7f); puff(e.pos - vec3(0, 0.8f, 0), 14, 1.2f);
        if (enemyModel(e.type).ok) { if (rags.size() >= 10) rags.erase(rags.begin()); rags.push_back(makeRagdoll(e, dir, ES[e.type].knock, rrng)); }
        if (grng.chance(0.3f)) world.pickups.push_back(Pickup{e.pos - vec3(0, 1.2f, 0), 1, true, grng.f() * 6});
    };

    auto spawnRat = [&](float speed) -> bool {
        std::vector<int> cand;
        for (size_t i = 0; i < world.holes.size(); i++) {
            const HolePair& h = world.holes[i]; float d = glm::length((h.pA + h.pB) * 0.5f - pos);
            if (d > 3.0f && d < 28.0f) cand.push_back((int)i);
        }
        if (cand.empty()) return false;
        const HolePair& h = world.holes[cand[grng.next() % cand.size()]]; const Tube& tb = world.tubes[h.tube];
        Rat r; r.speed = speed; r.phase = grng.f() * 6;
        bool rev = grng.chance(0.5f); vec3 pa = rev ? h.pB : h.pA, na = rev ? h.nB : h.nA, pb = rev ? h.pA : h.pB, nb = rev ? h.nA : h.nB;
        float xa = rev ? h.xB : h.xA, xb = rev ? h.xA : h.xB; int ia = rev ? h.iB : h.iA, ib = rev ? h.iA : h.iB;
        r.pts.push_back(pa - na * 0.4f); r.pts.push_back(pa + na * 0.05f);
        for (int m = 1; m < 4; m++) {
            float t = m / 4.0f; int ri = (int)std::lround(mixf((float)ia, (float)ib, t)); float x = mixf(xa, xb, t) + grng.r(-0.4f, 0.4f) * std::sin(t * PI);
            x = std::min(std::max(x, std::min(xa, xb) - 0.2f), std::max(xa, xb) + 0.2f);
            r.pts.push_back(tb.toWorld(ri, x, tb.floorY(ri, x) + 0.0f));
        }
        r.pts.push_back(pb + nb * 0.05f); r.pts.push_back(pb - nb * 0.4f);
        r.cum.assign(r.pts.size(), 0); for (size_t k = 1; k < r.pts.size(); k++) r.cum[k] = r.cum[k - 1] + glm::length(r.pts[k] - r.pts[k - 1]);
        rats.push_back(r); return true;
    };

    // --- coup de pioche : rayon + éclaboussure sur les blocs / planches voisins, toiles, ennemis (dégâts, repoussement, ragdoll)
    auto axeHit = [&](vec3 eye, vec3 front) {
        const float REACH = 3.2f, SPLASH = 0.62f;
        float t; int b = world.rayBoard(eye, front, forceBreak ? 12.0f : REACH, t);
        vec3 P = eye + front * (b >= 0 ? t : 2.0f);
        std::vector<int> list; world.boardsNear(P, SPLASH, list);
        if (b >= 0 && std::find(list.begin(), list.end(), b) == list.end()) list.push_back(b);
        int broken = 0, stoneHit = 0, stoneBroken = 0;
        for (int id : list) {
            bool isStone = world.wood.boards[id].role == 7; world.hitBoard(id, P, front);
            if (isStone) stoneHit++;
            if (!world.wood.boards[id].alive) { broken++; if (isStone) stoneBroken++; }
        }
        if (stoneHit) {
            audio.sfx(stoneBroken ? aud::SF_ROCKBREAK : aud::SF_ROCK, stoneBroken ? 1.0f : 0.9f, (int)(prng.f() * 200));
            puff(P, 6 + 6 * stoneBroken, 1.3f); camShake = std::max(camShake, 0.06f + 0.03f * stoneBroken); hitStop = std::max(hitStop, stoneBroken ? 0.075f : 0.045f);
            for (int k = 0; k < 8; k++) {                                       // étincelles
                Part q; q.p = P; q.v = (-front * 0.8f + vec3(prng.r(-1, 1), prng.r(0.2f, 1.4f), prng.r(-1, 1))) * prng.r(1.5f, 3.6f);
                q.maxLife = q.life = prng.r(0.3f, 0.65f); q.seed = 0; q.size = 0.05f; q.grow = 0; q.kind = 5; q.inten = 1.1f; parts.push_back(q);
            }
        } else if (!list.empty()) audio.sfx(broken ? aud::SF_BREAK : aud::SF_HITWOOD, 0.9f);
        if (broken && !stoneHit) puff(P, 10, 1.0f);
        std::printf("Pioche : %zu bloc(s)/planche(s) touche(s), %d casse(s)", list.size(), broken); std::puts("");
        int w = world.clearWebs(eye, front, REACH + 0.4f); (void)w;
        vec3 fdir = glm::normalize(vec3(front.x, 0, front.z) + vec3(1e-6f));
        for (auto& e : enemies) if (e.alive) {
            vec3 c = e.pos - vec3(0, 1.65f, 0) + vec3(0, ES[e.type].centerH, 0), oc = c - eye; float tt = glm::dot(oc, front);
            if (tt < 0 || tt > REACH + 0.5f) continue;
            if (glm::length(oc - front * tt) < ES[e.type].radius + 0.35f) {
                const EnemyStat& st = ES[e.type];
                e.hp -= 1; e.hurt = 0.3f; e.hurtLean = 1.0f; e.awake = true; e.stun = st.stun; e.atkT = -1; e.vel = fdir * st.knock;
                audio.sfx(aud::SF_FLESH, 0.95f); hitStop = std::max(hitStop, 0.07f); camShake = std::max(camShake, 0.1f);
                for (int k = 0; k < 7; k++) {                                   // gerbe de braises rouges
                    Part q; q.p = c; q.v = (fdir * 1.4f + vec3(prng.r(-1, 1), prng.r(0.0f, 1.4f), prng.r(-1, 1))) * prng.r(0.8f, 2.2f);
                    q.maxLife = q.life = prng.r(0.5f, 1.1f); q.seed = prng.f() * 20; q.size = 0.05f; q.grow = 0; q.kind = 2; q.inten = 4.0f; parts.push_back(q);
                }
                puff(c, 3, 0.9f);
                if (e.hp <= 0) killEnemy(e, fdir);
            }
        }
    };

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        double now = glfwGetTime(); float dt = (float)std::min(now - lastT, 0.05); lastT = now;
        if (!shot.empty()) dt = 1.0f / 30.0f;
        const float rdt = dt;
        if (hitStop > 0.0f) { hitStop -= rdt; dt *= 0.1f; }                    // arrêt sur image au moment de l'impact
        {   // Echap : pause / retour ; F11 : plein écran ; flèches / Entrée : navigation dans les menus
            bool kEsc = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (kEsc && !keyEsc) {
                if (screen == SC_PLAY) { screen = SC_PAUSE; audio.sfx(aud::SF_CLICK, 0.6f); }
                else if (screen == SC_PAUSE) { screen = SC_PLAY; audio.sfx(aud::SF_CLICK, 0.6f, 1); }
                else if (screen == SC_SETTINGS || screen == SC_CONTROLS) { screen = backTo; saveSettings(cfg); audio.sfx(aud::SF_CLICK, 0.6f); menuSel = 0; }
            }
            keyEsc = kEsc;
            bool k11 = glfwGetKey(win, GLFW_KEY_F11) == GLFW_PRESS; if (k11 && !keyF11) cfg.fullscreen = !cfg.fullscreen; keyF11 = k11;
            bool u = glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS, d = glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS, l = glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS, r = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS;
            bool ret = glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
            uiEnter = ret && !keyRet && screen != SC_PLAY; uiLeft = l && !keyLf && screen != SC_PLAY; uiRight = r && !keyRt && screen != SC_PLAY;
            if (screen != SC_PLAY) {
                if (d && !keyDn) { menuSel = (menuSel + 1) % std::max(menuCountPrev, 1); audio.sfx(aud::SF_CLICK, 0.35f, 1); }
                if (u && !keyUp) { menuSel = (menuSel + std::max(menuCountPrev, 1) - 1) % std::max(menuCountPrev, 1); audio.sfx(aud::SF_CLICK, 0.35f, 1); }
            }
            keyUp = u; keyDn = d; keyLf = l; keyRt = r; keyRet = ret;
            if (cfg.fullscreen != (glfwGetWindowMonitor(win) != nullptr) && shot.empty()) applyFullscreen(win, cfg.fullscreen, wst);
        }
        const bool live = (screen == SC_PLAY);
        const bool titleCam = (screen == SC_TITLE) || ((screen == SC_SETTINGS || screen == SC_CONTROLS) && backTo == SC_TITLE);
        if (!live) dt = 0.0f; else menuSel = 0;
        if (glfwWindowShouldClose(win)) break;
        audio.duck = live ? 1.0f : 0.5f;
        audio.setVolumes(cfg.master, cfg.music, cfg.sfx);
        if (shot.empty() && (screen != SC_PLAY) != cursorFree) { cursorFree = !cursorFree; glfwSetInputMode(win, GLFW_CURSOR, cursorFree ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED); firstMouse = true; }
        simT += dt; if (!live) menuT += rdt; float T = (float)(simT + menuT) + 3.0f; if (state == PLAY) playTime += dt;

        // --- entrées
        double mx, my; glfwGetCursorPos(win, &mx, &my);
        if (firstMouse) { mx0 = mx; my0 = my; firstMouse = false; }
        if (shot.empty() && state == PLAY && live) { yaw += (float)(mx - mx0) * 0.0023f * cfg.sens; pitch -= (float)(my - my0) * 0.0023f * cfg.sens; }
        mx0 = mx; my0 = my; pitch = std::max(-1.5f, std::min(1.5f, pitch));
        vec3 front(std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch));
        vec3 flat = glm::normalize(vec3(std::cos(yaw), 0, std::sin(yaw))), rightv = glm::normalize(glm::cross(flat, vec3(0, 1, 0)));
        bool kF = glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS; if (kF && !keyF && live) fly = !fly; keyF = kF;
        bool kR = glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS; if (kR && !keyR && live) { restart(true); for (int s = 0; s < SH; s++) slotLight[s] = -1; rats.clear(); } keyR = kR;
        bool kEn = glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS; if (kEn && !keyEnter && state != PLAY && live) { restart(false); rats.clear(); } keyEnter = kEn;
        bool mL = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (mL && !mouseL && swingT < 0 && state == PLAY && live) { swingT = 0; swingHit = false; audio.sfx(aud::SF_SWING, 0.7f); } mouseL = mL;
        bool kE = glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS || (autoE && frame == 3);
        if (kE && !keyE && state == PLAY && live && !world.doorOpen && glm::length(pos - world.doorPos) < 3.6f) {
            if (keys >= world.keysNeeded) { world.openDoor(); audio.sfx(aud::SF_DOOR, 1.0f); say("LA PORTE S'OUVRE..."); puff(world.doorPos, 18, 1.5f); }
            else say("IL FAUT " + std::to_string(world.keysNeeded) + " CLES (" + std::to_string(keys) + "/" + std::to_string(world.keysNeeded) + ")");
        }
        keyE = kE;
        vec3 mv(0); vec3 fw = fly ? front : flat;
        if (state == PLAY && live) {
            if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) mv += fw;
            if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) mv -= fw;
            if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) mv += rightv;
            if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) mv -= rightv;
            if (fly) { if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) mv += vec3(0, 1, 0); if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) mv -= vec3(0, 1, 0); }
        }
        if (autoWalk) mv = fw;
        // déplacement lissé : accélération / décélération progressives, glissement le long des parois, balancement de la tête
        bool wantMove = glm::length(mv) > 0, shiftDown = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        bool sprint = wantMove && !fly && shiftDown, air = jumpOff > 0.0f;
        float speed = fly ? (shiftDown ? 9.0f : 4.5f) : (sprint ? 7.2f : 3.4f);
        vec3 wish = wantMove ? glm::normalize(mv) * speed : vec3(0);
        pvel += (wish - pvel) * (1.0f - std::exp(-(wantMove ? (sprint ? 13.0f : 11.0f) : 8.5f) * (air ? 0.35f : 1.0f) * dt));
        if (!fly) pvel.y = 0;
        {   // saut (Espace)
            bool kSp = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (kSp && !keySpace && !fly && !air && jumpV == 0.0f && state == PLAY && live) { jumpV = 5.2f; audio.sfx(aud::SF_STEP, 0.32f, world.inHub(pos) ? 1 : 0); }
            keySpace = kSp;
            if (fly) { jumpOff = 0; jumpV = 0; }
            else if (jumpOff > 0.0f || jumpV != 0.0f) {
                jumpV -= 15.0f * dt; jumpOff = std::min(0.95f, jumpOff + jumpV * dt);
                if (jumpOff <= 0.0f) { jumpOff = 0.0f; if (jumpV < -2.5f) { audio.sfx(aud::SF_STEP, 0.5f, world.inHub(pos) ? 1 : 0); camShake = std::max(camShake, 0.035f); } jumpV = 0.0f; }
            }
        }
        vec3 stepv = pvel * dt; float stepLen = glm::length(stepv); if (!fly && !air) walked += stepLen;
        pos += stepv; vec3 afterMove = pos;
        if (!noclip) world.collideBoards(pos, fly);
        vec3 boardCorr = pos - afterMove;
        if (!noclip) {
            vec3 c = world.constrain(pos, fly, hints);
            if (fly) pos = c;
            else {                                       // la caméra suit le relief vite (plus vite en montée), sans tirer le joueur en arrière
                pos.x = c.x; pos.z = c.z;
                if (groundEye < -1e8f || std::fabs(c.y - groundEye) > 2.0f) groundEye = c.y;
                groundEye += (c.y - groundEye) * std::min(1.0f, dt * (c.y > groundEye ? 24.0f : 16.0f));
                pos.y = groundEye + jumpOff;
            }
        }
        {   // glissement le long des obstacles : seules les vraies collisions freinent
            auto slide = [&](vec3 corr, float minLen) { corr.y = 0; float cl = glm::length(corr); if (cl > minLen) { vec3 n = corr / cl; float vn = glm::dot(pvel, n); if (vn < 0) pvel -= n * vn; } };
            slide(boardCorr, 1e-4f); slide(pos - afterMove - boardCorr, std::max(0.004f, 0.4f * stepLen));
        }
        float vlen = glm::length(vec3(pvel.x, 0, pvel.z));
        sprintAmt += ((sprint && vlen > 4.0f ? 1.0f : 0.0f) - sprintAmt) * (1.0f - std::exp(-6.0f * dt));
        bobAmt += (std::min(1.0f, vlen / 3.0f) - bobAmt) * (1.0f - std::exp(-8.0f * dt));
        const float stride = walked * 2.1f;
        { static int lastStep = 0; int si = (int)std::floor(stride / PI); if (si != lastStep) { lastStep = si; if (bobAmt > 0.35f && !fly && state == PLAY && live) audio.sfx(aud::SF_STEP, sprint ? 0.42f : 0.28f, world.inHub(pos) ? 1 : 0); } }
        vec3 eye = pos;
        if (!fly && jumpOff <= 0.0f) eye += vec3(0, std::sin(stride * 2.0f) * (0.026f + 0.012f * sprintAmt) * bobAmt + std::sin(T * 1.3f) * 0.004f, 0) + rightv * (std::cos(stride) * 0.014f * bobAmt);
        camShake = std::max(0.0f, camShake - dt * 0.9f);
        if (camShake > 0.001f) eye += vec3(prng.r(-1, 1), prng.r(-1, 1), prng.r(-1, 1)) * camShake * 0.11f;
        if (live) {   // révèle les galeries (rayon 12 m) et les cases du camp (rayon 16 m) autour du joueur
            for (size_t t = 0; t < world.tubes.size(); t++) {
                const Tube& tb = world.tubes[t]; if (mapSeen[t].size() != (size_t)tb.N) continue;
                for (int i = 0; i < tb.N; i++) {
                    if (mapSeen[t][i]) continue; vec3 d = tb.rings[i].c - pos; if (glm::dot(d, d) > 144.0f) continue;
                    mapSeen[t][i] = 1; float xl, xr; const Ring& r = tb.rings[i];
                    if (!tb.wallRange(i, tb.floorY(i, 0.0f) + 1.2f, xl, xr)) { xl = -r.w * 0.8f; xr = r.w * 0.8f; }
                    vec3 L = r.c + r.R * xl, Rr = r.c + r.R * xr; mapEdge[t][i] = vec4(L.x, L.z, Rr.x, Rr.z);
                }
            }
            if (world.inHub(pos)) for (int cz = 0; cz < 60; cz++) for (int cxx = 0; cxx < 80; cxx++) {
                float wx = -160.0f + (cxx + 0.5f) * 4.0f, wz = world.hubFront - 8.0f + (cz + 0.5f) * 4.0f;
                if ((wx - pos.x) * (wx - pos.x) + (wz - pos.z) * (wz - pos.z) < 256.0f) hubSeen[(size_t)cz * 80 + cxx] = 1;
            }
        }
        if (titleCam) {      // plan large cinématique : la montagne, ses cristaux géants et l'entrée de la mine
            float tt = (float)menuT;
            eye = vec3(26.0f * std::sin(tt * 0.085f), world.hubG + 5.5f + 1.6f * std::sin(tt * 0.13f), world.hubFront + 62.0f + 9.0f * std::cos(tt * 0.07f));
            vec3 tgt(9.0f * std::sin(tt * 0.05f + 1.0f), world.hubG + 26.0f + 3.0f * std::sin(tt * 0.09f), world.hubFront - 8.0f);
            front = glm::normalize(tgt - eye);
        }

        // --- pioche : armement (0-0,18 s), frappe (0,18-0,28 s), récupération ; le coup porte en fin de frappe
        if (forceAxe) swingT = forceSwing;
        if (forceBreak && (frame == 3 || frame == 8 || frame == 13 || frame == 18)) axeHit(eye, front);
        if (swingT >= 0 && !forceAxe) {
            swingT += dt;
            if (!swingHit && swingT > 0.26f) { swingHit = true; axeHit(eye, front); }
            if (swingT > 0.64f) swingT = -1.0f;
        }
        { float a = swingT, k = 0.0f;
          if (a >= 0) { if (a < 0.18f) k = 0.025f * (a / 0.18f); else if (a < 0.3f) k = mixf(0.025f, -0.06f, (a - 0.18f) / 0.12f); else k = mixf(-0.06f, 0.0f, std::min(1.0f, (a - 0.3f) / 0.3f)); }
          camKick += (k - camKick) * (1.0f - std::exp(-28.0f * dt)); }

        if (glm::length(eyeOff) > 0) eye += rightv * eyeOff.x + vec3(0, eyeOff.y, 0) - flat * eyeOff.z;       // (test) caméra décalée pour observer les ennemis

        // --- biome courant, musique
        float bf = world.biomeAtPos(pos); int bNow = (int)std::floor(bf + 0.5f);
        if (bNow != curBiome) { curBiome = bNow; biomeBanner = 4.0f; audio.setBiome(bNow); }
        biomeBanner = std::max(0.0f, biomeBanner - dt); msgT = std::max(0.0f, msgT - dt); hurtFlash = std::max(0.0f, hurtFlash - dt);

        // --- ennemis : poursuite, attaque en 3 temps (armement -> fente -> récupération), repoussement, état sonné
        for (auto& e : enemies) if (e.alive) {
            const EnemyStat& st = ES[e.type];
            vec3 d = pos - e.pos; d.y = 0; float dh = glm::length(d), d3 = glm::length(pos - e.pos);
            if (!e.awake && state == PLAY && d3 < st.aggro && dh < st.aggro) { e.awake = true; audio.sfx(aud::SF_ROAR, std::max(0.2f, 1.0f - d3 / 20.0f)); }
            if (e.awake && d3 > 36.0f) e.awake = false;
            e.hurt = std::max(0.0f, e.hurt - dt); e.hurtLean = std::max(0.0f, e.hurtLean - dt * 3.0f); e.cd -= dt; e.stun -= dt;
            bool pushed = glm::length(e.vel) > 0.05f;
            if (pushed) { e.pos += e.vel * dt; e.vel *= std::max(0.0f, 1.0f - 6.0f * dt); }
            if (e.awake && state == PLAY && e.stun <= 0) {
                vec3 dir = d / std::max(dh, 1e-3f);
                if (e.atkT < 0) {
                    vec3 nf = glm::normalize(glm::mix(e.face, dir, std::min(1.0f, dt * 10.0f)) + vec3(1e-5f)); e.face = nf;
                    if (dh > st.reach * 0.6f) { e.pos += dir * st.speed * dt; e.anim += dt * st.speed * 3.2f; }
                    if (dh < st.reach * 0.95f && std::fabs(pos.y - e.pos.y) < 1.5f && e.cd <= 0) { e.atkT = 0; e.atkHit = false; e.cd = st.cd + st.wind; audio.sfx(aud::SF_ROAR, 0.3f); }
                } else {
                    e.atkT += dt; e.face = glm::normalize(glm::mix(e.face, dir, std::min(1.0f, dt * (e.atkT < st.wind ? 8.0f : 1.5f))) + vec3(1e-5f));
                    if (!e.atkHit && e.atkT >= st.wind) {
                        e.atkHit = true;
                        if (dh < st.reach + 0.5f && std::fabs(pos.y - e.pos.y) < 1.7f) { hurtPlayer(st.dmg); camShake = std::max(camShake, 0.25f); }
                    }
                    if (e.atkT >= st.wind && e.atkT < st.wind + ATK_STRIKE && dh > st.reach * 0.5f) e.pos += dir * (e.type == 1 ? 7.5f : 3.4f) * dt;      // fente
                    if (e.atkT > st.wind + ATK_STRIKE + ATK_RECOVER) e.atkT = -1.0f;
                }
            } else if (e.stun > 0) e.atkT = -1.0f;
            if (e.awake || pushed) { world.collideBoards(e.pos, false); e.pos = world.constrain(e.pos, false, e.hints); }
        }
        // --- « la Dame » : à l'entrée derrière nous, au bout du couloir des catacombes, puis juste avant la sortie
        if (dameDist > 0 && frame == 0) { hn.ev = 1; hn.st[1] = 1; hn.dis = -1; hn.p = pos + flat * dameDist - vec3(0, 1.65f, 0); }
        if (dameDist > 0 && hn.ev >= 0) { vec3 to = pos - hn.p; to.y = 0; if (glm::length(to) > 1e-3f) hn.f = glm::normalize(to); }
        else if (state == PLAY && live && !world.tubes.empty()) {
            const Tube& mt = world.tubes[0];
            int pr = mt.locate(pos, hauntHint); hauntHint = pr; bool onMain = !world.inHub(pos) && glm::length(pos - mt.rings[pr].c) < 6.5f;
            auto feetAt = [&](int ring) { ring = std::max(1, std::min(mt.N - 2, ring)); return mt.toWorld(ring, 0.0f, mt.floorY(ring, 0.0f)); };
            if (hn.ev < 0) {
                if (hn.st[0] == 0 && onMain && pr > 40 && pr < 110) { hn.ev = 0; hn.p = feetAt(4); }
                else if (hn.st[1] == 0 && world.corrA >= 0 && onMain && pr > world.corrA - 70 && pr < world.corrB - 14) { hn.ev = 1; hn.p = feetAt(world.corrB - 5); }
                else if (hn.st[2] == 0 && world.doorOpen && onMain && pr > world.doorRing + 4 && glm::length(pos - world.exitPos) < 24.0f) { hn.ev = 2; hn.p = feetAt(std::max(world.doorRing - 2, pr - 18)); }
                if (hn.ev >= 0) { hn.st[hn.ev] = 1; hn.dis = -1; hn.lookT = 0; }
            }
            if (hn.ev >= 0 && hn.dis < 0) {
                vec3 to = pos - hn.p; to.y = 0; float dist = glm::length(to); if (dist > 1e-3f) hn.f = to / dist;      // elle nous fixe
                vec3 toHead = hn.p + vec3(0, 2.5f, 0) - eye; float lookDot = glm::dot(glm::normalize(toHead), front);
                bool go = false, loud = true;
                if (hn.ev == 0) { if (dist < 7.5f) go = true; else if (onMain && pr > 170) { go = true; loud = false; } }
                else if (hn.ev == 1) { if (dist < 7.0f) go = true; else if (onMain && pr > world.corrB + 15) { go = true; loud = false; } }
                else { if (lookDot > 0.8f) hn.lookT += dt; if (hn.lookT > 1.0f) go = true; }
                if (go) {
                    hn.dis = 0.0f; if (loud) { audio.sfx(aud::SF_GHOST, 1.0f); camShake = std::max(camShake, 0.12f); }
                    for (int k = 0; k < 45; k++) {
                        Part q; q.p = hn.p + vec3(prng.r(-0.35f, 0.35f), prng.r(0.1f, 2.7f), prng.r(-0.35f, 0.35f)); q.v = vec3(prng.r(-0.4f, 0.4f), prng.r(0.2f, 1.0f), prng.r(-0.4f, 0.4f));
                        q.maxLife = q.life = prng.r(0.8f, 1.8f); q.seed = prng.f() * 40; q.size = 0.035f; q.grow = 0; q.kind = 2; q.inten = 12.0f; parts.push_back(q);
                    }
                }
            }
        }
        if (hn.ev >= 0 && hn.dis >= 0) { hn.dis += dt; if (hn.dis > 0.45f) { hn.st[hn.ev] = 2; hn.ev = -1; } }
        auto hauntM = [&]() { vec3 up(0, 1, 0), f = hn.f, x = glm::cross(up, f); return glm::translate(mat4(1.0f), hn.p) * mat4(mat3(x, up, f)) * glm::scale(mat4(1.0f), vec3(1.08f)); };
        if (!wallHint) for (const auto& w : world.walls) if (glm::length(pos - w.c) < 8.0f) { wallHint = true; say("UN EBOULIS BARRE LA GALERIE : ABATS LES BLOCS A LA PIOCHE !"); break; }

        // --- ramassables
        for (auto& p : world.pickups) if (p.alive && glm::length(vec3(p.p.x, 0, p.p.z) - vec3(pos.x, 0, pos.z)) < 1.3f && std::fabs(p.p.y - (pos.y - 1.0f)) < 1.8f) {
            if (p.kind == 0) { p.alive = false; keys++; audio.sfx(aud::SF_PICKUP, 1.0f); say("CLE TROUVEE  " + std::to_string(keys) + "/" + std::to_string(world.keysNeeded)); }
            else if (hp < 100) { p.alive = false; hp = std::min(100.0f, hp + 35.0f); audio.sfx(aud::SF_PICKUP, 0.8f); say("+35 PV"); }
        }
        // --- sortie
        if (state == PLAY && world.doorOpen && glm::length(pos - world.exitPos) < 4.0f) { state = WIN; audio.sfx(aud::SF_PICKUP, 1.0f); }

        // --- gouttes d'eau des stalactites
        for (auto& dr : world.drips) {
            if (glm::length(dr.tip - pos) > 32.0f) continue;
            dr.timer -= dt;
            if (dr.timer <= 0) { dr.timer = drng2.r(4.0f, 15.0f); drops.push_back(Drop{dr.tip - vec3(0, 0.03f, 0), 0.0f, dr.tube, dr.ring, 0.0f}); }
        }
        for (size_t i = 0; i < drops.size();) {
            Drop& dp = drops[i]; dp.vy -= 9.8f * dt; dp.p.y += dp.vy * dt; dp.age += dt;
            const Tube& tb = world.tubes[dp.tube]; const Ring& r = tb.rings[dp.ring]; vec3 dd = dp.p - r.c;
            float x = glm::dot(dd, r.R), yl = glm::dot(dd, r.U);
            if (yl <= tb.floorY(dp.ring, x) || dp.age > 4.0f) {
                float dist = glm::length(dp.p - pos);
                for (int k = 0; k < 3; k++) { Part q; q.p = dp.p + vec3(0, 0.02f, 0); q.v = vec3(drng2.r(-0.4f, 0.4f), drng2.r(0.8f, 1.6f), drng2.r(-0.4f, 0.4f)); q.maxLife = q.life = 0.35f; q.seed = 0; q.size = 0.045f; q.grow = 0; q.kind = 5; q.inten = lightAt(dp.p) * 0.9f + 0.03f; parts.push_back(q); }
                drops[i] = drops.back(); drops.pop_back(); continue;
            }
            i++;
        }

        int fbw, fbh; glfwGetFramebufferSize(win, &fbw, &fbh);
        if (fbw < 8 || fbh < 8) continue;
        if (fbw != tg.w || fbh != tg.h) tg.create(fbw, fbh);
        float aspect = (float)fbw / fbh;
        float pk = pitch + camKick; vec3 frontV = titleCam ? front : vec3(std::cos(yaw) * std::cos(pk), std::sin(pk), std::sin(yaw) * std::cos(pk));
        float rollA = std::max(-1.0f, std::min(1.0f, glm::dot(pvel, rightv) / 5.6f)) * 0.02f; vec3 upv = glm::normalize(vec3(0, 1, 0) + rightv * rollA);
        mat4 proj = glm::perspective(glm::radians(76.0f + 8.0f * sprintAmt), aspect, 0.05f, 900.0f);
        mat4 view = glm::lookAt(eye, eye + frontV, upv), VP = proj * view;

        // --- débris, rats, ennemis, objets
        world.camPos = eye; world.camFwd = frontV;
        world.stepDebris(dt); world.buildDebrisMesh();
        for (const vec3& ev : world.ashEvents) if (parts.size() < 2600) {      // le bois détruit noircit puis part en cendres
            Part q; q.p = ev; q.v = vec3(prng.r(-0.25f, 0.25f), prng.r(0.3f, 0.8f), prng.r(-0.25f, 0.25f)); q.maxLife = q.life = prng.r(1.6f, 3.0f);
            q.seed = prng.f() * 30; q.size = prng.r(0.14f, 0.24f); q.grow = 0.22f; q.kind = 6; q.inten = lightAt(ev) + 0.05f; parts.push_back(q);
        }
        world.ashEvents.clear();
        ratTimer -= dt;
        if (forceRat && frame == std::max(0, shotFrames - 45)) spawnRat(1.0f);
        if (ratTimer <= 0) { ratTimer = grng.r(25.0f, 70.0f); spawnRat(grng.r(2.6f, 3.8f)); }
        ratM.clearCpu();
        for (auto& r : rats) {
            r.s += r.speed * dt; r.phase += dt * r.speed * 3.0f;
            if (r.s >= r.cum.back()) { r.alive = false; continue; }
            vec3 f; vec3 p = ratPos(r, r.s, f); buildRat(ratM, p, f, r.phase);
        }
        rats.erase(std::remove_if(rats.begin(), rats.end(), [](const Rat& r) { return !r.alive; }), rats.end());
        if (ratShow) buildRat(ratM, eye + flat * 1.3f - vec3(0, 1.62f, 0) + rightv * 0.15f, rightv * 0.7f + flat * 0.7f, T * 6.0f);
        ratM.upload(GL_STREAM_DRAW);
        gEnemyT = T;
        for (auto& rg : rags) stepRagdoll(world, rg, dt);
        rags.erase(std::remove_if(rags.begin(), rags.end(), [](const Ragdoll& rg) { return rg.t > 16.0f; }), rags.end());
        enemyM.clearCpu();
        for (auto& e : enemies) if (e.alive && glm::length(e.pos - eye) < 60.0f) buildEnemy(enemyM, e);
        for (auto& rg : rags) if (glm::length(rg.C - eye) < 60.0f) addRagdoll(enemyM, rg, rg.t < 14.0f ? 1.0f : std::max(0.02f, 1.0f - (rg.t - 14.0f) / 2.0f));
        enemyM.upload(GL_STREAM_DRAW);
        propsM.clearCpu();
        for (auto& p : world.pickups) if (p.alive && glm::length(p.p - eye) < 50.0f) {
            float a = T * 1.6f + p.phase; vec3 ax(std::cos(a), 0, std::sin(a)), ay(0, 1, 0), az(-std::sin(a), 0, std::cos(a)); vec3 c = p.p + vec3(0, 0.08f * std::sin(T * 2.0f + p.phase), 0); Rng z(1);
            if (p.kind == 0) {
                addEllipsoid(propsM, c + ax * 0.13f, ax, ay, az, vec3(0.075f, 0.075f, 0.016f), 18.0f, 0.0f);
                addBoard(propsM, c - ax * 0.06f, ax, ay, az, 0.17f, 0.018f, 0.018f, 18.0f, 0.0f, z, 0);
                addBoard(propsM, c - ax * 0.19f - ay * 0.035f, ay, ax, az, 0.035f, 0.02f, 0.016f, 18.0f, 0.0f, z, 0);
                addBoard(propsM, c - ax * 0.12f - ay * 0.03f, ay, ax, az, 0.03f, 0.02f, 0.016f, 18.0f, 0.0f, z, 0);
            } else {
                addEllipsoid(propsM, c, ax, ay, az, vec3(0.05f, 0.085f, 0.05f), 19.0f, 0.0f);
                addBoard(propsM, c + ay * 0.1f, ax, ay, az, 0.022f, 0.025f, 0.022f, 1.0f, 0.2f, z, 0);
            }
        }
        propsM.upload(GL_STREAM_DRAW);

        // --- lumières : toutes FIXES (aucun scintillement)
        const float LR = 19.0f;
        lv.clear();
        const float strideL = walked * 2.1f;
        mat4 Mlan = glm::inverse(view) * glm::translate(mat4(1), vec3(-0.37f + std::cos(strideL) * 0.006f * bobAmt, -0.335f + std::sin(strideL * 2.0f) * 0.008f * bobAmt, -0.45f)) * glm::rotate(mat4(1), std::sin(strideL) * 0.05f * bobAmt + std::sin(T * 0.9f) * 0.012f, vec3(0, 0, 1)) * glm::rotate(mat4(1), 0.1f, vec3(0, 1, 0)) * glm::scale(mat4(1), vec3(0.7f));
        // palette animée du camp : les couleurs des cristaux / orbes dérivent lentement sur tout le spectre
        auto hsv = [](float h, float sat, float val) {
            h -= std::floor(h); vec3 rgb(std::min(1.0f, std::max(0.0f, std::fabs(h * 6 - 3) - 1)), std::min(1.0f, std::max(0.0f, 2 - std::fabs(h * 6 - 2))), std::min(1.0f, std::max(0.0f, 2 - std::fabs(h * 6 - 4))));
            return glm::mix(vec3(1.0f), rgb, sat) * val; };
        static const float baseHue[5] = {0.86f, 0.52f, 0.10f, 0.37f, 0.74f};
        vec3 showCol[5];
        for (int k = 0; k < 5; k++) {
            showCol[k] = hsv(baseHue[k] + T * 0.02f, 0.88f, 1.0f); float pulse = 0.5f + 0.5f * std::sin(T * 1.3f + k * 1.3f);
            for (int c = 0; c < 3; c++) palFlame[(7 + k) * 3 + c] = showCol[k][c];
            int mi = (int)(gHM.crys[k] - 21.0f); if (mi >= 0 && mi < (int)gMdlA.size()) { gMdlA[mi] = vec4(showCol[k] * 0.18f, 40.0f); gMdlE[mi] = vec4(showCol[k] * (0.55f + 0.55f * pulse), -1.0f); }
        }
        vec3 lanPos = vec3(Mlan * vec4(0, 0, 0, 1));
        if (!titleCam) lv.push_back({lanPos, vec3(1.0f, 0.74f, 0.44f) * 0.6f, 0.0f, 100000, 1.0f, 0});
        for (size_t i = 0; i < world.torches.size(); i++) {
            const Torch& t = world.torches[i]; if (t.needsDoor && !world.doorOpen) continue;
            float dist = glm::length(t.pos - eye); if (dist > 60.0f) continue;
            vec3 col = t.col; float inten = t.inten;
            if (t.biome >= 7) {                                       // cristaux du camp : couleur qui dérive + onde de luminosité qui parcourt la montagne
                int sl = t.biome - 7; float wave = 0.5f + 0.5f * std::sin(T * 1.15f + t.pos.x * 0.05f + t.pos.y * 0.035f + sl * 0.9f);
                col = showCol[sl] * 1.25f; inten *= 0.55f + 0.95f * wave;
            } else if (t.biome == HUB_BIOME && t.flame) inten *= 1.0f + 0.06f * std::sin(T * 1.6f + t.pos.x * 0.7f);       // les feux respirent doucement
            lv.push_back({t.pos, col * inten, dist, (int)i, 1.0f, t.biome});
        }
        std::sort(lv.begin(), lv.end(), [](const LV& a, const LV& b) { return a.dist < b.dist; });
        if (lv.size() > 20) lv.resize(20);
        if (titleCam || world.inHub(eye) || glm::length(eye - world.tubes[0].rings[0].c) < 24.0f) {
            for (int k = 0; k < 5; k++) {                              // orbes de couleur qui dérivent au-dessus de la place
                float a = T * (0.2f + 0.04f * k) + k * 1.256f;
                vec3 p(21.0f * std::sin(a) + 7.0f * std::sin(T * 0.31f + k), world.hubG + 2.6f + 1.7f * std::sin(T * 0.7f + k * 1.9f), world.hubFront + 27.0f + 15.0f * std::cos(a * 0.8f + k));
                lv.push_back({p, showCol[k] * (2.8f + 0.8f * std::sin(T * 0.9f + k)), glm::length(p - eye), 300000 + k, 1.0f, 7 + k});
            }
            for (int k = 0; k < 3; k++) {                              // projecteurs qui balaient la face de la montagne
                float a = T * 0.22f + k * 2.09f; int sl = (k * 2 + 1) % 5;
                vec3 p(38.0f * std::sin(a), world.hubG + 8.5f + 5.5f * std::sin(a * 1.7f + k), world.hubFront + 7.0f + 2.5f * std::sin(a * 2.1f));
                lv.push_back({p, showCol[sl] * 4.4f, glm::length(p - eye), 310000 + k, 1.0f, 7 + sl});
            }
        }
        if (hn.ev >= 0 && hn.dis < 0) { vec3 hp = vec3(hauntM() * vec4(gWomanHead, 1.0f)) + hn.f * 0.35f; lv.push_back({hp, vec3(1.0f, 0.06f, 0.04f) * 1.6f, glm::length(hp - eye), 400000, 1.0f, 12}); }
        {
            float kth = lv.size() >= (size_t)SH ? lv[SH - 1].dist : 1e9f;
            auto distOf = [&](int src) { for (auto& l : lv) if (l.src == src) return l.dist; return 1e9f; };
            for (int s = 0; s < SH; s++) if (slotLight[s] >= 0 && distOf(slotLight[s]) > 1.35f * kth + 1.0f) slotLight[s] = -1;
            for (size_t k = 0; k < lv.size() && k < (size_t)SH * 2; k++) {
                bool has = false; for (int s = 0; s < SH; s++) if (slotLight[s] == lv[k].src) has = true;
                if (has) continue;
                for (int s = 0; s < SH; s++) if (slotLight[s] < 0) { slotLight[s] = lv[k].src; break; }
            }
        }
        int nl = (int)lv.size(); GLfloat lpos[32 * 3] = {}, lcol[32 * 3] = {}; GLint lsh[32];
        for (int i = 0; i < 32; i++) lsh[i] = -1;
        for (int i = 0; i < nl; i++) {
            for (int c = 0; c < 3; c++) { lpos[i * 3 + c] = lv[i].p[c]; lcol[i * 3 + c] = lv[i].c[c]; }
            for (int s = 0; s < SH; s++) if (slotLight[s] == lv[i].src) lsh[i] = s;
        }

        // --- passes d'ombre
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo); glViewport(0, 0, SHRES, SHRES);
        glUseProgram(progShadow);
        static const vec3 fdir[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        static const vec3 fup[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
        mat4 sproj = glm::perspective(PI * 0.5f, 1.0f, 0.1f, SHFAR);
        for (int i = 0; i < nl; i++) {
            if (lsh[i] < 0) continue; vec3 lp = lv[i].p;
            glUniform3fv(U(progShadow, "uLP"), 1, glm::value_ptr(lp));
            for (int f = 0; f < 6; f++) {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, shadowTex, 0, lsh[i] * 6 + f);
                glClearColor(200, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                mat4 svp = sproj * glm::lookAt(lp, lp + fdir[f], fup[f]);
                glUniformMatrix4fv(U(progShadow, "uVP"), 1, GL_FALSE, glm::value_ptr(svp));
                auto keep = [&](const Chunk& c) { return glm::length(c.c - lp) < c.r + LR + 1.0f; };
                world.rock.draw(keep); world.wood.draw(keep); world.hubM.draw(keep); world.debrisM.drawAll(); ratM.drawAll(); enemyM.drawAll();
            }
        }

        // --- scène HDR
        glBindFramebuffer(GL_FRAMEBUFFER, tg.msFbo); glViewport(0, 0, fbw, fbh);
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const vec3 sunDir = glm::normalize(vec3(0.55f, 0.27f, 0.62f)), sunCol(1.05f, 0.58f, 0.40f), haze(0.30f, 0.17f, 0.27f);
        {   // ciel (visible depuis le camp et par l'ouverture de la mine)
            float dMouth = glm::length(eye - world.tubes[0].rings[0].c);
            float skyAmt = world.inHub(eye) ? 1.0f : std::pow(std::max(0.0f, 1.0f - dMouth / 30.0f), 2.0f);
            if (skyAmt > 0.001f) {
                glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glUseProgram(progSky); glBindVertexArray(quadVao);
                glUniformMatrix4fv(U(progSky, "uInvVP"), 1, GL_FALSE, glm::value_ptr(glm::inverse(VP))); glUniform3fv(U(progSky, "uCam"), 1, glm::value_ptr(eye));
                glUniform3fv(U(progSky, "uSunDir"), 1, glm::value_ptr(sunDir)); glUniform1f(U(progSky, "uAmt"), skyAmt); glUniform1f(U(progSky, "uTime"), T);
                glDrawArrays(GL_TRIANGLES, 0, 3); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
            }
        }
        glUseProgram(progScene);
        if (!gMdlA.empty()) { glUniform4fv(U(progScene, "uMdlA"), (GLsizei)gMdlA.size(), glm::value_ptr(gMdlA[0])); glUniform4fv(U(progScene, "uMdlE"), (GLsizei)gMdlE.size(), glm::value_ptr(gMdlE[0])); }
        glUniform3fv(U(progScene, "uSunDir"), 1, glm::value_ptr(sunDir)); glUniform3fv(U(progScene, "uSunCol"), 1, glm::value_ptr(sunCol));
        glUniform3fv(U(progScene, "uHaze"), 1, glm::value_ptr(haze)); glUniform2f(U(progScene, "uHubC"), 0.0f, world.hubFront + 10.0f); glUniform1f(U(progScene, "uHubG"), world.hubG);
        mat4 I(1.0f);
        glUniformMatrix4fv(U(progScene, "uVP"), 1, GL_FALSE, glm::value_ptr(VP)); glUniformMatrix4fv(U(progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(I));
        glUniform3fv(U(progScene, "uCam"), 1, glm::value_ptr(eye)); glUniform1f(U(progScene, "uTime"), T);
        glUniform1i(U(progScene, "uNL"), nl); glUniform3fv(U(progScene, "uLPos"), 32, lpos); glUniform3fv(U(progScene, "uLCol"), 32, lcol);
        glUniform1iv(U(progScene, "uLSh"), 32, lsh); glUniform1f(U(progScene, "uLR"), LR);
        glUniform3fv(U(progScene, "uTint"), 7, palTint); glUniform3fv(U(progScene, "uAmbB"), 7, palAmb);
        glUniform1fv(U(progScene, "uWetB"), 7, palWet); glUniform1fv(U(progScene, "uMossB"), 7, palMoss);
        glUniform1i(U(progScene, "uRockCol"), 0); glUniform1i(U(progScene, "uRockNrm"), 1);
        glUniform1i(U(progScene, "uWoodCol"), 2); glUniform1i(U(progScene, "uWoodNrm"), 3); glUniform1i(U(progScene, "uShadow"), 4);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texRockC); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texRockN);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, texWoodC); glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, texWoodN);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowTex);
        auto vis = [&](const Chunk& c) { return glm::length(c.c - eye) < c.r + 130.0f; };
        world.rock.draw(vis); world.wood.draw(vis); world.torchM.draw(vis);
        { auto visFar = [&](const Chunk& c) { return glm::length(c.c - eye) < c.r + 420.0f; }; world.hubM.draw(visFar); }
        world.debrisM.drawAll(); ratM.drawAll(); enemyM.drawAll(); propsM.drawAll();
        if (hn.ev >= 0) {
            mat4 Hw = hauntM(); glUniformMatrix4fv(U(progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(Hw)); glUniform1f(U(progScene, "uGhost"), hn.dis >= 0 ? hn.dis / 0.45f : 0.0f);
            womanM.drawAll();
            glUniformMatrix4fv(U(progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(I)); glUniform1f(U(progScene, "uGhost"), 0.0f);
        }
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE); world.webM.drawAll(); glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
        world.waterM.draw(vis);

        // --- poussière, flammes, braises, gouttes (additif)
        glEnable(GL_PROGRAM_POINT_SIZE); glBlendFunc(GL_ONE, GL_ONE);
        float px = fbh * 0.5f * proj[1][1];
        glUseProgram(progDust);
        glUniformMatrix4fv(U(progDust, "uVP"), 1, GL_FALSE, glm::value_ptr(VP)); glUniform3fv(U(progDust, "uCam"), 1, glm::value_ptr(eye));
        glUniform1f(U(progDust, "uTime"), T); glUniform1f(U(progDust, "uPx"), px);
        glUniform1i(U(progDust, "uNL"), nl); glUniform3fv(U(progDust, "uLPos"), 32, lpos); glUniform3fv(U(progDust, "uLCol"), 32, lcol);
        glUniform1f(U(progDust, "uLR"), LR);
        glBindVertexArray(dustVao); glDrawArrays(GL_POINTS, 0, 4500);

        // nuage de poussière quand du bois casse (nouveaux débris)
        static size_t lastDebris = 0;
        if (world.debris.size() > lastDebris && world.debris.size() - lastDebris >= 2) puff(world.debris.back().p, 12, 1.0f);
        lastDebris = world.debris.size();

        std::vector<float> pb;
        auto push = [&](vec3 p, float size, float kind, float seedv, float life, float inten) {
            float d[8] = {p.x, p.y, p.z, size, kind, seedv, life, inten}; pb.insert(pb.end(), d, d + 8);
        };
        for (auto& l : lv) {
            if (l.dist > 70.0f) continue;
            if (l.src == 100000) { push(l.p, 0.7f, 1, 0, 0, 0.6f); continue; }
            if (l.src == 400000) { mat4 Hw = hauntM(); for (int e = 0; e < 2; e++) push(vec3(Hw * vec4(gWomanEye[e], 1.0f)), 0.11f, 1, 0, 12.0f, 4.0f); continue; }
            if (l.src >= 300000) {                                                // orbes et projecteurs du camp
                bool beam = l.src >= 310000; push(l.p, beam ? 2.6f : 1.7f, 1, 0, (float)l.biome, beam ? 1.4f : 2.4f);
                if (!beam && prng.chance(std::min(1.0f, 6.0f * dt)) && parts.size() < 2500) {
                    Part e; e.p = l.p + vec3(prng.r(-0.25f, 0.25f), prng.r(-0.25f, 0.25f), prng.r(-0.25f, 0.25f)); e.v = vec3(prng.r(-0.5f, 0.5f), prng.r(-0.2f, 0.6f), prng.r(-0.5f, 0.5f));
                    e.maxLife = e.life = prng.r(0.9f, 2.0f); e.seed = prng.f() * 40; e.size = 0.04f; e.grow = 0; e.kind = 2; e.inten = (float)l.biome; parts.push_back(e);
                }
                continue;
            }
            if (!world.torches[l.src].flame) { push(l.p, 1.6f, 1, 0, (float)l.biome, 0.5f); continue; }
            vec3 base = l.p - vec3(0, 0.08f, 0);
            for (int j = 0; j < 3; j++) push(base + vec3(0, 0.05f * j, 0), 0.6f - 0.12f * j, 0, l.src * 1.37f + j * 0.41f, (float)l.biome, 1.0f);
            push(l.p, 2.2f, 1, 0, (float)l.biome, 1.0f);
            if (prng.chance(std::min(1.0f, 4.0f * dt)) && l.dist < 40.0f && parts.size() < 2500) {
                Part e; e.p = l.p + vec3(prng.r(-0.05f, 0.05f), 0.02f, prng.r(-0.05f, 0.05f));
                e.v = vec3(prng.r(-0.25f, 0.25f), prng.r(0.35f, 0.9f), prng.r(-0.25f, 0.25f)); e.maxLife = e.life = prng.r(1.2f, 3.2f);
                e.seed = prng.f() * 50; e.size = 0.035f; e.grow = 0; e.kind = 2; e.inten = (float)l.biome; parts.push_back(e);
            }
        }
        for (auto& p : world.pickups) if (p.alive && glm::length(p.p - eye) < 40.0f) push(p.p, 0.9f, 1, 0, p.kind == 0 ? 0.0f : 4.0f, 0.45f);
        for (auto& dp : drops) push(dp.p, 0.05f, 5, 0, 0, lightAt(dp.p) * 0.9f + 0.05f);
        for (size_t i = 0; i < parts.size();) {
            Part& e = parts[i]; e.life -= dt;
            if (e.life <= 0) { parts[i] = parts.back(); parts.pop_back(); continue; }
            if (e.kind == 2) e.v += vec3(std::sin(T * 3 + e.seed * 9), 0.15f, std::cos(T * 2.5f + e.seed * 5)) * (0.8f * dt);
            else if (e.kind == 5) e.v.y -= 9.8f * dt;
            else if (e.kind == 6) { e.v *= std::max(0.0f, 1.0f - 0.8f * dt); e.v.y += 0.25f * dt; }
            else e.v *= std::max(0.0f, 1.0f - 1.5f * dt);
            e.p += e.v * dt; e.size += e.grow * dt;
            push(e.p, e.size, (float)e.kind, e.seed, e.life / e.maxLife, e.inten); i++;
        }
        if (!pb.empty()) {
            glUseProgram(progPart);
            glUniformMatrix4fv(U(progPart, "uVP"), 1, GL_FALSE, glm::value_ptr(VP)); glUniform1f(U(progPart, "uPx"), px); glUniform1f(U(progPart, "uTime"), T);
            glUniform3fv(U(progPart, "uFlame"), 13, palFlame);
            glBindVertexArray(partVao); glBindBuffer(GL_ARRAY_BUFFER, partVbo);
            glBufferData(GL_ARRAY_BUFFER, pb.size() * 4, pb.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_POINTS, 0, (GLsizei)(pb.size() / 8));
        }
        glDisable(GL_BLEND); glDepthMask(GL_TRUE);

        // --- objets tenus en main : pioche (droite) et lanterne (gauche), par-dessus la scène
        if (!titleCam) {
            glClear(GL_DEPTH_BUFFER_BIT);
            glUseProgram(progScene);
            float wA = 0, sA = 0, rA = 0;                        // armement / frappe / récupération (0..1)
            if (swingT >= 0) { if (swingT < 0.18f) wA = swingT / 0.18f; else { wA = 1; if (swingT < 0.28f) sA = (swingT - 0.18f) / 0.10f; else { sA = 1; rA = std::min(1.0f, (swingT - 0.28f) / 0.36f); } } }
            float eW = easeOutF(wA), eS = easeInF(sA), eR = rA * rA * (3.0f - 2.0f * rA);
            auto ph = [&](float vUp, float vHit) { float v = vUp * eW; if (sA > 0) v = mixf(vUp, vHit, eS); if (rA > 0) v = vHit * (1.0f - eR); return v; };
            float rx = ph(0.85f, -1.05f), rz = ph(0.30f, -0.12f), ty = ph(0.03f, -0.04f), tz = ph(-0.02f, -0.20f), tx = ph(-0.10f, -0.10f);
            float bobX = std::cos(strideL) * 0.006f * bobAmt, bobY = std::sin(strideL * 2.0f) * 0.010f * bobAmt + std::sin(T * 1.3f) * 0.002f;
            mat4 M = glm::inverse(view) * glm::translate(mat4(1), vec3(0.31f + tx + bobX, -0.43f + ty + bobY, -0.60f + tz))
                   * glm::rotate(mat4(1), 0.14f + rz, vec3(0, 0, 1)) * glm::rotate(mat4(1), -0.50f + rx, vec3(1, 0, 0)) * glm::rotate(mat4(1), 0.45f + PI * 0.5f, vec3(0, 1, 0)) * glm::scale(mat4(1), vec3(0.86f));
            glUniformMatrix4fv(U(progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(M));
            axeM.drawAll();
            glUniformMatrix4fv(U(progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(Mlan));
            lanternM.drawAll();
        }
        glDisable(GL_DEPTH_TEST);

        // --- résolution MSAA + bloom + composition
        glBindFramebuffer(GL_READ_FRAMEBUFFER, tg.msFbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tg.resFbo);
        glBlitFramebuffer(0, 0, fbw, fbh, 0, 0, fbw, fbh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindVertexArray(quadVao); glActiveTexture(GL_TEXTURE0); glDisable(GL_BLEND);
        glUseProgram(progDown); glUniform1i(U(progDown, "uTex"), 0);
        GLuint src = tg.resTex; int sw2 = fbw, sh2 = fbh;
        for (int i = 0; i < BL; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, tg.blFbo[i]); glViewport(0, 0, tg.blW[i], tg.blH[i]);
            glBindTexture(GL_TEXTURE_2D, src);
            glUniform2f(U(progDown, "uTexel"), 1.0f / sw2, 1.0f / sh2); glUniform1i(U(progDown, "uFirst"), i == 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            src = tg.blTex[i]; sw2 = tg.blW[i]; sh2 = tg.blH[i];
        }
        glUseProgram(progUp); glUniform1i(U(progUp, "uTex"), 0); glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
        for (int i = BL - 2; i >= 0; i--) {
            glBindFramebuffer(GL_FRAMEBUFFER, tg.blFbo[i]); glViewport(0, 0, tg.blW[i], tg.blH[i]);
            glBindTexture(GL_TEXTURE_2D, tg.blTex[i + 1]); glUniform2f(U(progUp, "uTexel"), 1.0f / tg.blW[i + 1], 1.0f / tg.blH[i + 1]);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0, 0, fbw, fbh);
        glUseProgram(progComp);
        glUniform1i(U(progComp, "uHdr"), 0); glUniform1i(U(progComp, "uBloom"), 1);
        glUniform1f(U(progComp, "uTime"), T); glUniform1f(U(progComp, "uAspect"), aspect); glUniform1f(U(progComp, "uBloomK"), 0.55f);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tg.resTex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, tg.blTex[0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glActiveTexture(GL_TEXTURE0);

        // --- interface : écrans (titre / pause / réglages / commandes) et HUD (coeurs, clés, carte, messages)
        {
            ui.begin(fbw, fbh);
            const float fw = (float)fbw, fh = (float)fbh;
            const float sc = std::max(2.0f, std::floor(fh / 300.0f)), cx = fw * 0.5f, cy = fh * 0.5f;
            const float ts = std::max(2.0f, std::floor(fh / 330.0f)), bt = std::max(2.0f, std::floor(fh / 270.0f));
            {   // souris (pixels du framebuffer)
                int ww, wh; glfwGetWindowSize(win, &ww, &wh); double mxw, myw; glfwGetCursorPos(win, &mxw, &myw);
                float nx = (float)(mxw * fw / std::max(ww, 1)), ny = (float)(myw * fh / std::max(wh, 1));
                ms.moved = std::fabs(nx - ms.x) + std::fabs(ny - ms.y) > 0.5f; ms.x = nx; ms.y = ny;
                bool md = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS; ms.click = md && !ms.down; ms.down = md; if (!md) dragId = -1;
            }
            btnCount = 0;
            auto inside = [&](float x, float y, float w, float h) { return ms.x >= x && ms.x <= x + w && ms.y >= y && ms.y <= y + h; };
            auto panel = [&](float x, float y, float w, float h, float a) {
                const float t0[4] = {0.17f, 0.10f, 0.06f, 0.96f * a}, b0[4] = {0.07f, 0.04f, 0.03f, 0.97f * a};
                ui.rect(x - 7, y - 7, w + 14, h + 14, 0.0f, 0.0f, 0.0f, 0.55f * a); ui.rect(x - 4, y - 4, w + 8, h + 8, 0.80f, 0.52f, 0.16f, a);
                ui.rect(x - 2, y - 2, w + 4, h + 4, 0.10f, 0.05f, 0.02f, a); ui.rectGrad(x, y, w, h, t0, t0, b0, b0);
            };
            auto button = [&](const std::string& label, float x, float y, float w, float h) -> bool {
                int id = btnCount++; bool in = inside(x, y, w, h); if (in && ms.moved) menuSel = id; bool hot = (menuSel == id);
                if (hot) ui.rect(x - 8, y - 8, w + 16, h + 16, 1.0f, 0.55f, 0.15f, 0.20f + 0.10f * std::sin(T * 6.0f));
                ui.rect(x - 3, y - 3, w + 6, h + 6, 0.02f, 0.01f, 0.0f, 0.92f);
                const float ta[4] = {0.52f, 0.28f, 0.08f, 0.96f}, ba[4] = {0.32f, 0.15f, 0.04f, 0.96f}, tb[4] = {0.20f, 0.12f, 0.07f, 0.92f}, bb[4] = {0.11f, 0.065f, 0.04f, 0.92f};
                ui.rectGrad(x, y, w, h, hot ? ta : tb, hot ? ta : tb, hot ? ba : bb, hot ? ba : bb);
                ui.rect(x, y, w, 3, 1.0f, 0.82f, 0.45f, hot ? 0.9f : 0.3f); ui.rect(x, y + h - 3, w, 3, 0, 0, 0, 0.45f);
                float tw = ui.textWidth(label, bt) - bt; ui.text(x + (w - tw) * 0.5f + bt * 0.5f, y + (h - 7 * bt) * 0.5f + bt * 0.5f, bt, label, 0, 0, 0, 0.55f);
                ui.text(x + (w - tw) * 0.5f, y + (h - 7 * bt) * 0.5f, bt, label, hot ? 1.0f : 0.92f, hot ? 0.95f : 0.78f, hot ? 0.72f : 0.55f, 1.0f);
                bool act = (in && ms.click) || (hot && uiEnter); if (act) { audio.sfx(aud::SF_CLICK, 0.8f); uiEnter = false; ms.click = false; } return act;
            };
            auto slider = [&](const char* label, float& v, float lo, float hi, float step, float x, float y, float w, float h, bool pct) {
                int id = btnCount++; bool in = inside(x - 10, y - 6, w + 20, h + 12); if (in && ms.moved) menuSel = id; bool hot = (menuSel == id);
                float tx = x + w * 0.46f, tw = w * 0.40f; bool onTrack = inside(tx - 14, y - 8, tw + 28, h + 16);
                if (ms.click && onTrack) { dragId = id; menuSel = id; }
                if (dragId == id && ms.down) v = lo + (hi - lo) * std::min(1.0f, std::max(0.0f, (ms.x - tx) / tw));
                if (hot) { if (uiLeft) v = std::max(lo, v - step); if (uiRight) v = std::min(hi, v + step); }
                if (hot) ui.rect(x - 12, y - 8, w + 24, h + 16, 1.0f, 0.55f, 0.15f, 0.14f);
                ui.text(x, y + (h - 7 * bt) * 0.5f, bt, label, 1.0f, 0.88f, 0.62f, 1.0f);
                float f = (v - lo) / (hi - lo), th = h * 0.34f, ty = y + (h - th) * 0.5f;
                ui.rect(tx - 2, ty - 2, tw + 4, th + 4, 0.0f, 0.0f, 0.0f, 0.8f); ui.rect(tx, ty, tw, th, 0.16f, 0.09f, 0.05f, 1.0f);
                const float fa[4] = {1.0f, 0.78f, 0.28f, 1.0f}, fb[4] = {0.85f, 0.38f, 0.07f, 1.0f}; ui.rectGrad(tx, ty, tw * f, th, fa, fa, fb, fb);
                ui.rect(tx + tw * f - 5, y + 2, 10, h - 4, 0.0f, 0.0f, 0.0f, 0.9f); ui.rect(tx + tw * f - 3, y + 4, 6, h - 8, 1.0f, 0.92f, 0.7f, 1.0f);
                char buf[24]; if (pct) std::snprintf(buf, sizeof buf, "%d%%", (int)std::lround(v * 100.0f)); else std::snprintf(buf, sizeof buf, "%.1fX", v);
                ui.text(tx + tw + 22, y + (h - 7 * bt) * 0.5f, bt, buf, 1.0f, 0.95f, 0.8f, 1.0f);
            };
            auto toggle = [&](const char* label, bool& v, float x, float y, float w, float h) {
                int id = btnCount++; bool in = inside(x - 10, y - 6, w + 20, h + 12); if (in && ms.moved) menuSel = id; bool hot = (menuSel == id);
                if (hot) ui.rect(x - 12, y - 8, w + 24, h + 16, 1.0f, 0.55f, 0.15f, 0.14f);
                ui.text(x, y + (h - 7 * bt) * 0.5f, bt, label, 1.0f, 0.88f, 0.62f, 1.0f);
                float bx = x + w * 0.46f, bw = h * 1.9f;
                ui.rect(bx - 2, y + 2, bw + 4, h - 4, 0, 0, 0, 0.8f); ui.rect(bx, y + 4, bw, h - 8, v ? 0.75f : 0.2f, v ? 0.4f : 0.12f, v ? 0.08f : 0.08f, 1.0f);
                float kx = v ? bx + bw - (h - 8) : bx; ui.rect(kx, y + 4, h - 8, h - 8, 1.0f, 0.92f, 0.7f, 1.0f);
                ui.text(bx + bw + 22, y + (h - 7 * bt) * 0.5f, bt, v ? "OUI" : "NON", 1.0f, 0.95f, 0.8f, 1.0f);
                if ((in && ms.click) || (hot && (uiEnter || uiLeft || uiRight))) { v = !v; audio.sfx(aud::SF_CLICK, 0.8f, 1); }
            };
            auto vignette = [&](float a, float r, float g, float b, float k) {
                const float c0[4] = {r, g, b, a}, c1[4] = {r, g, b, 0.0f}; float th = fh * 0.30f * k, tw = fw * 0.20f * k;
                ui.rectGrad(0, 0, fw, th, c0, c0, c1, c1); ui.rectGrad(0, fh - th, fw, th, c1, c1, c0, c0);
                ui.rectGrad(0, 0, tw, fh, c0, c1, c1, c0); ui.rectGrad(fw - tw, 0, tw, fh, c1, c0, c0, c1);
            };
            auto keyIcon = [&](float x, float y, float s, bool gold) {
                float r = gold ? 1.0f : 0.25f, g = gold ? 0.78f : 0.2f, b = gold ? 0.18f : 0.15f, a = gold ? 1.0f : 0.7f;
                ui.disc(x + 3 * s, y + 3 * s, 3.2f * s, 0.05f, 0.03f, 0.0f, 0.8f); ui.disc(x + 3 * s, y + 3 * s, 2.5f * s, r, g, b, a); ui.disc(x + 3 * s, y + 3 * s, 1.0f * s, 0.1f, 0.06f, 0.02f, 1.0f);
                ui.rect(x + 5 * s, y + 2.2f * s, 9 * s, 1.6f * s, r, g, b, a); ui.rect(x + 11 * s, y + 3.8f * s, 1.6f * s, 2.4f * s, r, g, b, a); ui.rect(x + 13.6f * s, y + 3.8f * s, 1.6f * s, 3.0f * s, r, g, b, a);
            };
            const bool titleCamHud = titleCam;

            // =================================================== HUD de jeu
            if (!titleCamHud) {
                float hs = std::max(3.0f, std::floor(fh / 190.0f)), gap = hs * 1.6f, x0 = fh * 0.035f, y0 = fh * 0.035f;
                for (int i = 0; i < 10; i++) {                                           // 10 coeurs = 100 PV
                    int fill = hp >= (i + 1) * 10 - 0.01f ? 2 : (hp >= i * 10 + 5 - 0.01f ? 1 : 0);
                    float bob = (hp < 30.0f && fill > 0 && live) ? std::sin(T * 10.0f + i) * hs * 0.3f : 0.0f;
                    ui.heart(x0 + i * (7 * hs + gap), y0 + bob, hs, fill);
                }
                float ky = y0 + 7 * hs + hs * 2.4f;
                for (int k = 0; k < world.keysNeeded; k++) keyIcon(x0 + k * hs * 6.0f, ky, hs * 0.55f, k < keys);
                ui.text(x0, ky + hs * 4.4f, sc * 0.8f, "ENNEMIS TUES " + std::to_string(kills), 0.95f, 0.8f, 0.4f, 0.9f);
                if (live && biomeBanner > 0) { std::string n = BIOMES[std::min(6, std::max(0, curBiome))].name; float a = std::min(1.0f, biomeBanner); ui.text(cx - ui.textWidth(n, sc * 1.6f) * 0.5f, fh * 0.12f, sc * 1.6f, n, 0.95f, 0.88f, 0.7f, a); }
                if (msgT > 0) ui.text(cx - ui.textWidth(msg, sc) * 0.5f, fh * 0.72f, sc, msg, 1, 0.9f, 0.6f, std::min(1.0f, msgT));
                else if (state == PLAY && live && !world.doorOpen && glm::length(pos - world.doorPos) < 3.6f) { std::string h = "E : OUVRIR LA PORTE"; ui.text(cx - ui.textWidth(h, sc) * 0.5f, fh * 0.72f, sc, h, 1, 1, 1, 0.9f); }
                ui.rect(cx - 3.5f, cy - 3.5f, 7, 7, 0, 0, 0, 0.55f); ui.rect(cx - 2, cy - 2, 4, 4, 1, 1, 1, 0.95f);
                // coup reçu : rouge sur le pourtour de l'écran ; vie basse : pulsation + battement de coeur
                if (hurtFlash > 0) { vignette(std::min(0.92f, hurtFlash * 1.6f), 0.85f, 0.02f, 0.02f, 1.0f); ui.rect(0, 0, fw, fh, 0.8f, 0.03f, 0.02f, std::min(0.22f, hurtFlash * 0.3f)); }
                if (hp < 30 && state == PLAY) {
                    vignette(0.30f + 0.18f * std::sin(T * 6.0f), 0.6f, 0.0f, 0.0f, 0.8f);
                    if (live) { heartT -= rdt; if (heartT <= 0) { audio.sfx(aud::SF_HEART, 0.8f); heartT = 0.9f; } }
                }

                // ------------------- minicarte (en bas à droite) : galeries découvertes, clés, porte
                {
                    float msz = std::max(190.0f, std::min(fh * 0.30f, 380.0f)), mx_ = fw - msz - fh * 0.03f, my_ = fh - msz - fh * 0.05f, half = msz * 0.5f, mcx = mx_ + half, mcy = my_ + half;
                    float k = half / 40.0f;                                               // 40 m de rayon, la carte tourne avec le regard
                    auto toMap = [&](float wx, float wz) { float dx = wx - pos.x, dz = wz - pos.z; return vec2(mcx + (dx * rightv.x + dz * rightv.z) * k, mcy - (dx * flat.x + dz * flat.z) * k); };
                    ui.rect(mx_ - 5, my_ - 5, msz + 10, msz + 10, 0.0f, 0.0f, 0.0f, 0.55f); ui.rect(mx_, my_, msz, msz, 0.04f, 0.045f, 0.06f, 0.82f);
                    ui.flush();
                    // camp : cases découvertes, montagne
                    {
                        vec2 a = toMap(-90.0f, world.hubFront), b = toMap(90.0f, world.hubFront), c = toMap(90.0f, world.hubFront - 70.0f), d = toMap(-90.0f, world.hubFront - 70.0f);
                        ui.quad(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y, 0.20f, 0.19f, 0.21f, 0.95f);
                        for (int cz = 0; cz < 60; cz++) for (int cxx = 0; cxx < 80; cxx++) if (hubSeen[(size_t)cz * 80 + cxx]) {
                            float wx = -160.0f + cxx * 4.0f, wz = world.hubFront - 8.0f + cz * 4.0f; if (std::fabs(wx - pos.x) > 60 || std::fabs(wz - pos.z) > 60) continue;
                            vec2 p0 = toMap(wx, wz), p1 = toMap(wx + 4, wz), p2 = toMap(wx + 4, wz + 4), p3 = toMap(wx, wz + 4);
                            ui.quad(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, 0.16f, 0.30f, 0.13f, 0.95f);
                        }
                    }
                    // galeries
                    for (size_t t = 0; t < world.tubes.size(); t++) {
                        const Tube& tb = world.tubes[t]; if (mapSeen[t].size() != (size_t)tb.N) continue;
                        for (int i = 0; i + 1 < tb.N; i++) {
                            if (!mapSeen[t][i] || !mapSeen[t][i + 1]) continue;
                            if (std::fabs(tb.rings[i].c.x - pos.x) > 60.0f || std::fabs(tb.rings[i].c.z - pos.z) > 60.0f) continue;
                            vec4 e0 = mapEdge[t][i], e1 = mapEdge[t][i + 1]; vec2 l0 = toMap(e0.x, e0.y), r0 = toMap(e0.z, e0.w), l1 = toMap(e1.x, e1.y), r1 = toMap(e1.z, e1.w);
                            ui.quad(l0.x, l0.y, r0.x, r0.y, r1.x, r1.y, l1.x, l1.y, 0.20f, 0.24f, 0.30f, 1.0f);
                            ui.line(l0.x, l0.y, l1.x, l1.y, 2.0f, 0.62f, 0.78f, 0.90f, 1.0f); ui.line(r0.x, r0.y, r1.x, r1.y, 2.0f, 0.62f, 0.78f, 0.90f, 1.0f);
                        }
                    }
                    // porte, sortie, clés (toujours visibles pour guider le joueur)
                    auto marker = [&](vec3 wp, auto draw, bool arrow) {
                        vec2 m = toMap(wp.x, wp.z); float dx = m.x - mcx, dy = m.y - mcy, lim = half - 12.0f, mx2 = std::max(std::fabs(dx), std::fabs(dy));
                        if (mx2 > lim) { float f = lim / mx2; float ox = dx, oy = dy; m = vec2(mcx + dx * f, mcy + dy * f); if (arrow) { float l = std::sqrt(ox * ox + oy * oy); float ux = ox / l, uy = oy / l;
                            ui.tri(m.x + ux * 9, m.y + uy * 9, m.x - uy * 6 - ux * 3, m.y + ux * 6 - uy * 3, m.x + uy * 6 - ux * 3, m.y - ux * 6 - uy * 3, 1.0f, 0.82f, 0.2f, 0.95f); } }
                        draw(m);
                    };
                    for (auto& pk : world.pickups) if (pk.alive && pk.kind == 0) marker(pk.p, [&](vec2 m) {
                        float pu = 0.5f + 0.5f * std::sin(T * 4.0f + pk.phase); ui.disc(m.x, m.y, 9.0f + 3.0f * pu, 1.0f, 0.78f, 0.15f, 0.22f); keyIcon(m.x - 7, m.y - 4, 1.0f, true); }, true);
                    if (!world.doorOpen) marker(world.doorPos, [&](vec2 m) { ui.rect(m.x - 6, m.y - 6, 12, 12, 0.0f, 0.0f, 0.0f, 0.9f); ui.rect(m.x - 4.5f, m.y - 4.5f, 9, 9, 0.62f, 0.38f, 0.16f, 1.0f); ui.rect(m.x - 1, m.y - 1, 2, 2, 0.1f, 0.05f, 0.0f, 1.0f); }, false);
                    else marker(world.exitPos, [&](vec2 m) { ui.disc(m.x, m.y, 7.0f, 0.55f, 0.85f, 1.0f, 0.95f); ui.disc(m.x, m.y, 3.5f, 1.0f, 1.0f, 1.0f, 1.0f); }, true);
                    // joueur
                    ui.tri(mcx, mcy - 10, mcx - 6.5f, mcy + 7, mcx + 6.5f, mcy + 7, 0.0f, 0.0f, 0.0f, 0.9f); ui.tri(mcx, mcy - 7.5f, mcx - 4.5f, mcy + 5, mcx + 4.5f, mcy + 5, 1.0f, 1.0f, 1.0f, 1.0f);
                    ui.flushClip(mx_, my_, msz, msz);
                    // cadre, boussole, légende
                    ui.rect(mx_ - 5, my_ - 5, msz + 10, 5, 0.80f, 0.52f, 0.16f, 1.0f); ui.rect(mx_ - 5, my_ + msz, msz + 10, 5, 0.80f, 0.52f, 0.16f, 1.0f);
                    ui.rect(mx_ - 5, my_, 5, msz, 0.80f, 0.52f, 0.16f, 1.0f); ui.rect(mx_ + msz, my_, 5, msz, 0.80f, 0.52f, 0.16f, 1.0f);
                    { vec2 n = toMap(pos.x, pos.z - 100.0f); float dx = n.x - mcx, dy = n.y - mcy, l = std::sqrt(dx * dx + dy * dy); float nx = mcx + dx / l * (half - 11), ny = mcy + dy / l * (half - 11);
                      ui.disc(nx, ny, 9.0f, 0.0f, 0.0f, 0.0f, 0.7f); ui.text(nx - 2.5f * sc * 0.5f * 1.4f, ny - 3.5f * sc * 0.5f * 1.4f, sc * 0.5f * 1.4f, "N", 1.0f, 0.5f, 0.3f, 1.0f); }
                    ui.text(mx_, my_ - 8 * sc * 0.7f, sc * 0.7f, "CARTE", 0.95f, 0.8f, 0.45f, 0.95f);
                }
                if (state == DEAD) {
                    ui.rect(0, 0, fw, fh, 0.02f, 0.0f, 0.0f, 0.72f);
                    std::string a = "VOUS ETES MORT", b = "ENTREE : RETOUR AU CAMP   -   R : NOUVELLE GROTTE";
                    ui.text(cx - ui.textWidth(a, sc * 3) * 0.5f, cy - 40 * sc, sc * 3, a, 0.9f, 0.1f, 0.1f, 1); ui.text(cx - ui.textWidth(b, sc) * 0.5f, cy + 10 * sc, sc, b, 1, 0.9f, 0.8f, 1);
                } else if (state == WIN) {
                    ui.rect(0, 0, fw, fh, 0.9f, 0.95f, 1.0f, 0.55f);
                    std::string a = "VOUS ETES SORTI !", b = "TEMPS " + std::to_string((int)playTime) + " S   ENNEMIS TUES " + std::to_string(kills), c = "ENTREE : REJOUER   -   R : NOUVELLE GROTTE";
                    ui.text(cx - ui.textWidth(a, sc * 3) * 0.5f, cy - 40 * sc, sc * 3, a, 0.1f, 0.12f, 0.2f, 1);
                    ui.text(cx - ui.textWidth(b, sc) * 0.5f, cy + 8 * sc, sc, b, 0.1f, 0.12f, 0.2f, 1); ui.text(cx - ui.textWidth(c, sc) * 0.5f, cy + 24 * sc, sc, c, 0.1f, 0.12f, 0.2f, 1);
                }
            }

            // =================================================== écran titre
            auto drawLogo = [&](float yTop, float scl) {
                const std::string name = "GRIMDEEP"; float w = ui.textWidth(name, scl) - scl, x = cx - w * 0.5f, pulse = 0.5f + 0.5f * std::sin(T * 1.6f);
                ui.textGlow(x, yTop, scl, name, 1.0f, 0.42f, 0.06f, scl * 0.9f, 0.10f + 0.05f * pulse);
                for (int e = 7; e >= 1; e--) ui.text(x + e * scl * 0.10f, yTop + e * scl * 0.15f, scl, name, 0.20f + 0.02f * (7 - e), 0.045f, 0.015f, 1.0f);       // extrusion
                const float top[3] = {1.0f, 0.93f, 0.55f}, bot[3] = {0.95f, 0.30f, 0.04f}; ui.textGrad(x, yTop, scl, name, top, bot, 1.0f);
                const std::string sub = "A DWARF IN A CAVE"; float ss = std::max(2.0f, std::floor(scl / 4.0f)), sw = ui.textWidth(sub, ss) - ss, sy = yTop + 7 * scl + scl * 0.9f;
                ui.rect(cx - w * 0.5f, sy + 3.5f * ss - 1, (w - sw) * 0.5f - ss * 3, 3, 1.0f, 0.7f, 0.25f, 0.8f); ui.rect(cx + sw * 0.5f + ss * 3, sy + 3.5f * ss - 1, (w - sw) * 0.5f - ss * 3, 3, 1.0f, 0.7f, 0.25f, 0.8f);
                ui.text(cx - sw * 0.5f + ss * 0.4f, sy + ss * 0.4f, ss, sub, 0.0f, 0.0f, 0.0f, 0.6f); ui.text(cx - sw * 0.5f, sy, ss, sub, 1.0f, 0.88f, 0.6f, 1.0f);
            };
            const float bw = std::min(fw * 0.34f, 120.0f * bt), bh = 11.0f * bt, bgap = bh * 0.28f;
            if (screen == SC_TITLE) {
                const float dk[4] = {0, 0, 0, 0.62f}, cl[4] = {0, 0, 0, 0.0f};
                ui.rectGrad(0, 0, fw, fh * 0.42f, dk, dk, cl, cl); ui.rectGrad(0, fh * 0.60f, fw, fh * 0.40f, cl, cl, dk, dk);
                for (int i = 0; i < 46; i++) {                 // braises qui montent
                    float ph = i * 1.618f, life = std::fmod(T * (0.05f + 0.04f * std::fmod(ph, 1.0f)) + ph, 1.0f);
                    float ex = fw * std::fmod(ph * 0.371f + 0.05f * std::sin(T * 0.8f + ph * 7.0f), 1.0f), ey = fh * (1.02f - life * 1.04f), sz = (2.0f + 3.0f * std::fmod(ph * 2.3f, 1.0f)) * fh / 900.0f;
                    float a = std::sin(life * 3.14159f) * (0.55f + 0.35f * std::sin(T * 6.0f + ph * 11.0f));
                    ui.rect(ex - sz, ey - sz, sz * 2, sz * 2, 1.0f, 0.45f + 0.35f * std::fmod(ph, 1.0f), 0.08f, a * 0.35f); ui.rect(ex - sz * 0.5f, ey - sz * 0.5f, sz, sz, 1.0f, 0.85f, 0.45f, a);
                }
                drawLogo(fh * 0.07f, std::max(6.0f, std::floor(fh / 52.0f)));
                float y = fh * 0.50f, x = cx - bw * 0.5f;
                if (button("JOUER", x, y, bw, bh)) {
                    if (worldDirty) restart(true); else { hp = 100; keys = 0; kills = 0; state = PLAY; playTime = 0; hurtFlash = 0; msg.clear(); placeHub(); curBiome = -1; }
                    worldDirty = true; screen = SC_PLAY; menuSel = 0; for (int s2 = 0; s2 < SH; s2++) slotLight[s2] = -1; rats.clear(); say("TROUVE LES 3 CLES DANS LA MINE, OUVRE LA GRANDE PORTE ET SORS !");
                }
                y += bh + bgap; if (button("PARAMETRES", x, y, bw, bh)) { backTo = SC_TITLE; screen = SC_SETTINGS; menuSel = 0; }
                y += bh + bgap; if (button("COMMANDES", x, y, bw, bh)) { backTo = SC_TITLE; screen = SC_CONTROLS; menuSel = 0; }
                y += bh + bgap; if (button("QUITTER", x, y, bw, bh)) glfwSetWindowShouldClose(win, GLFW_TRUE);
                std::string f1 = "FLECHES / SOURIS : NAVIGUER    ENTREE / CLIC : VALIDER    F11 : PLEIN ECRAN";
                ui.text(cx - ui.textWidth(f1, ts) * 0.5f, fh - ts * 14, ts, f1, 0.9f, 0.75f, 0.5f, 0.85f);
            }
            // =================================================== pause
            if (screen == SC_PAUSE) {
                ui.rect(0, 0, fw, fh, 0.0f, 0.0f, 0.0f, 0.55f); vignette(0.5f, 0.0f, 0.0f, 0.0f, 1.0f);
                float pw = bw + bt * 20, ph = bh * 5 + bgap * 4 + bt * 36, px0 = cx - pw * 0.5f, py0 = cy - ph * 0.5f; panel(px0, py0, pw, ph, 1.0f);
                std::string t0 = "PAUSE"; const float top[3] = {1.0f, 0.9f, 0.5f}, bot[3] = {0.95f, 0.4f, 0.08f};
                ui.textGrad(cx - ui.textWidth(t0, bt * 1.8f) * 0.5f, py0 + bt * 7, bt * 1.8f, t0, top, bot, 1.0f);
                float y = py0 + bt * 26, x = cx - bw * 0.5f;
                if (button("REPRENDRE", x, y, bw, bh)) { screen = SC_PLAY; menuSel = 0; }
                y += bh + bgap; if (button("PARAMETRES", x, y, bw, bh)) { backTo = SC_PAUSE; screen = SC_SETTINGS; menuSel = 0; }
                y += bh + bgap; if (button("COMMANDES", x, y, bw, bh)) { backTo = SC_PAUSE; screen = SC_CONTROLS; menuSel = 0; }
                y += bh + bgap; if (button("MENU PRINCIPAL", x, y, bw, bh)) { screen = SC_TITLE; menuSel = 0; }
                y += bh + bgap; if (button("QUITTER", x, y, bw, bh)) glfwSetWindowShouldClose(win, GLFW_TRUE);
            }
            // =================================================== paramètres
            if (screen == SC_SETTINGS) {
                ui.rect(0, 0, fw, fh, 0.0f, 0.0f, 0.0f, 0.45f);
                float pw = std::min(fw * 0.7f, 260.0f * bt), rowH = bh * 1.3f, ph = rowH * 6.2f + bt * 38, px0 = cx - pw * 0.5f, py0 = cy - ph * 0.5f; panel(px0, py0, pw, ph, 1.0f);
                std::string t0 = "PARAMETRES"; const float top[3] = {1.0f, 0.9f, 0.5f}, bot[3] = {0.95f, 0.4f, 0.08f};
                ui.textGrad(cx - ui.textWidth(t0, bt * 1.6f) * 0.5f, py0 + bt * 6, bt * 1.6f, t0, top, bot, 1.0f);
                float x = px0 + bt * 10, w = pw - bt * 20, y = py0 + bt * 28;
                slider("VOLUME GENERAL", cfg.master, 0.0f, 1.0f, 0.05f, x, y, w, rowH * 0.8f, true); y += rowH;
                slider("MUSIQUE", cfg.music, 0.0f, 1.0f, 0.05f, x, y, w, rowH * 0.8f, true); y += rowH;
                slider("EFFETS SONORES", cfg.sfx, 0.0f, 1.0f, 0.05f, x, y, w, rowH * 0.8f, true); y += rowH;
                slider("SENSIBILITE SOURIS", cfg.sens, 0.2f, 2.5f, 0.1f, x, y, w, rowH * 0.8f, false); y += rowH;
                toggle("PLEIN ECRAN (F11)", cfg.fullscreen, x, y, w, rowH * 0.8f); y += rowH * 1.25f;
                if (button("RETOUR", cx - bw * 0.35f, y, bw * 0.7f, bh)) { screen = backTo; saveSettings(cfg); menuSel = 0; }
            }
            // =================================================== commandes
            if (screen == SC_CONTROLS) {
                ui.rect(0, 0, fw, fh, 0.0f, 0.0f, 0.0f, 0.5f);
                struct Row { const char* key; const char* what; };
                static const Row rows[] = {{"ZQSD / WASD", "SE DEPLACER"}, {"SOURIS", "REGARDER AUTOUR DE SOI"}, {"MAJ", "COURIR"}, {"CLIC GAUCHE", "COUP DE PIOCHE (EBOULIS, PLANCHES, MONSTRES)"},
                    {"E", "OUVRIR LA GRANDE PORTE (3 CLES)"}, {"F", "MARCHE / VOL LIBRE (ESPACE, CTRL)"}, {"ECHAP", "PAUSE ET MENU"},
                    {"F11", "PLEIN ECRAN"}, {"F12", "CAPTURE D'ECRAN"}, {"R", "NOUVELLE GROTTE (RETOUR AU CAMP)"}, {"ENTREE", "RECOMMENCER APRES LA MORT"}};
                const int nr = (int)(sizeof(rows) / sizeof(rows[0])); float rowH = ts * 14.0f, pw = std::min(fw * 0.9f, 330.0f * ts), ph = rowH * (nr + 3.4f) + bh + ts * 22, px0 = cx - pw * 0.5f, py0 = cy - ph * 0.5f; panel(px0, py0, pw, ph, 1.0f);
                std::string t0 = "COMMANDES"; const float top[3] = {1.0f, 0.9f, 0.5f}, bot[3] = {0.95f, 0.4f, 0.08f};
                ui.textGrad(cx - ui.textWidth(t0, bt * 1.6f) * 0.5f, py0 + bt * 5, bt * 1.6f, t0, top, bot, 1.0f);
                float y = py0 + bt * 24, kx = px0 + ts * 8, dx = px0 + pw * 0.30f;
                for (int i = 0; i < nr; i++) {
                    float kw = ui.textWidth(rows[i].key, ts) + ts * 6;
                    ui.rect(kx - 2, y - 2, kw + 4, ts * 10.5f + 4, 0, 0, 0, 0.8f);
                    const float t1[4] = {0.55f, 0.50f, 0.42f, 1.0f}, b1[4] = {0.30f, 0.26f, 0.21f, 1.0f}; ui.rectGrad(kx, y, kw, ts * 10.5f, t1, t1, b1, b1);
                    ui.rect(kx, y, kw, 2, 1, 1, 1, 0.35f); ui.text(kx + ts * 3, y + ts * 1.7f, ts, rows[i].key, 1.0f, 0.97f, 0.88f, 1.0f);
                    ui.text(dx, y + ts * 1.7f, ts, rows[i].what, 0.95f, 0.82f, 0.58f, 1.0f); y += rowH;
                }
                std::string obj = "OBJECTIF : TROUVE LES 3 CLES DANS LA MINE, OUVRE LA GRANDE PORTE", obj2 = "ET SORS A LA LUMIERE. LA CARTE (EN BAS A DROITE) MONTRE LES CLES !";
                ui.text(cx - ui.textWidth(obj, ts) * 0.5f, y + ts * 2, ts, obj, 0.7f, 0.9f, 1.0f, 1.0f); ui.text(cx - ui.textWidth(obj2, ts) * 0.5f, y + ts * 2 + rowH * 0.9f, ts, obj2, 0.7f, 0.9f, 1.0f, 1.0f); y += rowH * 2.5f;
                if (button("RETOUR", cx - bw * 0.35f, y, bw * 0.7f, bh)) { screen = backTo; menuSel = 0; }
            }
            menuCountPrev = std::max(btnCount, 1); uiEnter = false; uiLeft = false; uiRight = false;
            ui.flush();
        }

        // --- capture
        bool kP = glfwGetKey(win, GLFW_KEY_F12) == GLFW_PRESS;
        bool autoShot = !shot.empty() && frame == shotFrames;
        if ((kP && !keyP) || autoShot) {
            std::vector<uint8_t> px8((size_t)fbw * fbh * 3), flip((size_t)fbw * fbh * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadBuffer(GL_BACK); glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, px8.data());
            for (int y = 0; y < fbh; y++) std::memcpy(&flip[(size_t)y * fbw * 3], &px8[(size_t)(fbh - 1 - y) * fbw * 3], (size_t)fbw * 3);
            std::string name = autoShot ? shot : "capture_" + std::to_string((int)now) + ".png";
            writePNG(name.c_str(), flip, fbw, fbh); std::printf("Capture : %s", name.c_str()); std::puts("");
            if (autoShot) {
                size_t dead = 0; for (auto& bd : world.wood.boards) if (!bd.alive) dead++;
                std::printf("biome %d (%s) | PV %.0f | tues %d | cles %d/%d | planches detruites %zu | debris %zu | rats %zu | ennemis vivants %zu | gouttes %zu", curBiome, BIOMES[std::min(6, std::max(0, curBiome))].name, hp, kills, keys, world.keysNeeded, dead, world.debris.size(), rats.size(), (size_t)std::count_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; }), drops.size()); std::puts("");
                std::printf("%.1f ms/frame", (glfwGetTime() - tStart) * 1000.0 / frame); std::puts("");
                break;
            }
        }
        keyP = kP;
        glfwSwapBuffers(win); frame++; fpsN++;
        if (now - fpsT > 1.0 && shot.empty()) {
            char title[160]; std::snprintf(title, sizeof title, "GRIMDEEP - A Dwarf in a Cave | seed %u | %d FPS | %s", seed, fpsN, fly ? "vol" : "marche");
            glfwSetWindowTitle(win, title); fpsN = 0; fpsT = now;
        }
    }
    saveSettings(cfg);
    audio.stop();
    glfwTerminate();
    return 0;
}
