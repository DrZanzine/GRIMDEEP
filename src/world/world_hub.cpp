// =============================================================================
//  world/world_hub.cpp — le camp de base (surface)
//
//  Plaine au crépuscule devant un flanc de montagne percé par l'entrée de la mine :
//  sol ondulé avec un chemin de terre, montagne dont l'anneau intérieur épouse
//  exactement l'ouverture de la galerie, montagnes lointaines, matériel de mine
//  (chariot sur rails, caisses, tonneaux, sacs), forge sous auvent, feu de camp et
//  tente, lanternes, rochers, forêt, et cristaux géants lumineux dans la montagne.
//
//  Repère : la mine part vers -Z depuis l'anneau 0 de la galerie principale ; le camp
//  s'étend du côté +Z. `hubFrontZ` est le Z de l'entrée, et beaucoup de positions sont
//  exprimées en « zr » = distance devant l'entrée (Z - hubFrontZ).
//  Le joueur est retenu dans un rectangle de 116 x 112 m par des murs invisibles ; la
//  forêt dense au-delà fait office de décor.
// =============================================================================
#include "world/world.h"

#include <cstdio>

#include "core/noise.h"
#include "render/geometry.h"
#include "render/materials.h"

namespace {

/// Ajoute un sapin : tronc conique + 5 étages de cônes d'aiguilles.
void addPine(Mesh& m, vec3 base, float h, Rng& rng) {
    float seed = rng.f() * 10, tr = 0.16f + h * 0.016f;
    m.beginChunk();
    addCylinder(m, base - vec3(0, 0.4f, 0), base + vec3(0, h * 0.9f, 0), tr, tr * 0.2f, 7, gCustomMats.bark, seed, true, false);
    for (int i = 0; i < 5; i++) {
        float f = i / 4.0f, y0 = h * (0.16f + 0.56f * f), rad = h * (0.27f - 0.2f * f) * rng.r(0.9f, 1.1f), len = h * (0.36f - 0.12f * f);
        addCylinder(m, base + vec3(rng.r(-0.1f, 0.1f), y0, rng.r(-0.1f, 0.1f)), base + vec3(0, y0 + len, 0), rad, 0.0f, 9, gCustomMats.pine, seed + i, true, false);
    }
    m.endChunk();
}

/// Ajoute un feuillu : tronc + 5 boules de feuillage (parfois aux couleurs d'automne).
void addOak(Mesh& m, vec3 base, float h, Rng& rng) {
    float seed = rng.f() * 10, tr = 0.22f + h * 0.02f;
    m.beginChunk();
    addCylinder(m, base - vec3(0, 0.4f, 0), base + vec3(0.0f, h * 0.62f, 0.0f), tr, tr * 0.55f, 8, gCustomMats.bark, seed, true, false);
    vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);
    float lm = rng.f() < 0.25f ? 2.0f : (rng.f() < 0.5f ? 1.0f : 0.0f);
    for (int i = 0; i < 5; i++) {
        float a = 2 * PI * i / 5 + rng.f();
        float rr = h * 0.2f * (i == 0 ? 0.0f : 1.0f);
        vec3 c = base + vec3(std::cos(a) * rr, h * (0.72f + 0.1f * (i % 2)), std::sin(a) * rr);
        float rad = h * rng.r(0.2f, 0.27f);
        addEllipsoid(m, c, X, Y, Z, vec3(rad, rad * 0.82f, rad), gCustomMats.leaf[(int)(rng.f() * 2.0f + (lm == 2.0f ? 1.0f : 0.0f)) % 3], seed + i);
    }
    m.endChunk();
}

/// Abscisse X du chemin de terre en fonction de Z (même formule dans scene.frag pour la couleur du sol).
float hubPathX(float z) { return 3.2f * std::sin(z * 0.05f) + 1.2f * std::sin(z * 0.13f); }

}  // namespace

float World::hubGround(float x, float z) const {
    float zr = z - hubFrontZ, dm = std::sqrt(x * x + (zr - 8.0f) * (zr - 8.0f));
    float flat = smoothStep(20.0f, 55.0f, dm);  // place centrale plate, relief plus loin
    float n = fbm3D(vec3(x * 0.035f, 0.0f, z * 0.035f), 4, seed + 77) - 0.5f;
    float edge = smoothStep(50.0f, 105.0f, std::fabs(x)) * 5.0f + smoothStep(104.0f, 146.0f, zr) * 7.0f;  // le terrain remonte sur les bords
    return hubGroundY + n * 1.5f * flat + edge * (0.55f + (fbm3D(vec3(x * 0.05f, 3.0f, z * 0.05f), 3, seed + 78)));
}

