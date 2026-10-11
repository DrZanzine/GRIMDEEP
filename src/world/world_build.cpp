// =============================================================================
//  world/world_build.cpp — génération des galeries et du maillage de roche
//
//  Étapes de World::build() :
//    1. tracé de la galerie principale (~270 m, sinueuse, sans se recouper) ;
//    2. section déformée par du bruit (parois irrégulières, sol presque plat) ;
//    3. galeries latérales en cul-de-sac (4 au maximum) ;
//    4. maillage de roche avec ouvertures aux jonctions, fonds en dôme ;
//    5. stalactites / stalagmites ;
//    6. aménagement (boisage, décors, objets, ennemis) puis camp de base ;
//    7. attribution des biomes aux sommets et envoi au GPU.
// =============================================================================
#include "world/world.h"

#include <cstdio>

#include "core/noise.h"
#include "render/geometry.h"
#include "render/materials.h"

// ---------------------------------------------------------------------------
//  Tracé des galeries
// ---------------------------------------------------------------------------

/// Renvoie le nombre d'anneaux de `rs` utilisables avant la première collision avec
/// une galerie existante ou avec elle-même (on ignore la zone de raccordement à la
/// galerie parente `parent`, autour de l'anneau `pj`).
int World::validTubeLength(const std::vector<Ring>& rs, int parent, int pj) const {
    int n = (int)rs.size();
    for (int i = 0; i < n; i++) {
        for (size_t t = 0; t < tubes.size(); t++) {
            const Tube& o = tubes[t];
            for (int j = 0; j < o.N; j++) {
                if ((int)t == parent && std::abs(j - pj) < 60 && i < 50) continue;
                vec3 d = rs[i].c - o.rings[j].c;
                float lim = rs[i].w + o.rings[j].w + 1.6f;
                if (std::fabs(d.x) < lim && std::fabs(d.z) < lim && std::fabs(d.y) < 6.0f && glm::dot(d, d) < lim * lim) return i;
            }
        }
        for (int j = 0; j + 70 < i; j += 2) {  // auto-intersection
            vec3 d = rs[i].c - rs[j].c;
            float lim = rs[i].w + rs[j].w + 1.6f;
            if (glm::dot(d, d) < lim * lim && std::fabs(d.y) < 6.0f) return i;
        }
    }
    return n;
}

/// Trace une galerie de `Nmax` anneaux au plus, à partir de `p0` avec le cap `yaw0`.
/// Le cap dérive selon un bruit 1D, rappelé vers le cap initial par un « ressort ».
/// On fait jusqu'à 40 essais (décalages de bruit différents) et on garde le plus long
/// tracé valide. Galerie principale : plus large, avec des chambres et le couloir des catacombes.
void World::generateTube(Tube& tb, int Nmax, vec3 p0, float yaw0, float pitch0, bool isMain, Rng& rng, int parent,
                         int pj, float branchW) {
    int bestLen = -1;
    std::vector<Ring> best;
    for (int attempt = 0; attempt < 40; attempt++) {
        float off = rng.r(0, 500);
        std::vector<Ring> rs(Nmax);
        float yaw = yaw0;
        vec3 pos = p0;
        for (int i = 0; i < Nmax; i++) {
            float sd = i * tb.ds;  // abscisse en mètres
            float amp = isMain ? 0.26f : 0.2f, spring = isMain ? 0.012f : 0.035f;
            float kc = isMain ? corridorWeight(0, i) : 0.0f;
            bool frozen = isMain && corridorStart >= 0 && i >= corridorStart - 12 && i <= corridorEnd;
            // couloir : cap figé (ligne droite)
            if (!frozen) yaw += tb.ds * (amp * (valueNoise1D(sd * 0.05f + off, seed) * 2 - 1) - spring * (yaw - yaw0));
            if (isMain) yaw = std::max(yaw0 - 1.15f, std::min(yaw0 + 1.15f, yaw));
            float pitch = (pitch0 + (isMain ? 0.12f : 0.05f) * (valueNoise1D(sd * 0.03f + off * 0.7f, seed + 3) * 2 - 1)) * (1.0f - kc);
            vec3 T(std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch));
            vec3 R = glm::normalize(glm::cross(T, vec3(0, 1, 0))), U = glm::cross(R, T);
            Ring r;
            r.c = pos; r.T = T; r.R = R; r.U = U; r.yaw = yaw; r.pitch = pitch;
            if (isMain) {
                float ch = smoothStep(0.62f, 0.82f, valueNoise1D(sd * 0.022f + off * 0.3f, seed + 5));  // chambres plus vastes
                r.w = 2.1f + 0.7f * (valueNoise1D(sd * 0.07f + off, seed + 7) * 2 - 1) + 2.4f * ch;
                r.h = 3.0f + 0.5f * (valueNoise1D(sd * 0.05f + off, seed + 9) * 2 - 1) + 1.6f * ch;
                r.w = mixf(r.w, 1.55f, kc);  // couloir étroit et haut
                r.h = mixf(r.h, 3.4f, kc);
            } else {
                r.w = branchW + 0.3f * (valueNoise1D(sd * 0.09f + off, seed + 7) * 2 - 1);
                r.h = 2.75f + 0.3f * (valueNoise1D(sd * 0.06f + off, seed + 9) * 2 - 1);
            }
            rs[i] = r;
            pos += T * tb.ds;
        }
        int len = validTubeLength(rs, parent, pj);
        if (len > bestLen) { bestLen = len; best = rs; }
        if (len >= Nmax) break;
    }
    best.resize(std::max(0, bestLen));
    tb.rings = best;
    tb.N = (int)best.size();
    tb.isMain = isMain;
    tb.parent = parent;
    tb.junction = pj;
}

