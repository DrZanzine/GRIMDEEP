// =============================================================================
//  game/game_ambient.cpp — ambiance : « la Dame », gouttes d'eau, rats, débris, poussière
// =============================================================================
#include "game/game.h"

#include <algorithm>

float Game::lightAt(vec3 p) const {
    // somme des contributions des lumières de l'image précédente (canal rouge), bornée
    float s = 0;
    for (auto& l : lv) {
        float d2 = glm::dot(l.p - p, l.p - p);
        if (d2 < 19.0f * 19.0f) {
            float w = 1 - d2 / (19.0f * 19.0f);
            s += w * w / (0.25f + d2 * 0.26f) * l.c.r;
        }
    }
    return std::min(s * 0.05f, 1.2f);
}

void Game::puff(vec3 at, int n, float strength) {
    for (int k = 0; k < n; k++) {
        Particle q;
        q.p = at + vec3(prng.r(-0.3f, 0.3f), prng.r(-0.2f, 0.3f), prng.r(-0.3f, 0.3f));
        q.v = vec3(prng.r(-0.5f, 0.5f), prng.r(0.0f, 0.6f), prng.r(-0.5f, 0.5f));
        q.maxLife = q.life = prng.r(1.0f, 2.2f);
        q.seed = prng.f() * 30;
        q.size = prng.r(0.35f, 0.7f);
        q.grow = 0.4f;
        q.kind = ParticleKind::DUST;
        q.inten = (lightAt(at) + 0.02f) * strength;  // la poussière n'est visible que si elle est éclairée
        parts.push_back(q);
    }
}

// ---------------------------------------------------------------------------
//  Rats
// ---------------------------------------------------------------------------

bool Game::spawnRat(float speed) {
    // paire de trous entre 3 et 28 m du joueur
    std::vector<int> cand;
    for (size_t i = 0; i < world.holes.size(); i++) {
        const HolePair& h = world.holes[i];
        float d = glm::length((h.pA + h.pB) * 0.5f - pos);
        if (d > 3.0f && d < 28.0f) cand.push_back((int)i);
    }
    if (cand.empty()) return false;
    const HolePair& h = world.holes[cand[grng.next() % cand.size()]];
    const Tube& tb = world.tubes[h.tube];
    Rat r;
    r.speed = speed;
    r.phase = grng.f() * 6;
    // sens de parcours au hasard
    bool rev = grng.chance(0.5f);
    vec3 pa = rev ? h.pB : h.pA, na = rev ? h.nB : h.nA, pb = rev ? h.pA : h.pB, nb = rev ? h.nA : h.nB;
    float xa = rev ? h.xB : h.xA, xb = rev ? h.xA : h.xB;
    int ia = rev ? h.iB : h.iA, ib = rev ? h.iA : h.iB;
    // chemin : du fond du trou A, zigzag au sol, jusqu'au fond du trou B
    r.pts.push_back(pa - na * 0.4f);
    r.pts.push_back(pa + na * 0.05f);
    for (int m = 1; m < 4; m++) {
        float t = m / 4.0f;
        int ri = (int)std::lround(mixf((float)ia, (float)ib, t));
        float x = mixf(xa, xb, t) + grng.r(-0.4f, 0.4f) * std::sin(t * PI);
        x = std::min(std::max(x, std::min(xa, xb) - 0.2f), std::max(xa, xb) + 0.2f);
        r.pts.push_back(tb.toWorld(ri, x, tb.floorY(ri, x) + 0.0f));
    }
    r.pts.push_back(pb + nb * 0.05f);
    r.pts.push_back(pb - nb * 0.4f);
    r.cum.assign(r.pts.size(), 0);
    for (size_t k = 1; k < r.pts.size(); k++) r.cum[k] = r.cum[k - 1] + glm::length(r.pts[k] - r.pts[k - 1]);
    rats.push_back(r);
    return true;
}

// ---------------------------------------------------------------------------
//  « La Dame »
// ---------------------------------------------------------------------------