void World::buildHub() {
    hubMesh = Mesh();
    hubSolids.clear();
    const Tube& tb = tubes[0];
    const Ring& r0 = tb.rings[0];
    const int M = tb.M;
    hubFrontZ = r0.c.z;
    float fl0 = tb.floorY(0, 0.0f);
    hubGroundY = tb.toWorld(0, 0.0f, fl0).y;
    Rng R(seed * 7u + 5u);
    vec3 X(1, 0, 0), Y(0, 1, 0), Z(0, 0, 1);

    // --- petits outils de construction (coordonnées : x, zr = distance devant l'entrée)
    auto G = [&](float x, float zr) { return hubGround(x, hubFrontZ + zr); };                // altitude du sol
    auto solid = [&](float x, float zr, float r) { hubSolids.push_back({x, hubFrontZ + zr, r}); };  // obstacle
    auto box = [&](vec3 c, vec3 half, float ang, float mat) {  // boîte posée, tournée autour de Y
        vec3 dl(std::cos(ang), 0, std::sin(ang)), dt(-std::sin(ang), 0, std::cos(ang));
        addBoard(hubMesh, c, dl, Y, dt, half.x, half.y, half.z, mat, R.f() * 10, R, 0.004f);
    };
    auto sack = [&](float x, float zr, float s) {
        float y = G(x, zr);
        vec3 c(x, y + 0.28f * s, hubFrontZ + zr);
        addEllipsoid(hubMesh, c, X, Y, Z, vec3(0.34f * s, 0.30f * s, 0.26f * s), gCustomMats.sack, R.f());
        addEllipsoid(hubMesh, c + vec3(0, 0.3f * s, 0), X, Y, Z, vec3(0.11f * s, 0.08f * s, 0.11f * s), gCustomMats.sack, R.f());
    };
    auto barrel = [&](float x, float zr) {
        float y = G(x, zr) - 0.05f;
        vec3 b(x, y, hubFrontZ + zr);
        hubMesh.beginChunk();
        addCylinder(hubMesh, b, b + vec3(0, 0.95f, 0), 0.40f, 0.40f, 12, Mat::WOOD, R.f() * 10);
        addCylinder(hubMesh, b + vec3(0, 0.3f, 0), b + vec3(0, 0.65f, 0), 0.435f, 0.435f, 12, Mat::WOOD, R.f() * 10, false, false);  // renflement
        for (float h : {0.12f, 0.5f, 0.84f})  // cerclages
            addCylinder(hubMesh, b + vec3(0, h - 0.02f, 0), b + vec3(0, h + 0.02f, 0), 0.445f, 0.445f, 12, Mat::STEEL, 0.0f, false, false);
        hubMesh.endChunk();
        solid(x, zr, 0.5f);
    };
    auto crate = [&](float x, float zr, float yoff, float ang, float s) {
        float y = G(x, zr) - 0.05f + yoff;
        hubMesh.beginChunk();
        box(vec3(x, y + 0.45f * s, hubFrontZ + zr), vec3(0.45f * s, 0.45f * s, 0.45f * s), ang, Mat::WOOD);
        vec3 dl(std::cos(ang), 0, std::sin(ang));
        for (float o : {-0.3f, 0.3f})  // cerclage
            box(vec3(x, y + 0.45f * s, hubFrontZ + zr) + dl * (o * s), vec3(0.03f, 0.465f * s, 0.465f * s), ang, Mat::STEEL);
        hubMesh.endChunk();
        if (yoff < 0.01f) solid(x, zr, 0.7f * s);  // une caisse empilée n'ajoute pas d'obstacle
    };
    auto torch = [&](vec3 base, float h) {
        hubMesh.beginChunk();
        addBoardBetween(hubMesh, base, base + vec3(0, h, 0), Z, 0.05f, 0.05f, Mat::WOOD, R, 0.0f, 0.004f);
        addBoardBetween(hubMesh, base + vec3(0, h - 0.25f, 0), base + vec3(0, h + 0.1f, 0), Z, 0.08f, 0.08f, Mat::TAR, R, 0.0f, 0.01f);
        hubMesh.endChunk();
        addLight(base + vec3(0, h + 0.28f, 0), vec3(1.0f, 0.5f, 0.17f), 4.4f, HUB_BIOME, true);
    };

    // ---- 1) sol : grille 1,5 m, découpée au pied de la montagne, blocs de 12x12 cases pour le culling
    {
        const float X0 = -115, X1 = 115, Z0 = -56, Z1 = 146, st = 1.5f;
        int nx = (int)((X1 - X0) / st) + 1, nz = (int)((Z1 - Z0) / st) + 1;
        uint32_t base = (uint32_t)hubMesh.v.size();
        for (int iz = 0; iz < nz; iz++)
            for (int ix = 0; ix < nx; ix++) {
                float x = X0 + ix * st, z = hubFrontZ + Z0 + iz * st, y = hubGround(x, z);
                float gx = (hubGround(x + 1, z) - hubGround(x - 1, z)) * 0.5f, gz = (hubGround(x, z + 1) - hubGround(x, z - 1)) * 0.5f;
                hubMesh.v.push_back({vec3(x, y, z), glm::normalize(vec3(-gx, 1, -gz)), vec2(0), Mat::GRASS, 0, 0});
            }
        // limite du sol au pied de la montagne (en avant de l'entrée sur les côtés)
        auto zEdge = [](float x) {
            float ax = std::fabs(x);
            return ax < 8.0f ? 0.0f : (ax < 25.0f ? -8.0f : -0.5f * (ax - 25.0f) - 8.0f);
        };
        const int B = 12;
        for (int bz = 0; bz < nz - 1; bz += B)
            for (int bx = 0; bx < nx - 1; bx += B) {
                uint32_t i0 = (uint32_t)hubMesh.idx.size();
                for (int iz = bz; iz < std::min(bz + B, nz - 1); iz++)
                    for (int ix = bx; ix < std::min(bx + B, nx - 1); ix++) {
                        float cx = X0 + (ix + 0.5f) * st, czr = Z0 + (iz + 0.5f) * st;
                        if (czr < zEdge(cx)) continue;
                        uint32_t a = base + iz * nx + ix, b = a + 1, c = a + nx + 1, d = a + nx;
                        hubMesh.idx.insert(hubMesh.idx.end(), {a, b, c, a, c, d});
                    }
                if (hubMesh.idx.size() == i0) continue;
                Chunk ch;
                ch.first = i0;
                ch.count = (uint32_t)hubMesh.idx.size() - i0;
                ch.c = vec3(X0 + (bx + B * 0.5f) * st, hubGroundY, hubFrontZ + Z0 + (bz + B * 0.5f) * st);
                ch.r = B * st * 0.75f + 6.0f;
                hubMesh.chunks.push_back(ch);
            }
    }

    // ---- 2) flanc de montagne : maillage polaire dont l'anneau intérieur est exactement l'ouverture de la galerie
    std::vector<vec3> cliff;
    const int K = M * 3, S = 36;  // K points par anneau (3 par point de section), S anneaux vers l'extérieur
    {
        // anneaux des galeries (repère de l'entrée) : la façade est repoussée / ramenée pour rester toujours devant elles
        struct RC { float x, y, d, R; };
        std::vector<RC> rc;
        int nClamp = 0;
        float worstClamp = 0.0f;
        for (const Tube& t : tubes)
            for (int i = 0; i < t.N; i += 2) {
                vec3 dd = t.rings[i].c - r0.c;
                float d = glm::dot(dd, r0.T);
                if (d < 5.0f) continue;
                rc.push_back({glm::dot(dd, r0.R), glm::dot(dd, r0.U), d, std::max(t.rings[i].w, t.rings[i].h) + 2.2f});
            }
        cliff.resize((size_t)(S + 1) * K);
        std::vector<vec3> ref(cliff.size());
        for (int k = 0; k < K; k++) {
            int k0 = k / 3, k1 = (k0 + 1) % M;
            float f = (k % 3) / 3.0f;
            vec2 in = glm::mix(tb.loc[k0], tb.loc[k1], f);  // point de l'ouverture de la galerie
            float th = 2 * PI * (k0 + f) / M, ct = std::cos(th), sn = std::sin(th);
            // silhouette extérieure : plusieurs sommets (sinus élevés à des puissances)
            float ry = 34.0f + 70.0f * std::pow(std::max(0.0f, std::sin(3 * th + 1.0f)), 1.6f) + 46.0f * std::pow(std::max(0.0f, std::sin(5 * th + 2.2f)), 2.0f) +
                       26.0f * std::pow(std::max(0.0f, std::sin(9 * th + 0.4f)), 2.0f),
                  rx = 114.0f + 6.0f * std::sin(5 * th);
            vec2 out(ct * rx, sn >= 0 ? sn * ry : sn * 7.0f);
            for (int s = 0; s <= S; s++) {
                float t = (float)s / S, e = std::pow(t, 1.45f);
                vec2 q = glm::mix(in, out, e);
                float yAbove = q.y - fl0, zoff = std::max(0.5f * std::max(0.0f, std::fabs(q.x) - 25.0f), 0.62f * std::max(0.0f, yAbove - 3.0f));
                vec3 wp = r0.c + r0.R * q.x + r0.U * q.y;
                float nz = (fbm3D(wp * 0.045f, 4, seed + 501) - 0.5f) * 26.0f + (fbm3D(wp * 0.17f, 3, seed + 502) - 0.5f) * 8.0f +
                           (fbm3D(wp * 0.6f, 2, seed + 503) - 0.5f) * 1.0f;
                nz = std::max(nz, -0.4f) * smoothStep(2.0f, 12.0f, yAbove) * smoothStep(0.0f, 0.2f, t);
                zoff *= smoothStep(0.0f, 0.1f, t);
                float off = zoff + nz, lim = 1e9f;
                // la façade ne doit jamais traverser une galerie
                for (const RC& c : rc) {
                    float dx = q.x - c.x, dy = q.y - c.y, rr = c.R + 3.0f;
                    if (dx * dx + dy * dy < rr * rr) lim = std::min(lim, c.d - c.R - 1.2f);
                }
                if (lim < off) {
                    worstClamp = std::max(worstClamp, off - std::max(lim, 0.0f));
                    off = std::max(lim, 0.0f) * smoothStep(0.0f, 0.1f, t);
                    nClamp++;
                }
                cliff[(size_t)s * K + k] = wp + r0.T * off;
                ref[(size_t)s * K + k] = r0.c + r0.T * 80.0f;
            }
        }
        std::printf("Montagne : %d sommets ramenes devant les galeries (max %.1f m)\n", nClamp, worstClamp);
        hubMesh.beginChunk();
        addGrid(hubMesh, cliff, ref, S + 1, K, -1.0f, Mat::ROCK, 0.0f);
        // chunk toujours visible (énorme)
        hubMesh.endChunk();
        if (!hubMesh.chunks.empty()) {
            hubMesh.chunks.back().c = r0.c + r0.T * 20.0f;
            hubMesh.chunks.back().r = 400.0f;
        }
    }

    // ---- 3) montagnes lointaines (silhouettes dans la brume)
    {
        hubMesh.beginChunk();
        for (int i = 0; i < 16; i++) {
            float a = 2 * PI * i / 16 + R.r(-0.1f, 0.1f), rad = R.r(230, 280);
            vec3 c(std::cos(a) * rad, hubGroundY - 14.0f, hubFrontZ + 60.0f + std::sin(a) * rad);
            if (c.z < hubFrontZ + 25.0f) continue;  // jamais derrière le front de la montagne (les galeries y passent)
            addEllipsoid(hubMesh, c, X, Y, Z, vec3(R.r(70, 110), R.r(45, 80), R.r(70, 110)), Mat::ROCK, R.f());
        }
        hubMesh.endChunk();
        if (!hubMesh.chunks.empty()) {
            hubMesh.chunks.back().c = vec3(0, hubGroundY, hubFrontZ + 60.0f);
            hubMesh.chunks.back().r = 600.0f;
        }
    }

    // ---- 4) entrée de la mine : cadre de soutènement, enseigne, torches, rails
    {
        float a1, a2;
        tb.wallRange(0, fl0 + 1.0f, a1, a2);
        float cy = tb.ceilY(0, 0.0f);
        float xl = a1 + 0.3f, xr = a2 - 0.3f;
        float tz = -0.9f;  // (t négatif = côté camp)
        hubMesh.beginChunk();
        for (float xx : {xl, xr})
            for (int b = 0; b < 2; b++)  // poteaux
                addBoardBetween(hubMesh, tb.toWorld(0, xx + (b - 0.5f) * 0.05f, fl0 - 0.2f, tz + b * 0.1f), tb.toWorld(0, xx, cy - 0.2f, tz + b * 0.1f),
                                r0.T, 0.17f, 0.06f, Mat::WOOD, R, 0.0f, 0.006f);
        for (int b = 0; b < 2; b++)  // linteau
            addBoardBetween(hubMesh, tb.toWorld(0, xl - 0.45f, cy - 0.15f, tz + b * 0.1f), tb.toWorld(0, xr + 0.45f, cy - 0.15f, tz + b * 0.1f), r0.T,
                            0.17f, 0.07f, Mat::WOOD, R, 0.0f, 0.006f);
        for (float sg : {-1.0f, 1.0f})  // contrefiches
            addBoardBetween(hubMesh, tb.toWorld(0, sg > 0 ? xr : xl, cy - 1.7f, tz - 0.1f), tb.toWorld(0, sg > 0 ? xr - 1.1f : xl + 1.1f, cy - 0.3f, tz - 0.1f),
                            r0.T, 0.1f, 0.05f, Mat::WOOD, R, 0.0f, 0.006f);
        // enseigne en planches cerclées
        vec3 sc = tb.toWorld(0, (xl + xr) * 0.5f, cy + 0.65f, tz - 0.25f);
        addBoardBetween(hubMesh, sc - r0.R * 1.7f, sc + r0.R * 1.7f, r0.T, 0.32f, 0.04f, Mat::WOOD, R, 0.0f, 0.004f);
        for (float o : {-1.2f, 0.0f, 1.2f})
            addBoardBetween(hubMesh, sc + r0.R * o - r0.U * 0.34f, sc + r0.R * o + r0.U * 0.34f, r0.T, 0.04f, 0.055f, Mat::STEEL, R, 0.0f, 0.0f);
        hubMesh.endChunk();
        for (float xx : {xl - 0.9f, xr + 0.9f}) {  // torches de part et d'autre
            vec3 b = tb.toWorld(0, xx, fl0 - 0.05f, -1.4f);
            torch(vec3(b.x, G(b.x, b.z - hubFrontZ), b.z), 2.3f);
            solid(b.x, b.z - hubFrontZ, 0.3f);
        }
        // rails + traverses jusqu'au seuil
        hubMesh.beginChunk();
        for (float xx : {-0.55f, 0.55f})
            addBoardBetween(hubMesh, vec3(xx, G(xx, 1.0f) + 0.07f, hubFrontZ + 0.6f), vec3(xx, G(xx, 30.0f) + 0.07f, hubFrontZ + 30.0f), Y, 0.035f, 0.035f,
                            Mat::STEEL, R, 0.0f, 0.0f);
        for (float zr = 1.0f; zr < 30.0f; zr += 0.9f) box(vec3(0, G(0, zr) + 0.03f, hubFrontZ + zr), vec3(0.95f, 0.04f, 0.09f), R.r(-0.05f, 0.05f), Mat::WOOD);
        hubMesh.endChunk();
    }

    // ---- 5) matériel de mine
    // chariot sur rails chargé de minerai
    {
        float zc = 11.0f, y = G(0, zc);
        hubMesh.beginChunk();
        box(vec3(0, y + 0.42f, hubFrontZ + zc), vec3(0.6f, 0.06f, 0.95f), 0.0f, Mat::WOOD);  // fond
        for (float sx : {-1.0f, 1.0f}) box(vec3(sx * 0.62f, y + 0.74f, hubFrontZ + zc), vec3(0.05f, 0.35f, 0.95f), 0.0f, Mat::WOOD);
        for (float sz : {-1.0f, 1.0f}) box(vec3(0, y + 0.74f, hubFrontZ + zc + sz * 0.97f), vec3(0.62f, 0.35f, 0.05f), 0.0f, Mat::WOOD);
        for (float sz : {-0.55f, 0.0f, 0.55f}) box(vec3(0, y + 0.74f, hubFrontZ + zc + sz), vec3(0.66f, 0.04f, 0.035f), 0.0f, Mat::STEEL);
        for (float sx : {-1.0f, 1.0f})  // roues
            for (float sz : {-0.6f, 0.6f})
                addCylinder(hubMesh, vec3(sx * 0.58f, y + 0.29f, hubFrontZ + zc + sz), vec3(sx * 0.7f, y + 0.29f, hubFrontZ + zc + sz), 0.27f, 0.27f, 12, Mat::STEEL, 0.0f);
        for (int i = 0; i < 7; i++)  // minerai
            addEllipsoid(hubMesh, vec3(R.r(-0.4f, 0.4f), y + 1.05f + R.r(0, 0.12f), hubFrontZ + zc + R.r(-0.7f, 0.7f)), X, Y, Z, vec3(R.r(0.18f, 0.3f)), Mat::ROCK, R.f());
        for (int i = 0; i < 5; i++)  // pépites d'or
            addEllipsoid(hubMesh, vec3(R.r(-0.4f, 0.4f), y + 1.18f + R.r(0, 0.1f), hubFrontZ + zc + R.r(-0.7f, 0.7f)), X, Y, Z, vec3(R.r(0.07f, 0.12f)), Mat::GOLD, R.f());
        hubMesh.endChunk();
        for (float dz : {-0.7f, 0.0f, 0.7f}) solid(0, zc + dz, 0.8f);
        // minerai répandu devant l'entrée
        hubMesh.beginChunk();
        for (int i = 0; i < 9; i++)
            addEllipsoid(hubMesh, vec3(R.r(-2.5f, 2.5f), G(0, 4.0f) + 0.1f, hubFrontZ + R.r(2.0f, 6.0f)), X, Y, Z, vec3(R.r(0.12f, 0.3f), 0.12f, R.r(0.12f, 0.3f)),
                         Mat::ROCK, R.f());
        hubMesh.endChunk();
    }
    // forge sous auvent, à gauche
    {
        const float fx = -17.5f, fz = 14.0f;
        float y = G(fx, fz) - 0.15f;
        vec3 F(fx, y, hubFrontZ + fz);
        hubMesh.beginChunk();
        box(F + vec3(0, 0.6f, 0), vec3(1.4f, 0.6f, 1.7f), 0.0f, Mat::ROCK);          // massif de pierre
        box(F + vec3(-1.15f, 2.3f, 0), vec3(0.35f, 1.7f, 1.7f), 0.0f, Mat::ROCK);    // dosseret
        box(F + vec3(-1.15f, 5.2f, 0), vec3(0.6f, 2.3f, 0.6f), 0.0f, Mat::ROCK);     // cheminée
        box(F + vec3(0.5f, 1.22f, 0), vec3(0.8f, 0.05f, 1.25f), 0.0f, Mat::RED_GLOW);  // lit de braises
        for (float sz : {-1.3f, 1.3f}) box(F + vec3(0.55f, 1.35f, sz), vec3(0.8f, 0.14f, 0.2f), 0.0f, Mat::ROCK);
        for (int i = 0; i < 6; i++)  // braises
            addEllipsoid(hubMesh, F + vec3(R.r(-0.2f, 1.0f), 1.28f, R.r(-1.0f, 1.0f)), X, Y, Z, vec3(R.r(0.12f, 0.2f), 0.1f, R.r(0.12f, 0.2f)), Mat::YELLOW_GLOW, R.f());
        for (float sx : {-2.9f, 3.2f})  // auvent : poteaux
            for (float sz : {-3.1f, 3.1f})
                addBoardBetween(hubMesh, F + vec3(sx, -0.1f, sz), F + vec3(sx, 3.6f, sz), Z, 0.1f, 0.1f, Mat::WOOD, R, 0.0f, 0.006f);
        for (float sz : {-3.1f, 3.1f})  // sablières
            addBoardBetween(hubMesh, F + vec3(-2.9f, 3.65f, sz), F + vec3(3.2f, 3.65f, sz), Y, 0.1f, 0.08f, Mat::WOOD, R, 0.0f, 0.004f);
        for (int i = 0; i < 9; i++)  // toit en planches
            addBoardBetween(hubMesh, F + vec3(-3.0f, 3.78f + 0.02f * i, -3.3f + i * 0.8f), F + vec3(3.4f, 3.7f, -3.3f + i * 0.8f), Z, 0.0f + 0.4f, 0.03f,
                            Mat::WOOD, R, 0.0f, 0.004f);
        box(F + vec3(3.9f, 0.3f, -0.9f), vec3(0.42f, 0.4f, 0.42f), 0.0f, Mat::WOOD);  // billot + enclume
        box(F + vec3(3.9f, 0.9f, -0.9f), vec3(0.55f, 0.16f, 0.2f), 0.0f, Mat::STEEL);
        box(F + vec3(3.9f, 1.15f, -0.9f), vec3(0.3f, 0.1f, 0.2f), 0.0f, Mat::STEEL);
        addCylinder(hubMesh, F + vec3(4.45f, 0.95f, -0.9f), F + vec3(5.0f, 0.9f, -0.9f), 0.12f, 0.02f, 8, Mat::STEEL, 0.0f);  // bigorne
        for (int i = 0; i < 3; i++) {  // râtelier d'outils
            addBoardBetween(hubMesh, F + vec3(-0.6f + i * 0.5f, 0.0f, -3.0f), F + vec3(-0.6f + i * 0.5f, 1.8f, -3.0f), X, 0.03f, 0.03f, Mat::WOOD, R, 0.0f, 0.004f);
            box(F + vec3(-0.6f + i * 0.5f, 1.85f, -3.0f), vec3(0.03f, 0.05f, 0.34f), 0.0f, Mat::STEEL);
        }
        hubMesh.endChunk();
        barrel(fx + 3.2f, fz + 2.6f);
        barrel(fx + 4.0f, fz + 2.2f);
        sack(fx + 1.0f, fz + 4.4f, 1.0f);
        sack(fx + 1.6f, fz + 4.6f, 0.9f);
        sack(fx + 1.3f, fz + 5.2f, 1.0f);
        solid(fx, fz - 1.3f, 1.5f);
        solid(fx, fz, 1.5f);
        solid(fx, fz + 1.3f, 1.5f);
        solid(fx + 3.9f, fz - 0.9f, 0.7f);
        addLight(F + vec3(0.6f, 1.9f, -0.5f), vec3(1.0f, 0.42f, 0.12f), 4.8f, HUB_BIOME, true);
        addLight(F + vec3(0.6f, 1.9f, 0.7f), vec3(1.0f, 0.48f, 0.14f), 4.4f, HUB_BIOME, true);
    }
    // caisses, tonneaux et sacs à droite
    crate(14.0f, 8.0f, 0.0f, 0.3f, 1.0f);
    crate(15.1f, 8.4f, 0.0f, -0.2f, 1.0f);
    crate(14.4f, 8.1f, 0.9f, 0.1f, 0.95f);
    crate(13.3f, 9.4f, 0.0f, 0.5f, 0.9f);
    barrel(17.0f, 8.6f);
    barrel(17.9f, 9.6f);
    barrel(16.4f, 10.0f);
    sack(12.0f, 10.8f, 1.0f);
    sack(12.7f, 11.2f, 1.1f);
    sack(11.7f, 11.6f, 0.9f);
    sack(12.4f, 10.3f, 0.9f);
    sack(-4.5f, 5.0f, 1.0f);
    sack(-5.2f, 5.5f, 0.9f);
    sack(-4.8f, 6.3f, 1.1f);
    solid(-4.8f, 5.6f, 1.2f);
    // feu de camp, rondins, tente
    {
        const float cx = 11.0f, cz = 26.0f;
        float y = G(cx, cz);
        hubMesh.beginChunk();
        for (int i = 0; i < 9; i++) {  // cercle de pierres
            float a = 2 * PI * i / 9;
            addEllipsoid(hubMesh, vec3(cx + std::cos(a) * 0.85f, y + 0.12f, hubFrontZ + cz + std::sin(a) * 0.85f), X, Y, Z, vec3(0.28f, 0.2f, 0.26f), Mat::ROCK, R.f());
        }
        for (int i = 0; i < 4; i++) {  // bûches
            float a = PI * i / 4;
            addBoardBetween(hubMesh, vec3(cx - std::cos(a) * 0.55f, y + 0.12f, hubFrontZ + cz - std::sin(a) * 0.55f),
                            vec3(cx + std::cos(a) * 0.55f, y + 0.3f, hubFrontZ + cz + std::sin(a) * 0.55f), Y, 0.07f, 0.07f, Mat::TAR, R, 0.0f, 0.01f);
        }
        for (int i = 0; i < 3; i++) {  // rondins pour s'asseoir
            float a = 0.5f + i * 2.1f;
            vec3 c(cx + std::cos(a) * 2.6f, y + 0.26f, hubFrontZ + cz + std::sin(a) * 2.6f);
            addCylinder(hubMesh, c - vec3(-std::sin(a), 0, std::cos(a)) * 0.7f, c + vec3(-std::sin(a), 0, std::cos(a)) * 0.7f, 0.24f, 0.24f, 8, gCustomMats.bark, 0.0f);
        }
        hubMesh.endChunk();
        solid(cx, cz, 1.0f);
        addLight(vec3(cx, y + 0.9f, hubFrontZ + cz), vec3(1.0f, 0.5f, 0.16f), 4.4f, HUB_BIOME, true);
        // tente : deux pans de toile, faîtage, rabat rouge
        const float tx = 21.0f, tzr = 22.0f;
        float ty = G(tx, tzr) - 0.05f;
        hubMesh.beginChunk();
        for (float sg : {-1.0f, 1.0f}) {
            addBoardBetween(hubMesh, vec3(tx, ty + 2.3f, hubFrontZ + tzr), vec3(tx + sg * 1.9f, ty, hubFrontZ + tzr), Z, 1.9f, 0.02f, gCustomMats.canvas, R, 0.0f, 0.0f);
            addBoardBetween(hubMesh, vec3(tx + sg * 1.9f, ty, hubFrontZ + tzr - 1.9f), vec3(tx + sg * 1.9f, ty + 0.04f, hubFrontZ + tzr + 1.9f), Y, 0.05f, 0.05f,
                            Mat::WOOD, R, 0.0f, 0.0f);
        }
        addBoardBetween(hubMesh, vec3(tx, ty + 2.35f, hubFrontZ + tzr - 2.0f), vec3(tx, ty + 2.35f, hubFrontZ + tzr + 2.0f), X, 0.06f, 0.06f, Mat::WOOD, R, 0.0f, 0.0f);
        addBoardBetween(hubMesh, vec3(tx, ty + 2.3f, hubFrontZ + tzr + 1.9f), vec3(tx, ty + 0.1f, hubFrontZ + tzr + 1.9f), X, 0.9f, 0.015f, gCustomMats.cloth, R, 0.0f, 0.0f);
        hubMesh.endChunk();
        solid(tx, tzr, 2.1f);
    }
    // lanternes le long du chemin + panneau
    for (int i = 0; i < 4; i++) {
        float zr = 34.0f + i * 20.0f, x = hubPathX(hubFrontZ + zr) + (i % 2 ? 3.4f : -3.4f), y = G(x, zr);
        hubMesh.beginChunk();
        addBoardBetween(hubMesh, vec3(x, y - 0.1f, hubFrontZ + zr), vec3(x, y + 2.9f, hubFrontZ + zr), Z, 0.07f, 0.07f, Mat::WOOD, R, 0.0f, 0.004f);
        addBoardBetween(hubMesh, vec3(x, y + 2.8f, hubFrontZ + zr), vec3(x + (i % 2 ? -0.5f : 0.5f), y + 2.8f, hubFrontZ + zr), Z, 0.04f, 0.04f, Mat::WOOD, R, 0.0f, 0.0f);
        addEllipsoid(hubMesh, vec3(x + (i % 2 ? -0.5f : 0.5f), y + 2.55f, hubFrontZ + zr), X, Y, Z, vec3(0.13f, 0.2f, 0.13f), Mat::LANTERN_GLASS, 0.0f);
        hubMesh.endChunk();
        solid(x, zr, 0.3f);
        addLight(vec3(x + (i % 2 ? -0.5f : 0.5f), y + 2.55f, hubFrontZ + zr), vec3(1.0f, 0.74f, 0.42f), 3.4f, HUB_BIOME, false);
    }
    {
        float zr = 90.0f, x = hubPathX(hubFrontZ + zr) + 2.6f, y = G(x, zr);
        hubMesh.beginChunk();
        addBoardBetween(hubMesh, vec3(x, y - 0.1f, hubFrontZ + zr), vec3(x, y + 2.4f, hubFrontZ + zr), Z, 0.07f, 0.07f, Mat::WOOD, R, 0.0f, 0.004f);
        box(vec3(x - 0.4f, y + 2.1f, hubFrontZ + zr), vec3(0.75f, 0.14f, 0.025f), 0.0f, Mat::WOOD);
        box(vec3(x + 0.3f, y + 1.7f, hubFrontZ + zr), vec3(0.6f, 0.14f, 0.025f), 0.0f, Mat::WOOD);
        hubMesh.endChunk();
        solid(x, zr, 0.25f);
    }

    // ---- 6) rochers au pied de la montagne et semés dans la plaine
    hubMesh.beginChunk();
    for (int i = 0; i < 46; i++) {
        float x = R.r(-60.0f, 60.0f), zr = R.f() < 0.6f ? R.r(2.0f, 22.0f) : R.r(10.0f, 100.0f);
        if (std::fabs(x) < 5.0f && zr < 30.0f) continue;               // pas sur les rails
        if (std::fabs(x - hubPathX(hubFrontZ + zr)) < 2.5f) continue;  // pas sur le chemin
        float rad = R.r(0.5f, 2.2f);
        addEllipsoid(hubMesh, vec3(x, G(x, zr) + rad * 0.25f, hubFrontZ + zr), X, Y, Z, vec3(rad, rad * R.r(0.5f, 0.8f), rad * R.r(0.8f, 1.2f)), Mat::ROCK, R.f());
        solid(x, zr, rad * 0.8f);
    }
    hubMesh.endChunk();

    // ---- 7) arbres : clairsemés dans la plaine, forêt dense au-delà des murs invisibles
    for (int i = 0; i < 900; i++) {
        float x = R.r(-112.0f, 112.0f), zr = R.r(4.0f, 142.0f);
        bool play = std::fabs(x) < 58.0f && zr < 112.0f;  // dans la zone jouable
        if (play && R.f() > 0.07f) continue;              // plaine presque dégagée
        if (std::fabs(x - hubPathX(hubFrontZ + zr)) < 4.5f && zr > 20.0f) continue;
        if (std::sqrt(x * x + (zr - 14.0f) * (zr - 14.0f)) < 28.0f && play) continue;
        if (zr < 14.0f && std::fabs(x) < 40.0f) continue;
        float h = R.r(6.0f, 15.0f), y = G(x, zr);
        vec3 b(x, y, hubFrontZ + zr);
        if (R.f() < 0.6f) addPine(hubMesh, b, h, R);
        else addOak(hubMesh, b, h * 0.8f, R);
        if (play) solid(x, zr, 0.55f);
    }

    // ---- 8) cristaux géants dans la montagne (et deux au sol), colorés et lumineux
    {
        // grappe de `count` cristaux (prisme + pointe) plantée en `at`, orientée selon la normale `n`
        auto crystalCluster = [&](vec3 at, vec3 n, int hue, int count, float scale) {
            hubMesh.beginChunk();
            for (int i = 0; i < count; i++) {
                vec3 d = glm::normalize(n * 0.7f + vec3(R.r(-0.7f, 0.7f), R.r(0.1f, 0.8f), R.r(-0.7f, 0.7f)) + vec3(0, 0.3f, 0));
                float len = R.r(9.0f, 28.0f) * scale, rad = R.r(1.0f, 3.0f) * scale;
                int hv = (hue + (R.f() < 0.25f ? 1 : 0)) % 5;
                vec3 b = at - n * 1.5f + vec3(R.r(-1.2f, 1.2f), R.r(-0.6f, 0.6f), R.r(-1.2f, 1.2f)) * scale;
                vec3 mid = b + d * (len * 0.72f), tip = b + d * len;
                size_t v0 = hubMesh.v.size();
                addCylinder(hubMesh, b, mid, rad, rad * 0.9f, 6, gCustomMats.crystal[hv], R.f(), false, false);
                size_t v1 = hubMesh.v.size();
                addCylinder(hubMesh, mid, tip, rad * 0.9f, 0.0f, 6, gCustomMats.crystal[hv], R.f(), true, false);
                // uv.y : 0 à la base .. 1 à la pointe (le shader intensifie la lueur vers la pointe)
                for (size_t q = v0; q < v1; q++) hubMesh.v[q].uv.y *= 0.72f;
                for (size_t q = v1; q < hubMesh.v.size(); q++) hubMesh.v[q].uv.y = std::min(1.0f, 0.72f + 0.28f * hubMesh.v[q].uv.y);
            }
            hubMesh.endChunk();
            addLight(at + n * 4.0f, CRYSTAL_COLORS[hue] * 1.2f, 5.0f, 7 + hue, false);
        };
        for (int c = 0; c < 9; c++) {
            for (int tries = 0; tries < 50; tries++) {  // point de la façade assez haut, tourné vers le camp
                int s = 7 + (int)(R.f() * 20), k = 3 + (int)(R.f() * (K - 6));
                vec3 wp = cliff[(size_t)s * K + k];
                vec3 n = glm::normalize(glm::cross(cliff[(size_t)(s + 1) * K + k] - cliff[(size_t)(s - 1) * K + k],
                                                   cliff[(size_t)s * K + (k + 1) % K] - cliff[(size_t)s * K + (k + K - 1) % K]));
                if (glm::dot(n, wp - (r0.c + r0.T * 80.0f)) < 0) n = -n;
                float h = wp.y - hubGroundY;
                if (h < 10.0f || h > 60.0f || std::fabs(wp.x) < 7.0f || n.z < 0.15f) continue;
                crystalCluster(wp, n, c % 5, 4 + (int)(R.f() * 4), 1.0f);
                break;
            }
        }
        for (float sx : {-30.0f, 33.0f}) {
            float zr = 9.0f;
            crystalCluster(vec3(sx, G(sx, zr), hubFrontZ + zr), vec3(0, 0.6f, 0.4f), sx < 0 ? 4 : 1, 5, 0.85f);
            solid(sx, zr, 3.0f);
        }
    }

    for (auto& v : hubMesh.v) v.biome = (float)HUB_BIOME;
    hubSpawn = vec3(hubPathX(hubFrontZ + 100.0f), G(hubPathX(hubFrontZ + 100.0f), 100.0f) + 1.65f, hubFrontZ + 100.0f);
    std::printf("Camp de base : %zu sommets, %zu obstacles\n", hubMesh.v.size(), hubSolids.size());
}

