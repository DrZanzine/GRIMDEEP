// =============================================================================
//  world/world_rockfall.cpp — éboulis : murs de blocs de pierre à abattre
//
//  Un éboulis remplit toute la section d'une galerie avec des blocs posés en
//  quinconce, sur 2 ou 3 épaisseurs. Chaque bloc est une planche (Board) de rôle 7 :
//  il a une collision et encaisse 2 coups de pioche. Quand un bloc casse, il se
//  fend en 4 morceaux ; les blocs qui perdent leur appui s'écroulent à leur tour.
// =============================================================================
#include "world/world.h"

#include <cstdio>

#include "render/geometry.h"
#include "render/materials.h"

/// Construit un éboulis de `layers` épaisseurs centré sur l'anneau `i` de la galerie `ti`.
void World::addRockfall(int ti, int i, int layers, Rng& rng) {
    Tube& tb = tubes[ti];
    int wid = (int)rockfalls.size();
    Rockfall W;
    W.tube = ti;
    W.ring = i;
    W.c = tb.rings[i].c + tb.rings[i].U * 1.0f;
    woodMesh.beginChunk();
    woodMesh.geom = 0;
    const float STEP = 0.74f;  // hauteur d'une rangée de blocs
    for (int l = 0; l < layers; l++) {
        // chaque épaisseur est placée sur son propre anneau (0,78 m d'écart)
        float t0 = (l - (layers - 1) * 0.5f) * 0.78f;
        int ri = std::min(tb.N - 2, std::max(1, i + (int)std::lround(t0 / tb.ds)));
        const Ring& r = tb.rings[ri];
        float fy0 = tb.floorY(ri, 0.0f), xl0, xr0;
        if (!tb.wallRange(ri, fy0 + 0.8f, xl0, xr0)) continue;
        float base = std::min(fy0, std::min(tb.floorY(ri, xl0 + 0.4f), tb.floorY(ri, xr0 - 0.4f))) - 0.12f;
        for (int row = 0; row < 10; row++) {
            float y0 = base + row * STEP, y1 = y0 + STEP, yc = (y0 + y1) * 0.5f, xl, xr;
            if (!tb.wallRange(ri, yc, xl, xr)) break;
            // largeur de la rangée : la plus grande entre le bas, le milieu et le haut
            float a, b2;
            if (tb.wallRange(ri, y0 + 0.05f, a, b2)) { xl = std::min(xl, a); xr = std::max(xr, b2); }
            if (tb.wallRange(ri, y1 - 0.05f, a, b2)) { xl = std::min(xl, a); xr = std::max(xr, b2); }
            float xm = (xl + xr) * 0.5f, ceilY = std::min(tb.ceilY(ri, xm), tb.ceilY(ri, xm + (xr - xl) * 0.25f));
            bool last = y1 > ceilY - 0.3f;  // dernière rangée : les blocs s'enfoncent dans le plafond
            if (last) y1 = ceilY + 0.25f;
            float hh = (y1 - y0) * 0.5f;
            yc = (y0 + y1) * 0.5f;
            float x = xl - 0.15f - (((row + l) & 1) ? 0.3f : 0.0f);  // quinconce
            while (x < xr + 0.15f) {
                float w = rng.r(0.62f, 0.9f), x0 = std::max(x, xl - 0.15f), x1 = std::min(x + w, xr + 0.15f);
                x += w;
                if (x1 - x0 < 0.18f) continue;
                float xc = (x0 + x1) * 0.5f, phi = rng.r(-0.12f, 0.12f), cs = std::cos(phi), sn = std::sin(phi);
                vec3 dl = r.R * cs + r.T * sn, dt = -r.R * sn + r.T * cs, c = tb.toWorld(ri, xc, yc + rng.r(-0.03f, 0.03f), rng.r(-0.05f, 0.05f));
                size_t before = woodMesh.boards.size();
                addBoard(woodMesh, c, dl, r.U, dt, (x1 - x0) * 0.5f, hh + 0.02f, rng.r(0.36f, 0.46f), Mat::STONE_BLOCK, rng.f() * 10.0f, rng, 0.035f);
                if (woodMesh.boards.size() == before) continue;
                Board& bd = woodMesh.boards.back();
                bd.tube = ti;
                bd.role = 7;
                bd.hp = 2;
                bd.wall = wid;
                bd.side = (last ? 1 : 0) | (row == 0 ? 2 : 0);
                W.blocks.push_back((int)woodMesh.boards.size() - 1);
            }
            if (last) break;
        }
    }
    woodMesh.endChunk();
    std::printf("  eboulis : galerie %d, %.0f m, %zu blocs\n", ti, i * tb.ds, W.blocks.size());
    if (!W.blocks.empty()) rockfalls.push_back(W);
}

