// =============================================================================
//  game/game_player.cpp — entrées, déplacement du joueur, caméra, objets à ramasser
//
//  Le joueur est un point « à hauteur des yeux » (1,65 m au-dessus du sol). Son
//  déplacement est lissé (accélération / décélération progressives), contraint par
//  les galeries (World::constrain) et repoussé par les planches (World::collideBoards).
//  La caméra ajoute le balancement de la marche, le saut et les secousses.
// =============================================================================
#include "game/game.h"

#include <algorithm>

// ---------------------------------------------------------------------------
//  Entrées
// ---------------------------------------------------------------------------

void Game::handleSystemKeys() {
    // Echap : pause / retour
    bool kEsc = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (kEsc && !keyEsc) {
        if (screen == SC_PLAY) { screen = SC_PAUSE; audio.sfx(aud::SF_CLICK, 0.6f); }
        else if (screen == SC_PAUSE) { screen = SC_PLAY; audio.sfx(aud::SF_CLICK, 0.6f, 1); }
        else if (screen == SC_SETTINGS || screen == SC_CONTROLS) {
            screen = backTo;
            saveSettings(cfg);
            audio.sfx(aud::SF_CLICK, 0.6f);
            menuSel = 0;
        }
    }
    keyEsc = kEsc;
    // F11 : plein écran
    bool k11 = glfwGetKey(win, GLFW_KEY_F11) == GLFW_PRESS;
    if (k11 && !keyF11) cfg.fullscreen = !cfg.fullscreen;
    keyF11 = k11;
    // flèches / Entrée : navigation dans les menus
    bool u = glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS, d = glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS;
    bool l = glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS, r = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool ret = glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    uiEnter = ret && !keyRet && screen != SC_PLAY;
    uiLeft = l && !keyLf && screen != SC_PLAY;
    uiRight = r && !keyRt && screen != SC_PLAY;
    if (screen != SC_PLAY) {
        if (d && !keyDn) { menuSel = (menuSel + 1) % std::max(menuCountPrev, 1); audio.sfx(aud::SF_CLICK, 0.35f, 1); }
        if (u && !keyUp) { menuSel = (menuSel + std::max(menuCountPrev, 1) - 1) % std::max(menuCountPrev, 1); audio.sfx(aud::SF_CLICK, 0.35f, 1); }
    }
    keyUp = u; keyDn = d; keyLf = l; keyRt = r; keyRet = ret;
    if (cfg.fullscreen != (glfwGetWindowMonitor(win) != nullptr) && opt.shot.empty()) applyFullscreen(win, cfg.fullscreen, wst);
}

void Game::handleGameplayInput() {
    // --- souris : orientation du regard
    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    if (firstMouse) { mx0 = mx; my0 = my; firstMouse = false; }
    if (opt.shot.empty() && state == PLAY && live) {
        yaw += (float)(mx - mx0) * 0.0023f * cfg.sens;
        pitch -= (float)(my - my0) * 0.0023f * cfg.sens;
    }
    mx0 = mx;
    my0 = my;
    pitch = std::max(-1.5f, std::min(1.5f, pitch));
    front = vec3(std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch));
    flat = glm::normalize(vec3(std::cos(yaw), 0, std::sin(yaw)));
    rightv = glm::normalize(glm::cross(flat, vec3(0, 1, 0)));

    // --- touches
    bool kF = glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS;  // F : marche / vol libre
    if (kF && !keyF && live) fly = !fly;
    keyF = kF;
    bool kR = glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS;  // R : nouvelle grotte
    if (kR && !keyR && live) {
        restart(true);
        for (int s = 0; s < SHADOW_SLOTS; s++) slotLight[s] = -1;
        rats.clear();
    }
    keyR = kR;
    bool kEn = glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS;  // Entrée : recommencer après la mort / la victoire
    if (kEn && !keyEnter && state != PLAY && live) {
        restart(false);
        rats.clear();
    }
    keyEnter = kEn;
    bool mL = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;  // clic : coup de pioche
    if (mL && !mouseL && swingT < 0 && state == PLAY && live) {
        swingT = 0;
        swingHit = false;
        audio.sfx(aud::SF_SWING, 0.7f);
    }
    mouseL = mL;
    bool kE = glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS || (opt.autoE && frame == 3);  // E : ouvrir la grande porte
    if (kE && !keyE && state == PLAY && live && !world.doorOpen && glm::length(pos - world.doorPos) < 3.6f) {
        if (keys >= world.keysNeeded) {
            world.openDoor();
            audio.sfx(aud::SF_DOOR, 1.0f);
            say("LA PORTE S'OUVRE...");
            puff(world.doorPos, 18, 1.5f);
        } else say("IL FAUT " + std::to_string(world.keysNeeded) + " CLES (" + std::to_string(keys) + "/" + std::to_string(world.keysNeeded) + ")");
    }
    keyE = kE;
}

// ---------------------------------------------------------------------------
//  Déplacement
// ---------------------------------------------------------------------------

