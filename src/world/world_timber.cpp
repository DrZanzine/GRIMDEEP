// =============================================================================
//  world/world_timber.cpp — boisage de la mine et aménagement des galeries
//
//  Le boisage est fait de planches (Board) enregistrées dans woodMesh : chacune a
//  une collision et peut être cassée à la pioche. On trouve :
//    - des cadres de soutènement (2 poteaux doublés, linteau, contrefiches) ;
//    - du coffrage (planches horizontales contre les parois entre deux cadres) ;
//    - des échelles et des barricades (modèle 3D + planches invisibles de collision) ;
//    - des planches appuyées ou tombées au sol ;
//    - les supports des torches murales.
//  furnishGalleries() orchestre l'aménagement complet de chaque galerie.
// =============================================================================
#include "world/world.h"

#include <algorithm>

#include "core/noise.h"
#include "render/geometry.h"
#include "render/materials.h"
#include "render/model_loader.h"

/// Ajoute une planche de bois de `a` à `b` dans woodMesh et la rattache à la galerie
/// `ti`, au cadre `fid` avec le rôle `role` (voir Board). Les poteaux et linteaux
/// encaissent 2 coups. Renvoie l'index de la planche, ou -1 si a et b sont confondus.
int World::addTimber(int ti, int fid, int role, int side, vec3 a, vec3 b, vec3 hint, float hw, float ht, Rng& rng,
                     float ext, float jit) {
    size_t before = woodMesh.boards.size();
    addBoardBetween(woodMesh, a, b, hint, hw, ht, Mat::WOOD, rng, ext, jit);
    if (woodMesh.boards.size() == before) return -1;
    Board& bd = woodMesh.boards.back();
    bd.tube = ti;
    bd.frame = fid;
    bd.role = role;
    bd.side = side;
    bd.hp = (role == 1 || role == 2) ? 2 : 1;
    return (int)woodMesh.boards.size() - 1;
}

/// Ajoute une toile d'araignée triangulaire de sommet `corner`, tendue le long des
/// directions `A` et `B` sur `size` mètres. `fid` : cadre porteur (-1 : paroi).
void World::addWeb(int fid, vec3 corner, vec3 A, vec3 B, float size) {
    vec3 n = glm::normalize(glm::cross(A, B));
    uint32_t i0 = (uint32_t)webMesh.idx.size();
    uint32_t base = (uint32_t)webMesh.v.size();
    // Vertex::seed = taille (le shader adapte l'épaisseur des fils)
    webMesh.v.push_back({corner, n, vec2(0, 0), Mat::WEB, size});
    webMesh.v.push_back({corner + A * size, n, vec2(1, 0), Mat::WEB, size});
    webMesh.v.push_back({corner + B * size, n, vec2(0, 1), Mat::WEB, size});
    webMesh.idx.insert(webMesh.idx.end(), {base, base + 1, base + 2});
    Web w;
    w.c = corner + (A + B) * size * 0.33f;
    w.r = size * 0.6f;
    w.i0 = i0;
    w.alive = true;
    w.frame = fid;
    webs.push_back(w);
    if (fid >= 0) frames[fid].webs.push_back((int)webs.size() - 1);
}

