// =============================================================================
//  render/materials.cpp — palette de matériaux génériques
// =============================================================================
#include "render/materials.h"

MaterialPalette gMaterials;
CustomMaterials gCustomMats;

const vec3 CRYSTAL_COLORS[5] = {vec3(1.0f, 0.25f, 0.9f), vec3(0.25f, 0.8f, 1.0f), vec3(1.0f, 0.65f, 0.15f),
                                vec3(0.35f, 1.0f, 0.45f), vec3(0.62f, 0.35f, 1.0f)};

float MaterialPalette::append(vec3 kd, float ns, vec3 ke, float gloss) {
    if ((int)diffuse.size() >= MAX_MODEL_MATERIALS) return Mat::STEEL;
    diffuse.push_back(vec4(kd, ns));
    emissive.push_back(vec4(ke, gloss));
    return Mat::MODEL_BASE + (float)diffuse.size() - 1.0f;
}

float MaterialPalette::findOrAppend(vec3 kd, float ns, vec3 ke, float gloss) {
    for (size_t i = 0; i < diffuse.size(); i++)
        if (diffuse[i] == vec4(kd, ns) && emissive[i] == vec4(ke, gloss)) return Mat::MODEL_BASE + (float)i;
    return append(kd, ns, ke, gloss);
}

void registerCustomMaterials() {
    MaterialPalette& p = gMaterials;
    CustomMaterials& m = gCustomMats;
    m.leaf[0] = p.findOrAppend(vec3(0.11f, 0.28f, 0.07f), 10, vec3(0), 0.05f);
    m.leaf[1] = p.findOrAppend(vec3(0.19f, 0.34f, 0.09f), 10, vec3(0), 0.05f);
    m.leaf[2] = p.findOrAppend(vec3(0.42f, 0.27f, 0.07f), 10, vec3(0), 0.05f);
    m.pine = p.findOrAppend(vec3(0.05f, 0.17f, 0.09f), 10, vec3(0), 0.05f);
    m.bark = p.findOrAppend(vec3(0.17f, 0.11f, 0.07f), 8, vec3(0), 0.03f);
    m.sack = p.findOrAppend(vec3(0.50f, 0.40f, 0.26f), 6, vec3(0), 0.02f);
    m.canvas = p.findOrAppend(vec3(0.62f, 0.56f, 0.44f), 6, vec3(0), 0.02f);
    m.cloth = p.findOrAppend(vec3(0.45f, 0.10f, 0.08f), 6, vec3(0), 0.02f);
    // brillance -1 = ombrage « cristal » dans le shader
    for (int i = 0; i < 5; i++) m.crystal[i] = p.findOrAppend(CRYSTAL_COLORS[i] * 0.18f, 40, CRYSTAL_COLORS[i] * 0.85f, -1.0f);
    m.dynamite = p.findOrAppend(vec3(0.62f, 0.07f, 0.05f), 18, vec3(0), 0.25f);
    m.flash = p.findOrAppend(vec3(0.9f, 0.55f, 0.45f), 8, vec3(3.2f, 1.1f, 0.7f), 0.1f);
}
