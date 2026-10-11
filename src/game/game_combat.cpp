// =============================================================================
//  game/game_combat.cpp — coups de pioche, ennemis, dynamite et explosions
// =============================================================================
#include "game/game.h"

#include <algorithm>
#include <cstdio>

// ---------------------------------------------------------------------------
//  Dégâts
// ---------------------------------------------------------------------------

void Game::hurtPlayer(float dmg) {
    if (state != PLAY || opt.god) return;
    hp -= dmg;
    hurtFlash = 0.7f;
    audio.sfx(aud::SF_HURT, 0.9f);
    if (hp <= 0) {
        hp = 0;
        state = DEAD;
        audio.sfx(aud::SF_DIE, 1.0f);
    }
}

void Game::killEnemy(Enemy& e, vec3 dir) {
    e.alive = false;
    kills++;
    audio.sfx(aud::SF_DIE, 0.7f);
    puff(e.pos - vec3(0, 0.8f, 0), 14, 1.2f);
    if (enemyModel(e.type).ok) {  // ragdoll (10 au maximum : le plus ancien disparaît)
        if (rags.size() >= 10) rags.erase(rags.begin());
        rags.push_back(makeRagdoll(e, dir, ENEMY_STATS[e.type].knock, rrng));
    }
    if (grng.chance(0.3f)) world.pickups.push_back(Pickup{e.pos - vec3(0, 1.2f, 0), 1, true, grng.f() * 6});  // 30 % : lâche une fiole
}

// ---------------------------------------------------------------------------
//  Coup de pioche
// ---------------------------------------------------------------------------

/// Rayon dans l'axe du regard (portée 3,2 m) + éclaboussure sur les blocs / planches
/// voisins du point d'impact, toiles déchirées, ennemis touchés (dégâts, repoussement,
/// étourdissement, ragdoll s'il meurt).
void Game::axeHit(vec3 eyePos, vec3 dir) {
    const float REACH = 3.2f, SPLASH = 0.62f;
    float t;
    int b = world.rayBoard(eyePos, dir, opt.forceBreak ? 12.0f : REACH, t);
    vec3 P = eyePos + dir * (b >= 0 ? t : 2.0f);  // point d'impact
    std::vector<int> list;
    world.boardsNear(P, SPLASH, list);
    if (b >= 0 && std::find(list.begin(), list.end(), b) == list.end()) list.push_back(b);
    int broken = 0, stoneHit = 0, stoneBroken = 0;
    for (int id : list) {
        bool isStone = world.woodMesh.boards[id].role == 7;
        world.hitBoard(id, P, dir);
        if (isStone) stoneHit++;
        if (!world.woodMesh.boards[id].alive) {
            broken++;
            if (isStone) stoneBroken++;
        }
    }
    if (stoneHit) {  // pierre : son, poussière, secousse, arrêt sur image, étincelles
        audio.sfx(stoneBroken ? aud::SF_ROCKBREAK : aud::SF_ROCK, stoneBroken ? 1.0f : 0.9f, (int)(prng.f() * 200));
        puff(P, 6 + 6 * stoneBroken, 1.3f);
        camShake = std::max(camShake, 0.06f + 0.03f * stoneBroken);
        hitStop = std::max(hitStop, stoneBroken ? 0.075f : 0.045f);
        for (int k = 0; k < 8; k++) {
            Particle q;
            q.p = P;
            q.v = (-dir * 0.8f + vec3(prng.r(-1, 1), prng.r(0.2f, 1.4f), prng.r(-1, 1))) * prng.r(1.5f, 3.6f);
            q.maxLife = q.life = prng.r(0.3f, 0.65f);
            q.seed = 0; q.size = 0.05f; q.grow = 0; q.kind = ParticleKind::SPARK; q.inten = 1.1f;
            parts.push_back(q);
        }
    } else if (!list.empty()) audio.sfx(broken ? aud::SF_BREAK : aud::SF_HITWOOD, 0.9f);
    if (broken && !stoneHit) puff(P, 10, 1.0f);
    std::printf("Pioche : %zu bloc(s)/planche(s) touche(s), %d casse(s)", list.size(), broken);
    std::puts("");
    world.clearWebs(eyePos, dir, REACH + 0.4f);
    // ennemis : le rayon doit passer à moins de (rayon + 0,35 m) du centre de leur corps
    vec3 fdir = glm::normalize(vec3(dir.x, 0, dir.z) + vec3(1e-6f));
    for (auto& e : enemies)
        if (e.alive) {
            vec3 c = e.pos - vec3(0, 1.65f, 0) + vec3(0, ENEMY_STATS[e.type].centerH, 0), oc = c - eyePos;
            float tt = glm::dot(oc, dir);
            if (tt < 0 || tt > REACH + 0.5f) continue;
            if (glm::length(oc - dir * tt) < ENEMY_STATS[e.type].radius + 0.35f) {
                const EnemyStat& st = ENEMY_STATS[e.type];
                e.hp -= 1;
                e.hurt = 0.3f;
                e.hurtLean = 1.0f;
                e.awake = true;
                e.stun = st.stun;
                e.atkT = -1;  // l'attaque en cours est interrompue
                e.vel = fdir * st.knock;
                audio.sfx(aud::SF_FLESH, 0.95f);
                hitStop = std::max(hitStop, 0.07f);
                camShake = std::max(camShake, 0.1f);
                for (int k = 0; k < 7; k++) {  // gerbe de braises rouges
                    Particle q;
                    q.p = c;
                    q.v = (fdir * 1.4f + vec3(prng.r(-1, 1), prng.r(0.0f, 1.4f), prng.r(-1, 1))) * prng.r(0.8f, 2.2f);
                    q.maxLife = q.life = prng.r(0.5f, 1.1f);
                    q.seed = prng.f() * 20; q.size = 0.05f; q.grow = 0; q.kind = ParticleKind::EMBER; q.inten = 4.0f;
                    parts.push_back(q);
                }
                puff(c, 3, 0.9f);
                if (e.hp <= 0) killEnemy(e, fdir);
            }
        }
}

