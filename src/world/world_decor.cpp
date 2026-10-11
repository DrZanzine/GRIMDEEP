// =============================================================================
//  world/world_decor.cpp — biomes, lumières et décors des galeries
//
//  - biomeAt / biomeAtPos : quel biome à quel endroit (plateaux + courtes transitions) ;
//  - flaques d'eau (ou de lave dans la forge), trous de rats ;
//  - décors propres aux biomes : champignons luminescents (grottes humides),
//    cristaux (antre des cristaux), ossements et crânes (catacombes).
// =============================================================================
#include "world/world.h"

#include <algorithm>

#include "core/noise.h"
#include "render/geometry.h"
#include "render/materials.h"

// ---------------------------------------------------------------------------
//  Biomes
// ---------------------------------------------------------------------------

float World::biomeAt(int ti, int i) const {
    const Tube& tb = tubes[ti];
    if (!tb.isMain) return std::floor(biomeAt(tb.parent, tb.junction) + 0.5f);  // une galerie latérale hérite du biome de sa jonction
    if (doorRing <= 0) return 0.0f;
    if (i >= doorRing) return (float)EXIT_BIOME;
    // 5 biomes répartis uniformément jusqu'à la porte
    float u = 5.0f * (float)i / (float)doorRing, fl = std::floor(u);
    return fl + smoothStep(0.86f, 1.0f, u - fl);  // plateaux + courtes transitions
}

float World::biomeAtPos(vec3 p) const {
    if (tubes.empty()) return 0.0f;
    if (inHub(p)) return (float)HUB_BIOME;
    // anneau le plus proche (un sur quatre suffit) toutes galeries confondues
    int bt = 0, bi = 0;
    float bd = 1e18f;
    for (size_t t = 0; t < tubes.size(); t++)
        for (int i = 0; i < tubes[t].N; i += 4) {
            vec3 d = p - tubes[t].rings[i].c;
            float dd = glm::dot(d, d);
            if (dd < bd) { bd = dd; bt = (int)t; bi = i; }
        }
    return biomeAt(bt, std::min(tubes[bt].N - 1, bi));
}

/// Écrit le biome de chaque sommet du maillage (le shader en déduit teinte, humidité, mousse).
void World::assignBiomes(Mesh& m) {
    for (auto& v : m.v) v.biome = biomeAtPos(v.p);
}

/// Ajoute une lumière fixe au monde.
void World::addLight(vec3 pos, vec3 col, float inten, int biome, bool flame) {
    WorldLight t;
    t.pos = pos;
    t.col = col;
    t.inten = inten;
    t.biome = biome;
    t.flame = flame;
    lights.push_back(t);
}

// ---------------------------------------------------------------------------
//  Flaques et trous de rats
// ---------------------------------------------------------------------------