/// Calcule la section déformée de chaque anneau de la galerie `ti` (tb.loc et tb.P) :
/// superellipse aplatie en bas, déformée par plusieurs octaves de bruit et des crêtes.
/// Dans le couloir des catacombes, la section devient une ogive lisse à nervures.
void World::computeSection(size_t ti) {
    Tube& tb = tubes[ti];
    int N = tb.N, M = tb.M;
    tb.loc.resize((size_t)N * M);
    tb.P.resize((size_t)N * M);
    uint32_t sd = seed + (uint32_t)ti * 131;
    for (int i = 0; i < N; i++) {
        const Ring& r = tb.rings[i];
        float kc = corridorWeight((int)ti, i);
        for (int k = 0; k < M; k++) {
            float th = 2 * PI * k / M, c = std::cos(th), s = std::sin(th);
            // couloir : parois droites, voûte en ogive
            float px = std::copysign(std::pow(std::fabs(c), mixf(0.75f, 0.32f, kc)), c) * r.w;
            float py = std::copysign(std::pow(std::fabs(s), mixf(0.8f, 0.55f, kc)), s) * r.h;
            if (py < 0) py *= 0.34f;
            vec3 pw = r.c + r.R * px + r.U * py;
            float rg = 1.0f - std::fabs(2.0f * fbm3D(pw * 0.7f, 3, sd + 21) - 1.0f);  // crêtes / fractures
            float d = (fbm3D(pw * 0.33f, 4, sd) - 0.5f) * 1.0f + (rg - 0.5f) * 0.5f
                    + (fbm3D(pw * 1.7f, 3, sd + 99) - 0.5f) * 0.38f + (fbm3D(pw * 5.0f, 2, sd + 12) - 0.5f) * 0.12f;
            d += 0.05f * std::sin(pw.y * 5.0f + 3.0f * fbm3D(pw * 0.5f, 2, sd + 7));
            if (py < 0) d *= 0.14f;  // sol presque plat (marche fluide)
            d *= 1.0f - 0.95f * kc;
            // nervures régulières, comme des côtes
            if (kc > 0 && py > -0.2f) d -= kc * 0.14f * std::pow(std::max(0.0f, std::cos(i * tb.ds * PI / 2.4f)), 24.0f);
            float r0 = std::sqrt(px * px + py * py), r1 = std::max(r0 + d, 0.8f);
            vec2 l(px * r1 / r0, py * r1 / r0);
            tb.loc[(size_t)i * M + k] = l;
            tb.P[(size_t)i * M + k] = r.c + r.R * l.x + r.U * l.y;
        }
    }
}