/// Trois apparitions, une seule fois chacune :
///   0. à l'entrée de la mine, derrière le joueur (40 à 110 anneaux) — disparaît si l'on s'approche ;
///   1. au bout du long couloir des catacombes — disparaît si l'on s'approche ;
///   2. près de la sortie, derrière le joueur — disparaît après 1 s de regard.
/// En disparaissant, elle se dissout (0,45 s) dans une gerbe d'étincelles rouges.
void Game::updateLady() {
    if (opt.ladyDist > 0 && frame == 0) {  // (test) apparition forcée devant le joueur
        hn.ev = 1;
        hn.st[1] = 1;
        hn.dis = -1;
        hn.p = pos + flat * opt.ladyDist - vec3(0, 1.65f, 0);
    }
    if (opt.ladyDist > 0 && hn.ev >= 0) {
        vec3 to = pos - hn.p;
        to.y = 0;
        if (glm::length(to) > 1e-3f) hn.f = glm::normalize(to);
    } else if (state == PLAY && live && !world.tubes.empty()) {
        const Tube& mt = world.tubes[0];
        int pr = mt.locate(pos, hauntHint);
        hauntHint = pr;
        bool onMain = !world.inHub(pos) && glm::length(pos - mt.rings[pr].c) < 6.5f;  // dans la galerie principale
        auto feetAt = [&](int ring) {
            ring = std::max(1, std::min(mt.N - 2, ring));
            return mt.toWorld(ring, 0.0f, mt.floorY(ring, 0.0f));
        };
        if (hn.ev < 0) {  // déclenchement des événements
            if (hn.st[0] == 0 && onMain && pr > 40 && pr < 110) { hn.ev = 0; hn.p = feetAt(4); }
            else if (hn.st[1] == 0 && world.corridorStart >= 0 && onMain && pr > world.corridorStart - 70 && pr < world.corridorEnd - 14) { hn.ev = 1; hn.p = feetAt(world.corridorEnd - 5); }
            else if (hn.st[2] == 0 && world.doorOpen && onMain && pr > world.doorRing + 4 && glm::length(pos - world.exitPos) < 24.0f) { hn.ev = 2; hn.p = feetAt(std::max(world.doorRing - 2, pr - 18)); }
            if (hn.ev >= 0) { hn.st[hn.ev] = 1; hn.dis = -1; hn.lookT = 0; }
        }
        if (hn.ev >= 0 && hn.dis < 0) {
            vec3 to = pos - hn.p;
            to.y = 0;
            float dist = glm::length(to);
            if (dist > 1e-3f) hn.f = to / dist;  // elle nous fixe
            vec3 toHead = hn.p + vec3(0, 2.5f, 0) - eye;
            float lookDot = glm::dot(glm::normalize(toHead), front);
            bool go = false, loud = true;  // disparaît ? (loud : avec un son et une secousse)
            if (hn.ev == 0) {
                if (dist < 7.5f) go = true;
                else if (onMain && pr > 170) { go = true; loud = false; }
            } else if (hn.ev == 1) {
                if (dist < 7.0f) go = true;
                else if (onMain && pr > world.corridorEnd + 15) { go = true; loud = false; }
            } else {
                if (lookDot > 0.8f) hn.lookT += dt;
                if (hn.lookT > 1.0f) go = true;
            }
            if (go) {
                hn.dis = 0.0f;
                if (loud) {
                    audio.sfx(aud::SF_GHOST, 1.0f);
                    camShake = std::max(camShake, 0.12f);
                }
                for (int k = 0; k < 45; k++) {
                    Particle q;
                    q.p = hn.p + vec3(prng.r(-0.35f, 0.35f), prng.r(0.1f, 2.7f), prng.r(-0.35f, 0.35f));
                    q.v = vec3(prng.r(-0.4f, 0.4f), prng.r(0.2f, 1.0f), prng.r(-0.4f, 0.4f));
                    q.maxLife = q.life = prng.r(0.8f, 1.8f);
                    q.seed = prng.f() * 40; q.size = 0.035f; q.grow = 0; q.kind = ParticleKind::EMBER; q.inten = 12.0f;  // couleur 12 : rouge
                    parts.push_back(q);
                }
            }
        }
    }
    if (hn.ev >= 0 && hn.dis >= 0) {  // dissolution
        hn.dis += dt;
        if (hn.dis > 0.45f) {
            hn.st[hn.ev] = 2;
            hn.ev = -1;
        }
    }
}

mat4 Game::ladyMatrix() const {
    vec3 up(0, 1, 0), f = hn.f, x = glm::cross(up, f);
    return glm::translate(mat4(1.0f), hn.p) * mat4(mat3(x, up, f)) * glm::scale(mat4(1.0f), vec3(1.08f));
}

