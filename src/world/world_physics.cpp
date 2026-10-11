// =============================================================================
//  world/world_physics.cpp — collisions, coups de pioche, casse et débris
//
//  - Le joueur (et les ennemis) sont contraints à l'intérieur des galeries par
//    constrain() : on se place dans le repère de l'anneau le plus proche, on borne
//    l'abscisse entre les parois et on pose les yeux à 1,65 m au-dessus du sol.
//  - Les planches vivantes repoussent les personnages (collideBoards).
//  - Les coups font perdre des PV aux planches ; une planche cassée devient un ou
//    deux débris physiques (boîtes qui tombent, rebondissent et s'endorment).
//  - Les débris de bois noircissent puis partent en cendres ; les pierres
//    s'effritent dès qu'on ne les regarde plus.
// =============================================================================
#include "world/world.h"

#include <algorithm>

#include "render/geometry.h"
#include "render/materials.h"

namespace {

/// Repousse une sphère (centre `p`, rayon `r`) hors de la boîte orientée `b`.
/// Avec `horizontalOnly`, la poussée est limitée au plan horizontal (on ne monte pas
/// sur les planches en marchant). Renvoie vrai s'il y avait contact.
bool pushSphereOut(vec3& p, float r, const Board& b, bool horizontalOnly) {
    vec3 d = p - b.c;
    vec3 l(glm::dot(d, b.dl), glm::dot(d, b.dw), glm::dot(d, b.dt));  // coordonnées locales
    vec3 h(b.hl, b.hw, b.ht);
    vec3 q = glm::clamp(l, -h, h);  // point de la boîte le plus proche
    vec3 dd = l - q;
    float d2 = glm::dot(dd, dd);
    if (d2 >= r * r) return false;
    vec3 n;
    if (d2 < 1e-9f) {  // centre dans la boîte : sortir par la face la plus proche
        vec3 pen = h - glm::abs(l);
        int a = 0;
        if (pen.y < pen.x) a = 1;
        if (pen.z < pen[a]) a = 2;
        vec3 nl(0);
        nl[a] = l[a] >= 0 ? 1.f : -1.f;
        n = b.dl * nl.x + b.dw * nl.y + b.dt * nl.z;
        float push = pen[a] + r;
        if (horizontalOnly) n.y = 0;
        if (glm::length(n) < 1e-4f) return false;
        p += glm::normalize(n) * push;
        return true;
    }
    float dist = std::sqrt(d2);
    vec3 nl = dd / dist;
    n = b.dl * nl.x + b.dw * nl.y + b.dt * nl.z;
    if (horizontalOnly) {
        n.y = 0;
        if (glm::length(n) < 1e-4f) return false;
        n = glm::normalize(n);
    }
    p += n * (r - dist);
    return true;
}

}  // namespace

void World::collideBoards(vec3& p, bool fly) const {
    for (int iter = 0; iter < 2; iter++)
        for (const Board& b : woodMesh.boards) {
            if (!b.alive) continue;
            if (glm::length(b.c - p) > b.hl + 2.2f) continue;
            if (fly) pushSphereOut(p, 0.35f, b, false);
            else {
                // en marchant : trois sphères (tête, hanches, pieds), poussée horizontale
                for (float off : {1.2f, 0.65f, 0.05f}) {
                    vec3 q = p - vec3(0, off, 0);
                    pushSphereOut(q, 0.3f, b, true);
                    p.x = q.x;
                    p.z = q.z;
                }
            }
        }
}

