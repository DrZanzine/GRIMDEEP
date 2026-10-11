// =============================================================================
//  render/textures.cpp — génération des textures procédurales
// =============================================================================
#include "render/textures.h"

#include "core/math_utils.h"
#include "core/noise.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

namespace {

/// Bruit de valeur 2D périodique (période px x py cellules) : se répète sans couture.
float tileableNoise2D(float x, float y, int px, int py, uint32_t s) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = quinticFade(x - xi), fy = quinticFade(y - yi);
    auto h = [&](int a, int b) {
        a = ((a % px) + px) % px;
        b = ((b % py) + py) % py;
        return hash3D(a, b, 0, s);
    };
    return mixf(mixf(h(xi, yi), h(xi + 1, yi), fx), mixf(h(xi, yi + 1), h(xi + 1, yi + 1), fx), fy);
}

/// Bruit fractal 2D périodique sur [0,1]² : `oct` octaves à partir de `base` cellules.
float tileableFbm2D(float u, float v, int base, int oct, uint32_t s) {
    float a = 0.5f, sum = 0, norm = 0;
    int p = base;
    for (int o = 0; o < oct; o++) {
        sum += a * tileableNoise2D(u * p, v * p, p, p, s + o * 31);
        norm += a;
        a *= 0.5f;
        p *= 2;
    }
    return sum / norm;
}

/// Résultat d'une requête de Worley : distances aux deux germes les plus proches.
struct WorleyCell {
    float F1, F2;  ///< distance au germe le plus proche et au deuxième
    float dx, dy;  ///< vecteur (point - germe le plus proche)
    int id;        ///< identifiant de la cellule la plus proche
};

/// Bruit cellulaire de Worley périodique (C x C cellules sur [0,1]²).
WorleyCell worleyCell(float u, float v, int C, uint32_t s) {
    float x = u * C, y = v * C;
    int cx = (int)std::floor(x), cy = (int)std::floor(y);
    WorleyCell w;
    w.F1 = w.F2 = 9;
    w.dx = w.dy = 0;
    w.id = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int gx = cx + dx, gy = cy + dy;
            int wx = ((gx % C) + C) % C, wy = ((gy % C) + C) % C;
            float fx = gx + hash3D(wx, wy, 1, s), fy = gy + hash3D(wx, wy, 2, s);  // germe de la cellule
            float d = std::sqrt((fx - x) * (fx - x) + (fy - y) * (fy - y));
            if (d < w.F1) { w.F2 = w.F1; w.F1 = d; w.dx = x - fx; w.dy = y - fy; w.id = wy * C + wx; }
            else if (d < w.F2) w.F2 = d;
        }
    return w;
}

/// Convertit [0,1] en octet (avec arrondi et saturation).
uint8_t toByte(float x) { return (uint8_t)(std::min(std::max(x, 0.0f), 1.0f) * 255.0f + 0.5f); }

/// Convertit une couleur linéaire en octet sRGB (gamma 2,2).
uint8_t linearToSrgbByte(float lin) { return toByte(std::pow(std::min(std::max(lin, 0.0f), 1.0f), 1.0f / 2.2f)); }

/// Calcule une carte de normales (RGBA8) à partir d'une carte de hauteur périodique N x N.
void heightToNormal(int N, const std::vector<float>& H, float strength, std::vector<uint8_t>& nrm) {
    nrm.resize((size_t)N * N * 4);
    auto h = [&](int x, int y) { return H[(size_t)((y + N) % N) * N + ((x + N) % N)]; };
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float dx = (h(x + 1, y) - h(x - 1, y)) * strength, dy = (h(x, y + 1) - h(x, y - 1)) * strength;
            vec3 n = glm::normalize(vec3(-dx, -dy, 1.0f));
            size_t o = ((size_t)y * N + x) * 4;
            nrm[o] = toByte(n.x * 0.5f + 0.5f);
            nrm[o + 1] = toByte(n.y * 0.5f + 0.5f);
            nrm[o + 2] = toByte(n.z * 0.5f + 0.5f);
            nrm[o + 3] = 255;
        }
}

}  // namespace