void Game::updateMovement() {
    // --- direction voulue (ZQSD / WASD / flèches ; Espace / Ctrl en vol libre)
    vec3 mv(0);
    vec3 fw = fly ? front : flat;
    if (state == PLAY && live) {
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) mv += fw;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) mv -= fw;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) mv += rightv;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS) mv -= rightv;
        if (fly) {
            if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) mv += vec3(0, 1, 0);
            if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) mv -= vec3(0, 1, 0);
        }
    }
    if (opt.autoWalk) mv = fw;

    // --- déplacement lissé : accélération / décélération progressives (moins de contrôle en l'air)
    bool wantMove = glm::length(mv) > 0, shiftDown = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    bool sprint = wantMove && !fly && shiftDown, air = jumpOff > 0.0f;
    float speed = fly ? (shiftDown ? 9.0f : 4.5f) : (sprint ? 7.2f : 3.4f);
    vec3 wish = wantMove ? glm::normalize(mv) * speed : vec3(0);
    pvel += (wish - pvel) * (1.0f - std::exp(-(wantMove ? (sprint ? 13.0f : 11.0f) : 8.5f) * (air ? 0.35f : 1.0f) * dt));
    if (!fly) pvel.y = 0;

    // --- saut (Espace) : décalage vertical de la caméra, gravité 15 m/s²
    {
        bool kSp = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (kSp && !keySpace && !fly && !air && jumpV == 0.0f && state == PLAY && live) {
            jumpV = 5.2f;
            audio.sfx(aud::SF_STEP, 0.32f, world.inHub(pos) ? 1 : 0);
        }
        keySpace = kSp;
        if (fly) { jumpOff = 0; jumpV = 0; }
        else if (jumpOff > 0.0f || jumpV != 0.0f) {
            jumpV -= 15.0f * dt;
            jumpOff = std::min(0.95f, jumpOff + jumpV * dt);
            if (jumpOff <= 0.0f) {  // atterrissage
                jumpOff = 0.0f;
                if (jumpV < -2.5f) {
                    audio.sfx(aud::SF_STEP, 0.5f, world.inHub(pos) ? 1 : 0);
                    camShake = std::max(camShake, 0.035f);
                }
                jumpV = 0.0f;
            }
        }
    }

    // --- application du mouvement et collisions
    vec3 stepv = pvel * dt;
    float stepLen = glm::length(stepv);
    if (!fly && !air) walked += stepLen;
    pos += stepv;
    vec3 afterMove = pos;
    if (!opt.noclip) world.collideBoards(pos, fly);
    vec3 boardCorr = pos - afterMove;
    if (!opt.noclip) {
        vec3 c = world.constrain(pos, fly, hints);
        if (fly) pos = c;
        else {  // la caméra suit le relief vite (plus vite en montée), sans tirer le joueur en arrière
            pos.x = c.x;
            pos.z = c.z;
            if (groundEye < -1e8f || std::fabs(c.y - groundEye) > 2.0f) groundEye = c.y;
            groundEye += (c.y - groundEye) * std::min(1.0f, dt * (c.y > groundEye ? 24.0f : 16.0f));
            pos.y = groundEye + jumpOff;
        }
    }
    {  // glissement le long des obstacles : seules les vraies collisions freinent
        auto slide = [&](vec3 corr, float minLen) {
            corr.y = 0;
            float cl = glm::length(corr);
            if (cl > minLen) {
                vec3 n = corr / cl;
                float vn = glm::dot(pvel, n);
                if (vn < 0) pvel -= n * vn;  // on retire la composante qui pousse dans l'obstacle
            }
        };
        slide(boardCorr, 1e-4f);
        slide(pos - afterMove - boardCorr, std::max(0.004f, 0.4f * stepLen));
    }

    // --- course, balancement de la tête, bruits de pas
    float vlen = glm::length(vec3(pvel.x, 0, pvel.z));
    sprintAmt += ((sprint && vlen > 4.0f ? 1.0f : 0.0f) - sprintAmt) * (1.0f - std::exp(-6.0f * dt));
    bobAmt += (std::min(1.0f, vlen / 3.0f) - bobAmt) * (1.0f - std::exp(-8.0f * dt));
    const float stride = walked * 2.1f;
    {
        int si = (int)std::floor(stride / PI);  // un pas tous les pi radians de foulée
        if (si != lastStepIndex) {
            lastStepIndex = si;
            if (bobAmt > 0.35f && !fly && state == PLAY && live) audio.sfx(aud::SF_STEP, sprint ? 0.42f : 0.28f, world.inHub(pos) ? 1 : 0);
        }
    }
    eye = pos;
    if (!fly && jumpOff <= 0.0f)
        eye += vec3(0, std::sin(stride * 2.0f) * (0.026f + 0.012f * sprintAmt) * bobAmt + std::sin(T * 1.3f) * 0.004f, 0) + rightv * (std::cos(stride) * 0.014f * bobAmt);
    camShake = std::max(0.0f, camShake - dt * 0.9f);
    if (camShake > 0.001f) eye += vec3(prng.r(-1, 1), prng.r(-1, 1), prng.r(-1, 1)) * camShake * 0.11f;
}