// ---------------------------------------------------------------------------
//  Maillage de roche
// ---------------------------------------------------------------------------

/// Construit le maillage des parois de toutes les galeries : ouvre les jonctions,
/// calcule les normales lisses, découpe en chunks de 12 anneaux, ferme les bouts.
void World::buildRockMesh() {
    // 1) ouvertures aux jonctions : on retire les quads de chaque galerie qui sont dans l'autre
    for (auto& tb : tubes) tb.cut.assign((size_t)std::max(0, tb.N - 1) * tb.M, 0);
    for (size_t bi = 1; bi < tubes.size(); bi++) {
        Tube& br = tubes[bi];
        Tube& mn = tubes[br.parent];
        int M = br.M;
        // vrai si le point q est à l'intérieur de la galerie t (anneaux r0..r1), avec une marge
        auto insideTube = [&](const Tube& t, int r0, int r1, vec3 q, float tmin, float margin) {
            int best = r0;
            float bt = 1e9f;
            for (int i = r0; i <= r1; i++) {
                float tt = std::fabs(glm::dot(q - t.rings[i].c, t.rings[i].T));
                if (tt < bt) { bt = tt; best = i; }
            }
            vec3 d = q - t.rings[best].c;
            float x = glm::dot(d, t.rings[best].R), y = glm::dot(d, t.rings[best].U), tt = glm::dot(d, t.rings[best].T);
            if (best == r0 && tt < tmin) return false;
            return std::sqrt(x * x + y * y) < t.radiusAt(best, std::atan2(y, x)) - margin;
        };
        int j = br.junction, lo = std::max(0, j - 14), hi = std::min(mn.N - 2, j + 14);
        for (int i = lo; i <= hi; i++)
            for (int k = 0; k < mn.M; k++) {
                int a = i * mn.M + k, b = i * mn.M + (k + 1) % mn.M, c = (i + 1) * mn.M + (k + 1) % mn.M, d = (i + 1) * mn.M + k;
                vec3 q = (mn.P[a] + mn.P[b] + mn.P[c] + mn.P[d]) * 0.25f;
                if (insideTube(br, 0, std::min(br.N - 1, 40), q, -0.3f, 0.22f)) mn.cut[(size_t)i * mn.M + k] = 1;
            }
        for (int i = 0; i < std::min(br.N - 1, 16); i++)
            for (int k = 0; k < M; k++) {
                int a = i * M + k, b = i * M + (k + 1) % M, c = (i + 1) * M + (k + 1) % M, d = (i + 1) * M + k;
                vec3 q = (br.P[a] + br.P[b] + br.P[c] + br.P[d]) * 0.25f;
                if (insideTube(mn, lo, hi + 1, q, -1e9f, 0.22f)) br.cut[(size_t)i * M + k] = 1;
            }
    }
    // 2) sommets, normales lisses (orientées vers l'axe de la galerie), chunks
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti];
        int N = tb.N, M = tb.M;
        tb.vBase = (int)rockMesh.v.size();
        std::vector<vec3> Nn(tb.P.size(), vec3(0));
        for (int i = 0; i < N - 1; i++)
            for (int k = 0; k < M; k++) {
                int a = i * M + k, b = i * M + (k + 1) % M, c = (i + 1) * M + (k + 1) % M, d = (i + 1) * M + k;
                vec3 n1v = glm::cross(tb.P[b] - tb.P[a], tb.P[c] - tb.P[a]), n2v = glm::cross(tb.P[c] - tb.P[a], tb.P[d] - tb.P[a]);
                Nn[a] += n1v + n2v; Nn[b] += n1v; Nn[c] += n1v + n2v; Nn[d] += n2v;
            }
        for (size_t i = 0; i < tb.P.size(); i++) {
            vec3 n = glm::normalize(Nn[i] + vec3(1e-6f));
            if (glm::dot(n, tb.rings[i / M].c - tb.P[i]) < 0) n = -n;
            float kc = corridorWeight((int)ti, (int)(i / M));
            rockMesh.v.push_back({tb.P[i], n, vec2(0), kc > 0.01f ? Mat::ROCK_CORRIDOR : Mat::ROCK, kc});
        }
        const int cr = 12;  // anneaux par chunk
        for (int i0 = 0; i0 < N - 1; i0 += cr) {
            int i1 = std::min(i0 + cr, N - 1);
            rockMesh.beginChunk();
            for (int i = i0; i < i1; i++)
                for (int k = 0; k < M; k++) {
                    if (tb.cut[(size_t)i * M + k]) continue;
                    uint32_t a = tb.vBase + i * M + k, b = tb.vBase + i * M + (k + 1) % M,
                             c = tb.vBase + (i + 1) * M + (k + 1) % M, d = tb.vBase + (i + 1) * M + k;
                    rockMesh.idx.insert(rockMesh.idx.end(), {a, b, c, a, c, d});
                }
            rockMesh.endChunk((size_t)tb.vBase + (size_t)i0 * M, (size_t)tb.vBase + (size_t)(i1 + 1) * M);
        }
        // fond : lumière du jour pour la galerie principale (sortie), dôme de roche sinon
        addTubeEnd((int)ti, N - 1, +1, tb.isMain ? Mat::EXIT_SKY : Mat::ROCK);
    }
}