vec3 World::constrain(vec3 p, bool fly, std::vector<int>& hints) const {
    if (inHub(p)) return constrainHub(p, fly);
    // Candidat = une galerie proche, exprimée dans le repère de son anneau le plus proche.
    struct C {
        int k, ring;              // galerie, anneau
        float x, y, t;            // coordonnées locales
        float lo, hi, fl;         // abscisses permises (marche) et hauteur du sol
        bool in;                  // déjà dans l'espace permis
        float disp;               // déplacement nécessaire pour y revenir
    };
    std::vector<C> cs;
    if (hints.size() < tubes.size()) hints.resize(tubes.size(), -1);
    for (size_t k = 0; k < tubes.size(); k++) {
        const Tube& tb = tubes[k];
        if (tb.N == 0) continue;
        int ring = tb.locate(p, hints[k]);
        vec3 d = p - tb.rings[ring].c;
        if (glm::dot(d, d) > 49.0f) { hints[k] = -1; continue; }  // trop loin de cette galerie
        hints[k] = ring;
        const Ring& r = tb.rings[ring];
        C c;
        c.k = (int)k;
        c.ring = ring;
        c.x = glm::dot(d, r.R);
        c.y = glm::dot(d, r.U);
        c.t = glm::dot(d, r.T);
        // aux extrémités, on ne peut pas dépasser le premier / dernier anneau
        float tmin = (ring == 0) ? (tb.isMain ? -0.4f : -1.6f) : -1e9f, tmax = (ring == tb.N - 1) ? 0.4f : 1e9f;
        bool tOK = c.t >= tmin && c.t <= tmax;
        c.fl = tb.floorY(ring, c.x);
        c.lo = c.hi = 0;
        c.disp = 0;
        c.in = false;
        if (!fly) {
            // espace de marche : entre les parois à hauteur des yeux ET des genoux, avec 45 cm de marge
            float a1, a2, b1, b2;
            if (!tb.wallRange(ring, c.fl + 1.65f, a1, a2) || !tb.wallRange(ring, c.fl + 0.5f, b1, b2)) continue;
            c.lo = std::max(a1, b1) + 0.45f;
            c.hi = std::min(a2, b2) - 0.45f;
            if (c.lo > c.hi) {
                float m = (c.lo + c.hi) * 0.5f;
                c.lo = c.hi = m;
                c.in = std::fabs(c.x - m) < 0.15f && tOK;
            } else c.in = c.x >= c.lo && c.x <= c.hi && tOK;
            c.disp = std::fabs(c.x - std::min(std::max(c.x, c.lo), c.hi)) + (tOK ? 0 : 1.0f);
        } else {
            // vol libre : n'importe où dans la section, à 45 cm de la paroi
            float len = std::sqrt(c.x * c.x + c.y * c.y), rad = tb.radiusAt(ring, std::atan2(c.y, c.x)) - 0.45f;
            c.in = len <= rad && tOK;
            c.disp = std::max(0.0f, len - rad) + (tOK ? 0 : 1.0f);
        }
        cs.push_back(c);
    }
    if (cs.empty()) return p;
    // on préfère une galerie où l'on est déjà (la plus « centrée »), sinon la plus proche
    const C* pick = nullptr;
    for (const C& c : cs)
        if (c.in && (!pick || std::fabs(c.t) < std::fabs(pick->t))) pick = &c;
    if (!pick)
        for (const C& c : cs)
            if (!pick || c.disp < pick->disp) pick = &c;
    const Tube& tb = tubes[pick->k];
    float x = pick->x, y = pick->y, t = pick->t;
    if (!pick->in) {
        if (!fly) x = std::min(std::max(x, pick->lo), pick->hi);
        else {
            float len = std::sqrt(x * x + y * y), lim = tb.radiusAt(pick->ring, std::atan2(y, x)) - 0.45f;
            if (len > lim && len > 1e-4f) { x *= lim / len; y *= lim / len; }
        }
        float tmin = (pick->ring == 0) ? (tb.isMain ? -0.4f : -1.6f) : -1e9f, tmax = (pick->ring == tb.N - 1) ? 0.4f : 1e9f;
        t = std::min(std::max(t, tmin), tmax);
    }
    if (!fly) {
        y = tb.floorY(pick->ring, x) + 1.65f;  // hauteur des yeux
        // recalcule t sur la tangente horizontale pour que la pente ne tire pas le joueur
        const Ring& r = tb.rings[pick->ring];
        vec3 Th(r.T.x, 0, r.T.z);
        float cp = glm::length(Th);
        if (cp > 1e-3f) {
            Th /= cp;
            float tn = (glm::dot(p - r.c, Th) + r.T.y * y) / cp;
            float tmin = (pick->ring == 0) ? (tb.isMain ? -0.4f : -1.6f) : -1e9f, tmax = (pick->ring == tb.N - 1) ? 0.4f : 1e9f;
            t = std::min(std::max(tn, tmin), tmax);
        }
    }
    return tb.toWorld(pick->ring, x, y, t);
}

