// =============================================================================
//  world/world_entities.cpp — grande porte, sortie, clés, fioles et ennemis
//
//  Objectif du joueur : trouver les 3 clés (au fond des galeries latérales, gardées
//  par des ennemis), ouvrir la grande porte au bout de la galerie principale, puis
//  rejoindre la lumière de la sortie.
// =============================================================================
#include "world/world.h"

#include "render/geometry.h"
#include "render/materials.h"

/// Construit la porte massive à l'anneau doorRing : planches verticales jointives
/// (indestructibles, rôle 9) et deux bandes d'acier.
void World::addDoor(Rng& rng) {
    Tube& tb = tubes[0];
    int i = doorRing;
    const Ring& r = tb.rings[i];
    float fy = tb.floorY(i, 0.0f), xl, xr;
    if (!tb.wallRange(i, fy + 1.2f, xl, xr)) return;
    float W = (xr - xl) - 0.04f;
    int n = (int)std::ceil(W / 0.26f);
    float pw = W / n;  // largeur d'une planche
    woodMesh.beginChunk();
    for (int k = 0; k < n; k++) {
        float x = xl + 0.02f + (k + 0.5f) * pw, y0 = tb.floorY(i, x) - 0.1f, y1 = tb.ceilY(i, x) + 0.1f;
        int id = addTimber(0, -1, 9, 0, tb.toWorld(i, x, y0), tb.toWorld(i, x, y1), r.T, pw * 0.5f, 0.06f, rng, 0, 0.004f);
        if (id >= 0) {
            woodMesh.boards[id].hp = 99999;
            doorBoards.push_back(id);
        }
    }
    for (float h : {0.6f, 2.0f}) {  // bandes d'acier
        float y = fy + h, a, b2;
        if (!tb.wallRange(i, y, a, b2)) continue;
        size_t before = woodMesh.boards.size();
        addBoardBetween(woodMesh, tb.toWorld(i, a + 0.02f, y, -0.1f), tb.toWorld(i, b2 - 0.02f, y, -0.1f), r.T, 0.09f, 0.025f, Mat::STEEL, rng, 0, 0.0f);
        if (woodMesh.boards.size() > before) {
            Board& bd = woodMesh.boards.back();
            bd.role = 9;
            bd.hp = 99999;
            bd.tube = 0;
            doorBoards.push_back((int)woodMesh.boards.size() - 1);
        }
    }
    woodMesh.endChunk();
    doorPos = tb.toWorld(i, 0.0f, fy + 1.2f, -0.3f);
}

/// Prépare la fin de la galerie principale : cadre et torches devant la porte, la
/// porte elle-même, les lumières de la zone de sortie (allumées à l'ouverture) et le
/// point de sortie. Rien n'est fait si la galerie est trop courte.
void World::addDoorAndExit(Rng& rng) {
    Tube& tb = tubes[0];
    if (doorRing < 60 || tb.N - doorRing < 20) return;
    addFrame(0, doorRing - 3, rng, false);
    addTorch(0, doorRing - 6, 1, rng);
    addTorch(0, doorRing - 6, -1, rng);
    addDoor(rng);
    int ie = tb.N - 1;
    addLight(tb.toWorld(ie - 4, 0.0f, 1.2f), vec3(0.72f, 0.86f, 1.0f), 4.0f, EXIT_BIOME, false);
    lights.back().needsDoor = true;
    addLight(tb.toWorld(doorRing + (ie - doorRing) * 2 / 3, 0.0f, 1.5f), vec3(0.7f, 0.85f, 1.0f), 2.0f, EXIT_BIOME, false);
    lights.back().needsDoor = true;
    exitPos = tb.rings[ie].c;
}

void World::openDoor() {
    if (doorOpen) return;
    doorOpen = true;
    for (int id : doorBoards) {
        Board& b = woodMesh.boards[id];
        if (b.alive) {
            b.alive = false;
            woodMesh.killIndices(b.i0, b.n);
        }
    }
}

namespace {

/// Choisit le type d'ennemi selon le biome : araignées dans les grottes humides,
/// mélange araignées / rôdeurs chez les cristaux, golems de lave dans la forge.
int enemyTypeFor(int bi, Rng& rng) {
    switch (bi) {
        case 1: return 1;
        case 2: return rng.chance(0.6f) ? 1 : 0;
        case 4: return rng.chance(0.45f) ? 2 : 0;
        default: return 0;
    }
}

}  // namespace

/// Place les clés au fond des galeries latérales (chacune gardée par 2 ennemis et
/// précédée d'une fiole), complète si besoin dans la galerie principale, puis répartit
/// ennemis et fioles dans chacun des 5 biomes.
void World::placeEntities(Rng& rng) {
    int keys = 0;
    Tube& mn = tubes[0];
    for (size_t b = 1; b < tubes.size() && keys < 3; b++) {
        Tube& tb = tubes[b];
        int i = std::max(8, tb.N - 6);
        pickups.push_back(Pickup{tb.toWorld(i, 0.0f, tb.floorY(i, 0.0f) + 0.9f), 0, true, rng.f() * 6});
        keys++;
        pickups.push_back(Pickup{tb.toWorld(i - 12, 0.6f, tb.floorY(i - 12, 0.6f) + 0.5f), 1, true, rng.f() * 6});
        int bi = (int)std::floor(biomeAt((int)b, i) + 0.5f);
        for (int k = 0; k < 2; k++) {
            int ri = std::max(10, i - 7 - 6 * k);
            spawns.push_back(Spawn{enemyTypeFor(bi, rng), tb.toWorld(ri, rng.r(-0.4f, 0.4f), tb.floorY(ri, 0.0f) + 1.65f), (int)b});
        }
    }
    while (keys < 3 && doorRing > 160) {  // pas assez de galeries latérales : clés dans la galerie principale
        int i = 80 + rng.next() % std::max(1, doorRing - 140);
        float x = rng.r(-0.8f, 0.8f);
        int bi = (int)std::floor(biomeAt(0, i) + 0.5f);
        pickups.push_back(Pickup{mn.toWorld(i, x, mn.floorY(i, x) + 0.9f), 0, true, rng.f() * 6});
        keys++;
        spawns.push_back(Spawn{enemyTypeFor(bi, rng), mn.toWorld(i + 8, 0.0f, mn.floorY(i + 8, 0.0f) + 1.65f), 0});
    }
    keysNeeded = keys;
    static const int cnt[5] = {3, 4, 4, 5, 4};  // ennemis par biome
    for (int b = 0; b < 5 && doorRing > 160; b++) {
        int lo = (b == 0) ? 80 : b * doorRing / 5 + 10, hi = (b + 1) * doorRing / 5 - 10;
        for (int k = 0; k < cnt[b]; k++) {
            int i = lo + rng.next() % std::max(1, hi - lo);
            float x = rng.r(-0.7f, 0.7f);
            spawns.push_back(Spawn{enemyTypeFor(b, rng), mn.toWorld(i, x, mn.floorY(i, x) + 1.65f), 0});
        }
        for (int k = 0; k < 2; k++) {  // 2 fioles de soin par biome
            int i = lo + rng.next() % std::max(1, hi - lo);
            float x = rng.r(-0.9f, 0.9f);
            pickups.push_back(Pickup{mn.toWorld(i, x, mn.floorY(i, x) + 0.5f), 1, true, rng.f() * 6});
        }
    }
}
