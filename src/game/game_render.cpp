// =============================================================================
//  game/game_render.cpp — passes de rendu d'une image
//
//    1. ombres   : pour chaque lumière qui a une cube map, la distance à la lumière est
//                  rendue dans les 6 faces (shadow.vert/.frag) ;
//    2. scène    : ciel puis géométrie en HDR dans le tampon MSAA (scene.vert/.frag),
//                  la Dame, les toiles (alpha-to-coverage), l'eau (mélange) ;
//    3. particules : poussière, flammes, halos, braises, étincelles (additif) et fumée
//                  (mélange alpha, triée de loin à près) ;
//    4. objets en main : pioche et lanterne, dessinées par-dessus la scène ;
//    5. post-traitement : résolution MSAA, bloom, tonemapping ACES, vignettage, grain.
// =============================================================================
#include "game/game.h"

#include <algorithm>

#include "entities/props.h"
#include "render/materials.h"
#include "render/shader.h"

bool Game::prepareFrame() {
    glfwGetFramebufferSize(win, &fbw, &fbh);
    if (fbw < 8 || fbh < 8) return false;
    if (fbw != gfx.targets.w || fbh != gfx.targets.h) gfx.targets.create(fbw, fbh);
    aspect = (float)fbw / fbh;
    // visée finale : regard + inclinaison du coup de pioche ; légère inclinaison latérale en se déplaçant
    float pk = pitch + camKick;
    frontV = titleCam ? front : vec3(std::cos(yaw) * std::cos(pk), std::sin(pk), std::sin(yaw) * std::cos(pk));
    float rollA = std::max(-1.0f, std::min(1.0f, glm::dot(pvel, rightv) / 5.6f)) * 0.02f;
    vec3 upv = glm::normalize(vec3(0, 1, 0) + rightv * rollA);
    // champ de vision : 76° + course + souffle d'explosion
    proj = glm::perspective(glm::radians(76.0f + 8.0f * sprintAmt + fovPunch), aspect, 0.05f, 900.0f);
    view = glm::lookAt(eye, eye + frontV, upv);
    VP = proj * view;
    return true;
}

void Game::buildDynamicMeshes() {
    enemyMesh.clearCpu();
    for (auto& e : enemies)
        if (e.alive && glm::length(e.pos - eye) < 60.0f) appendEnemyMesh(enemyMesh, e, T);
    for (auto& rg : rags)  // les corps rétrécissent pendant leurs 2 dernières secondes
        if (glm::length(rg.C - eye) < 60.0f) appendRagdollMesh(enemyMesh, rg, rg.t < 14.0f ? 1.0f : std::max(0.02f, 1.0f - (rg.t - 14.0f) / 2.0f));
    enemyMesh.upload(GL_STREAM_DRAW);
    propsMesh.clearCpu();
    for (auto& p : world.pickups)
        if (p.alive && glm::length(p.p - eye) < 50.0f) appendPickupMesh(propsMesh, p, T);
    for (const Dynamite& d : dyns) appendDynamiteMesh(propsMesh, d);
    propsMesh.upload(GL_STREAM_DRAW);
}

void Game::renderShadows() {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, gfx.shadowFbo);
    glViewport(0, 0, SHADOW_RES, SHADOW_RES);
    glUseProgram(gfx.progShadow);
    // directions et vecteurs « haut » des 6 faces d'une cube map (+X, -X, +Y, -Y, +Z, -Z)
    static const vec3 fdir[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const vec3 fup[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
    mat4 sproj = glm::perspective(PI * 0.5f, 1.0f, 0.1f, SHADOW_FAR);
    for (int i = 0; i < nl; i++) {
        if (lsh[i] < 0) continue;
        vec3 lp = lv[i].p;
        glUniform3fv(uniformLoc(gfx.progShadow, "uLP"), 1, glm::value_ptr(lp));
        for (int f = 0; f < 6; f++) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gfx.shadowTex, 0, lsh[i] * 6 + f);
            glClearColor(200, 0, 0, 0);  // « très loin » par défaut
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            mat4 svp = sproj * glm::lookAt(lp, lp + fdir[f], fup[f]);
            glUniformMatrix4fv(uniformLoc(gfx.progShadow, "uVP"), 1, GL_FALSE, glm::value_ptr(svp));
            auto keep = [&](const Chunk& c) { return glm::length(c.c - lp) < c.r + LIGHT_RADIUS + 1.0f; };
            world.rockMesh.draw(keep);
            world.woodMesh.draw(keep);
            world.hubMesh.draw(keep);
            world.debrisMesh.drawAll();
            world.ratMesh.drawAll();
            enemyMesh.drawAll();
        }
    }
}