int World::rayBoard(vec3 o, vec3 d, float maxT, float& tHit) const {
    int best = -1;
    tHit = maxT;
    for (size_t i = 0; i < woodMesh.boards.size(); i++) {
        const Board& b = woodMesh.boards[i];
        if (!b.alive || b.role == 9) continue;  // la porte ne se casse pas
        if (glm::length(b.c - o) > maxT + b.hl + 0.5f) continue;
        // test rayon / boîte orientée par la méthode des « slabs », dans le repère de la planche
        vec3 oo = o - b.c;
        vec3 ol(glm::dot(oo, b.dl), glm::dot(oo, b.dw), glm::dot(oo, b.dt)), dl(glm::dot(d, b.dl), glm::dot(d, b.dw), glm::dot(d, b.dt));
        vec3 h = vec3(b.hl, b.hw, b.ht) + vec3(0.03f);
        float t0 = 0, t1 = tHit;
        bool ok = true;
        for (int a = 0; a < 3 && ok; a++) {
            if (std::fabs(dl[a]) < 1e-6f) {
                if (std::fabs(ol[a]) > h[a]) ok = false;
            } else {
                float ta = (-h[a] - ol[a]) / dl[a], tb2 = (h[a] - ol[a]) / dl[a];
                if (ta > tb2) std::swap(ta, tb2);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb2);
                if (t0 > t1) ok = false;
            }
        }
        if (ok && t0 < tHit) { tHit = t0; best = (int)i; }
    }
    return best;
}

void World::boardsNear(vec3 pt, float rad, std::vector<int>& out) const {
    for (size_t i = 0; i < woodMesh.boards.size(); i++) {
        const Board& b = woodMesh.boards[i];
        if (!b.alive || b.role == 9) continue;
        if (glm::length(b.c - pt) > b.hl + rad + 0.3f) continue;
        vec3 d = pt - b.c;
        vec3 l(glm::dot(d, b.dl), glm::dot(d, b.dw), glm::dot(d, b.dt));
        vec3 h(b.hl, b.hw, b.ht);
        if (glm::length(l - glm::clamp(l, -h, h)) < rad) out.push_back((int)i);
    }
}

void World::spawnPiece(const Board& b, float s0, float s1, vec3 vel) {
    if (std::fabs(s1 - s0) < 0.06f) return;  // morceau trop petit
    Debris d;
    d.p = b.c + b.dl * ((s0 + s1) * 0.5f);
    d.ax = b.dl;
    d.ay = b.dw;
    d.az = b.dt;
    d.h = vec3(std::fabs(s1 - s0) * 0.5f, b.hw, b.ht);
    d.v = vel + vec3(debrisRng.r(-1, 1), debrisRng.r(0, 1.5f), debrisRng.r(-1, 1));
    d.w = vec3(debrisRng.r(-6, 6), debrisRng.r(-6, 6), debrisRng.r(-6, 6));
    d.seed = debrisRng.f() * 10;
    d.tube = b.tube;
    d.ring = -1;
    if (debris.size() > 650) debris.erase(debris.begin());
    debris.push_back(d);
}

