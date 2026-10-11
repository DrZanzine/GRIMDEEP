// =============================================================================
//  game/game_menus.cpp — écrans de menu : titre, pause, paramètres, commandes
//
//  Les widgets (bouton, curseur, interrupteur) sont « immédiats » : ils sont dessinés
//  et testés à chaque image. Chacun reçoit un numéro dans l'ordre de dessin ; le
//  widget sélectionné (menuSel) peut être choisi à la souris ou aux flèches, et
//  activé par un clic ou la touche Entrée.
// =============================================================================
#include "game/game.h"

#include <algorithm>
#include <cstdio>

// ---------------------------------------------------------------------------
//  Widgets
// ---------------------------------------------------------------------------

bool Game::mouseInside(float x, float y, float w, float h) const { return ms.x >= x && ms.x <= x + w && ms.y >= y && ms.y <= y + h; }

void Game::drawPanel(float x, float y, float w, float h, float a) {
    UiRenderer& ui = gfx.ui;
    const float t0[4] = {0.17f, 0.10f, 0.06f, 0.96f * a}, b0[4] = {0.07f, 0.04f, 0.03f, 0.97f * a};
    ui.rect(x - 7, y - 7, w + 14, h + 14, 0.0f, 0.0f, 0.0f, 0.55f * a);   // ombre
    ui.rect(x - 4, y - 4, w + 8, h + 8, 0.80f, 0.52f, 0.16f, a);           // liseré doré
    ui.rect(x - 2, y - 2, w + 4, h + 4, 0.10f, 0.05f, 0.02f, a);
    ui.rectGrad(x, y, w, h, t0, t0, b0, b0);                               // fond en dégradé
}

bool Game::menuButton(const std::string& label, float x, float y, float w, float h) {
    UiRenderer& ui = gfx.ui;
    const float bt = uiBt;
    int id = btnCount++;
    bool in = mouseInside(x, y, w, h);
    if (in && ms.moved) menuSel = id;  // survol = sélection
    bool hot = (menuSel == id);
    if (hot) ui.rect(x - 8, y - 8, w + 16, h + 16, 1.0f, 0.55f, 0.15f, 0.20f + 0.10f * std::sin(T * 6.0f));  // halo pulsant
    ui.rect(x - 3, y - 3, w + 6, h + 6, 0.02f, 0.01f, 0.0f, 0.92f);
    const float ta[4] = {0.52f, 0.28f, 0.08f, 0.96f}, ba[4] = {0.32f, 0.15f, 0.04f, 0.96f}, tb[4] = {0.20f, 0.12f, 0.07f, 0.92f}, bb[4] = {0.11f, 0.065f, 0.04f, 0.92f};
    ui.rectGrad(x, y, w, h, hot ? ta : tb, hot ? ta : tb, hot ? ba : bb, hot ? ba : bb);
    ui.rect(x, y, w, 3, 1.0f, 0.82f, 0.45f, hot ? 0.9f : 0.3f);
    ui.rect(x, y + h - 3, w, 3, 0, 0, 0, 0.45f);
    float tw = ui.textWidth(label, bt) - bt;  // texte centré, avec une ombre portée
    ui.text(x + (w - tw) * 0.5f + bt * 0.5f, y + (h - 7 * bt) * 0.5f + bt * 0.5f, bt, label, 0, 0, 0, 0.55f);
    ui.text(x + (w - tw) * 0.5f, y + (h - 7 * bt) * 0.5f, bt, label, hot ? 1.0f : 0.92f, hot ? 0.95f : 0.78f, hot ? 0.72f : 0.55f, 1.0f);
    bool act = (in && ms.click) || (hot && uiEnter);
    if (act) {
        audio.sfx(aud::SF_CLICK, 0.8f);
        uiEnter = false;  // un seul widget activé par appui
        ms.click = false;
    }
    return act;
}