/// Ferme le bout d'une galerie à l'anneau `ring`, dans le sens `dir` (+1 = vers l'avant).
/// Matériau < 16 : cul-de-sac en dôme de roche (même bruit que la paroi).
/// Sinon : cône plat émissif (lumière du jour au fond de la sortie).
void World::addTubeEnd(int ti, int ring, int dir, float mat) {
    Tube& tb = tubes[ti];
    int M = tb.M;
    const Ring& r = tb.rings[ring];
    if (mat < 16.0f) {
        const int RW = 8;
        std::vector<vec3> Pp, ref;
        float L = std::min(2.4f, r.w * 0.9f + 0.6f);
        for (int q = 0; q <= RW; q++) {
            float t = (float)q / RW, sc = std::cos(t * PI * 0.5f);
            vec3 c = r.c + r.T * (std::sin(t * PI * 0.5f) * L * dir);
            for (int k = 0; k < M; k++) {
                vec3 rel = tb.P[(size_t)ring * M + k] - r.c, p = c + rel * sc;
                if (q > 0 && q < RW) p += glm::normalize(rel + vec3(1e-4f)) * (fbm3D(p * 0.6f, 3, seed + 31) - 0.5f) * 0.6f * std::sin(t * PI);
                Pp.push_back(p);
                ref.push_back(c);
            }
        }
        rockMesh.beginChunk();
        addGrid(rockMesh, Pp, ref, RW + 1, M, 1.0f, Mat::ROCK, 0.0f);
        rockMesh.endChunk();
        return;
    }
    vec3 apex = r.c + r.T * ((mat > 16.0f ? 3.5f : 2.0f) * dir) + vec3(0, mat > 16.0f ? 0.0f : -0.4f, 0);
    vec3 inward = -r.T * (float)dir;
    rockMesh.beginChunk();
    uint32_t base = (uint32_t)rockMesh.v.size();
    for (int k = 0; k < M; k++) rockMesh.v.push_back({tb.P[(size_t)ring * M + k], inward, vec2(0), mat, 0.0f});
    rockMesh.v.push_back({apex, inward, vec2(0), mat, 0.0f});
    for (int k = 0; k < M; k++)
        rockMesh.idx.insert(rockMesh.idx.end(), {base + (uint32_t)k, base + (uint32_t)((k + 1) % M), base + (uint32_t)M});
    rockMesh.endChunk();
    for (int k = 0; k <= M; k++) rockMesh.v[base + k].n = glm::normalize(inward + glm::normalize(rockMesh.v[base + k].p - r.c) * -0.3f);
}