// ---------------------------------------------------------------------------
//  Explosion
// ---------------------------------------------------------------------------

/// Boule de feu, éclair, onde de choc au sol, gerbes d'étincelles, braises, fumée noire,
/// éclats de roche, lumière aveuglante qui projette des ombres, ralenti, secousse, souffle
/// sur le joueur ; détruit blocs, planches, toiles, monstres et rats dans un rayon de 6,5 m.
void Game::explode(vec3 C) {
    const float R = DYN_RADIUS;
    // direction aléatoire, biaisée vers le haut
    auto rdir = [&](float upBias) {
        vec3 d(prng.r(-1, 1), prng.r(-1, 1) + upBias, prng.r(-1, 1));
        return glm::normalize(d + vec3(1e-4f));
    };
    // ajoute une particule (position, vitesse, durée de vie, taille, croissance, type, intensité)
    auto P = [&](vec3 p, vec3 v, float life, float size, float grow, int kind, float inten) {
        Particle q;
        q.p = p; q.v = v; q.maxLife = q.life = life; q.seed = prng.f() * 50; q.size = size; q.grow = grow; q.kind = kind; q.inten = inten;
        parts.push_back(q);
    };
    for (int k = 0; k < 4; k++) P(C, vec3(0), 0.09f + 0.02f * k, 1.5f + k * 0.8f, 16.0f, 7, 0.7f);  // éclair central
    for (int k = 0; k < 150; k++) { vec3 d = rdir(0.35f); P(C + d * prng.r(0.2f, 0.8f), d * prng.r(1.5f, 9.0f), prng.r(0.8f, 1.9f), prng.r(0.9f, 2.0f), prng.r(1.0f, 2.6f), 7, prng.r(0.14f, 0.3f)); }  // boule de feu
    for (int k = 0; k < 26; k++) { vec3 d = rdir(1.6f); P(C, d * prng.r(6.0f, 11.0f), prng.r(0.9f, 1.6f), prng.r(0.25f, 0.45f), prng.r(0.6f, 1.4f), 7, 0.6f); }  // langues de feu qui montent
    for (int k = 0; k < 130; k++) { vec3 d = rdir(0.6f); P(C + d * 0.3f, d * prng.r(0.8f, 4.5f) + vec3(0, prng.r(0.5f, 2.5f), 0), prng.r(3.5f, 7.5f), prng.r(1.0f, 2.0f), prng.r(0.6f, 1.4f), 8, prng.r(0.8f, 1.4f)); }  // fumée
    for (int k = 0; k < 200; k++) { vec3 d = rdir(0.4f); P(C, d * prng.r(6.0f, 20.0f), prng.r(0.35f, 1.3f), 0.06f, 0.0f, 5, 2.2f); }  // étincelles
    for (int k = 0; k < 140; k++) { vec3 d = rdir(0.8f); P(C, d * prng.r(2.0f, 9.0f), prng.r(1.5f, 4.5f), 0.05f, 0.0f, 2, 0.0f); }  // braises
    float flo = world.constrain(C, false, hints).y - 1.65f;  // altitude du sol sous l'explosion
    for (int k = 0; k < 110; k++) {  // onde de choc au sol
        float a = 2 * PI * k / 110 + prng.r(-0.03f, 0.03f);
        vec3 d(std::cos(a), 0, std::sin(a));
        P(vec3(C.x, flo + 0.25f, C.z) + d * 0.6f, d * prng.r(11.0f, 17.0f), prng.r(0.6f, 1.2f), 0.45f, 2.6f, 3, (lightAt(C) + 0.5f) * 1.8f);
    }
    for (int k = 0; k < 10; k++) world.spawnChips(C, rdir(0.8f) * 4.5f, 5, true);  // éclats de roche

    // --- destruction : blocs de pierre, planches (qui partent en braises), toiles, rats
    std::vector<int> ids;
    world.boardsNear(C, R, ids);
    for (int id : ids) {
        Board& b = world.woodMesh.boards[id];
        if (!b.alive) continue;
        vec3 d = glm::normalize(b.c - C + vec3(0, 0.3f, 0) + vec3(1e-4f));
        float k = std::max(0.0f, 1.0f - glm::length(b.c - C) / R);
        if (b.role == 7) world.breakStone(id, b.c, d * (5.0f + 9.0f * k));
        else world.breakBoard(id, b.c, d * (2.5f + 3.5f * k), false);
    }
    for (auto& db : world.debris)  // le bois fraîchement cassé brûle tout de suite
        if (!db.stone && db.life < 0.01f && glm::length(db.p - C) < R + 2.0f) db.life = WOOD_CHAR - 0.3f;
    for (auto& w : world.webs)
        if (w.alive && glm::length(w.c - C) < R) {
            w.alive = false;
            world.webMesh.killIndices(w.i0, 3);
        }
    for (auto& r : rats)
        if (r.alive) {
            vec3 f;
            if (glm::length(ratPositionAt(r, r.s, f) - C) < R) r.alive = false;
        }
    // --- monstres : pulvérisés (ragdoll projeté) dans le rayon, soufflés jusqu'à 2 rayons
    for (auto& e : enemies)
        if (e.alive) {
            vec3 d = e.pos - C;
            d.y = 0;
            float dist = glm::length(e.pos - C);
            vec3 dir = glm::length(d) > 1e-3f ? glm::normalize(d) : vec3(1, 0, 0);
            if (dist < R) {
                size_t n0 = rags.size();
                e.hp = 0;
                killEnemy(e, dir);
                if (rags.size() > n0 || (rags.size() == 10 && n0 == 10)) {  // un ragdoll vient d'être créé
                    Ragdoll& rg = rags.back();
                    float k = 1.0f - dist / R;
                    rg.v = dir * (7.0f + 13.0f * k) + vec3(0, 5.0f + 6.0f * k, 0);
                    rg.w = vec3(prng.r(-9, 9), prng.r(-9, 9), prng.r(-9, 9));
                }
                puff(e.pos - vec3(0, 0.8f, 0), 10, 1.5f);
            } else if (dist < 2.0f * R) {
                float k = 1.0f - dist / (2.0f * R);
                e.vel = dir * 14.0f * k;
                e.stun = 1.4f;
                e.hurt = 0.3f;
                e.hurtLean = 1.0f;
                e.atkT = -1;
                e.awake = true;
            }
        }
    for (auto& rg : rags) {  // les corps déjà au sol sont relancés
        vec3 d = rg.C - C;
        float dist = glm::length(d);
        if (dist < R && dist > 1e-3f) {
            rg.asleep = false;
            rg.rest = 0;
            rg.v += glm::normalize(d) * 10.0f * (1.0f - dist / R) + vec3(0, 4.0f, 0);
        }
    }
    // --- joueur : souffle, sans dégâts (c'est une démonstration), effets d'écran et de son
    {
        vec3 d = pos - C;
        d.y = 0;
        float dist = glm::length(pos - C), k = std::max(0.0f, 1.0f - dist / (R * 1.6f));
        if (k > 0 && glm::length(d) > 1e-3f) {
            pvel += glm::normalize(d) * 8.0f * k;
            if (jumpOff <= 0.0f && !fly) jumpV = 3.5f * k;
        }
        float kv = std::max(0.15f, std::min(1.0f, 1.0f - dist / 35.0f));
        camShake = std::max(camShake, 0.75f * kv);
        fovPunch = std::max(fovPunch, 11.0f * kv);
        flashW = std::max(flashW, 0.55f * kv);
        muffle = std::max(muffle, 0.8f * kv);
        hitStop = std::max(hitStop, 0.07f);
        slowMo = 0.75f;
        audio.sfx(aud::SF_BOOM, 1.0f);
        audio.sfx(aud::SF_ROCKBREAK, 1.0f);
        audio.sfx(aud::SF_RING, kv);
    }
    blasts.push_back(Blast{C, 0.0f});
}