vec3 World::constrainHub(vec3 p, bool fly) const {
    const float HW = 58.0f, HD = 112.0f;  // demi-largeur et profondeur de la zone jouable
    const Tube& tb = tubes[0];
    const Ring& r0 = tb.rings[0];
    vec3 q = p;
    q.x = std::min(std::max(q.x, -HW), HW);
    q.z = std::min(q.z, hubFrontZ + HD);
    // la montagne bloque, sauf dans l'axe de l'entrée de la mine
    float zMin = hubFrontZ + 0.3f, lo = 0, hi = 0;
    bool corridor = false;
    {
        float fl = tb.floorY(0, 0.0f), a1, a2, b1, b2;
        if (tb.wallRange(0, fl + 1.65f, a1, a2) && tb.wallRange(0, fl + 0.5f, b1, b2)) {
            lo = std::max(a1, b1) + 0.45f;
            hi = std::min(a2, b2) - 0.45f;
            corridor = lo < hi;
        }
    }
    float xr = glm::dot(q - r0.c, r0.R);
    if (!corridor || xr < lo - 0.05f || xr > hi + 0.05f) q.z = std::max(q.z, zMin);
    else if (q.z < zMin + 1.2f) q.x += r0.R.x * (std::min(std::max(xr, lo), hi) - xr);
    // obstacles cylindriques
    for (const HubSolid& s : hubSolids) {
        float dx = q.x - s.x, dz = q.z - s.z, rr = s.r + 0.35f, d2 = dx * dx + dz * dz;
        if (d2 < rr * rr) {
            float d = std::sqrt(std::max(d2, 1e-8f));
            q.x = s.x + dx / d * rr;
            q.z = s.z + dz / d * rr;
        }
    }
    float g = hubGround(q.x, q.z);
    q.y = fly ? std::max(q.y, g + 0.6f) : g + 1.65f;
    return q;
}
