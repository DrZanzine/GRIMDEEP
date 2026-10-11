// =============================================================================
//  core/noise.cpp — implémentation du hachage et du bruit procédural
// =============================================================================
#include "core/noise.h"

uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

float hash3D(int x, int y, int z, uint32_t s) {
    // Combinaison linéaire avec de grands nombres premiers, puis double avalanche.
    uint32_t h = hash32((uint32_t)x * 374761393U + (uint32_t)y * 668265263U + (uint32_t)z * 2147483629U + s * 1274126177U);
    h = hash32(h ^ (h >> 13));
    return (h & 0xFFFFFF) / 16777216.0f;
}

float valueNoise3D(float x, float y, float z, uint32_t s) {
    // Cellule entière contenant le point et position fractionnaire lissée dans la cellule.
    int xi = (int)std::floor(x), yi = (int)std::floor(y), zi = (int)std::floor(z);
    float fx = quinticFade(x - xi), fy = quinticFade(y - yi), fz = quinticFade(z - zi);

    // Valeurs aléatoires aux 8 coins de la cellule.
    float c000 = hash3D(xi, yi, zi, s), c100 = hash3D(xi + 1, yi, zi, s);
    float c010 = hash3D(xi, yi + 1, zi, s), c110 = hash3D(xi + 1, yi + 1, zi, s);
    float c001 = hash3D(xi, yi, zi + 1, s), c101 = hash3D(xi + 1, yi, zi + 1, s);
    float c011 = hash3D(xi, yi + 1, zi + 1, s), c111 = hash3D(xi + 1, yi + 1, zi + 1, s);

    // Interpolation trilinéaire (x, puis y, puis z).
    return mixf(mixf(mixf(c000, c100, fx), mixf(c010, c110, fx), fy),
                mixf(mixf(c001, c101, fx), mixf(c011, c111, fx), fy), fz);
}

float fbm3D(vec3 p, int oct, uint32_t s) {
    float amplitude = 0.5f, sum = 0, norm = 0;
    for (int o = 0; o < oct; o++) {
        sum += amplitude * valueNoise3D(p.x, p.y, p.z, s + o * 17);  // graine différente par octave
        norm += amplitude;
        p *= 2.03f;          // fréquence légèrement non entière : évite les motifs alignés
        amplitude *= 0.5f;
    }
    return sum / norm;
}

float valueNoise1D(float x, uint32_t s) {
    int xi = (int)std::floor(x);
    float f = x - xi;
    f = f * f * (3 - 2 * f);
    return mixf(hash3D(xi, 0, 0, s), hash3D(xi + 1, 0, 0, s), f);
}