/// Ajoute `count` flaques au sol de la galerie `ti` (lave dans la forge infernale,
/// parfois avec une lueur rouge). Le niveau de l'eau suit le relief du sol.
void World::addPuddles(int ti, int count, Rng& rng) {
    Tube& tb = tubes[ti];
    for (int n = 0; n < count; n++) {
        int lim = tb.isMain ? doorRing - 8 : tb.N - 10;
        int i = 10 + rng.next() % std::max(1, lim - 10);
        const Ring& r = tb.rings[i];
        int bi = (int)std::floor(biomeAt(ti, i) + 0.5f);
        if (bi >= 5 || nearCorridor(ti, i, 3)) continue;
        float pmat = bi == 4 ? Mat::LAVA : Mat::WATER;
        float fy = tb.floorY(i, 0.0f), xl, xr;
        if (!tb.wallRange(i, fy + 0.6f, xl, xr)) continue;
        float x0 = rng.r(xl + 0.9f, xr - 0.9f), rx = rng.r(0.5f, 1.4f) * (bi == 1 ? 1.35f : 1.0f), rz = rx * rng.r(0.9f, 2.2f);
        // niveau : un peu au-dessus de la médiane du sol sur le pourtour
        std::vector<float> fl;
        for (int q = 0; q < 12; q++) {
            float a = 2 * PI * q / 12;
            int ii = std::max(1, std::min(tb.N - 2, i + (int)std::lround(std::sin(a) * rz * 0.5f / tb.ds)));
            fl.push_back(tb.floorY(ii, x0 + std::cos(a) * rx * 0.8f));
        }
        std::sort(fl.begin(), fl.end());
        float level = fl[7] + 0.03f;
        // disque bruité (éventail de 22 triangles) ; uv.x = 0 au centre, 1 au bord (fondu)
        const int seg = 22;
        waterMesh.beginChunk();
        uint32_t base = (uint32_t)waterMesh.v.size();
        float sd = rng.f() * 20;
        vec3 cw = tb.toWorld(i, x0, level);
        waterMesh.v.push_back({cw, vec3(0, 1, 0), vec2(0, 0), pmat, sd});
        if (bi == 4 && rng.chance(0.4f)) addLight(cw + vec3(0, 0.35f, 0), vec3(1.0f, 0.32f, 0.06f), 1.2f, 4, false);
        for (int q = 0; q < seg; q++) {
            float a = 2 * PI * q / seg;
            float nz = 0.7f + 0.6f * valueNoise3D(std::cos(a) * 1.5f + sd, std::sin(a) * 1.5f, sd, seed + 3);
            vec3 p = cw + r.R * (std::cos(a) * rx * nz) + r.T * (std::sin(a) * rz * nz);
            p.y = cw.y;
            waterMesh.v.push_back({p, vec3(0, 1, 0), vec2(1, 0), pmat, sd});
        }
        for (int q = 0; q < seg; q++)
            waterMesh.idx.insert(waterMesh.idx.end(), {base, base + 1 + (uint32_t)q, base + 1 + (uint32_t)((q + 1) % seg)});
        waterMesh.endChunk();
    }
}

/// Ajoute deux trous de rats reliés : l'un à l'anneau `i`, l'autre `k` anneaux plus
/// loin sur la paroi opposée. Les rats sortent de l'un et rentrent dans l'autre.
void World::addRatHoles(int ti, int i, int k, Rng& rng) {
    Tube& tb = tubes[ti];
    int M = tb.M;
    // Creuse un trou au ras du sol côté `side` ; renvoie sa position, normale et abscisse.
    auto makeHole = [&](int ring, int side, vec3& pOut, vec3& nOut, float& xOut) -> bool {
        float fy = tb.floorY(ring, 0.0f), xl, xr;
        float yy = fy + 0.28f;
        if (!tb.wallRange(ring, yy, xl, xr)) return false;
        float xw = side > 0 ? xr : xl;
        // sommet de paroi le plus proche du point visé
        int bk = 0;
        float bd = 1e9f;
        for (int q = 0; q < M; q++) {
            vec2 l = tb.loc[(size_t)ring * M + q];
            float d = (l.x - xw) * (l.x - xw) + (l.y - yy) * (l.y - yy);
            if (d < bd) { bd = d; bk = q; }
        }
        const Vertex& v = rockMesh.v[(size_t)tb.vBase + (size_t)ring * M + bk];
        vec3 n = v.n, p = v.p;
        vec3 e1 = glm::normalize(glm::cross(n, vec3(0, 1, 0))), e2 = glm::cross(n, e1);
        pOut = p;
        nOut = n;
        xOut = tb.loc[(size_t)ring * M + bk].x;
        rockMesh.beginChunk();
        // fond noir du trou
        uint32_t base = (uint32_t)rockMesh.v.size();
        rockMesh.v.push_back({p + n * 0.012f, n, vec2(0), Mat::HOLE, 0.0f});
        const int seg = 14;
        for (int q = 0; q < seg; q++) {
            float a = 2 * PI * q / seg;
            rockMesh.v.push_back({p + n * 0.012f + e1 * (std::cos(a) * 0.1f) + e2 * (std::sin(a) * 0.085f), n, vec2(0), Mat::HOLE, 0.0f});
        }
        for (int q = 0; q < seg; q++) rockMesh.idx.insert(rockMesh.idx.end(), {base, base + 1 + (uint32_t)q, base + 1 + (uint32_t)((q + 1) % seg)});
        // bourrelet de roche autour de l'entrée
        std::vector<vec3> P, ref;
        const int rows = 4, cols = 14;
        float rr[rows] = {0.24f, 0.17f, 0.125f, 0.1f}, oo[rows] = {-0.03f, 0.04f, 0.035f, 0.0f};
        for (int r2 = 0; r2 < rows; r2++)
            for (int c = 0; c < cols; c++) {
                float a = 2 * PI * c / cols, nz = 0.85f + 0.3f * hash3D(c, r2, ring, seed + 77);
                P.push_back(p + n * oo[r2] + e1 * (std::cos(a) * rr[r2] * nz) + e2 * (std::sin(a) * rr[r2] * 0.9f * nz));
                ref.push_back(p + n * 2.0f);
            }
        addGrid(rockMesh, P, ref, rows, cols, 1.0f, Mat::ROCK, 0.0f);
        rockMesh.endChunk();
        return true;
    };
    int side = rng.chance(0.5f) ? 1 : -1;
    HolePair h;
    h.tube = ti;
    h.iA = i;
    h.iB = std::min(tb.N - 6, i + k);
    if (!makeHole(h.iA, side, h.pA, h.nA, h.xA)) return;
    if (!makeHole(h.iB, -side, h.pB, h.nB, h.xB)) return;
    holes.push_back(h);
}

