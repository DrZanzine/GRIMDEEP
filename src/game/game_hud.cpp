// =============================================================================
//  game/game_hud.cpp — affichage tête haute : cœurs, clés, messages, carte
//
//  Toutes les tailles sont proportionnelles à la hauteur de l'écran (uiSc, uiTs, uiBt)
//  pour que l'interface reste lisible quelle que soit la résolution.
// =============================================================================
#include "game/game.h"

#include <algorithm>

void Game::drawInterface() {
    UiRenderer& ui = gfx.ui;
    ui.begin(fbw, fbh);
    uiW = (float)fbw;
    uiH = (float)fbh;
    uiSc = std::max(2.0f, std::floor(uiH / 300.0f));
    uiCx = uiW * 0.5f;
    uiCy = uiH * 0.5f;
    uiTs = std::max(2.0f, std::floor(uiH / 330.0f));
    uiBt = std::max(2.0f, std::floor(uiH / 270.0f));
    {  // souris (convertie en pixels du framebuffer)
        int ww, wh;
        glfwGetWindowSize(win, &ww, &wh);
        double mxw, myw;
        glfwGetCursorPos(win, &mxw, &myw);
        float nx = (float)(mxw * uiW / std::max(ww, 1)), ny = (float)(myw * uiH / std::max(wh, 1));
        ms.moved = std::fabs(nx - ms.x) + std::fabs(ny - ms.y) > 0.5f;
        ms.x = nx;
        ms.y = ny;
        bool md = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        ms.click = md && !ms.down;
        ms.down = md;
        if (!md) dragId = -1;
    }
    btnCount = 0;
    if (!titleCam) drawHud();
    // écrans de menu (tests successifs : un bouton peut changer d'écran pendant l'image)
    if (screen == SC_TITLE) drawTitleScreen();
    if (screen == SC_PAUSE) drawPauseScreen();
    if (screen == SC_SETTINGS) drawSettingsScreen();
    if (screen == SC_CONTROLS) drawControlsScreen();
    menuCountPrev = std::max(btnCount, 1);
    uiEnter = false;
    uiLeft = false;
    uiRight = false;
    ui.flush();
}

void Game::drawKeyIcon(float x, float y, float s, bool gold) {
    UiRenderer& ui = gfx.ui;
    float r = gold ? 1.0f : 0.25f, g = gold ? 0.78f : 0.2f, b = gold ? 0.18f : 0.15f, a = gold ? 1.0f : 0.7f;  // clé grise si pas encore trouvée
    ui.disc(x + 3 * s, y + 3 * s, 3.2f * s, 0.05f, 0.03f, 0.0f, 0.8f);
    ui.disc(x + 3 * s, y + 3 * s, 2.5f * s, r, g, b, a);
    ui.disc(x + 3 * s, y + 3 * s, 1.0f * s, 0.1f, 0.06f, 0.02f, 1.0f);
    ui.rect(x + 5 * s, y + 2.2f * s, 9 * s, 1.6f * s, r, g, b, a);
    ui.rect(x + 11 * s, y + 3.8f * s, 1.6f * s, 2.4f * s, r, g, b, a);
    ui.rect(x + 13.6f * s, y + 3.8f * s, 1.6f * s, 3.0f * s, r, g, b, a);
}

void Game::drawVignette(float a, float r, float g, float b, float k) {
    UiRenderer& ui = gfx.ui;
    const float c0[4] = {r, g, b, a}, c1[4] = {r, g, b, 0.0f};
    float th = uiH * 0.30f * k, tw = uiW * 0.20f * k;
    ui.rectGrad(0, 0, uiW, th, c0, c0, c1, c1);
    ui.rectGrad(0, uiH - th, uiW, th, c1, c1, c0, c0);
    ui.rectGrad(0, 0, tw, uiH, c0, c1, c1, c0);
    ui.rectGrad(uiW - tw, 0, tw, uiH, c1, c0, c0, c1);
}

