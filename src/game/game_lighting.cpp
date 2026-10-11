// =============================================================================
//  game/game_lighting.cpp — sélection des lumières de l'image et des ombres
//
//  Le shader accepte au plus 32 lumières ponctuelles. À chaque image on retient :
//    - la lanterne du joueur ;
//    - les 20 lumières fixes les plus proches (dans la mine, toutes FIXES : aucun
//      scintillement ; au camp, les feux « respirent » et les cristaux changent de couleur) ;
//    - au camp : 5 orbes colorées qui dérivent et 3 projecteurs sur la montagne ;
//    - les mèches de dynamite, les explosions, la lueur des yeux de la Dame.
//  Les 4 lumières les plus proches reçoivent une cube map d'ombre ; l'affectation est
//  conservée d'une image à l'autre (hystérésis) pour éviter les sauts d'ombres.
// =============================================================================
#include "game/game.h"

#include <algorithm>

#include "render/materials.h"

void Game::buildLights() {
    lv.clear();
    // --- lanterne tenue dans la main gauche (suit la caméra avec un léger balancement)
    const float strideL = walked * 2.1f;
    Mlan = glm::inverse(view) *
           glm::translate(mat4(1), vec3(-0.37f + std::cos(strideL) * 0.006f * bobAmt, -0.335f + std::sin(strideL * 2.0f) * 0.008f * bobAmt, -0.45f)) *
           glm::rotate(mat4(1), std::sin(strideL) * 0.05f * bobAmt + std::sin(T * 0.9f) * 0.012f, vec3(0, 0, 1)) * glm::rotate(mat4(1), 0.1f, vec3(0, 1, 0)) *
           glm::scale(mat4(1), vec3(0.7f));

    // --- palette animée du camp : les couleurs des cristaux / orbes dérivent lentement sur tout le spectre
    auto hsv = [](float h, float sat, float val) {
        h -= std::floor(h);
        vec3 rgb(std::min(1.0f, std::max(0.0f, std::fabs(h * 6 - 3) - 1)), std::min(1.0f, std::max(0.0f, 2 - std::fabs(h * 6 - 2))),
                 std::min(1.0f, std::max(0.0f, 2 - std::fabs(h * 6 - 4))));
        return glm::mix(vec3(1.0f), rgb, sat) * val;
    };
    static const float baseHue[5] = {0.86f, 0.52f, 0.10f, 0.37f, 0.74f};
    for (int k = 0; k < 5; k++) {
        showCol[k] = hsv(baseHue[k] + T * 0.02f, 0.88f, 1.0f);
        float pulse = 0.5f + 0.5f * std::sin(T * 1.3f + k * 1.3f);
        for (int c = 0; c < 3; c++) palFlame[(7 + k) * 3 + c] = showCol[k][c];
        int mi = (int)(gCustomMats.crystal[k] - Mat::MODEL_BASE);  // matériau du cristal géant dans la palette
        if (mi >= 0 && mi < (int)gMaterials.diffuse.size()) {
            gMaterials.diffuse[mi] = vec4(showCol[k] * 0.18f, 40.0f);
            gMaterials.emissive[mi] = vec4(showCol[k] * (0.55f + 0.55f * pulse), -1.0f);
        }
    }

    // --- lanterne + lumières fixes (60 m au plus), triées par distance, 20 au maximum
    vec3 lanPos = vec3(Mlan * vec4(0, 0, 0, 1));
    if (!titleCam) lv.push_back({lanPos, vec3(1.0f, 0.74f, 0.44f) * 0.6f, 0.0f, 100000, 1.0f, 0});
    for (size_t i = 0; i < world.lights.size(); i++) {
        const WorldLight& t = world.lights[i];
        if (t.needsDoor && !world.doorOpen) continue;
        float dist = glm::length(t.pos - eye);
        if (dist > 60.0f) continue;
        vec3 col = t.col;
        float inten = t.inten;
        if (t.biome >= 7) {  // cristaux du camp : couleur qui dérive + onde de luminosité qui parcourt la montagne
            int sl = t.biome - 7;
            float wave = 0.5f + 0.5f * std::sin(T * 1.15f + t.pos.x * 0.05f + t.pos.y * 0.035f + sl * 0.9f);
            col = showCol[sl] * 1.25f;
            inten *= 0.55f + 0.95f * wave;
        } else if (t.biome == HUB_BIOME && t.flame) inten *= 1.0f + 0.06f * std::sin(T * 1.6f + t.pos.x * 0.7f);  // les feux respirent doucement
        lv.push_back({t.pos, col * inten, dist, (int)i, 1.0f, t.biome});
    }
    std::sort(lv.begin(), lv.end(), [](const LightInfo& a, const LightInfo& b) { return a.dist < b.dist; });
    if (lv.size() > 20) lv.resize(20);

    // --- spectacle lumineux du camp (visible aussi depuis l'entrée de la mine)
    if (titleCam || world.inHub(eye) || glm::length(eye - world.tubes[0].rings[0].c) < 24.0f) {
        for (int k = 0; k < 5; k++) {  // orbes de couleur qui dérivent au-dessus de la place
            float a = T * (0.2f + 0.04f * k) + k * 1.256f;
            vec3 p(21.0f * std::sin(a) + 7.0f * std::sin(T * 0.31f + k), world.hubGroundY + 2.6f + 1.7f * std::sin(T * 0.7f + k * 1.9f),
                   world.hubFrontZ + 27.0f + 15.0f * std::cos(a * 0.8f + k));
            lv.push_back({p, showCol[k] * (2.8f + 0.8f * std::sin(T * 0.9f + k)), glm::length(p - eye), 300000 + k, 1.0f, 7 + k});
        }
        for (int k = 0; k < 3; k++) {  // projecteurs qui balaient la face de la montagne
            float a = T * 0.22f + k * 2.09f;
            int sl = (k * 2 + 1) % 5;
            vec3 p(38.0f * std::sin(a), world.hubGroundY + 8.5f + 5.5f * std::sin(a * 1.7f + k), world.hubFrontZ + 7.0f + 2.5f * std::sin(a * 2.1f));
            lv.push_back({p, showCol[sl] * 4.4f, glm::length(p - eye), 310000 + k, 1.0f, 7 + sl});
        }
    }
    // --- mèches de dynamite (lueur qui vacille)
    for (size_t i = 0; i < dyns.size(); i++) {
        vec3 tp = fuseTip(dyns[i]);
        lv.push_back({tp + vec3(0, 0.05f, 0), vec3(1.0f, 0.55f, 0.2f) * (0.7f + 0.3f * std::sin(T * 47.0f + i)), glm::length(tp - eye), 510000 + (int)i, 1.0f, 0});
    }
    // --- explosions : éclair aveuglant puis feu qui décroît ; insérées en tête de liste
    int blastSrc = -1;
    for (size_t i = 0; i < blasts.size(); i++) {
        const Blast& b = blasts[i];
        float t = b.t, fire = 3.2f * (1.0f - smoothStep(0.5f, 6.5f, t)) * (0.8f + 0.2f * std::sin(T * 23.0f + i * 3.0f));
        float I = (t < 0.08f ? 38.0f : 38.0f * std::exp(-(t - 0.08f) * 8.0f)) + fire;
        vec3 col = glm::mix(vec3(1.0f, 0.5f, 0.18f), vec3(1.0f, 0.88f, 0.65f), std::exp(-t * 4.0f));
        lv.insert(lv.begin() + std::min<size_t>(1, lv.size()), LightInfo{b.c + vec3(0, 0.6f, 0), col * I, 0.0f, 500000 + (int)i, 1.0f, 0});
        if (blastSrc < 0 && t < 2.0f) blastSrc = 500000 + (int)i;
    }
    // --- lueur rouge des yeux de la Dame
    if (hn.ev >= 0 && hn.dis < 0) {
        vec3 headPos = vec3(ladyMatrix() * vec4(lady.head, 1.0f)) + hn.f * 1.1f + vec3(0, 0.25f, 0);
        lv.push_back({headPos, vec3(1.0f, 0.06f, 0.04f) * 0.45f, glm::length(headPos - eye), 400000, 1.0f, 12});
    }

    // --- cubes d'ombre : on libère ceux des lumières devenues lointaines, puis on affecte les plus proches
    {
        float kth = lv.size() >= (size_t)SHADOW_SLOTS ? lv[SHADOW_SLOTS - 1].dist : 1e9f;
        auto distOf = [&](int src) {
            for (auto& l : lv)
                if (l.src == src) return l.dist;
            return 1e9f;
        };
        for (int s = 0; s < SHADOW_SLOTS; s++)
            if (slotLight[s] >= 0 && distOf(slotLight[s]) > 1.35f * kth + 1.0f) slotLight[s] = -1;
        for (size_t k = 0; k < lv.size() && k < (size_t)SHADOW_SLOTS * 2; k++) {
            bool has = false;
            for (int s = 0; s < SHADOW_SLOTS; s++)
                if (slotLight[s] == lv[k].src) has = true;
            if (has) continue;
            for (int s = 0; s < SHADOW_SLOTS; s++)
                if (slotLight[s] < 0) { slotLight[s] = lv[k].src; break; }
        }
        if (blastSrc >= 0) {  // l'explosion projette toujours des ombres
            bool has = false;
            for (int s = 0; s < SHADOW_SLOTS; s++)
                if (slotLight[s] == blastSrc) has = true;
            if (!has) slotLight[SHADOW_SLOTS - 1] = blastSrc;
        }
    }

    // --- tableaux envoyés aux shaders
    if (lv.size() > MAX_LIGHTS) lv.resize(MAX_LIGHTS);
    nl = (int)lv.size();
    for (int i = 0; i < MAX_LIGHTS * 3; i++) { lpos[i] = 0; lcol[i] = 0; }
    for (int i = 0; i < MAX_LIGHTS; i++) lsh[i] = -1;
    for (int i = 0; i < nl; i++) {
        for (int c = 0; c < 3; c++) {
            lpos[i * 3 + c] = lv[i].p[c];
            lcol[i * 3 + c] = lv[i].c[c];
        }
        for (int s = 0; s < SHADOW_SLOTS; s++)
            if (slotLight[s] == lv[i].src) lsh[i] = s;
    }
}