// ---------------------------------------------------------------------------
//  Ennemis
// ---------------------------------------------------------------------------

void Game::updateEnemies() {
    for (auto& e : enemies)
        if (e.alive) {
            const EnemyStat& st = ENEMY_STATS[e.type];
            vec3 d = pos - e.pos;
            d.y = 0;
            float dh = glm::length(d), d3 = glm::length(pos - e.pos);  // distances horizontale et 3D au joueur
            if (!e.awake && state == PLAY && d3 < st.aggro && dh < st.aggro) {  // s'éveille en rugissant
                e.awake = true;
                audio.sfx(aud::SF_ROAR, std::max(0.2f, 1.0f - d3 / 20.0f));
            }
            if (e.awake && d3 > 36.0f) e.awake = false;  // abandonne la poursuite
            e.hurt = std::max(0.0f, e.hurt - dt);
            e.hurtLean = std::max(0.0f, e.hurtLean - dt * 3.0f);
            e.cd -= dt;
            e.stun -= dt;
            bool pushed = glm::length(e.vel) > 0.05f;
            if (pushed) {  // repoussement qui s'amortit
                e.pos += e.vel * dt;
                e.vel *= std::max(0.0f, 1.0f - 6.0f * dt);
            }
            if (e.awake && state == PLAY && e.stun <= 0) {
                vec3 dir = d / std::max(dh, 1e-3f);
                if (e.atkT < 0) {  // poursuite : se tourne vers le joueur et avance
                    vec3 nf = glm::normalize(glm::mix(e.face, dir, std::min(1.0f, dt * 10.0f)) + vec3(1e-5f));
                    e.face = nf;
                    if (dh > st.reach * 0.6f) {
                        e.pos += dir * st.speed * dt;
                        e.anim += dt * st.speed * 3.2f;
                    }
                    if (dh < st.reach * 0.95f && std::fabs(pos.y - e.pos.y) < 1.5f && e.cd <= 0) {  // à portée : attaque
                        e.atkT = 0;
                        e.atkHit = false;
                        e.cd = st.cd + st.wind;
                        audio.sfx(aud::SF_ROAR, 0.3f);
                    }
                } else {  // attaque en 3 temps : armement -> fente -> récupération
                    e.atkT += dt;
                    e.face = glm::normalize(glm::mix(e.face, dir, std::min(1.0f, dt * (e.atkT < st.wind ? 8.0f : 1.5f))) + vec3(1e-5f));
                    if (!e.atkHit && e.atkT >= st.wind) {  // fin de l'armement : le coup porte si le joueur n'a pas esquivé
                        e.atkHit = true;
                        if (dh < st.reach + 0.5f && std::fabs(pos.y - e.pos.y) < 1.7f) {
                            hurtPlayer(st.dmg);
                            camShake = std::max(camShake, 0.25f);
                        }
                    }
                    if (e.atkT >= st.wind && e.atkT < st.wind + ATK_STRIKE && dh > st.reach * 0.5f) e.pos += dir * (e.type == 1 ? 7.5f : 3.4f) * dt;  // fente
                    if (e.atkT > st.wind + ATK_STRIKE + ATK_RECOVER) e.atkT = -1.0f;
                }
            } else if (e.stun > 0) e.atkT = -1.0f;
            if (e.awake || pushed) {
                world.collideBoards(e.pos, false);
                e.pos = world.constrain(e.pos, false, e.hints);
            }
        }
}