void World::spawnChips(vec3 at, vec3 push, int n, bool stone) {
    // galerie la plus proche (pour les collisions des éclats)
    int bt = 0;
    float bd = 1e9f;
    for (size_t t = 0; t < tubes.size(); t++) {
        int r = tubes[t].locate(at, -1);
        float dd = glm::length(tubes[t].rings[r].c - at);
        if (dd < bd) { bd = dd; bt = (int)t; }
    }
    for (int k = 0; k < n; k++) {
        Debris d;
        d.p = at + vec3(debrisRng.r(-0.05f, 0.05f), debrisRng.r(-0.05f, 0.05f), debrisRng.r(-0.05f, 0.05f));
        vec3 a = glm::normalize(vec3(debrisRng.r(-1, 1), debrisRng.r(-1, 1), debrisRng.r(-1, 1)) + vec3(0.01f));
        vec3 b = glm::normalize(glm::cross(a, vec3(0.3f, 0.8f, 0.5f))), c = glm::cross(a, b);
        d.ax = a;
        d.ay = b;
        d.az = c;
        d.h = vec3(debrisRng.r(0.03f, 0.1f), debrisRng.r(0.01f, 0.03f), debrisRng.r(0.008f, 0.02f));
        d.v = push * debrisRng.r(1.0f, 3.5f) + vec3(debrisRng.r(-1.5f, 1.5f), debrisRng.r(0.5f, 2.5f), debrisRng.r(-1.5f, 1.5f));
        d.w = vec3(debrisRng.r(-12, 12), debrisRng.r(-12, 12), debrisRng.r(-12, 12));
        d.seed = debrisRng.f() * 10;
        d.tube = bt;
        d.ring = -1;
        if (stone) {
            d.stone = true;
            d.h = vec3(debrisRng.r(0.04f, 0.1f), debrisRng.r(0.03f, 0.07f), debrisRng.r(0.03f, 0.08f));
            d.v *= 1.3f;
        }
        if (debris.size() > 650) debris.erase(debris.begin());
        debris.push_back(d);
    }
}

void World::breakBoard(int bi, vec3 hit, vec3 push, bool whole) {
    Board& b = woodMesh.boards[bi];
    if (!b.alive) return;
    b.alive = false;
    woodMesh.killIndices(b.i0, b.n);
    if (b.group >= 0 && props[b.group].alive) {  // l'échelle / la barricade entière s'effondre
        ModelProp& L = props[b.group];
        L.alive = false;
        woodMesh.killIndices(L.i0, L.n);
        for (int id : L.boards)
            if (id != bi && woodMesh.boards[id].alive) breakBoard(id, woodMesh.boards[id].c, vec3(0), true);
    }
    if (whole) spawnPiece(b, -b.hl, b.hl, push * 0.3f);
    else {
        // cassée en deux au point d'impact
        float s = std::min(std::max(glm::dot(hit - b.c, b.dl), -b.hl * 0.6f), b.hl * 0.6f);
        spawnPiece(b, -b.hl, s, push * 3.0f - b.dl * 1.5f);
        spawnPiece(b, s, b.hl, push * 3.0f + b.dl * 1.5f);
        spawnChips(hit, push, 6);
    }
    if (b.frame >= 0) checkCollapse(b.frame);
}

void World::hitBoard(int bi, vec3 hit, vec3 push) {
    Board& b = woodMesh.boards[bi];
    b.hp--;
    if (b.role == 7) {  // bloc d'éboulis
        if (b.hp <= 0) breakStone(bi, hit, push);
        else spawnChips(hit, push, 5, true);
        return;
    }
    if (b.hp <= 0) breakBoard(bi, hit, push, false);
    else spawnChips(hit, push, 4);
}

void World::checkCollapse(int fid) {
    TimberFrame& f = frames[fid];
    for (int s = 0; s < 2; s++) {
        if (f.fell[s]) continue;
        bool any = false;
        for (int id : f.post[s])
            if (woodMesh.boards[id].alive) any = true;
        if (any) continue;
        // plus aucun poteau de ce côté : contrefiches, linteau et toiles tombent
        f.fell[s] = true;
        for (int id : f.brace[s])
            if (woodMesh.boards[id].alive) breakBoard(id, woodMesh.boards[id].c, vec3(0), true);
        for (int id : f.beam)
            if (woodMesh.boards[id].alive) breakBoard(id, woodMesh.boards[id].c, vec3(0.0f, -0.5f, 0.0f), true);
        for (int id : f.brace[1 - s])
            if (woodMesh.boards[id].alive) breakBoard(id, woodMesh.boards[id].c, vec3(0), true);
        for (int w : f.webs)
            if (webs[w].alive) {
                webs[w].alive = false;
                webMesh.killIndices(webs[w].i0, 3);
            }
    }
}