/// Ajoute une stalactite / stalagmite : cône bruité et légèrement courbé, de `base`
/// le long de `axis` sur `len` mètres, rayon de base `r0`.
void World::addCone(vec3 base, vec3 axis, float len, float r0, Rng& rng) {
    const int rows = 9, cols = 10;
    vec3 e1 = glm::normalize(glm::cross(axis, std::fabs(axis.x) < 0.9f ? vec3(1, 0, 0) : vec3(0, 0, 1)));
    vec3 e2 = glm::cross(axis, e1);
    vec3 bend = vec3(rng.r(-1, 1), 0, rng.r(-1, 1)) * len * 0.08f;
    std::vector<vec3> P, ref;
    float sd = rng.f() * 50;
    for (int r = 0; r < rows; r++) {
        float t = r / float(rows - 1);
        float rad = r0 * std::pow(1 - t, 1.15f) + 0.004f;
        vec3 c = base + axis * (len * t) + bend * t * t;
        for (int k = 0; k < cols; k++) {
            float a = 2 * PI * k / cols;
            float nz = 0.75f + 0.5f * valueNoise3D(std::cos(a) * 2 + sd, t * 5, std::sin(a) * 2, seed + 55);
            P.push_back(c + (e1 * std::cos(a) + e2 * std::sin(a)) * rad * nz);
            ref.push_back(c);
        }
    }
    addGrid(rockMesh, P, ref, rows, cols, -1.0f, Mat::ROCK, 0.0f);
}

/// Sème des stalactites (62 %) et stalagmites dans toutes les galeries, sauf près des
/// jonctions et du couloir. Chaque stalactite devient une source de gouttes (Drip).
void World::addStalactites(Rng& rng) {
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti];
        int M = tb.M;
        int count = tb.isMain ? 150 : 22;
        rockMesh.beginChunk();
        size_t vstart = rockMesh.v.size();
        int inChunk = 0;
        for (int n = 0; n < count; n++) {
            int i = 8 + rng.next() % (tb.N - 16);
            if ((!tb.isMain && i < 16) || nearCorridor((int)ti, i, 2)) continue;
            bool nearJ = false;
            for (size_t b = 1; b < tubes.size(); b++)
                if (tubes[b].parent == (int)ti && std::abs(i - tubes[b].junction) < 14) nearJ = true;
            if (nearJ) continue;
            bool ceil = rng.chance(0.62f);
            int k = ceil ? (int)(M * rng.r(0.08f, 0.42f)) : (int)(M * rng.r(0.58f, 0.92f));  // point de la section (haut / bas)
            Vertex v = rockMesh.v[(size_t)tb.vBase + (size_t)i * M + k];
            float len = ceil ? rng.r(0.25f, 1.5f) : rng.r(0.15f, 0.8f);
            vec3 axis = glm::normalize((ceil ? vec3(0, -1, 0) : vec3(0, 1, 0)) + v.n * 0.15f);
            addCone(v.p - v.n * 0.05f - axis * 0.04f, axis, len, len * rng.r(0.17f, 0.28f) + 0.04f, rng);
            if (ceil) drips.push_back(Drip{v.p + axis * (len - 0.1f), (int)ti, i, rng.r(0.0f, 10.0f)});
            if (++inChunk == 8) {
                rockMesh.endChunk(vstart, rockMesh.v.size());
                rockMesh.beginChunk();
                vstart = rockMesh.v.size();
                inChunk = 0;
            }
        }
        rockMesh.endChunk(vstart, rockMesh.v.size());
    }
}

// ---------------------------------------------------------------------------
//  Génération complète
// ---------------------------------------------------------------------------

void World::destroy() {
    hubMesh.destroy();
    rockMesh.destroy();
    woodMesh.destroy();
    torchMesh.destroy();
    webMesh.destroy();
    waterMesh.destroy();
    debrisMesh.destroy();
    ratMesh.destroy();
}

