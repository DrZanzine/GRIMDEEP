// =============================================================================
//  entities/rat.cpp — trajet et maillage des rats
// =============================================================================
#include "entities/rat.h"

#include "render/geometry.h"
#include "render/materials.h"

vec3 ratPositionAt(const Rat& r, float s, vec3& fwd) {
    size_t k = 1;
    while (k + 1 < r.cum.size() && r.cum[k] < s) k++;  // segment contenant s
    float seg = std::max(1e-4f, r.cum[k] - r.cum[k - 1]), t = std::min(1.0f, std::max(0.0f, (s - r.cum[k - 1]) / seg));
    fwd = glm::normalize(r.pts[k] - r.pts[k - 1] + vec3(1e-6f));
    return glm::mix(r.pts[k - 1], r.pts[k], t);
}

void appendRatMesh(Mesh& m, vec3 pos, vec3 fwd, float phase) {
    vec3 up(0, 1, 0);
    vec3 ax = glm::normalize(vec3(fwd.x, 0, fwd.z) + vec3(1e-6f)), az = glm::normalize(glm::cross(ax, up)), ay = up;
    float sc = 1.2f;
    pos += ay * (0.012f * std::sin(phase * 2.0f));  // petits bonds
    // ellipsoïde en coordonnées locales (x avant, y haut, z côté)
    auto E = [&](vec3 c, vec3 rad, float mat, float seed) { addEllipsoid(m, pos + (ax * c.x + ay * c.y + az * c.z) * sc, ax, ay, az, rad * sc, mat, seed); };
    // corps, croupe, tête, museau, oreilles, yeux (Vertex::seed : 0 fourrure, 0,6 peau rose, 1 queue)
    E(vec3(0.0f, 0.055f, 0), vec3(0.11f, 0.052f, 0.048f), Mat::FUR, 0);
    E(vec3(-0.07f, 0.058f, 0), vec3(0.07f, 0.058f, 0.054f), Mat::FUR, 0);
    E(vec3(0.125f, 0.052f, 0), vec3(0.058f, 0.04f, 0.034f), Mat::FUR, 0);
    E(vec3(0.18f, 0.044f, 0), vec3(0.042f, 0.024f, 0.02f), Mat::FUR, 0.6f);
    E(vec3(0.112f, 0.092f, 0.03f), vec3(0.014f, 0.02f, 0.008f), Mat::FUR, 0.6f);
    E(vec3(0.112f, 0.092f, -0.03f), vec3(0.014f, 0.02f, 0.008f), Mat::FUR, 0.6f);
    E(vec3(0.15f, 0.066f, 0.024f), vec3(0.007f, 0.007f, 0.007f), Mat::RED_GLOW, 0);
    E(vec3(0.15f, 0.066f, -0.024f), vec3(0.007f, 0.007f, 0.007f), Mat::RED_GLOW, 0);
    for (int k = 0; k < 7; k++) {  // queue ondulante
        float f = k / 6.0f;
        E(vec3(-0.13f - 0.045f * k, 0.04f - 0.003f * k, std::sin(phase * 1.5f - k * 0.9f) * 0.03f * f), vec3(0.03f, 0.006f, 0.006f), Mat::FUR, 1.0f);
    }
    for (int k = 0; k < 4; k++) {  // pattes (trot)
        float front = (k < 2) ? 0.07f : -0.07f, side = (k & 1) ? 0.04f : -0.04f, sw = std::sin(phase * 2.6f + k * 1.7f);
        E(vec3(front + sw * 0.035f, 0.022f + std::max(0.0f, std::cos(phase * 2.6f + k * 1.7f)) * 0.012f, side), vec3(0.022f, 0.026f, 0.011f), Mat::FUR, 0.6f);
    }
}