int World::clearWebs(vec3 o, vec3 d, float reach) {
    int n = 0;
    for (auto& w : webs) {
        if (!w.alive) continue;
        vec3 oc = w.c - o;
        float t = glm::dot(oc, d);
        if (t < 0 || t > reach + 0.6f) continue;
        float dist = glm::length(oc - d * t);
        if (dist < w.r + 0.55f) {
            w.alive = false;
            webMesh.killIndices(w.i0, 3);
            n++;
        }
    }
    return n;
}

void World::stepDebris(float dt) {
    // --- vieillissement : effritement des pierres, cendres du bois
    for (Debris& d : debris) {
        d.life += dt;
        if (d.stone) {  // pierre : au bout de 10 s, dès qu'on ne la regarde plus, elle s'effrite en poussière
            vec3 tc = d.p - camPos;
            float dl = glm::length(tc);
            bool seen = dl < 45.0f && glm::dot(tc / std::max(dl, 1e-3f), camFwd) > 0.42f;
            d.unseen = seen ? 0.0f : d.unseen + dt;
            if (d.die < 0 && d.life > 10.0f && d.unseen > 0.3f) { d.die = 0; d.sleep = false; }
            if (d.die >= 0) {
                d.die += dt;
                if (debrisRng.chance(std::min(1.0f, dt * 10.0f)))
                    ashEvents.push_back(d.p + vec3(debrisRng.r(-1, 1), debrisRng.r(0, 1), debrisRng.r(-1, 1)) * d.h.x);
            }
        }
        if (!d.stone && d.life > WOOD_ASH && d.life < WOOD_GONE && debrisRng.chance(std::min(1.0f, dt * 16.0f)))
            ashEvents.push_back(d.p + vec3(debrisRng.r(-d.h.x, d.h.x) * 0.6f, debrisRng.r(-0.05f, 0.1f), debrisRng.r(-0.1f, 0.1f)));
    }
    debris.erase(std::remove_if(debris.begin(), debris.end(), [&](const Debris& d) {
                     if (d.stone) return d.die > 1.4f || d.life > 90.0f;
                     if (d.life < WOOD_GONE) return false;
                     for (int k = 0; k < 4; k++) ashEvents.push_back(d.p);  // dernière bouffée de cendres
                     return true;
                 }),
                 debris.end());
    // --- mouvement : gravité, rotation, collision avec le sol et les parois de la galerie
    for (Debris& d : debris) {
        if (d.die >= 0) { d.p.y -= 0.25f * dt; continue; }  // s'enfonce en s'effritant
        if (d.sleep) continue;
        d.age += dt;
        d.v.y -= 9.8f * dt;
        d.p += d.v * dt;
        float wl = glm::length(d.w);
        if (wl > 1e-4f) {
            mat3 R = mat3(glm::rotate(mat4(1), wl * dt, d.w / wl));
            d.ax = R * d.ax;
            d.ay = R * d.ay;
            d.az = R * d.az;
        }
        const Tube& tb = tubes[d.tube];
        d.ring = tb.locate(d.p, d.ring);
        const Ring& r = tb.rings[d.ring];
        // pénétration maximale d'un des 8 coins sous le sol
        float maxPen = 0;
        for (int k = 0; k < 8; k++) {
            vec3 cp = d.p + d.ax * ((k & 1 ? 1.f : -1.f) * d.h.x) + d.ay * ((k & 2 ? 1.f : -1.f) * d.h.y) + d.az * ((k & 4 ? 1.f : -1.f) * d.h.z);
            vec3 dd = cp - r.c;
            float pen = tb.floorY(d.ring, glm::dot(dd, r.R)) - glm::dot(dd, r.U);
            maxPen = std::max(maxPen, pen);
        }
        bool ground = false;
        if (maxPen > 0) {  // rebond amorti + frottement
            d.p += r.U * maxPen;
            float vn = glm::dot(d.v, r.U);
            if (vn < 0) d.v -= r.U * (vn * 1.25f);
            d.v -= (d.v - r.U * glm::dot(d.v, r.U)) * std::min(1.0f, 5.0f * dt);
            d.w *= std::max(0.0f, 1.0f - 4.0f * dt);
            ground = true;
        }
        // parois
        vec3 dd = d.p - r.c;
        float x = glm::dot(dd, r.R), y = glm::dot(dd, r.U), xl, xr;
        if (tb.wallRange(d.ring, y, xl, xr)) {
            float m = 0.12f;
            if (x < xl + m) {
                d.p += r.R * (xl + m - x);
                float vr = glm::dot(d.v, r.R);
                if (vr < 0) d.v -= r.R * (vr * 1.3f);
            }
            if (x > xr - m) {
                d.p -= r.R * (x - (xr - m));
                float vr = glm::dot(d.v, r.R);
                if (vr > 0) d.v -= r.R * (vr * 1.3f);
            }
        }
        // mise à plat puis sommeil quand le débris est presque immobile au sol
        if (ground && glm::length(d.v) < 0.5f) d.sleepT += dt;
        else d.sleepT = 0;
        if (d.sleepT > 0.12f) {
            d.v = vec3(0);
            d.w = vec3(0);
            vec3* ax[3] = {&d.ax, &d.ay, &d.az};
            int a = 0;
            float best = 0;
            for (int q = 0; q < 3; q++) {  // axe le plus vertical
                float v = std::fabs(glm::dot(*ax[q], r.U));
                if (v > best) { best = v; a = q; }
            }
            vec3 target = glm::dot(*ax[a], r.U) >= 0 ? r.U : -r.U;
            vec3 axis = glm::cross(*ax[a], target);
            float sl = glm::length(axis);
            if (sl > 1e-4f) {  // on le fait basculer doucement sur sa face la plus proche
                float ang = std::asin(std::min(1.0f, sl)) * std::min(1.0f, 10.0f * dt);
                mat3 R = mat3(glm::rotate(mat4(1), ang, axis / sl));
                d.ax = R * d.ax;
                d.ay = R * d.ay;
                d.az = R * d.az;
            }
            if (sl < 0.02f && d.sleepT > 0.4f) d.sleep = true;
        }
        if (d.age > 30) d.sleep = true;
    }
}