void World::build(uint32_t sd) {
    destroy();
    seed = sd;
    hubMesh = Mesh(); rockMesh = Mesh(); woodMesh = Mesh(); torchMesh = Mesh();
    webMesh = Mesh(); waterMesh = Mesh(); debrisMesh = Mesh(); ratMesh = Mesh();
    lights.clear(); frames.clear(); webs.clear(); holes.clear(); debris.clear(); tubes.clear();
    pickups.clear(); spawns.clear(); drips.clear(); doorBoards.clear(); props.clear(); rockfalls.clear();
    doorOpen = false;
    keysNeeded = 0;
    doorRing = 0;
    Rng rng(sd);

    // --- galerie principale (le couloir des catacombes est prévu à ~63 % du parcours)
    tubes.push_back(Tube());
    corridorStart = (int)(900 * 0.92f * 0.635f);
    corridorEnd = corridorStart + 92;  // ~28 m dans les catacombes
    generateTube(tubes[0], 900, vec3(0), 0.0f, 0.0f, true, rng, -1, -1, 0);
    doorRing = (int)(tubes[0].N * 0.92f);
    if (corridorEnd + 40 > doorRing || corridorStart < 5 * doorRing * 3 / 25) corridorStart = corridorEnd = -1;  // galerie trop courte
    // après la porte, la galerie s'élargit vers la sortie
    for (int i = doorRing; i < tubes[0].N; i++) {
        float k = smoothStep((float)doorRing, (float)doorRing + 25.0f, (float)i);
        tubes[0].rings[i].w *= 1.0f + 0.5f * k;
        tubes[0].rings[i].h *= 1.0f + 0.3f * k;
    }
    computeSection(0);

    // --- galeries latérales en culs-de-sac (espacées d'au moins 110 anneaux)
    int wantBranches = 4;
    for (int attempt = 0; attempt < 14 && (int)tubes.size() - 1 < wantBranches; attempt++) {
        Tube& mn = tubes[0];
        if (mn.N < 300) break;
        int j = 70 + rng.next() % std::max(1, doorRing - 150);
        bool clash = false;
        for (size_t b = 1; b < tubes.size(); b++)
            if (std::abs(tubes[b].junction - j) < 110) clash = true;
        if (nearCorridor(0, j, 25)) clash = true;
        if (clash) continue;
        int side = rng.chance(0.5f) ? 1 : -1;
        const Ring& r = mn.rings[j];
        float yawB = r.yaw + side * (PI * 0.5f - 0.3f);
        float wB = rng.r(1.5f, 1.9f), hB = 2.75f;
        float fy = mn.floorY(j, 0.0f), xl, xr;
        if (!mn.wallRange(j, fy + 1.0f, xl, xr)) continue;
        float xw = side > 0 ? xr : xl;
        vec3 W = mn.toWorld(j, xw, fy + 0.34f * hB + 0.02f);  // départ dans la paroi de la galerie principale
        vec3 dB(std::sin(yawB), 0, -std::cos(yawB));
        Tube tb;
        tb.side = side;
        generateTube(tb, 50 + (int)(rng.next() % 60), W - dB * 1.0f, yawB, 0.0f, false, rng, 0, j, wB);
        if (tb.N < 45) continue;
        tb.junction = j;
        tubes.push_back(tb);
        computeSection(tubes.size() - 1);
    }

    // --- maillages et contenu
    buildRockMesh();
    addStalactites(rng);
    furnishGalleries(rng);
    buildHub();
    assignBiomes(rockMesh); assignBiomes(woodMesh); assignBiomes(torchMesh); assignBiomes(webMesh); assignBiomes(waterMesh);
    rockMesh.upload(); woodMesh.upload(); torchMesh.upload(); webMesh.upload(); waterMesh.upload(); hubMesh.upload();

    // --- résumé dans la console
    for (size_t b = 1; b < tubes.size(); b++)
        std::printf("  galerie %zu : jonction a %.0f m (%s), longueur %.0f m\n", b, tubes[b].junction * tubes[0].ds,
                    tubes[b].side > 0 ? "droite" : "gauche", tubes[b].N * tubes[b].ds);
    for (auto& h : holes)
        std::printf("  trous de rats : galerie %d, %.0f m -> %.0f m\n", h.tube, h.iA * tubes[h.tube].ds, h.iB * tubes[h.tube].ds);
    std::printf("Grotte #%u : %zu galeries, %zu torches, %zu planches, %zu toiles, %zu trous de rats\n", sd, tubes.size(),
                lights.size(), woodMesh.boards.size(), webs.size(), holes.size() * 2);
}