void Game::drawHud() {
    UiRenderer& ui = gfx.ui;
    const float fw = uiW, fh = uiH, sc = uiSc, cx = uiCx, cy = uiCy;
    // --- 10 cœurs = 100 PV (ils tremblent quand la vie est basse)
    float hs = std::max(3.0f, std::floor(fh / 190.0f)), gap = hs * 1.6f, x0 = fh * 0.035f, y0 = fh * 0.035f;
    for (int i = 0; i < 10; i++) {
        int fill = hp >= (i + 1) * 10 - 0.01f ? 2 : (hp >= i * 10 + 5 - 0.01f ? 1 : 0);
        float bob = (hp < 30.0f && fill > 0 && live) ? std::sin(T * 10.0f + i) * hs * 0.3f : 0.0f;
        ui.heart(x0 + i * (7 * hs + gap), y0 + bob, hs, fill);
    }
    // --- clés, ennemis tués, nom du biome, message, invite « E »
    float ky = y0 + 7 * hs + hs * 2.4f;
    for (int k = 0; k < world.keysNeeded; k++) drawKeyIcon(x0 + k * hs * 6.0f, ky, hs * 0.55f, k < keys);
    ui.text(x0, ky + hs * 4.4f, sc * 0.8f, "ENNEMIS TUES " + std::to_string(kills), 0.95f, 0.8f, 0.4f, 0.9f);
    if (live && biomeBanner > 0) {
        std::string n = BIOMES[std::min(6, std::max(0, curBiome))].name;
        float a = std::min(1.0f, biomeBanner);
        ui.text(cx - ui.textWidth(n, sc * 1.6f) * 0.5f, fh * 0.12f, sc * 1.6f, n, 0.95f, 0.88f, 0.7f, a);
    }
    if (msgT > 0) ui.text(cx - ui.textWidth(msg, sc) * 0.5f, fh * 0.72f, sc, msg, 1, 0.9f, 0.6f, std::min(1.0f, msgT));
    else if (state == PLAY && live && !world.doorOpen && glm::length(pos - world.doorPos) < 3.6f) {
        std::string h = "E : OUVRIR LA PORTE";
        ui.text(cx - ui.textWidth(h, sc) * 0.5f, fh * 0.72f, sc, h, 1, 1, 1, 0.9f);
    }
    ui.rect(cx - 3.5f, cy - 3.5f, 7, 7, 0, 0, 0, 0.55f);  // réticule
    ui.rect(cx - 2, cy - 2, 4, 4, 1, 1, 1, 0.95f);
    // --- effets d'écran : flash d'explosion, coup reçu (rouge sur le pourtour), vie basse (pulsation + battement de cœur)
    if (flashW > 0.001f) {
        ui.rect(0, 0, fw, fh, 1.0f, 0.93f, 0.8f, std::min(0.95f, flashW));
        drawVignette(std::min(0.9f, flashW), 1.0f, 0.45f, 0.1f, 1.2f);
    }
    if (hurtFlash > 0) {
        drawVignette(std::min(0.92f, hurtFlash * 1.6f), 0.85f, 0.02f, 0.02f, 1.0f);
        ui.rect(0, 0, fw, fh, 0.8f, 0.03f, 0.02f, std::min(0.22f, hurtFlash * 0.3f));
    }
    if (hp < 30 && state == PLAY) {
        drawVignette(0.30f + 0.18f * std::sin(T * 6.0f), 0.6f, 0.0f, 0.0f, 0.8f);
        if (live) {
            heartT -= rdt;
            if (heartT <= 0) {
                audio.sfx(aud::SF_HEART, 0.8f);
                heartT = 0.9f;
            }
        }
    }
    drawMinimap();
    // --- écrans de fin
    if (state == DEAD) {
        ui.rect(0, 0, fw, fh, 0.02f, 0.0f, 0.0f, 0.72f);
        std::string a = "VOUS ETES MORT", b = "ENTREE : RETOUR AU CAMP   -   R : NOUVELLE GROTTE";
        ui.text(cx - ui.textWidth(a, sc * 3) * 0.5f, cy - 40 * sc, sc * 3, a, 0.9f, 0.1f, 0.1f, 1);
        ui.text(cx - ui.textWidth(b, sc) * 0.5f, cy + 10 * sc, sc, b, 1, 0.9f, 0.8f, 1);
    } else if (state == WIN) {
        ui.rect(0, 0, fw, fh, 0.9f, 0.95f, 1.0f, 0.55f);
        std::string a = "VOUS ETES SORTI !", b = "TEMPS " + std::to_string((int)playTime) + " S   ENNEMIS TUES " + std::to_string(kills),
                    c = "ENTREE : REJOUER   -   R : NOUVELLE GROTTE";
        ui.text(cx - ui.textWidth(a, sc * 3) * 0.5f, cy - 40 * sc, sc * 3, a, 0.1f, 0.12f, 0.2f, 1);
        ui.text(cx - ui.textWidth(b, sc) * 0.5f, cy + 8 * sc, sc, b, 0.1f, 0.12f, 0.2f, 1);
        ui.text(cx - ui.textWidth(c, sc) * 0.5f, cy + 24 * sc, sc, c, 0.1f, 0.12f, 0.2f, 1);
    }
}

