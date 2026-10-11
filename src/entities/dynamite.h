// =============================================================================
//  entities/dynamite.h — bâtons de dynamite (touche G) et explosions
//
//  Le bâton est lancé devant le joueur, rebondit (collisions avec le sol, les parois
//  et les planches), sa mèche crépite et raccourcit, puis il explose au bout de
//  DYN_FUSE secondes : tout est détruit dans un rayon de DYN_RADIUS mètres.
//  La logique est dans game/game_combat.cpp ; ce fichier décrit les données.
// =============================================================================
#pragma once

#include <vector>

#include "core/math_utils.h"

/// Durée de la mèche (s) et rayon de destruction de l'explosion (m).
constexpr float DYN_FUSE = 2.2f, DYN_RADIUS = 6.5f;

/// Bâton de dynamite en vol ou posé.
struct Dynamite {
    vec3 p = vec3(0);        ///< position
    vec3 v = vec3(0);        ///< vitesse
    vec3 w = vec3(0);        ///< vitesse angulaire
    mat3 R = mat3(1.0f);     ///< orientation (R[0] = axe du bâton)
    float t = 0;             ///< temps écoulé depuis le lancer
    float hiss = 0;          ///< temps avant le prochain crépitement sonore
    bool rest = false;       ///< immobile au sol
    std::vector<int> hints;  ///< anneaux courants (World::constrain)
};

/// Explosion en cours (lumière, feu au sol, fumée qui continue de monter).
struct Blast {
    vec3 c = vec3(0);  ///< centre
    float t = 0;       ///< temps écoulé
};

/// Position du bout incandescent de la mèche (elle raccourcit avec le temps).
inline vec3 fuseTip(const Dynamite& d) {
    float f = 0.11f * std::max(0.0f, 1.0f - d.t / DYN_FUSE);
    return d.p + d.R[0] * (0.1f + f * 0.55f) + d.R[1] * (0.025f + f * 0.8f);
}