void World::breakStone(int bi, vec3 hit, vec3 push) {
    (void)hit;
    Board& b = woodMesh.boards[bi];
    if (!b.alive) return;
    b.alive = false;
    woodMesh.killIndices(b.i0, b.n);
    vec3 pu = push;
    pu.y *= 0.3f;
    for (int k = 0; k < 4; k++) {  // 4 morceaux (octants) qui retombent
        float sx = (debrisRng.chance(0.5f) ? 1.f : -1.f), sy = (debrisRng.chance(0.5f) ? 1.f : -1.f), sz = (debrisRng.chance(0.5f) ? 1.f : -1.f);
        Debris d;
        d.p = b.c + b.dl * (sx * b.hl * 0.5f) + b.dw * (sy * b.hw * 0.5f) + b.dt * (sz * b.ht * 0.5f);
        d.ax = b.dl;
        d.ay = b.dw;
        d.az = b.dt;
        d.h = vec3(b.hl * 0.5f, b.hw * 0.5f, b.ht * 0.5f) * debrisRng.r(0.7f, 1.0f);
        d.v = pu * debrisRng.r(1.0f, 2.5f) + glm::normalize(d.p - b.c + vec3(1e-3f)) * debrisRng.r(0.8f, 2.0f) + vec3(0, debrisRng.r(0.5f, 2.2f), 0);
        d.w = vec3(debrisRng.r(-5, 5), debrisRng.r(-5, 5), debrisRng.r(-5, 5));
        d.seed = debrisRng.f() * 10;
        d.tube = b.tube;
        d.ring = -1;
        d.stone = true;
        if (debris.size() > 650) debris.erase(debris.begin());
        debris.push_back(d);
    }
    spawnChips(b.c, push, 9, true);
    if (b.wall >= 0) settleStone(b.wall);
}

/// Un bloc sans appui (ni sol, ni bloc dessous, ni plafond calé entre deux voisins)
/// s'écroule. On répète jusqu'à 6 passes car une chute peut en entraîner d'autres.
void World::settleStone(int wid) {
    Rockfall& W = rockfalls[wid];
    for (int pass = 0; pass < 6; pass++) {
        std::vector<int> fall;
        for (int id : W.blocks) {
            const Board& b = woodMesh.boards[id];
            if (!b.alive || (b.side & 2)) continue;  // posé au sol : toujours stable
            bool sup = false, left = false, right = false;
            for (int jd : W.blocks) {
                if (jd == id) continue;
                const Board& o = woodMesh.boards[jd];
                if (!o.alive) continue;
                vec3 d = o.c - b.c;
                float dy = d.y, dx = glm::dot(d, b.dl), dz = glm::dot(d, b.dt);
                if (std::fabs(dz) > (o.ht + b.ht) * 0.9f) continue;  // pas dans la même épaisseur
                if (dy < 0 && dy > -(o.hw + b.hw) - 0.25f && std::fabs(dx) < (o.hl + b.hl) * 0.9f) sup = true;  // bloc dessous
                if (std::fabs(dy) < 0.25f && dx > 0 && dx < o.hl + b.hl + 0.3f) right = true;
                if (std::fabs(dy) < 0.25f && dx < 0 && dx > -(o.hl + b.hl + 0.3f)) left = true;
            }
            if (sup) continue;
            if ((b.side & 1) && left && right) continue;  // calé contre le plafond entre deux voisins
            fall.push_back(id);
        }
        if (fall.empty()) break;
        for (int id : fall) {
            Board& b = woodMesh.boards[id];
            if (!b.alive) continue;
            b.alive = false;
            woodMesh.killIndices(b.i0, b.n);
            Debris d;
            d.p = b.c;
            d.ax = b.dl;
            d.ay = b.dw;
            d.az = b.dt;
            d.h = vec3(b.hl, b.hw, b.ht) * 0.92f;
            d.v = vec3(debrisRng.r(-0.6f, 0.6f), -0.3f, debrisRng.r(-0.6f, 0.6f));
            d.w = vec3(debrisRng.r(-3, 3), debrisRng.r(-3, 3), debrisRng.r(-3, 3));
            d.seed = debrisRng.f() * 10;
            d.tube = b.tube;
            d.ring = -1;
            d.stone = true;
            if (debris.size() > 650) debris.erase(debris.begin());
            debris.push_back(d);
        }
    }
}
