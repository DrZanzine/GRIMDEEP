// =============================================================================
//  world/tube.cpp — requêtes géométriques sur une galerie
// =============================================================================
#include "world/tube.h"

vec3 Tube::toWorld(int i, float x, float y, float t) const {
    const Ring& r = rings[i];
    return r.c + r.R * x + r.U * y + r.T * t;
}

bool Tube::wallRange(int i, float y, float& xl, float& xr) const {
    // On parcourt les segments de la section et on garde les intersections avec la
    // droite horizontale de hauteur y (la plus à gauche et la plus à droite).
    bool f = false;
    xl = 1e9f;
    xr = -1e9f;
    for (int k = 0; k < M; k++) {
        vec2 a = loc[(size_t)i * M + k], b = loc[(size_t)i * M + (k + 1) % M];
        if ((a.y - y) * (b.y - y) <= 0 && a.y != b.y) {
            float t = (y - a.y) / (b.y - a.y), x = a.x + t * (b.x - a.x);
            xl = std::min(xl, x);
            xr = std::max(xr, x);
            f = true;
        }
    }
    return f;
}

float Tube::vertRange(int i, float x, bool top) const {
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

float Tube::floorY(int i, float x) const {
    float y = vertRange(i, x, false);
    return y > 1e8f ? -1.0f : y;
}

float Tube::ceilY(int i, float x) const {
    float y = vertRange(i, x, true);
    return y < -1e8f ? 2.0f : y;
}

float Tube::radiusAt(int i, float phi) const {
    // Intersection du rayon (cos phi, sin phi) avec chaque segment de la section.
    vec2 d(std::cos(phi), std::sin(phi));
    float best = 1.0f;
    for (int k = 0; k < M; k++) {
        vec2 a = loc[(size_t)i * M + k], b = loc[(size_t)i * M + (k + 1) % M], e = b - a;
        float den = d.x * e.y - d.y * e.x;
        if (std::fabs(den) < 1e-6f) continue;
        float t = (a.x * e.y - a.y * e.x) / den, s = (a.x * d.y - a.y * d.x) / den;
        if (t > 0 && s >= -0.001f && s <= 1.001f) best = t;
    }
    return best;
}

int Tube::locate(vec3 p, int hint) const {
    int lo = 0, hi = N - 1;
    if (hint >= 0) {
        lo = std::max(0, hint - 40);
        hi = std::min(N - 1, hint + 40);
    }
    int best = lo;
    float bd = 1e18f;
    for (int i = lo; i <= hi; i++) {
        vec3 d = p - rings[i].c;
        // sans indice : distance au centre ; avec indice : distance le long de la tangente
        float score = (hint < 0) ? glm::dot(d, d) : std::fabs(glm::dot(d, rings[i].T));
        if (score < bd) { bd = score; best = i; }
    }
    return best;
}