void Game::revealMap() {
    // galeries : anneaux à moins de 12 m (on mémorise les bords gauche / droit pour la carte)
    for (size_t t = 0; t < world.tubes.size(); t++) {
        const Tube& tb = world.tubes[t];
        if (mapSeen[t].size() != (size_t)tb.N) continue;
        for (int i = 0; i < tb.N; i++) {
            if (mapSeen[t][i]) continue;
            vec3 d = tb.rings[i].c - pos;
            if (glm::dot(d, d) > 144.0f) continue;
            mapSeen[t][i] = 1;
            float xl, xr;
            const Ring& r = tb.rings[i];
            if (!tb.wallRange(i, tb.floorY(i, 0.0f) + 1.2f, xl, xr)) { xl = -r.w * 0.8f; xr = r.w * 0.8f; }
            vec3 L = r.c + r.R * xl, Rr = r.c + r.R * xr;
            mapEdge[t][i] = vec4(L.x, L.z, Rr.x, Rr.z);
        }
    }
    // camp : cases de 4 m à moins de 16 m
    if (world.inHub(pos))
        for (int cz = 0; cz < 60; cz++)
            for (int cxx = 0; cxx < 80; cxx++) {
                float wx = -160.0f + (cxx + 0.5f) * 4.0f, wz = world.hubFrontZ - 8.0f + (cz + 0.5f) * 4.0f;
                if ((wx - pos.x) * (wx - pos.x) + (wz - pos.z) * (wz - pos.z) < 256.0f) hubSeen[(size_t)cz * 80 + cxx] = 1;
            }
}

void Game::updateTitleCamera() {
    // plan large cinématique : la montagne, ses cristaux géants et l'entrée de la mine
    float tt = (float)menuT;
    eye = vec3(26.0f * std::sin(tt * 0.085f), world.hubGroundY + 5.5f + 1.6f * std::sin(tt * 0.13f), world.hubFrontZ + 62.0f + 9.0f * std::cos(tt * 0.07f));
    vec3 tgt(9.0f * std::sin(tt * 0.05f + 1.0f), world.hubGroundY + 26.0f + 3.0f * std::sin(tt * 0.09f), world.hubFrontZ - 8.0f);
    front = glm::normalize(tgt - eye);
}

void Game::updatePickaxe() {
    // pioche : armement (0-0,18 s), frappe (0,18-0,28 s), récupération ; le coup porte à 0,26 s
    if (opt.forceAxe) swingT = opt.forceSwing;
    if (opt.forceBreak && (frame == 3 || frame == 8 || frame == 13 || frame == 18)) axeHit(eye, front);
    if (swingT >= 0 && !opt.forceAxe) {
        swingT += dt;
        if (!swingHit && swingT > 0.26f) {
            swingHit = true;
            axeHit(eye, front);
        }
        if (swingT > 0.64f) swingT = -1.0f;
    }
    // la caméra accompagne le geste (léger relevé puis plongée)
    float a = swingT, k = 0.0f;
    if (a >= 0) {
        if (a < 0.18f) k = 0.025f * (a / 0.18f);
        else if (a < 0.3f) k = mixf(0.025f, -0.06f, (a - 0.18f) / 0.12f);
        else k = mixf(-0.06f, 0.0f, std::min(1.0f, (a - 0.3f) / 0.3f));
    }
    camKick += (k - camKick) * (1.0f - std::exp(-28.0f * dt));
}

void Game::updateBiome() {
    float bf = world.biomeAtPos(pos);
    int bNow = (int)std::floor(bf + 0.5f);
    if (bNow != curBiome) {  // nouveau biome : bannière et musique
        curBiome = bNow;
        biomeBanner = 4.0f;
        audio.setBiome(bNow);
    }
    biomeBanner = std::max(0.0f, biomeBanner - dt);
    msgT = std::max(0.0f, msgT - dt);
    hurtFlash = std::max(0.0f, hurtFlash - dt);
}

void Game::updatePickups() {
    for (auto& p : world.pickups)
        if (p.alive && glm::length(vec3(p.p.x, 0, p.p.z) - vec3(pos.x, 0, pos.z)) < 1.3f && std::fabs(p.p.y - (pos.y - 1.0f)) < 1.8f) {
            if (p.kind == 0) {
                p.alive = false;
                keys++;
                audio.sfx(aud::SF_PICKUP, 1.0f);
                say("CLE TROUVEE  " + std::to_string(keys) + "/" + std::to_string(world.keysNeeded));
            } else if (hp < 100) {  // une fiole n'est ramassée que si l'on est blessé
                p.alive = false;
                hp = std::min(100.0f, hp + 35.0f);
                audio.sfx(aud::SF_PICKUP, 0.8f);
                say("+35 PV");
            }
        }
    // sortie : porte ouverte et lumière du jour atteinte
    if (state == PLAY && world.doorOpen && glm::length(pos - world.exitPos) < 4.0f) {
        state = WIN;
        audio.sfx(aud::SF_PICKUP, 1.0f);
    }
}