void Game::menuSlider(const char* label, float& v, float lo, float hi, float step, float x, float y, float w, float h, bool pct) {
    UiRenderer& ui = gfx.ui;
    const float bt = uiBt;
    int id = btnCount++;
    bool in = mouseInside(x - 10, y - 6, w + 20, h + 12);
    if (in && ms.moved) menuSel = id;
    bool hot = (menuSel == id);
    float tx = x + w * 0.46f, tw = w * 0.40f;  // piste
    bool onTrack = mouseInside(tx - 14, y - 8, tw + 28, h + 16);
    if (ms.click && onTrack) { dragId = id; menuSel = id; }
    if (dragId == id && ms.down) v = lo + (hi - lo) * std::min(1.0f, std::max(0.0f, (ms.x - tx) / tw));  // glisser
    if (hot) {  // flèches gauche / droite
        if (uiLeft) v = std::max(lo, v - step);
        if (uiRight) v = std::min(hi, v + step);
    }
    if (hot) ui.rect(x - 12, y - 8, w + 24, h + 16, 1.0f, 0.55f, 0.15f, 0.14f);
    ui.text(x, y + (h - 7 * bt) * 0.5f, bt, label, 1.0f, 0.88f, 0.62f, 1.0f);
    float f = (v - lo) / (hi - lo), th = h * 0.34f, ty = y + (h - th) * 0.5f;
    ui.rect(tx - 2, ty - 2, tw + 4, th + 4, 0.0f, 0.0f, 0.0f, 0.8f);
    ui.rect(tx, ty, tw, th, 0.16f, 0.09f, 0.05f, 1.0f);
    const float fa[4] = {1.0f, 0.78f, 0.28f, 1.0f}, fb[4] = {0.85f, 0.38f, 0.07f, 1.0f};
    ui.rectGrad(tx, ty, tw * f, th, fa, fa, fb, fb);
    ui.rect(tx + tw * f - 5, y + 2, 10, h - 4, 0.0f, 0.0f, 0.0f, 0.9f);  // poignée
    ui.rect(tx + tw * f - 3, y + 4, 6, h - 8, 1.0f, 0.92f, 0.7f, 1.0f);
    char buf[24];
    if (pct) std::snprintf(buf, sizeof buf, "%d%%", (int)std::lround(v * 100.0f));
    else std::snprintf(buf, sizeof buf, "%.1fX", v);
    ui.text(tx + tw + 22, y + (h - 7 * bt) * 0.5f, bt, buf, 1.0f, 0.95f, 0.8f, 1.0f);
}

void Game::menuToggle(const char* label, bool& v, float x, float y, float w, float h) {
    UiRenderer& ui = gfx.ui;
    const float bt = uiBt;
    int id = btnCount++;
    bool in = mouseInside(x - 10, y - 6, w + 20, h + 12);
    if (in && ms.moved) menuSel = id;
    bool hot = (menuSel == id);
    if (hot) ui.rect(x - 12, y - 8, w + 24, h + 16, 1.0f, 0.55f, 0.15f, 0.14f);
    ui.text(x, y + (h - 7 * bt) * 0.5f, bt, label, 1.0f, 0.88f, 0.62f, 1.0f);
    float bx = x + w * 0.46f, bw = h * 1.9f;
    ui.rect(bx - 2, y + 2, bw + 4, h - 4, 0, 0, 0, 0.8f);
    ui.rect(bx, y + 4, bw, h - 8, v ? 0.75f : 0.2f, v ? 0.4f : 0.12f, v ? 0.08f : 0.08f, 1.0f);
    float kx = v ? bx + bw - (h - 8) : bx;  // bouton à droite = OUI
    ui.rect(kx, y + 4, h - 8, h - 8, 1.0f, 0.92f, 0.7f, 1.0f);
    ui.text(bx + bw + 22, y + (h - 7 * bt) * 0.5f, bt, v ? "OUI" : "NON", 1.0f, 0.95f, 0.8f, 1.0f);
    if ((in && ms.click) || (hot && (uiEnter || uiLeft || uiRight))) {
        v = !v;
        audio.sfx(aud::SF_CLICK, 0.8f, 1);
    }
}

// ---------------------------------------------------------------------------
//  Écrans
// ---------------------------------------------------------------------------

