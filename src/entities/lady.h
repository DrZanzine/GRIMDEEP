// =============================================================================
//  entities/lady.h — « la Dame » (easter egg)
//
//  Grande silhouette décharnée (~100 000 triangles) : robe blanche déchirée, peau
//  marbrée de veines, longs cheveux noirs, yeux rouges lumineux. Le maillage est
//  construit une seule fois dans son propre repère (pieds à l'origine, regard vers +Z)
//  puis placé par une matrice au moment de ses apparitions (voir game_ambient.cpp).
//  Matériaux : Mat::LADY_SKIN, LADY_DRESS, LADY_HAIR, LADY_EYE, LADY_DARK.
// =============================================================================
#pragma once

#include "render/mesh.h"

/// Maillage de la Dame et points remarquables (repère local).
struct LadyModel {
    Mesh mesh;       ///< maillage complet
    vec3 eye[2];     ///< position des deux yeux (lueurs rouges)
    vec3 head;       ///< centre de la tête (lumière rouge de ses yeux)
};

/// Construit le maillage de la Dame (côté CPU ; appeler ensuite lady.mesh.upload()).
void buildLady(LadyModel& lady);