void Game::drawMinimap() {
    // Carte de 40 m de rayon centrée sur le joueur, qui tourne avec le regard (le haut = devant).
    UiRenderer& ui = gfx.ui;
    const float fw = uiW, fh = uiH, sc = uiSc;
    float msz = std::max(190.0f, std::min(fh * 0.30f, 380.0f)), mx_ = fw - msz - fh * 0.03f, my_ = fh - msz - fh * 0.05f;
    float half = msz * 0.5f, mcx = mx_ + half, mcy = my_ + half;
    float k = half / 40.0f;  // pixels par mètre
    auto toMap = [&](float wx, float wz) {  // monde (x, z) -> écran
        float dx = wx - pos.x, dz = wz - pos.z;
        return vec2(mcx + (dx * rightv.x + dz * rightv.z) * k, mcy - (dx * flat.x + dz * flat.z) * k);
    };
    ui.rect(mx_ - 5, my_ - 5, msz + 10, msz + 10, 0.0f, 0.0f, 0.0f, 0.55f);
    ui.rect(mx_, my_, msz, msz, 0.04f, 0.045f, 0.06f, 0.82f);
    ui.flush();  // le contenu de la carte est découpé au rectangle (flushClip plus bas)
    // camp : montagne, cases découvertes
    {
        vec2 a = toMap(-90.0f, world.hubFrontZ), b = toMap(90.0f, world.hubFrontZ), c = toMap(90.0f, world.hubFrontZ - 70.0f), d = toMap(-90.0f, world.hubFrontZ - 70.0f);
        ui.quad(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y, 0.20f, 0.19f, 0.21f, 0.95f);
        for (int cz = 0; cz < 60; cz++)
            for (int cxx = 0; cxx < 80; cxx++)
                if (hubSeen[(size_t)cz * 80 + cxx]) {
                    float wx = -160.0f + cxx * 4.0f, wz = world.hubFrontZ - 8.0f + cz * 4.0f;
                    if (std::fabs(wx - pos.x) > 60 || std::fabs(wz - pos.z) > 60) continue;
                    vec2 p0 = toMap(wx, wz), p1 = toMap(wx + 4, wz), p2 = toMap(wx + 4, wz + 4), p3 = toMap(wx, wz + 4);
                    ui.quad(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, 0.16f, 0.30f, 0.13f, 0.95f);
                }
    }
    // galeries découvertes : sol + bords
    for (size_t t = 0; t < world.tubes.size(); t++) {
        const Tube& tb = world.tubes[t];
        if (mapSeen[t].size() != (size_t)tb.N) continue;
        for (int i = 0; i + 1 < tb.N; i++) {
            if (!mapSeen[t][i] || !mapSeen[t][i + 1]) continue;
            if (std::fabs(tb.rings[i].c.x - pos.x) > 60.0f || std::fabs(tb.rings[i].c.z - pos.z) > 60.0f) continue;
            vec4 e0 = mapEdge[t][i], e1 = mapEdge[t][i + 1];
            vec2 l0 = toMap(e0.x, e0.y), r0 = toMap(e0.z, e0.w), l1 = toMap(e1.x, e1.y), r1 = toMap(e1.z, e1.w);
            ui.quad(l0.x, l0.y, r0.x, r0.y, r1.x, r1.y, l1.x, l1.y, 0.20f, 0.24f, 0.30f, 1.0f);
            ui.line(l0.x, l0.y, l1.x, l1.y, 2.0f, 0.62f, 0.78f, 0.90f, 1.0f);
            ui.line(r0.x, r0.y, r1.x, r1.y, 2.0f, 0.62f, 0.78f, 0.90f, 1.0f);
        }
    }
    // porte, sortie, clés (toujours visibles pour guider le joueur ; hors carte : collés au bord, avec une flèche)
    auto marker = [&](vec3 wp, auto draw, bool arrow) {
        vec2 m = toMap(wp.x, wp.z);
        float dx = m.x - mcx, dy = m.y - mcy, lim = half - 12.0f, mx2 = std::max(std::fabs(dx), std::fabs(dy));
        if (mx2 > lim) {
            float f = lim / mx2;
            float ox = dx, oy = dy;
            m = vec2(mcx + dx * f, mcy + dy * f);
            if (arrow) {
                float l = std::sqrt(ox * ox + oy * oy);
                float ux = ox / l, uy = oy / l;
                ui.tri(m.x + ux * 9, m.y + uy * 9, m.x - uy * 6 - ux * 3, m.y + ux * 6 - uy * 3, m.x + uy * 6 - ux * 3, m.y - ux * 6 - uy * 3, 1.0f, 0.82f, 0.2f, 0.95f);
            }
        }
        draw(m);
    };
    for (auto& pk : world.pickups)
        if (pk.alive && pk.kind == 0)
            marker(pk.p, [&](vec2 m) {
                float pu = 0.5f + 0.5f * std::sin(T * 4.0f + pk.phase);
                ui.disc(m.x, m.y, 9.0f + 3.0f * pu, 1.0f, 0.78f, 0.15f, 0.22f);
                drawKeyIcon(m.x - 7, m.y - 4, 1.0f, true);
            }, true);
    if (!world.doorOpen)
        marker(world.doorPos, [&](vec2 m) {
            ui.rect(m.x - 6, m.y - 6, 12, 12, 0.0f, 0.0f, 0.0f, 0.9f);
            ui.rect(m.x - 4.5f, m.y - 4.5f, 9, 9, 0.62f, 0.38f, 0.16f, 1.0f);
            ui.rect(m.x - 1, m.y - 1, 2, 2, 0.1f, 0.05f, 0.0f, 1.0f);
        }, false);
    else
        marker(world.exitPos, [&](vec2 m) {
            ui.disc(m.x, m.y, 7.0f, 0.55f, 0.85f, 1.0f, 0.95f);
            ui.disc(m.x, m.y, 3.5f, 1.0f, 1.0f, 1.0f, 1.0f);
        }, true);
    // joueur (flèche toujours vers le haut)
    ui.tri(mcx, mcy - 10, mcx - 6.5f, mcy + 7, mcx + 6.5f, mcy + 7, 0.0f, 0.0f, 0.0f, 0.9f);
    ui.tri(mcx, mcy - 7.5f, mcx - 4.5f, mcy + 5, mcx + 4.5f, mcy + 5, 1.0f, 1.0f, 1.0f, 1.0f);
    ui.flushClip(mx_, my_, msz, msz);
    // cadre, boussole (nord = -Z), légende
    ui.rect(mx_ - 5, my_ - 5, msz + 10, 5, 0.80f, 0.52f, 0.16f, 1.0f);
    ui.rect(mx_ - 5, my_ + msz, msz + 10, 5, 0.80f, 0.52f, 0.16f, 1.0f);
    ui.rect(mx_ - 5, my_, 5, msz, 0.80f, 0.52f, 0.16f, 1.0f);
    ui.rect(mx_ + msz, my_, 5, msz, 0.80f, 0.52f, 0.16f, 1.0f);
    {
        vec2 n = toMap(pos.x, pos.z - 100.0f);
        float dx = n.x - mcx, dy = n.y - mcy, l = std::sqrt(dx * dx + dy * dy);
        float nx = mcx + dx / l * (half - 11), ny = mcy + dy / l * (half - 11);
        ui.disc(nx, ny, 9.0f, 0.0f, 0.0f, 0.0f, 0.7f);
        ui.text(nx - 2.5f * sc * 0.5f * 1.4f, ny - 3.5f * sc * 0.5f * 1.4f, sc * 0.5f * 1.4f, "N", 1.0f, 0.5f, 0.3f, 1.0f);
    }
    ui.text(mx_, my_ - 8 * sc * 0.7f, sc * 0.7f, "CARTE", 0.95f, 0.8f, 0.45f, 0.95f);
}