void Game::drawLogo(float yTop, float scl) {
    UiRenderer& ui = gfx.ui;
    const float cx = uiCx;
    const std::string name = "GRIMDEEP";
    float w = ui.textWidth(name, scl) - scl, x = cx - w * 0.5f, pulse = 0.5f + 0.5f * std::sin(T * 1.6f);
    ui.textGlow(x, yTop, scl, name, 1.0f, 0.42f, 0.06f, scl * 0.9f, 0.10f + 0.05f * pulse);  // halo
    for (int e = 7; e >= 1; e--) ui.text(x + e * scl * 0.10f, yTop + e * scl * 0.15f, scl, name, 0.20f + 0.02f * (7 - e), 0.045f, 0.015f, 1.0f);  // extrusion
    const float top[3] = {1.0f, 0.93f, 0.55f}, bot[3] = {0.95f, 0.30f, 0.04f};
    ui.textGrad(x, yTop, scl, name, top, bot, 1.0f);
    // sous-titre encadré de deux filets
    const std::string sub = "A DWARF IN A CAVE";
    float ss = std::max(2.0f, std::floor(scl / 4.0f)), sw = ui.textWidth(sub, ss) - ss, sy = yTop + 7 * scl + scl * 0.9f;
    ui.rect(cx - w * 0.5f, sy + 3.5f * ss - 1, (w - sw) * 0.5f - ss * 3, 3, 1.0f, 0.7f, 0.25f, 0.8f);
    ui.rect(cx + sw * 0.5f + ss * 3, sy + 3.5f * ss - 1, (w - sw) * 0.5f - ss * 3, 3, 1.0f, 0.7f, 0.25f, 0.8f);
    ui.text(cx - sw * 0.5f + ss * 0.4f, sy + ss * 0.4f, ss, sub, 0.0f, 0.0f, 0.0f, 0.6f);
    ui.text(cx - sw * 0.5f, sy, ss, sub, 1.0f, 0.88f, 0.6f, 1.0f);
}

void Game::drawTitleScreen() {
    UiRenderer& ui = gfx.ui;
    const float fw = uiW, fh = uiH, cx = uiCx, bt = uiBt, ts = uiTs;
    const float bw = std::min(fw * 0.34f, 120.0f * bt), bh = 11.0f * bt, bgap = bh * 0.28f;
    // assombrissement haut et bas
    const float dk[4] = {0, 0, 0, 0.62f}, cl[4] = {0, 0, 0, 0.0f};
    ui.rectGrad(0, 0, fw, fh * 0.42f, dk, dk, cl, cl);
    ui.rectGrad(0, fh * 0.60f, fw, fh * 0.40f, cl, cl, dk, dk);
    for (int i = 0; i < 46; i++) {  // braises qui montent (position déterministe en fonction du temps)
        float ph = i * 1.618f, life = std::fmod(T * (0.05f + 0.04f * std::fmod(ph, 1.0f)) + ph, 1.0f);
        float ex = fw * std::fmod(ph * 0.371f + 0.05f * std::sin(T * 0.8f + ph * 7.0f), 1.0f), ey = fh * (1.02f - life * 1.04f),
              sz = (2.0f + 3.0f * std::fmod(ph * 2.3f, 1.0f)) * fh / 900.0f;
        float a = std::sin(life * 3.14159f) * (0.55f + 0.35f * std::sin(T * 6.0f + ph * 11.0f));
        ui.rect(ex - sz, ey - sz, sz * 2, sz * 2, 1.0f, 0.45f + 0.35f * std::fmod(ph, 1.0f), 0.08f, a * 0.35f);
        ui.rect(ex - sz * 0.5f, ey - sz * 0.5f, sz, sz, 1.0f, 0.85f, 0.45f, a);
    }
    drawLogo(fh * 0.07f, std::max(6.0f, std::floor(fh / 52.0f)));
    float y = fh * 0.50f, x = cx - bw * 0.5f;
    if (menuButton("JOUER", x, y, bw, bh)) startPlaying();
    y += bh + bgap;
    if (menuButton("PARAMETRES", x, y, bw, bh)) { backTo = SC_TITLE; screen = SC_SETTINGS; menuSel = 0; }
    y += bh + bgap;
    if (menuButton("COMMANDES", x, y, bw, bh)) { backTo = SC_TITLE; screen = SC_CONTROLS; menuSel = 0; }
    y += bh + bgap;
    if (menuButton("QUITTER", x, y, bw, bh)) glfwSetWindowShouldClose(win, GLFW_TRUE);
    std::string f1 = "FLECHES / SOURIS : NAVIGUER    ENTREE / CLIC : VALIDER    F11 : PLEIN ECRAN";
    ui.text(cx - ui.textWidth(f1, ts) * 0.5f, fh - ts * 14, ts, f1, 0.9f, 0.75f, 0.5f, 0.85f);
}

