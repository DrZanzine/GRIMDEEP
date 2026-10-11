// =============================================================================
//  entities/rat.h — rats qui traversent les galeries d'un trou à l'autre
//
//  Un rat suit une polyligne : il sort d'un trou de rat, longe le sol en zigzag et
//  rentre dans le trou jumeau (voir HolePair dans world/world.h). Le maillage est
//  reconstruit à chaque image à partir d'ellipsoïdes (corps, tête, queue, pattes).
// =============================================================================
#pragma once

#include <vector>

#include "render/mesh.h"

/// Rat en déplacement le long d'un chemin.
struct Rat {
    std::vector<vec3> pts;   ///< points du chemin
    std::vector<float> cum;  ///< abscisse curviligne cumulée de chaque point
    float s = 0;             ///< distance parcourue
    float speed = 3.2f;      ///< vitesse (m/s)
    float phase = 0;         ///< phase de l'animation (pattes, queue)
    bool alive = true;       ///< faux une fois arrivé (ou soufflé par une explosion)
};

/// Position du rat à l'abscisse `s` de son chemin ; `fwd` reçoit la direction de marche.
vec3 ratPositionAt(const Rat& r, float s, vec3& fwd);

/// Ajoute un rat au maillage, placé en `pos`, tourné vers `fwd`, animé selon `phase`.
void appendRatMesh(Mesh& m, vec3 pos, vec3 fwd, float phase);