/// Ajoute un cadre de soutènement à l'anneau `i` de la galerie `ti` : poteaux doublés
/// de chaque côté, linteau doublé, contrefiches (toujours présentes si `doorway`),
/// parfois une barricade, et des toiles dans les coins hauts.
void World::addFrame(int ti, int i, Rng& rng, bool doorway) {
    Tube& tb = tubes[ti];
    const Ring& r = tb.rings[i];
    int fid = (int)frames.size();
    frames.push_back(TimberFrame());
    frames[fid].tube = ti;
    float fy = tb.floorY(i, 0.0f), xl0, xr0;
    if (!tb.wallRange(i, fy + 0.7f, xl0, xr0)) return;
    float xl = xl0 + 0.18f, xr = xr0 - 0.18f;
    float fyl = tb.floorY(i, xl), fyr = tb.floorY(i, xr);
    float tyl = tb.ceilY(i, xl + 0.12f) - 0.2f, tyr = tb.ceilY(i, xr - 0.12f) - 0.2f;
    woodMesh.beginChunk();
    // poteaux (2 par côté, légèrement inclinés)
    for (int s = 0; s < 2; s++) {
        float x = s ? xr : xl, y0 = s ? fyr : fyl, y1 = s ? tyr : tyl, lean = rng.r(-0.05f, 0.05f);
        for (int b = 0; b < 2; b++) {
            float t = (b - 0.5f) * 0.095f + rng.r(-0.01f, 0.01f);
            vec3 a = tb.toWorld(i, x + rng.r(-0.02f, 0.02f), y0 - 0.1f, t), c = tb.toWorld(i, x + lean, y1 - 0.12f, t);
            int id = addTimber(ti, fid, 1, s, a, c, r.T, 0.15f, 0.045f, rng);
            if (id >= 0) frames[fid].post[s].push_back(id);
        }
    }
    // linteau (2 planches)
    for (int b = 0; b < 2; b++) {
        float t = (b - 0.5f) * 0.095f + rng.r(-0.01f, 0.01f), sh = b ? 0.12f : -0.06f;
        vec3 a = tb.toWorld(i, xl - 0.25f + sh, tyl, t), c = tb.toWorld(i, xr + 0.25f + sh, tyr, t);
        int id = addTimber(ti, fid, 2, 0, a, c, r.T, 0.14f, 0.045f, rng);
        if (id >= 0) frames[fid].beam.push_back(id);
    }
    // contrefiches en diagonale sous le linteau
    for (int s = 0; s < 2; s++) {
        if (!doorway && !rng.chance(0.75f)) continue;
        float sg = s ? -1.f : 1.f, x = s ? xr : xl, ty = s ? tyr : tyl;
        vec3 a = tb.toWorld(i, x, ty - rng.r(0.9f, 1.3f), 0.14f), b = tb.toWorld(i, x + sg * rng.r(0.9f, 1.2f), ty - 0.1f, 0.14f);
        int id = addTimber(ti, fid, 3, s, a, b, r.T, 0.085f, 0.035f, rng);
        if (id >= 0) frames[fid].brace[s].push_back(id);
    }
    // barricade en travers (ou, sans modèle, une simple planche)
    if (rng.chance(0.5f) && !addBarricade(ti, i, rng)) {
        float y = std::max(fyl, fyr) + rng.r(0.35f, 0.6f);
        addTimber(ti, fid, 0, 0, tb.toWorld(i, xl, y, -0.14f), tb.toWorld(i, xr, y + rng.r(-0.05f, 0.05f), -0.14f), r.T, 0.07f, 0.035f, rng);
    }
    woodMesh.endChunk();
    // toiles d'araignée dans les coins hauts du cadre
    for (int s = 0; s < 2; s++)
        if (rng.chance(0.6f)) {
            float x = s ? xr - 0.14f : xl + 0.14f, ty = s ? tyr : tyl;
            float sg = s ? -1.f : 1.f;
            vec3 corner = tb.toWorld(i, x, ty - 0.13f, 0.1f);
            vec3 A = glm::normalize(tb.toWorld(i, x + sg * 1.0f, ty - 0.13f, 0.1f) - corner), B = -r.U;
            addWeb(fid, corner, A, B, rng.r(0.5f, 1.2f));
        }
}