// ---------------------------------------------------------------------------
//  Décors des biomes
// ---------------------------------------------------------------------------

/// Ajoute un cristal hexagonal (prisme terminé par une pointe) dans rockMesh.
/// `hue` choisit la teinte (violet / cyan) ; uv.y = 0 à la base, 1 à la pointe.
void World::addCrystal(vec3 base, vec3 axis, float len, float rad, float hue) {
    vec3 e1 = glm::normalize(glm::cross(axis, std::fabs(axis.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 0, 1))), e2 = glm::cross(axis, e1);
    const int rows = 4, cols = 6;
    std::vector<vec3> P, ref;
    const float rr[rows] = {1.0f, 1.0f, 0.75f, 0.0f}, tt[rows] = {0.0f, 0.62f, 0.86f, 1.0f};
    for (int r = 0; r < rows; r++) {
        vec3 c = base + axis * (len * tt[r]);
        for (int k = 0; k < cols; k++) {
            float a = 2 * PI * k / cols + 0.3f;
            P.push_back(c + (e1 * std::cos(a) + e2 * std::sin(a)) * rad * rr[r]);
            ref.push_back(c);
        }
    }
    size_t v0 = rockMesh.v.size();
    addGrid(rockMesh, P, ref, rows, cols, -1.0f, Mat::CRYSTAL, hue);
    for (int r = 0; r < rows; r++)
        for (int k = 0; k < cols; k++) rockMesh.v[v0 + (size_t)r * cols + k].uv = vec2((float)k / cols, tt[r]);
}