// ---------------------------------------------------------------------------
//  Gouttes d'eau
// ---------------------------------------------------------------------------

void Game::updateDrips() {
    for (auto& dr : world.drips) {  // seules les stalactites à moins de 32 m gouttent
        if (glm::length(dr.tip - pos) > 32.0f) continue;
        dr.timer -= dt;
        if (dr.timer <= 0) {
            dr.timer = drng2.r(4.0f, 15.0f);
            drops.push_back(Drop{dr.tip - vec3(0, 0.03f, 0), 0.0f, dr.tube, dr.ring, 0.0f});
        }
    }
    for (size_t i = 0; i < drops.size();) {
        Drop& dp = drops[i];
        dp.vy -= 9.8f * dt;
        dp.p.y += dp.vy * dt;
        dp.age += dt;
        const Tube& tb = world.tubes[dp.tube];
        const Ring& r = tb.rings[dp.ring];
        vec3 dd = dp.p - r.c;
        float x = glm::dot(dd, r.R), yl = glm::dot(dd, r.U);
        if (yl <= tb.floorY(dp.ring, x) || dp.age > 4.0f) {  // impact : 3 gouttelettes
            for (int k = 0; k < 3; k++) {
                Particle q;
                q.p = dp.p + vec3(0, 0.02f, 0);
                q.v = vec3(drng2.r(-0.4f, 0.4f), drng2.r(0.8f, 1.6f), drng2.r(-0.4f, 0.4f));
                q.maxLife = q.life = 0.35f;
                q.seed = 0; q.size = 0.045f; q.grow = 0; q.kind = ParticleKind::SPARK; q.inten = lightAt(dp.p) * 0.9f + 0.03f;
                parts.push_back(q);
            }
            drops[i] = drops.back();
            drops.pop_back();
            continue;
        }
        i++;
    }
}

// ---------------------------------------------------------------------------
//  Débris, cendres, rats, ragdolls
// ---------------------------------------------------------------------------

void Game::updateDebrisAndRats() {
    // --- débris (les pierres s'effritent hors du champ de la caméra)
    world.camPos = eye;
    world.camFwd = frontV;
    world.stepDebris(dt);
    world.buildDebrisMesh();
    for (const vec3& ev : world.ashEvents)  // le bois détruit noircit puis part en cendres
        if (parts.size() < 2600) {
            Particle q;
            q.p = ev;
            q.v = vec3(prng.r(-0.25f, 0.25f), prng.r(0.3f, 0.8f), prng.r(-0.25f, 0.25f));
            q.maxLife = q.life = prng.r(1.6f, 3.0f);
            q.seed = prng.f() * 30;
            q.size = prng.r(0.14f, 0.24f);
            q.grow = 0.22f;
            q.kind = ParticleKind::ASH;
            q.inten = lightAt(ev) + 0.05f;
            parts.push_back(q);
        }
    world.ashEvents.clear();

    // --- rats : un toutes les 25 à 70 s
    ratTimer -= dt;
    if (opt.forceRat && frame == std::max(0, opt.shotFrames - 45)) spawnRat(1.0f);
    if (ratTimer <= 0) {
        ratTimer = grng.r(25.0f, 70.0f);
        spawnRat(grng.r(2.6f, 3.8f));
    }
    world.ratMesh.clearCpu();
    for (auto& r : rats) {
        r.s += r.speed * dt;
        r.phase += dt * r.speed * 3.0f;
        if (r.s >= r.cum.back()) { r.alive = false; continue; }  // arrivé dans l'autre trou
        vec3 f;
        vec3 p = ratPositionAt(r, r.s, f);
        appendRatMesh(world.ratMesh, p, f, r.phase);
    }
    rats.erase(std::remove_if(rats.begin(), rats.end(), [](const Rat& r) { return !r.alive; }), rats.end());
    if (opt.ratShow) appendRatMesh(world.ratMesh, eye + flat * 1.3f - vec3(0, 1.62f, 0) + rightv * 0.15f, rightv * 0.7f + flat * 0.7f, T * 6.0f);
    world.ratMesh.upload(GL_STREAM_DRAW);

    // --- ragdolls (disparaissent au bout de 16 s)
    for (auto& rg : rags) stepRagdoll(world, rg, dt);
    rags.erase(std::remove_if(rags.begin(), rags.end(), [](const Ragdoll& rg) { return rg.t > 16.0f; }), rags.end());
}