/// Ajoute du coffrage (planches horizontales tous les 26,5 cm) contre la paroi `side`
/// (+1 droite, -1 gauche) entre les anneaux `i` et `j` (deux cadres consécutifs).
void World::addLagging(int ti, int i, int j, int side, Rng& rng) {
    Tube& tb = tubes[ti];
    int mid = (i + j) / 2;
    const Ring& r = tb.rings[mid];
    float fy = tb.floorY(mid, 0.0f), topY = std::min(tb.ceilY(mid, 0.0f), 3.0f) - 0.9f;
    float y = fy + 0.3f + rng.r(0, 0.1f);
    woodMesh.beginChunk();
    woodMesh.geom = 1;  // planches = modèle plank.obj
    while (y < topY) {
        if (rng.chance(0.82f)) {  // quelques planches manquantes
            float xl, xr;
            if (!tb.wallRange(mid, y, xl, xr)) break;
            float xw = side > 0 ? xr : xl, span = (j - i) * tb.ds * 0.5f - 0.3f;
            if (span < 0.5f) break;
            float t0 = rng.r(-span, span * 0.4f), t1 = std::min(t0 + rng.r(1.2f, 2.6f), span + 0.2f);
            vec3 a = tb.toWorld(mid, xw - side * rng.r(-0.06f, 0.03f), y, t0),
                 b = tb.toWorld(mid, xw - side * rng.r(-0.06f, 0.03f), y + rng.r(-0.03f, 0.04f), t1);
            addTimber(ti, -1, 0, 0, a, b, r.R * (float)(-side), 0.13f, 0.03f, rng, 0.0f, 0.015f);
        }
        y += 0.265f;
    }
    woodMesh.geom = 0;
    woodMesh.endChunk();
}

/// Ajoute une échelle appuyée contre la paroi `side` à l'anneau `i`.
/// Avec le modèle ladder.obj : modèle visible + 11 planches invisibles (montants et
/// barreaux) qui s'effondrent ensemble. Sans modèle : échelle en boîtes.
void World::addLadder(int ti, int i, int side, Rng& rng) {
    Tube& tb = tubes[ti];
    const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr;
    float yTop = std::min(tb.ceilY(i, 0.0f) - 0.5f, fy + 3.1f);
    if (!tb.wallRange(i, yTop, xl, xr)) return;
    float xwTop = side > 0 ? xr : xl;
    float xBase = xwTop - side * 0.95f;
    float yBase = tb.floorY(i, xBase) + 0.02f;
    vec3 dirLadder = glm::normalize(tb.toWorld(i, xwTop - side * 0.07f, yTop) - tb.toWorld(i, xBase, yBase));
    woodMesh.beginChunk();
    ModelProp lad;
    woodMesh.geom = 2;  // planches invisibles : collision et destruction
    for (int s = 0; s < 2; s++) {  // montants
        float t = s ? 0.22f : -0.22f;
        lad.boards.push_back(addTimber(ti, -1, 0, 0, tb.toWorld(i, xBase, yBase, t), tb.toWorld(i, xwTop - side * 0.07f, yTop, t), r.T,
                                       0.04f, 0.025f, rng, 0.0f, 0.006f));
    }
    int rungs = 9;
    for (int k = 0; k < rungs; k++) {  // barreaux
        float f = (k + 0.7f) / (rungs + 0.3f);
        float x = mixf(xBase, xwTop - side * 0.07f, f), y = mixf(yBase, yTop, f);
        lad.boards.push_back(addTimber(ti, -1, 0, 0, tb.toWorld(i, x - side * 0.02f, y, -0.24f), tb.toWorld(i, x - side * 0.02f, y, 0.24f),
                                       dirLadder, 0.035f, 0.02f, rng, 0.0f, 0.005f));
    }
    woodMesh.geom = 0;
    lad.boards.erase(std::remove(lad.boards.begin(), lad.boards.end(), -1), lad.boards.end());
    if (gModels.ladder.ok) {
        // repère du modèle : y le long de l'échelle, x dans le sens de la galerie
        vec3 b0 = tb.toWorld(i, xBase, yBase, 0.0f), b1 = tb.toWorld(i, xwTop - side * 0.07f, yTop, 0.0f);
        vec3 ey = glm::normalize(b1 - b0), ex = glm::normalize(r.T - ey * glm::dot(r.T, ey)), ez = glm::cross(ex, ey);
        float len = glm::length(b1 - b0);
        mat4 W(1.0f);
        W[0] = vec4(ex * (0.44f / 0.52f), 0);
        W[1] = vec4(ey * (len / 3.0f), 0);
        W[2] = vec4(ez, 0);
        W[3] = vec4(b0, 1);
        lad.i0 = (uint32_t)woodMesh.idx.size();
        appendModel(woodMesh, gModels.ladder, W, rng.f() * 10.0f, vec2(rng.f() * 5.0f, rng.f() * 5.0f));
        lad.n = (uint32_t)woodMesh.idx.size() - lad.i0;
        lad.look = (b0 + b1) * 0.5f;
        lad.stand = tb.toWorld(i, 0.0f, tb.floorY(i, 0.0f) + 1.65f);
        int lid = (int)props.size();
        for (int id : lad.boards) woodMesh.boards[id].group = lid;
        props.push_back(lad);
    } else {  // pas de modèle : échelle en planches (boîtes)
        for (int id : lad.boards) {
            Board& b = woodMesh.boards[id];
            Rng z(id);
            b.i0 = (uint32_t)woodMesh.idx.size();
            addBoardBox(woodMesh, b.c, b.dl, b.dw, b.dt, b.hl, b.hw, b.ht, Mat::WOOD, z.f() * 10.0f, z, 0.005f);
            b.n = 36;
        }
    }
    woodMesh.endChunk();
}