void Game::renderScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, gfx.targets.msFbo);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    const vec3 sunDir = glm::normalize(vec3(0.55f, 0.27f, 0.62f)), sunCol(1.05f, 0.58f, 0.40f), haze(0.30f, 0.17f, 0.27f);  // soleil couchant du camp
    {  // ciel (visible depuis le camp et par l'ouverture de la mine)
        float dMouth = glm::length(eye - world.tubes[0].rings[0].c);
        float skyAmt = world.inHub(eye) ? 1.0f : std::pow(std::max(0.0f, 1.0f - dMouth / 30.0f), 2.0f);
        if (skyAmt > 0.001f) {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glUseProgram(gfx.progSky);
            glBindVertexArray(gfx.quadVao);
            glUniformMatrix4fv(uniformLoc(gfx.progSky, "uInvVP"), 1, GL_FALSE, glm::value_ptr(glm::inverse(VP)));
            glUniform3fv(uniformLoc(gfx.progSky, "uCam"), 1, glm::value_ptr(eye));
            glUniform3fv(uniformLoc(gfx.progSky, "uSunDir"), 1, glm::value_ptr(sunDir));
            glUniform1f(uniformLoc(gfx.progSky, "uAmt"), skyAmt);
            glUniform1f(uniformLoc(gfx.progSky, "uTime"), T);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
        }
    }
    // --- uniformes de la scène
    GLuint p = gfx.progScene;
    glUseProgram(p);
    if (!gMaterials.diffuse.empty()) {  // palette (animée : cristaux du camp)
        glUniform4fv(uniformLoc(p, "uMdlA"), (GLsizei)gMaterials.diffuse.size(), glm::value_ptr(gMaterials.diffuse[0]));
        glUniform4fv(uniformLoc(p, "uMdlE"), (GLsizei)gMaterials.emissive.size(), glm::value_ptr(gMaterials.emissive[0]));
    }
    glUniform3fv(uniformLoc(p, "uSunDir"), 1, glm::value_ptr(sunDir));
    glUniform3fv(uniformLoc(p, "uSunCol"), 1, glm::value_ptr(sunCol));
    glUniform3fv(uniformLoc(p, "uHaze"), 1, glm::value_ptr(haze));
    glUniform2f(uniformLoc(p, "uHubC"), 0.0f, world.hubFrontZ + 10.0f);
    glUniform1f(uniformLoc(p, "uHubG"), world.hubGroundY);
    mat4 I(1.0f);
    glUniformMatrix4fv(uniformLoc(p, "uVP"), 1, GL_FALSE, glm::value_ptr(VP));
    glUniformMatrix4fv(uniformLoc(p, "uModel"), 1, GL_FALSE, glm::value_ptr(I));
    glUniform3fv(uniformLoc(p, "uCam"), 1, glm::value_ptr(eye));
    glUniform1f(uniformLoc(p, "uTime"), T);
    glUniform1i(uniformLoc(p, "uNL"), nl);
    glUniform3fv(uniformLoc(p, "uLPos"), MAX_LIGHTS, lpos);
    glUniform3fv(uniformLoc(p, "uLCol"), MAX_LIGHTS, lcol);
    glUniform1iv(uniformLoc(p, "uLSh"), MAX_LIGHTS, lsh);
    glUniform1f(uniformLoc(p, "uLR"), LIGHT_RADIUS);
    glUniform3fv(uniformLoc(p, "uTint"), 7, palTint);
    glUniform3fv(uniformLoc(p, "uAmbB"), 7, palAmb);
    glUniform1fv(uniformLoc(p, "uWetB"), 7, palWet);
    glUniform1fv(uniformLoc(p, "uMossB"), 7, palMoss);
    glUniform1i(uniformLoc(p, "uRockCol"), 0);
    glUniform1i(uniformLoc(p, "uRockNrm"), 1);
    glUniform1i(uniformLoc(p, "uWoodCol"), 2);
    glUniform1i(uniformLoc(p, "uWoodNrm"), 3);
    glUniform1i(uniformLoc(p, "uShadow"), 4);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gfx.texRockC);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gfx.texRockN);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gfx.texWoodC);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, gfx.texWoodN);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, gfx.shadowTex);
    // --- géométrie opaque (culling par distance : 130 m dans la mine, 420 m pour le camp)
    auto vis = [&](const Chunk& c) { return glm::length(c.c - eye) < c.r + 130.0f; };
    world.rockMesh.draw(vis);
    world.woodMesh.draw(vis);
    world.torchMesh.draw(vis);
    {
        auto visFar = [&](const Chunk& c) { return glm::length(c.c - eye) < c.r + 420.0f; };
        world.hubMesh.draw(visFar);
    }
    world.debrisMesh.drawAll();
    world.ratMesh.drawAll();
    enemyMesh.drawAll();
    propsMesh.drawAll();
    if (hn.ev >= 0) {  // la Dame (uGhost > 0 : dissolution)
        mat4 Hw = ladyMatrix();
        glUniformMatrix4fv(uniformLoc(p, "uModel"), 1, GL_FALSE, glm::value_ptr(Hw));
        glUniform1f(uniformLoc(p, "uGhost"), hn.dis >= 0 ? hn.dis / 0.45f : 0.0f);
        lady.mesh.drawAll();
        glUniformMatrix4fv(uniformLoc(p, "uModel"), 1, GL_FALSE, glm::value_ptr(I));
        glUniform1f(uniformLoc(p, "uGhost"), 0.0f);
    }
    // --- toiles (transparence par alpha-to-coverage), puis eau (mélange, sans écrire la profondeur)
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    world.webMesh.drawAll();
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    world.waterMesh.draw(vis);
}