void World::buildDebrisMesh() {
    debrisMesh.clearCpu();
    Rng z(1);
    for (const Debris& d : debris) {
        if (d.stone) {  // pierre : ne brûle pas, rétrécit en fin de vie
            float ks = d.die < 0 ? 1.0f : std::max(0.03f, 1.0f - d.die / 1.4f);
            ks = ks * (2.0f - ks);
            debrisMesh.geom = 0;
            addBoard(debrisMesh, d.p, d.ax, d.ay, d.az, d.h.x * ks, d.h.y * ks, d.h.z * ks, Mat::STONE_BLOCK, d.seed, z, 0.012f);
            continue;
        }
        // morceaux de planche : modèle ; éclats et poteaux : boîtes
        debrisMesh.geom = (d.h.z <= 0.036f && d.h.y >= 0.065f && d.h.x >= 0.1f) ? 1 : 0;
        float k = d.life < WOOD_ASH ? 1.0f : std::max(0.03f, 1.0f - (d.life - WOOD_ASH) / (WOOD_GONE - WOOD_ASH));  // rétrécit en cendres
        k = k * (2.0f - k);
        // noirci ensuite (Vertex::seed = 20 + intensité des braises qui s'éteignent x 10)
        addBoard(debrisMesh, d.p, d.ax, d.ay, d.az, d.h.x * k, d.h.y * k, d.h.z * k, d.life < WOOD_CHAR ? Mat::WOOD : Mat::CHARRED_WOOD,
                 d.life < WOOD_CHAR ? d.seed : 20.0f + 10.0f * std::min(1.0f, std::max(0.0f, 1.0f - (d.life - WOOD_CHAR) / (WOOD_ASH - WOOD_CHAR))), z, 0.0f);
    }
    debrisMesh.geom = 0;
    debrisMesh.upload(GL_STREAM_DRAW);
}