/// Ajoute une barricade (planks_barricade.obj) qui barre toute la galerie à l'anneau
/// `i` : modèle visible étiré à la largeur de la galerie + planches invisibles pour la
/// collision. Comme une échelle, elle s'effondre d'un bloc (2 coups de pioche).
/// Renvoie faux si le modèle manque ou si la galerie est trop étroite / trop large.
bool World::addBarricade(int ti, int i, Rng& rng) {
    if (!gModels.barricade.ok) return false;
    Tube& tb = tubes[ti];
    const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr;
    if (!tb.wallRange(i, fy + 0.8f, xl, xr)) return false;
    float xm = (xl + xr) * 0.5f, Wd = xr - xl + 0.12f;
    float base = std::min(fy, std::min(tb.floorY(i, xl + 0.3f), tb.floorY(i, xr - 0.3f))) - 0.05f;
    float top = std::min(tb.ceilY(i, xm), std::min(tb.ceilY(i, xl + 0.3f), tb.ceilY(i, xr - 0.3f))) - 0.12f;
    float sx = Wd / 1.6f, sy = std::min(1.6f, std::max(0.8f, (top - base) / 2.0f));  // échelle du modèle (1,6 x 2 m)
    if (Wd < 1.0f || Wd > 6.0f) return false;
    vec3 o = tb.toWorld(i, xm, base, -0.14f), ex = r.R, ey = r.U, ez = glm::cross(ex, ey);
    ModelProp ob;
    ob.kind = 1;
    woodMesh.geom = 2;
    for (float h : {0.3f, 0.77f, 1.24f, 1.71f}) {  // planches horizontales
        vec3 c = o + ey * (h * sy) + ez * 0.045f;
        ob.boards.push_back(addTimber(ti, -1, 0, 0, c - ex * (Wd * 0.5f), c + ex * (Wd * 0.5f), ez, 0.1f * sy, 0.03f, rng, 0.0f, 0.0f));
    }
    for (float sg : {-1.0f, 1.0f}) {  // montants
        vec3 c = o + ex * (0.5f * sx * sg) + ez * 0.0175f;
        ob.boards.push_back(addTimber(ti, -1, 0, 0, c, c + ey * (2.0f * sy), ez, 0.1f * sx, 0.0175f, rng, 0.0f, 0.0f));
    }
    woodMesh.geom = 0;
    ob.boards.erase(std::remove(ob.boards.begin(), ob.boards.end(), -1), ob.boards.end());
    mat4 W(1.0f);
    W[0] = vec4(ex * sx, 0);
    W[1] = vec4(ey * sy, 0);
    W[2] = vec4(ez, 0);
    W[3] = vec4(o, 1);
    ob.i0 = (uint32_t)woodMesh.idx.size();
    appendModel(woodMesh, gModels.barricade, W, rng.f() * 10.0f, vec2(rng.f() * 5.0f, rng.f() * 5.0f));
    ob.n = (uint32_t)woodMesh.idx.size() - ob.i0;
    ob.look = o + ey * (1.0f * sy);
    ob.stand = tb.toWorld(i, xm, tb.floorY(i, xm) + 1.65f, -2.2f);
    int lid = (int)props.size();
    for (int id : ob.boards) {
        woodMesh.boards[id].group = lid;
        woodMesh.boards[id].hp = 2;
    }
    props.push_back(ob);
    return true;
}