void Game::renderParticles() {
    // --- poussière en suspension (4500 points autour de la caméra, positions calculées dans le shader)
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_ONE, GL_ONE);
    float px = fbh * 0.5f * proj[1][1];  // facteur taille (m) -> pixels
    glUseProgram(gfx.progDust);
    glUniformMatrix4fv(uniformLoc(gfx.progDust, "uVP"), 1, GL_FALSE, glm::value_ptr(VP));
    glUniform3fv(uniformLoc(gfx.progDust, "uCam"), 1, glm::value_ptr(eye));
    glUniform1f(uniformLoc(gfx.progDust, "uTime"), T);
    glUniform1f(uniformLoc(gfx.progDust, "uPx"), px);
    glUniform1i(uniformLoc(gfx.progDust, "uNL"), nl);
    glUniform3fv(uniformLoc(gfx.progDust, "uLPos"), MAX_LIGHTS, lpos);
    glUniform3fv(uniformLoc(gfx.progDust, "uLCol"), MAX_LIGHTS, lcol);
    glUniform1f(uniformLoc(gfx.progDust, "uLR"), LIGHT_RADIUS);
    glBindVertexArray(gfx.dustVao);
    glDrawArrays(GL_POINTS, 0, 4500);

    // nuage de poussière quand du bois casse (nouveaux débris)
    if (world.debris.size() > lastDebrisCount && world.debris.size() - lastDebrisCount >= 2) puff(world.debris.back().p, 12, 1.0f);
    lastDebrisCount = world.debris.size();

    // --- points à dessiner : 8 flottants (position, taille, type, graine, vie, intensité)
    std::vector<float> pb;
    auto push = [&](vec3 p, float size, float kind, float seedv, float life, float inten) {
        float d[8] = {p.x, p.y, p.z, size, kind, seedv, life, inten};
        pb.insert(pb.end(), d, d + 8);
    };
    // flammes et halos des lumières (et quelques braises qui s'en échappent)
    for (auto& l : lv) {
        if (l.dist > 70.0f) continue;
        if (l.src == 100000) { push(l.p, 0.7f, 1, 0, 0, 0.6f); continue; }  // lanterne
        if (l.src == 400000) {  // yeux de la Dame
            mat4 Hw = ladyMatrix();
            for (int e = 0; e < 2; e++) push(vec3(Hw * vec4(lady.eye[e], 1.0f)), 0.11f, 1, 0, 12.0f, 4.0f);
            continue;
        }
        if (l.src >= 300000) {  // orbes et projecteurs du camp ; explosions et mèches (>= 500000) passent aussi ici : halo + braises
            bool beam = l.src >= 310000;
            push(l.p, beam ? 2.6f : 1.7f, 1, 0, (float)l.biome, beam ? 1.4f : 2.4f);
            if (!beam && prng.chance(std::min(1.0f, 6.0f * dt)) && parts.size() < 2500) {
                Particle e;
                e.p = l.p + vec3(prng.r(-0.25f, 0.25f), prng.r(-0.25f, 0.25f), prng.r(-0.25f, 0.25f));
                e.v = vec3(prng.r(-0.5f, 0.5f), prng.r(-0.2f, 0.6f), prng.r(-0.5f, 0.5f));
                e.maxLife = e.life = prng.r(0.9f, 2.0f);
                e.seed = prng.f() * 40; e.size = 0.04f; e.grow = 0; e.kind = ParticleKind::EMBER; e.inten = (float)l.biome;
                parts.push_back(e);
            }
            continue;
        }
        if (!world.lights[l.src].flame) { push(l.p, 1.6f, 1, 0, (float)l.biome, 0.5f); continue; }  // lumière sans flamme : halo
        vec3 base = l.p - vec3(0, 0.08f, 0);
        for (int j = 0; j < 3; j++) push(base + vec3(0, 0.05f * j, 0), 0.6f - 0.12f * j, 0, l.src * 1.37f + j * 0.41f, (float)l.biome, 1.0f);  // 3 langues de flamme
        push(l.p, 2.2f, 1, 0, (float)l.biome, 1.0f);
        if (prng.chance(std::min(1.0f, 4.0f * dt)) && l.dist < 40.0f && parts.size() < 2500) {
            Particle e;
            e.p = l.p + vec3(prng.r(-0.05f, 0.05f), 0.02f, prng.r(-0.05f, 0.05f));
            e.v = vec3(prng.r(-0.25f, 0.25f), prng.r(0.35f, 0.9f), prng.r(-0.25f, 0.25f));
            e.maxLife = e.life = prng.r(1.2f, 3.2f);
            e.seed = prng.f() * 50; e.size = 0.035f; e.grow = 0; e.kind = ParticleKind::EMBER; e.inten = (float)l.biome;
            parts.push_back(e);
        }
    }
    for (auto& p : world.pickups)  // halo des objets (doré / rouge)
        if (p.alive && glm::length(p.p - eye) < 40.0f) push(p.p, 0.9f, 1, 0, p.kind == 0 ? 0.0f : 4.0f, 0.45f);
    for (const Dynamite& d : dyns) {  // mèche : éclat + gerbe d'étincelles
        vec3 tp = fuseTip(d);
        push(tp, 0.35f + 0.1f * std::sin(T * 61.0f), 1, 0, 0.0f, 2.5f);
        push(tp, 0.12f, 7, d.t * 9.0f, 0.95f, 2.0f);
        for (int k = 0; k < 2; k++)
            if (parts.size() < 5000) {
                Particle q;
                q.p = tp;
                q.v = vec3(prng.r(-1.2f, 1.2f), prng.r(0.5f, 2.2f), prng.r(-1.2f, 1.2f));
                q.maxLife = q.life = prng.r(0.15f, 0.4f);
                q.seed = 0; q.size = 0.035f; q.grow = 0; q.kind = ParticleKind::SPARK; q.inten = 1.6f;
                parts.push_back(q);
            }
    }
    for (const Blast& b : blasts)  // sol qui brûle encore, fumée qui continue de monter
        if (b.t < 6.0f && parts.size() < 5000) {
            float k = 1.0f - b.t / 6.0f;
            int nf = (int)(dt * 70.0f * k + prng.f()), ns = (int)(dt * 14.0f * k + prng.f());
            for (int n = 0; n < nf; n++) {
                float a = prng.r(0, 2 * PI), r = std::sqrt(prng.f()) * 2.2f;
                Particle q;
                q.p = b.c + vec3(std::cos(a) * r, -0.1f, std::sin(a) * r);
                q.v = vec3(prng.r(-0.2f, 0.2f), prng.r(0.8f, 2.0f), prng.r(-0.2f, 0.2f));
                q.maxLife = q.life = prng.r(0.4f, 0.9f);
                q.seed = prng.f() * 50; q.size = prng.r(0.15f, 0.35f) * (0.5f + k); q.grow = 0.5f; q.kind = ParticleKind::FIRE; q.inten = 1.3f * k;
                parts.push_back(q);
            }
            for (int n = 0; n < ns; n++) {
                Particle q;
                q.p = b.c + vec3(prng.r(-1.2f, 1.2f), 0.4f, prng.r(-1.2f, 1.2f));
                q.v = vec3(prng.r(-0.3f, 0.3f), prng.r(0.8f, 1.8f), prng.r(-0.3f, 0.3f));
                q.maxLife = q.life = prng.r(3.0f, 5.0f);
                q.seed = prng.f() * 50; q.size = 0.5f; q.grow = 0.9f; q.kind = ParticleKind::SMOKE; q.inten = 0.6f;
                parts.push_back(q);
            }
        }
    std::vector<float> ps;  // fumée : mélange alpha, triée de loin à près
    for (auto& dp : drops) push(dp.p, 0.05f, 5, 0, 0, lightAt(dp.p) * 0.9f + 0.05f);  // gouttes en chute

    // --- mise à jour des particules (mouvement propre à chaque type)
    for (size_t i = 0; i < parts.size();) {
        Particle& e = parts[i];
        e.life -= dt;
        if (e.life <= 0) { parts[i] = parts.back(); parts.pop_back(); continue; }
        if (e.kind == ParticleKind::EMBER) e.v += vec3(std::sin(T * 3 + e.seed * 9), 0.15f, std::cos(T * 2.5f + e.seed * 5)) * (0.8f * dt);  // flotte
        else if (e.kind == ParticleKind::SPARK) e.v.y -= 9.8f * dt;                                                                       // tombe
        else if (e.kind == ParticleKind::ASH) { e.v *= std::max(0.0f, 1.0f - 0.8f * dt); e.v.y += 0.25f * dt; }                           // monte doucement
        else if (e.kind == ParticleKind::FIRE) { e.v *= std::max(0.0f, 1.0f - 3.0f * dt); e.v.y += 2.0f * dt; e.grow *= std::max(0.0f, 1.0f - 1.5f * dt); }
        else if (e.kind == ParticleKind::SMOKE) { e.v *= std::max(0.0f, 1.0f - 0.9f * dt); e.v.y += 0.7f * dt; }
        else e.v *= std::max(0.0f, 1.0f - 1.5f * dt);
        e.p += e.v * dt;
        e.size += e.grow * dt;
        if (e.kind == ParticleKind::SMOKE) {
            float d8[8] = {e.p.x, e.p.y, e.p.z, e.size, 8.0f, e.seed, e.life / e.maxLife, e.inten};
            ps.insert(ps.end(), d8, d8 + 8);
        } else push(e.p, e.size, (float)e.kind, e.seed, e.life / e.maxLife, e.inten);
        i++;
    }
    // --- fumée (alpha prémultiplié, de la plus lointaine à la plus proche)
    if (!ps.empty()) {
        size_t n8 = ps.size() / 8;
        std::vector<size_t> ord(n8);
        for (size_t k = 0; k < n8; k++) ord[k] = k;
        auto dz = [&](size_t k) {
            vec3 p(ps[k * 8], ps[k * 8 + 1], ps[k * 8 + 2]);
            return glm::dot(p - eye, p - eye);
        };
        std::sort(ord.begin(), ord.end(), [&](size_t a2, size_t b2) { return dz(a2) > dz(b2); });
        std::vector<float> so(ps.size());
        for (size_t k = 0; k < n8; k++) std::copy(ps.begin() + ord[k] * 8, ps.begin() + ord[k] * 8 + 8, so.begin() + k * 8);
        glUseProgram(gfx.progPart);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUniformMatrix4fv(uniformLoc(gfx.progPart, "uVP"), 1, GL_FALSE, glm::value_ptr(VP));
        glUniform1f(uniformLoc(gfx.progPart, "uPx"), px);
        glUniform1f(uniformLoc(gfx.progPart, "uTime"), T);
        glUniform3fv(uniformLoc(gfx.progPart, "uFlame"), 13, palFlame);
        glBindVertexArray(gfx.partVao);
        glBindBuffer(GL_ARRAY_BUFFER, gfx.partVbo);
        glBufferData(GL_ARRAY_BUFFER, so.size() * 4, so.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS, 0, (GLsizei)n8);
        glBlendFunc(GL_ONE, GL_ONE);
    }
    // --- tout le reste en additif
    if (!pb.empty()) {
        glUseProgram(gfx.progPart);
        glUniformMatrix4fv(uniformLoc(gfx.progPart, "uVP"), 1, GL_FALSE, glm::value_ptr(VP));
        glUniform1f(uniformLoc(gfx.progPart, "uPx"), px);
        glUniform1f(uniformLoc(gfx.progPart, "uTime"), T);
        glUniform3fv(uniformLoc(gfx.progPart, "uFlame"), 13, palFlame);
        glBindVertexArray(gfx.partVao);
        glBindBuffer(GL_ARRAY_BUFFER, gfx.partVbo);
        glBufferData(GL_ARRAY_BUFFER, pb.size() * 4, pb.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_POINTS, 0, (GLsizei)(pb.size() / 8));
    }
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void Game::renderHeldItems() {
    if (!titleCam) {
        glClear(GL_DEPTH_BUFFER_BIT);  // toujours devant le décor
        glUseProgram(gfx.progScene);
        // animation de la pioche : armement / frappe / récupération (0..1)
        float wA = 0, sA = 0, rA = 0;
        if (swingT >= 0) {
            if (swingT < 0.18f) wA = swingT / 0.18f;
            else {
                wA = 1;
                if (swingT < 0.28f) sA = (swingT - 0.18f) / 0.10f;
                else { sA = 1; rA = std::min(1.0f, (swingT - 0.28f) / 0.36f); }
            }
        }
        float eW = easeOut(wA), eS = easeIn(sA), eR = rA * rA * (3.0f - 2.0f * rA);
        auto ph = [&](float vUp, float vHit) {  // valeur d'un paramètre : vers vUp (armé), vHit (impact), puis retour à 0
            float v = vUp * eW;
            if (sA > 0) v = mixf(vUp, vHit, eS);
            if (rA > 0) v = vHit * (1.0f - eR);
            return v;
        };
        float rx = ph(0.85f, -1.05f), rz = ph(0.30f, -0.12f), ty = ph(0.03f, -0.04f), tz = ph(-0.02f, -0.20f), tx = ph(-0.10f, -0.10f);
        const float strideL = walked * 2.1f;
        float bobX = std::cos(strideL) * 0.006f * bobAmt, bobY = std::sin(strideL * 2.0f) * 0.010f * bobAmt + std::sin(T * 1.3f) * 0.002f;
        mat4 M = glm::inverse(view) * glm::translate(mat4(1), vec3(0.31f + tx + bobX, -0.43f + ty + bobY, -0.60f + tz)) *
                 glm::rotate(mat4(1), 0.14f + rz, vec3(0, 0, 1)) * glm::rotate(mat4(1), -0.50f + rx, vec3(1, 0, 0)) *
                 glm::rotate(mat4(1), 0.45f + PI * 0.5f, vec3(0, 1, 0)) * glm::scale(mat4(1), vec3(0.86f));
        glUniformMatrix4fv(uniformLoc(gfx.progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(M));
        axeMesh.drawAll();
        glUniformMatrix4fv(uniformLoc(gfx.progScene, "uModel"), 1, GL_FALSE, glm::value_ptr(Mlan));
        lanternMesh.drawAll();
    }
    glDisable(GL_DEPTH_TEST);
}

void Game::postProcess() {
    RenderTargets& tg = gfx.targets;
    // --- résolution MSAA
    glBindFramebuffer(GL_READ_FRAMEBUFFER, tg.msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tg.resFbo);
    glBlitFramebuffer(0, 0, fbw, fbh, 0, 0, fbw, fbh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // --- bloom : descente (le premier niveau ne garde que les zones très lumineuses)...
    glBindVertexArray(gfx.quadVao);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_BLEND);
    glUseProgram(gfx.progDown);
    glUniform1i(uniformLoc(gfx.progDown, "uTex"), 0);
    GLuint src = tg.resTex;
    int sw2 = fbw, sh2 = fbh;
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, tg.blFbo[i]);
        glViewport(0, 0, tg.blW[i], tg.blH[i]);
        glBindTexture(GL_TEXTURE_2D, src);
        glUniform2f(uniformLoc(gfx.progDown, "uTexel"), 1.0f / sw2, 1.0f / sh2);
        glUniform1i(uniformLoc(gfx.progDown, "uFirst"), i == 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        src = tg.blTex[i];
        sw2 = tg.blW[i];
        sh2 = tg.blH[i];
    }
    // ... puis remontée en additionnant chaque niveau au précédent
    glUseProgram(gfx.progUp);
    glUniform1i(uniformLoc(gfx.progUp, "uTex"), 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = BLOOM_LEVELS - 2; i >= 0; i--) {
        glBindFramebuffer(GL_FRAMEBUFFER, tg.blFbo[i]);
        glViewport(0, 0, tg.blW[i], tg.blH[i]);
        glBindTexture(GL_TEXTURE_2D, tg.blTex[i + 1]);
        glUniform2f(uniformLoc(gfx.progUp, "uTexel"), 1.0f / tg.blW[i + 1], 1.0f / tg.blH[i + 1]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);
    // --- composition finale à l'écran
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbw, fbh);
    glUseProgram(gfx.progComp);
    glUniform1i(uniformLoc(gfx.progComp, "uHdr"), 0);
    glUniform1i(uniformLoc(gfx.progComp, "uBloom"), 1);
    glUniform1f(uniformLoc(gfx.progComp, "uTime"), T);
    glUniform1f(uniformLoc(gfx.progComp, "uAspect"), aspect);
    glUniform1f(uniformLoc(gfx.progComp, "uBloomK"), 0.55f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tg.resTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tg.blTex[0]);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
}