// ---------------------------------------------------------------------------
//  Dynamite
// ---------------------------------------------------------------------------

void Game::updateDynamite() {
    // G lance un bâton (6 au maximum en même temps)
    bool kG = glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS || frame == opt.dynFrame;
    if (kG && !keyG && state == PLAY && live && dyns.size() < 6) {
        Dynamite d;
        d.p = eye + front * 0.45f + rightv * 0.18f - vec3(0, 0.12f, 0);
        d.v = front * 10.5f + vec3(0, 2.8f, 0) + pvel * 0.6f;
        d.w = vec3(prng.r(-9, 9), prng.r(-9, 9), prng.r(-9, 9));
        d.hints = hints;
        dyns.push_back(d);
        audio.sfx(aud::SF_SWING, 0.8f);
        camKick -= 0.03f;
    }
    keyG = kG;
    for (size_t i = 0; i < dyns.size();) {
        Dynamite& d = dyns[i];
        d.t += dt;
        d.hiss -= dt;
        if (d.hiss <= 0) {  // crépitement de la mèche
            d.hiss = 0.22f;
            audio.sfx(aud::SF_FUSE, std::max(0.05f, 0.7f - glm::length(d.p - pos) / 25.0f));
        }
        if (!d.rest) {  // vol balistique, rebonds sur les planches, les parois et le sol
            d.v.y -= 9.8f * dt;
            vec3 np = d.p + d.v * dt, q = np;
            world.collideBoards(q, true);
            vec3 c = world.constrain(q, false, d.hints);
            vec3 corr(c.x - np.x, 0, c.z - np.z);
            corr += vec3(q.x - np.x, q.y - np.y, q.z - np.z) * vec3(0, 1, 0);
            np.x = c.x;
            np.z = c.z;
            if (glm::length(corr) > 1e-4f) {
                vec3 n = glm::normalize(corr);
                float vn = glm::dot(d.v, n);
                if (vn < 0) d.v -= n * (vn * 1.45f);
                d.v *= 0.8f;
                d.w *= 0.8f;
            }
            float fl = c.y - 1.65f;
            if (np.y < fl + 0.04f) {
                np.y = fl + 0.04f;
                if (d.v.y < 0) {
                    if (d.v.y < -2.0f) audio.sfx(aud::SF_HITWOOD, 0.25f);
                    d.v.y = -d.v.y * 0.3f;
                }
                d.v.x *= 0.8f;
                d.v.z *= 0.8f;
                d.w *= 0.65f;
                if (glm::length(d.v) < 0.3f) d.rest = true;
            }
            d.p = np;
            float wl = glm::length(d.w);
            if (wl > 1e-4f) d.R = mat3(glm::rotate(mat4(1.0f), wl * dt, d.w / wl)) * d.R;
        }
        if (d.t >= DYN_FUSE) {  // BOUM
            vec3 c = d.p + vec3(0, 0.15f, 0);
            dyns.erase(dyns.begin() + i);
            explode(c);
            continue;
        }
        i++;
    }
    for (auto& b : blasts) b.t += dt;
    blasts.erase(std::remove_if(blasts.begin(), blasts.end(), [](const Blast& b) { return b.t > 7.0f; }), blasts.end());
    fovPunch = std::max(0.0f, fovPunch - dt * 18.0f);
    flashW = std::max(0.0f, flashW - rdt * 3.5f);
}