/// Ajoute une torche murale côté `side` à l'anneau `i` : planche de fixation, manche
/// incliné, brides d'acier, étoupe goudronnée, et la lumière (couleur du biome).
void World::addTorch(int ti, int i, int side, Rng& rng) {
    Tube& tb = tubes[ti];
    const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr, yb = fy + 1.7f;
    if (!tb.wallRange(i, yb, xl, xr)) return;
    float xw = side > 0 ? xr : xl;
    torchMesh.beginChunk();
    // planche de fixation clouée au mur
    addBoardBetween(torchMesh, tb.toWorld(i, xw - side * 0.035f, yb - 0.4f), tb.toWorld(i, xw - side * 0.035f, yb + 0.35f), r.R * (float)side,
                    0.075f, 0.022f, Mat::WOOD, rng, 0.0f, 0.006f);
    // manche incliné
    vec3 base = tb.toWorld(i, xw - side * 0.095f, yb - 0.35f);
    vec3 dir = glm::normalize(r.R * (-side * 0.26f) + r.U * 0.96f + r.T * rng.r(-0.05f, 0.05f));
    vec3 top = base + dir * 1.15f;
    addBoardBetween(torchMesh, base, top, r.T, 0.04f, 0.04f, Mat::WOOD, rng, 0.0f, 0.004f);
    // brides métalliques
    for (float f : {0.12f, 0.38f}) {
        vec3 p = base + dir * (1.15f * f);
        addBoardBetween(torchMesh, p - dir * 0.022f, p + dir * 0.022f, r.R, 0.06f, 0.06f, Mat::STEEL, rng, 0.0f, 0.0f);
    }
    // étoupe goudronnée + goutte de poix
    addBoardBetween(torchMesh, top - dir * 0.22f, top + dir * 0.04f, r.T, 0.072f, 0.072f, Mat::TAR, rng, 0.0f, 0.01f);
    addBoardBetween(torchMesh, top - dir * 0.02f, top + dir * 0.07f, r.T, 0.055f, 0.055f, Mat::TAR, rng, 0.0f, 0.012f);
    torchMesh.endChunk();
    int bi = (int)std::floor(biomeAt(ti, i));
    bi = std::min(4, std::max(0, bi));
    addLight(top + dir * 0.13f + r.U * 0.04f, BIOMES[bi].torch, BIOMES[bi].torchI, bi, true);
}

