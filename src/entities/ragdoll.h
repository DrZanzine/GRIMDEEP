// =============================================================================
//  entities/ragdoll.h — corps d'un ennemi tué (« ragdoll »)
//
//  À la mort, le tronc devient un solide rigide qui bascule et rebondit (les points
//  de contact sont les coins de sa boîte englobante). Chaque membre (groupe animable
//  du modèle) est une masse au bout d'une rotule, simulée par un pendule de Verlet :
//  il pend, se balance et traîne au sol. Le corps disparaît au bout de 16 s.
//  Disponible seulement si l'ennemi a un modèle 3D.
// =============================================================================
#pragma once

#include <vector>

#include "core/random.h"
#include "entities/enemy.h"

struct World;

/// État d'un ragdoll.
struct Ragdoll {
    int type = 0;           ///< type d'ennemi
    mat3 R = mat3(1.0f);    ///< orientation du tronc
    vec3 C = vec3(0);       ///< centre de masse du tronc
    vec3 v = vec3(0);       ///< vitesse
    vec3 w = vec3(0);       ///< vitesse angulaire
    float sc = 1;           ///< échelle du modèle
    float t = 0;            ///< temps écoulé depuis la mort
    float rest = 0;         ///< temps passé presque immobile au sol
    bool asleep = false;    ///< immobile : le tronc n'est plus simulé

    /// Membre : extrémité simulée par un pendule de Verlet accroché à sa rotule.
    struct Limb {
        int g;          ///< groupe du modèle
        vec3 tip;       ///< position actuelle de l'extrémité (centre du membre)
        vec3 prev;      ///< position précédente (Verlet)
        float L;        ///< longueur rotule -> extrémité
        vec3 restL;     ///< direction de repos dans le repère du tronc
    };
    std::vector<Limb> limbs;  ///< membres
    std::vector<int> hints;   ///< anneaux courants (World::constrain)
};

/// Crée le ragdoll de l'ennemi `e`, projeté dans la direction `dir` avec la force `knock`.
Ragdoll makeRagdoll(const Enemy& e, vec3 dir, float knock, Rng& rng);

/// Fait avancer la simulation de `dt` secondes (sous-pas de 12 ms), avec collisions
/// contre le sol, les parois et les planches du monde.
void stepRagdoll(World& world, Ragdoll& r, float dt);

/// Ajoute le ragdoll au maillage ; `k` = facteur d'échelle (rétrécit pour disparaître).
void appendRagdollMesh(Mesh& m, const Ragdoll& r, float k);