void Game::drawPauseScreen() {
    UiRenderer& ui = gfx.ui;
    const float fw = uiW, fh = uiH, cx = uiCx, cy = uiCy, bt = uiBt;
    const float bw = std::min(fw * 0.34f, 120.0f * bt), bh = 11.0f * bt, bgap = bh * 0.28f;
    ui.rect(0, 0, fw, fh, 0.0f, 0.0f, 0.0f, 0.55f);
    drawVignette(0.5f, 0.0f, 0.0f, 0.0f, 1.0f);
    float pw = bw + bt * 20, ph = bh * 5 + bgap * 4 + bt * 36, px0 = cx - pw * 0.5f, py0 = cy - ph * 0.5f;
    drawPanel(px0, py0, pw, ph, 1.0f);
    std::string t0 = "PAUSE";
    const float top[3] = {1.0f, 0.9f, 0.5f}, bot[3] = {0.95f, 0.4f, 0.08f};
    ui.textGrad(cx - ui.textWidth(t0, bt * 1.8f) * 0.5f, py0 + bt * 7, bt * 1.8f, t0, top, bot, 1.0f);
    float y = py0 + bt * 26, x = cx - bw * 0.5f;
    if (menuButton("REPRENDRE", x, y, bw, bh)) { screen = SC_PLAY; menuSel = 0; }
    y += bh + bgap;
    if (menuButton("PARAMETRES", x, y, bw, bh)) { backTo = SC_PAUSE; screen = SC_SETTINGS; menuSel = 0; }
    y += bh + bgap;
    if (menuButton("COMMANDES", x, y, bw, bh)) { backTo = SC_PAUSE; screen = SC_CONTROLS; menuSel = 0; }
    y += bh + bgap;
    if (menuButton("MENU PRINCIPAL", x, y, bw, bh)) { screen = SC_TITLE; menuSel = 0; }
    y += bh + bgap;
    if (menuButton("QUITTER", x, y, bw, bh)) glfwSetWindowShouldClose(win, GLFW_TRUE);
}

void Game::drawSettingsScreen() {
    UiRenderer& ui = gfx.ui;
    const float fw = uiW, fh = uiH, cx = uiCx, cy = uiCy, bt = uiBt;
    const float bw = std::min(fw * 0.34f, 120.0f * bt), bh = 11.0f * bt;
    ui.rect(0, 0, fw, fh, 0.0f, 0.0f, 0.0f, 0.45f);
    float pw = std::min(fw * 0.7f, 260.0f * bt), rowH = bh * 1.3f, ph = rowH * 6.2f + bt * 38, px0 = cx - pw * 0.5f, py0 = cy - ph * 0.5f;
    drawPanel(px0, py0, pw, ph, 1.0f);
    std::string t0 = "PARAMETRES";
    const float top[3] = {1.0f, 0.9f, 0.5f}, bot[3] = {0.95f, 0.4f, 0.08f};
    ui.textGrad(cx - ui.textWidth(t0, bt * 1.6f) * 0.5f, py0 + bt * 6, bt * 1.6f, t0, top, bot, 1.0f);
    float x = px0 + bt * 10, w = pw - bt * 20, y = py0 + bt * 28;
    menuSlider("VOLUME GENERAL", cfg.master, 0.0f, 1.0f, 0.05f, x, y, w, rowH * 0.8f, true);
    y += rowH;
    menuSlider("MUSIQUE", cfg.music, 0.0f, 1.0f, 0.05f, x, y, w, rowH * 0.8f, true);
    y += rowH;
    menuSlider("EFFETS SONORES", cfg.sfx, 0.0f, 1.0f, 0.05f, x, y, w, rowH * 0.8f, true);
    y += rowH;
    menuSlider("SENSIBILITE SOURIS", cfg.sens, 0.2f, 2.5f, 0.1f, x, y, w, rowH * 0.8f, false);
    y += rowH;
    menuToggle("PLEIN ECRAN (F11)", cfg.fullscreen, x, y, w, rowH * 0.8f);
    y += rowH * 1.25f;
    if (menuButton("RETOUR", cx - bw * 0.35f, y, bw * 0.7f, bh)) { screen = backTo; saveSettings(cfg); menuSel = 0; }
}