/// Sème les décors propres au biome le long de la galerie `ti` :
/// champignons luminescents (biome 1), grappes de cristaux avec lueur (biome 2),
/// ossements épars et crânes (biome 3).
void World::addDecor(int ti, Rng& rng) {
    Tube& tb = tubes[ti];
    int lim = tb.isMain ? doorRing - 8 : tb.N - 8;
    int count = tb.isMain ? 120 : 16;
    for (int n = 0; n < count; n++) {
        int i = 12 + rng.next() % std::max(1, lim - 12);
        int bi = (int)std::floor(biomeAt(ti, i) + 0.5f);
        bool nearJ = false;
        for (size_t b = 1; b < tubes.size(); b++)
            if (tubes[b].parent == ti && std::abs(i - tubes[b].junction) < 14) nearJ = true;
        if (nearJ || nearCorridor(ti, i, 3)) continue;
        const Ring& r = tb.rings[i];
        float fy = tb.floorY(i, 0.0f), xl, xr;
        if (!tb.wallRange(i, fy + 0.5f, xl, xr)) continue;
        float x = rng.r(xl + 0.5f, xr - 0.5f);
        vec3 base = tb.toWorld(i, x, tb.floorY(i, x) - 0.02f);
        if (bi == 1 && rng.chance(0.6f)) {  // touffe de champignons : pied + chapeau luminescent
            rockMesh.beginChunk();
            int cnt = 2 + rng.next() % 4;
            for (int k = 0; k < cnt; k++) {
                float h = rng.r(0.1f, 0.45f), rc = rng.r(0.05f, 0.14f);
                vec3 p = base + r.R * rng.r(-0.35f, 0.35f) + r.T * rng.r(-0.35f, 0.35f);
                addEllipsoid(rockMesh, p + r.U * (h * 0.5f), r.R, r.U, r.T, vec3(rc * 0.25f, h * 0.5f, rc * 0.25f), Mat::MUSHROOM, 0.2f);
                addEllipsoid(rockMesh, p + r.U * h, r.R, r.U, r.T, vec3(rc, rc * 0.5f, rc), Mat::MUSHROOM, 0.8f);
            }
            rockMesh.endChunk();
        } else if (bi == 2 && rng.chance(0.55f)) {  // grappe de cristaux
            rockMesh.beginChunk();
            float hue = rng.f();
            int cnt = 3 + rng.next() % 4;
            for (int k = 0; k < cnt; k++) {
                vec3 ax = glm::normalize(r.U + r.R * rng.r(-0.6f, 0.6f) + r.T * rng.r(-0.6f, 0.6f));
                addCrystal(base + r.R * rng.r(-0.3f, 0.3f) + r.T * rng.r(-0.3f, 0.3f), ax, rng.r(0.3f, 1.1f), rng.r(0.05f, 0.13f), hue);
            }
            rockMesh.endChunk();
            if (rng.chance(0.4f)) addLight(base + r.U * 0.8f, hue < 0.5f ? vec3(0.7f, 0.3f, 1.0f) : vec3(0.25f, 0.7f, 1.0f), 1.6f, 2, false);
        } else if (bi == 3 && rng.chance(0.6f)) {  // os épars, parfois un crâne (avec ses orbites)
            rockMesh.beginChunk();
            for (int k = 0; k < 4; k++) {
                float a = rng.r(0, 2 * PI);
                vec3 d = r.R * std::cos(a) + r.T * std::sin(a);
                addEllipsoid(rockMesh, base + r.R * rng.r(-0.3f, 0.3f) + r.T * rng.r(-0.3f, 0.3f) + r.U * 0.03f, d, r.U, glm::cross(d, r.U),
                             vec3(rng.r(0.12f, 0.26f), 0.02f, 0.02f), Mat::BONE, 0.0f);
            }
            if (rng.chance(0.6f)) {
                vec3 c = base + r.U * 0.1f;
                addEllipsoid(rockMesh, c, r.R, r.U, r.T, vec3(0.09f, 0.08f, 0.1f), Mat::BONE, 0.0f);
                addEllipsoid(rockMesh, c + r.R * 0.075f + r.U * 0.02f + r.T * 0.035f, r.R, r.U, r.T, vec3(0.03f), Mat::HOLE, 0.0f);
                addEllipsoid(rockMesh, c + r.R * 0.075f + r.U * 0.02f - r.T * 0.035f, r.R, r.U, r.T, vec3(0.03f), Mat::HOLE, 0.0f);
            }
            rockMesh.endChunk();
        }
    }
}