/// Aménage toutes les galeries : éboulis, cadres, coffrage, torches, échelles, planches
/// au sol, trous de rats, flaques, décors ; puis la porte, la sortie, les objets et les
/// ennemis ; enfin quelques toiles supplémentaires.
void World::furnishGalleries(Rng& rng) {
    woodMesh.track = true;
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti];
        int tIdx = (int)ti;
        std::vector<int> stations;  // anneaux des cadres de soutènement
        int i = tb.isMain ? 18 : 4;
        const int lim = tb.isMain ? doorRing - 10 : tb.N - 12;
        std::vector<char> doorway;
        // vrai si l'anneau est à moins de m anneaux d'une jonction avec une galerie latérale
        auto junc = [&](int ring, int m) {
            for (size_t b = 1; b < tubes.size(); b++)
                if (tubes[b].parent == tIdx && std::abs(ring - tubes[b].junction) < m) return true;
            return false;
        };
        // éboulis : murs de blocs de pierre qui bouchent toute la galerie (aucun cadre, échelle ni trou de rat à proximité)
        std::vector<int> wallRings;
        if (tb.isMain) {
            static const float WF[5] = {0.12f, 0.30f, 0.48f, 0.65f, 0.81f};  // jalons (fraction du trajet jusqu'à la porte)
            for (int w = 0; w < 5; w++) {  // pour chaque jalon, le point le plus étroit dans une fenêtre de +-14 m
                int best = -1;
                float bw = 1e9f;
                for (int k = (int)(doorRing * WF[w]) - 45; k <= (int)(doorRing * WF[w]) + 45; k += 2) {
                    float xl, xr, fy = tb.floorY(k, 0.0f);
                    if (k < 30 || junc(k, 40) || nearCorridor(0, k, 20) || !tb.wallRange(k, fy + 0.8f, xl, xr)) continue;
                    if (xr - xl < bw) { bw = xr - xl; best = k; }
                }
                if (best >= 0 && bw < 8.5f) wallRings.push_back(best);
            }
        } else if (tb.N > 70) {
            for (int tries = 0; tries < 10; tries++) {
                int k = tb.N / 2 + (int)(rng.next() % 12) - 6;
                float xl, xr, fy = tb.floorY(k, 0.0f);
                if (!tb.wallRange(k, fy + 0.8f, xl, xr) || xr - xl > 5.0f) continue;
                wallRings.push_back(k);
                break;
            }
        }
        auto nearWall = [&](int ring) {
            for (int wr : wallRings)
                if (std::abs(ring - wr) < 16) return true;
            return false;
        };
        auto nearJunction = [&](int ring) { return junc(ring, 15) || nearWall(ring) || nearCorridor(tIdx, ring, 4); };

        // --- cadres de soutènement tous les 4,8 à 8,5 m (le premier d'une galerie latérale marque son entrée)
        while (i < lim) {
            if (!nearJunction(i)) {
                stations.push_back(i);
                doorway.push_back(!tb.isMain && stations.size() == 1);
            }
            i += (int)(rng.r(4.8f, 8.5f) / tb.ds);
            if (!tb.isMain && stations.size() == 1) i = std::max(i, 20);
        }
        std::vector<char> has(stations.size(), 0);
        for (size_t s = 0; s < stations.size(); s++)
            if (doorway[s] || rng.chance(0.86f) || s < 2) {
                addFrame(tIdx, stations[s], rng, doorway[s] != 0);
                has[s] = 1;
            }
        // --- coffrage entre deux cadres voisins
        for (size_t s = 0; s + 1 < stations.size(); s++)
            if (has[s] && has[s + 1] && rng.chance(0.5f) && !nearWall((stations[s] + stations[s + 1]) / 2) && !nearWall(stations[s]) &&
                !nearWall(stations[s + 1]))
                for (int side = -1; side <= 1; side += 2)
                    if (rng.chance(0.7f)) addLagging(tIdx, stations[s], stations[s + 1], side, rng);
        // --- torches (un cadre sur deux, en alternant les côtés)
        int side = rng.chance(0.5f) ? 1 : -1;
        if (tb.isMain) {
            for (size_t s = 0; s < stations.size(); s += 2) {
                addTorch(tIdx, std::min(stations[s] + 2, tb.N - 2), side, rng);
                side = -side;
            }
        } else {
            addTorch(tIdx, std::min(tb.N - 2, 18), side, rng);
            if (tb.N > 70) addTorch(tIdx, tb.N - 10, -side, rng);
        }
        // --- échelles
        int ladderCount = tb.isMain ? 4 : 1;
        for (int n = 0; n < ladderCount; n++) {
            int k = tb.isMain ? 40 + rng.next() % std::max(1, doorRing - 110) : tb.N - 8;
            if (nearJunction(k)) continue;
            addLadder(tIdx, k, rng.chance(0.5f) ? 1 : -1, rng);
        }
        // --- planches appuyées contre la paroi ou tombées au sol
        int debrisN = tb.isMain ? 40 : 8;
        for (int n = 0; n < debrisN; n++) {
            int k = 14 + rng.next() % std::max(1, lim - 14);
            const Ring& r = tb.rings[k];
            if (nearJunction(k)) continue;
            float fy = tb.floorY(k, 0.0f), xl, xr;
            if (!tb.wallRange(k, fy + 0.5f, xl, xr)) continue;
            woodMesh.beginChunk();
            woodMesh.geom = 1;
            if (rng.chance(0.5f)) {  // appuyée
                int sd = rng.chance(0.5f) ? 1 : -1;
                float xw = sd > 0 ? xr : xl;
                float len = rng.r(1.5f, 2.6f), lean = rng.r(0.35f, 0.6f);
                vec3 b = tb.toWorld(k, xw - sd * (lean + 0.12f), tb.floorY(k, xw - sd * lean) + 0.04f, rng.r(-0.3f, 0.3f));
                vec3 t = b + r.U * (len * 0.92f) + r.R * ((float)sd * (lean * 0.9f));
                addTimber(tIdx, -1, 0, 0, b, t, r.T, 0.11f, 0.03f, rng, 0.0f, 0.015f);
            } else {  // à plat
                float x = rng.r(xl + 0.6f, xr - 0.6f), len = rng.r(0.8f, 2.2f);
                float ang = rng.r(0, PI);
                vec3 d = r.R * std::cos(ang) + r.T * std::sin(ang);
                vec3 c = tb.toWorld(k, x, tb.floorY(k, x) + 0.05f);
                addTimber(tIdx, -1, 0, 0, c - d * (len * 0.5f), c + d * (len * 0.5f) + r.U * rng.r(0.0f, 0.12f), r.U, 0.1f, 0.03f, rng, 0.0f, 0.015f);
            }
            woodMesh.geom = 0;
            woodMesh.endChunk();
        }
        // --- trous de rats par paires
        int pairs = tb.isMain ? 7 : 1;
        for (int n = 0; n < pairs; n++) {
            int k = 20 + rng.next() % std::max(1, lim - 50);
            if (nearJunction(k) || nearJunction(k + 30)) continue;
            addRatHoles(tIdx, k, 14 + rng.next() % 26, rng);
        }
        addPuddles(tIdx, tb.isMain ? 60 : 8, rng);
        addDecor(tIdx, rng);
        for (int wr : wallRings) addRockfall(tIdx, wr, tb.isMain ? 3 : 2, rng);
    }

    // --- porte, sortie, objets, ennemis
    addDoorAndExit(rng);
    placeEntities(rng);
    // rien dans les éboulis ni dans le couloir des catacombes (sauf les clés)
    auto inWall = [&](vec3 p) {
        for (const Rockfall& w : rockfalls)
            if (glm::length(p - w.c) < 3.4f) return true;
        return false;
    };
    auto inCorr = [&](vec3 p) {
        if (corridorStart < 0) return false;
        int r = tubes[0].locate(p, -1);
        return glm::length(p - tubes[0].rings[r].c) < 6.0f && nearCorridor(0, r, 6);
    };
    spawns.erase(std::remove_if(spawns.begin(), spawns.end(), [&](const Spawn& sp) { return inWall(sp.pos) || inCorr(sp.pos); }), spawns.end());
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [&](const Pickup& pk) { return pk.kind == 1 && inCorr(pk.p); }), pickups.end());
    if (corridorStart >= 0) {  // lumière froide au bout du couloir
        int ring = std::min(tubes[0].N - 2, corridorEnd + 7);
        addLight(tubes[0].toWorld(ring, 0.0f, tubes[0].floorY(ring, 0.0f) + 2.1f), vec3(0.55f, 0.68f, 0.92f), 3.4f, 5, false);
    }
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [&](const Pickup& pk) { return inWall(pk.p); }), pickups.end());

    // --- toiles supplémentaires accrochées au plafond / à la paroi
    for (size_t ti = 0; ti < tubes.size(); ti++) {
        Tube& tb = tubes[ti];
        for (int n = 0; n < (tb.isMain ? 14 : 3); n++) {
            int k = 14 + rng.next() % std::max(1, (tb.isMain ? doorRing - 8 : tb.N - 12) - 14);
            const Ring& r = tb.rings[k];
            if (nearCorridor((int)ti, k, 3)) continue;
            float xl, xr;
            float y = tb.ceilY(k, 0.0f) - 0.6f;
            if (!tb.wallRange(k, y, xl, xr)) continue;
            int sd = rng.chance(0.5f) ? 1 : -1;
            float xw = sd > 0 ? xr : xl;
            addWeb(-1, tb.toWorld(k, xw - sd * 0.12f, y, 0), r.T, -r.U, rng.r(0.7f, 1.4f));
        }
    }
}