void Game::drawControlsScreen() {
    UiRenderer& ui = gfx.ui;
    const float fw = uiW, fh = uiH, cx = uiCx, cy = uiCy, bt = uiBt, ts = uiTs;
    const float bw = std::min(fw * 0.34f, 120.0f * bt), bh = 11.0f * bt;
    ui.rect(0, 0, fw, fh, 0.0f, 0.0f, 0.0f, 0.5f);
    struct Row { const char* key; const char* what; };
    static const Row rows[] = {{"ZQSD / WASD", "SE DEPLACER"}, {"SOURIS", "REGARDER AUTOUR DE SOI"}, {"MAJ", "COURIR"},
                               {"CLIC GAUCHE", "COUP DE PIOCHE (EBOULIS, PLANCHES, MONSTRES)"}, {"E", "OUVRIR LA GRANDE PORTE (3 CLES)"},
                               {"G", "DYNAMITE (TOUT EXPLOSE !)"}, {"ESPACE", "SAUTER"}, {"F", "MARCHE / VOL LIBRE (ESPACE, CTRL)"},
                               {"ECHAP", "PAUSE ET MENU"}, {"F11", "PLEIN ECRAN"}, {"F12", "CAPTURE D'ECRAN"},
                               {"R", "NOUVELLE GROTTE (RETOUR AU CAMP)"}, {"ENTREE", "RECOMMENCER APRES LA MORT"}};
    const int nr = (int)(sizeof(rows) / sizeof(rows[0]));
    float rowH = ts * 14.0f, pw = std::min(fw * 0.9f, 330.0f * ts), ph = rowH * (nr + 3.4f) + bh + ts * 22, px0 = cx - pw * 0.5f, py0 = cy - ph * 0.5f;
    drawPanel(px0, py0, pw, ph, 1.0f);
    std::string t0 = "COMMANDES";
    const float top[3] = {1.0f, 0.9f, 0.5f}, bot[3] = {0.95f, 0.4f, 0.08f};
    ui.textGrad(cx - ui.textWidth(t0, bt * 1.6f) * 0.5f, py0 + bt * 5, bt * 1.6f, t0, top, bot, 1.0f);
    float y = py0 + bt * 24, kx = px0 + ts * 8, dx = px0 + pw * 0.30f;
    for (int i = 0; i < nr; i++) {  // touche dessinée comme une touche de clavier + description
        float kw = ui.textWidth(rows[i].key, ts) + ts * 6;
        ui.rect(kx - 2, y - 2, kw + 4, ts * 10.5f + 4, 0, 0, 0, 0.8f);
        const float t1[4] = {0.55f, 0.50f, 0.42f, 1.0f}, b1[4] = {0.30f, 0.26f, 0.21f, 1.0f};
        ui.rectGrad(kx, y, kw, ts * 10.5f, t1, t1, b1, b1);
        ui.rect(kx, y, kw, 2, 1, 1, 1, 0.35f);
        ui.text(kx + ts * 3, y + ts * 1.7f, ts, rows[i].key, 1.0f, 0.97f, 0.88f, 1.0f);
        ui.text(dx, y + ts * 1.7f, ts, rows[i].what, 0.95f, 0.82f, 0.58f, 1.0f);
        y += rowH;
    }
    std::string obj = "OBJECTIF : TROUVE LES 3 CLES DANS LA MINE, OUVRE LA GRANDE PORTE", obj2 = "ET SORS A LA LUMIERE. LA CARTE (EN BAS A DROITE) MONTRE LES CLES !";
    ui.text(cx - ui.textWidth(obj, ts) * 0.5f, y + ts * 2, ts, obj, 0.7f, 0.9f, 1.0f, 1.0f);
    ui.text(cx - ui.textWidth(obj2, ts) * 0.5f, y + ts * 2 + rowH * 0.9f, ts, obj2, 0.7f, 0.9f, 1.0f, 1.0f);
    y += rowH * 2.5f;
    if (menuButton("RETOUR", cx - bw * 0.35f, y, bw * 0.7f, bh)) { screen = backTo; menuSel = 0; }
}