void generateRockTexture(int N, std::vector<uint8_t>& col, std::vector<uint8_t>& nrm) {
    std::vector<float> H((size_t)N * N);
    col.resize((size_t)N * N * 4);
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            float u = (i + 0.5f) / N, v = (j + 0.5f) / N;
            // coordonnées légèrement ondulées : les bords de blocs ne sont pas rectilignes
            float wob = tileableFbm2D(u, v, 5, 3, 61) - 0.5f, wob2 = tileableFbm2D(u, v, 5, 3, 62) - 0.5f;
            float uu = u + wob * 0.07f, vv = v + wob2 * 0.07f;
            // deux échelles de blocs (A gros, B petits), chacun incliné et décalé en hauteur
            WorleyCell A = worleyCell(uu, vv, 4, 5), B = worleyCell(uu, vv, 10, 9);
            float tiltA = (A.dx * (hash3D(A.id, 1, 0, 31) - 0.5f) + A.dy * (hash3D(A.id, 2, 0, 31) - 0.5f)) * 0.9f;
            float tiltB = (B.dx * (hash3D(B.id, 1, 0, 33) - 0.5f) + B.dy * (hash3D(B.id, 2, 0, 33) - 0.5f)) * 0.9f;
            float stepA = hash3D(A.id, 3, 0, 31), stepB = hash3D(B.id, 3, 0, 33);
            float f = tileableFbm2D(u, v, 4, 6, 11), f2 = tileableFbm2D(u, v, 3, 4, 23), f3 = tileableFbm2D(u, v, 5, 3, 41);
            float ridge = 1.0f - std::fabs(tileableFbm2D(u, v, 3, 5, 71) * 2.0f - 1.0f);
            float grain = tileableFbm2D(u, v, 48, 3, 83);
            // fractures : là où les deux germes les plus proches sont à égale distance
            float crackA = 1.0f - smoothStep(0.0f, 0.028f, A.F2 - A.F1), crackB = 1.0f - smoothStep(0.0f, 0.025f, B.F2 - B.F1);
            float h = 0.28f * (stepA * 0.6f + tiltA) + 0.14f * (stepB * 0.5f + tiltB) + 0.22f * f + 0.12f * ridge + 0.14f * grain - 0.06f * crackA - 0.04f * crackB;
            H[(size_t)j * N + i] = h;
            // couleur : gris, ocre ou bleuté selon le bloc, assombri dans les fractures
            float tint = hash3D(A.id, 5, 0, 37);
            vec3 gray(0.25f, 0.25f, 0.265f), ochre(0.27f, 0.235f, 0.19f), blue(0.19f, 0.21f, 0.25f);
            vec3 a = tint < 0.5f ? glm::mix(gray, ochre, tint * 2.0f * 0.7f) : glm::mix(gray, blue, (tint - 0.5f) * 2.0f);
            a *= 0.75f + 0.5f * hash3D(B.id, 6, 0, 39);
            a = glm::mix(a, a * vec3(0.7f, 0.74f, 0.8f), smoothStep(0.5f, 0.8f, f3));
            a *= 0.45f + 0.9f * (0.5f * f2 + 0.5f * h * 2.0f);
            a *= 1.0f - 0.65f * crackA;
            a *= 1.0f - 0.35f * crackB;
            float fleck = smoothStep(0.88f, 0.92f, tileableNoise2D(u * 160, v * 160, 160, 160, 91));  // paillettes
            a += vec3(0.07f, 0.07f, 0.075f) * fleck;
            size_t o = ((size_t)j * N + i) * 4;
            col[o] = linearToSrgbByte(a.x);
            col[o + 1] = linearToSrgbByte(a.y);
            col[o + 2] = linearToSrgbByte(a.z);
            col[o + 3] = toByte(0.12f + 0.4f * f2 + 0.6f * fleck);  // brillance
        }
    heightToNormal(N, H, 7.0f, nrm);
}

void generateWoodTexture(int N, std::vector<uint8_t>& col, std::vector<uint8_t>& nrm) {
    std::vector<float> H((size_t)N * N);
    col.resize((size_t)N * N * 4);
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            float u = (i + 0.5f) / N, v = (j + 0.5f) / N;  // u : le long du fil
            float warp = tileableNoise2D(u * 2, v * 6, 2, 6, 7) * 1.8f + tileableNoise2D(u * 4, v * 12, 4, 12, 9) * 0.5f;
            float ring = 0.5f + 0.5f * std::sin(2 * PI * (v * 9.0f + warp));  // cernes de croissance
            ring = std::pow(ring, 1.6f);
            float streak = tileableNoise2D(u * 6, v * 96, 6, 96, 13) * 0.6f + tileableNoise2D(u * 12, v * 200, 12, 200, 17) * 0.4f;
            float crackN = tileableNoise2D(u * 3, v * 40, 3, 40, 21);
            float crack = smoothStep(0.46f, 0.5f, crackN) * (1.0f - smoothStep(0.5f, 0.54f, crackN));  // fentes fines
            float t = 0.45f * ring + 0.55f * streak;
            vec3 dark(0.15f, 0.10f, 0.065f), light(0.40f, 0.30f, 0.20f), gray(0.30f, 0.27f, 0.24f);
            vec3 a = glm::mix(dark, light, t);
            a = glm::mix(a, gray, 0.2f + 0.35f * tileableNoise2D(u * 3, v * 3, 3, 3, 33));  // bois grisé, vieilli
            a *= 1.0f - 0.75f * crack;
            H[(size_t)j * N + i] = 0.35f * ring + 0.35f * streak - 0.5f * crack;
            size_t o = ((size_t)j * N + i) * 4;
            col[o] = linearToSrgbByte(a.x);
            col[o + 1] = linearToSrgbByte(a.y);
            col[o + 2] = linearToSrgbByte(a.z);
            col[o + 3] = toByte(0.3f + 0.3f * streak);
        }
    heightToNormal(N, H, 14.0f, nrm);
}

GLuint createTexture2D(int N, const std::vector<uint8_t>& px, bool srgb) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f);
    return t;
}
