// =============================================================================
//  entities/particles.h — particules (flammes, braises, étincelles, poussière, fumée…)
//
//  Chaque particule est dessinée comme un point (sprite) par particles.vert/.frag.
//  Le type (`kind`, voir ParticleKind) choisit l'aspect dans le shader. Les
//  particules sont créées et mises à jour par le jeu (game_ambient.cpp, game_combat.cpp,
//  game_render.cpp).
// =============================================================================
#pragma once

#include "core/math_utils.h"

/// Aspect d'une particule (doit correspondre aux branches de particles.frag).
namespace ParticleKind {
constexpr int FLAME = 0;  ///< langue de flamme d'une torche (dessinée directement, pas simulée)
constexpr int GLOW = 1;   ///< halo lumineux autour d'une source (dessiné directement)
constexpr int EMBER = 2;  ///< braise / étincelle colorée qui flotte (couleur = palette uFlame)
constexpr int DUST = 3;   ///< bouffée de poussière
constexpr int SPARK = 5;  ///< étincelle ou gouttelette soumise à la gravité
constexpr int ASH = 6;    ///< cendre grise qui monte doucement
constexpr int FIRE = 7;   ///< boule de feu d'explosion (blanc -> jaune -> rouge sombre)
constexpr int SMOKE = 8;  ///< fumée noire (mélange alpha, triée de loin à près)
}  // namespace ParticleKind

/// Particule simulée.
struct Particle {
    vec3 p, v;       ///< position et vitesse
    float life;      ///< temps de vie restant
    float maxLife;   ///< durée de vie totale (life / maxLife = progression envoyée au shader)
    float seed;      ///< variation aléatoire de l'aspect
    float size;      ///< taille (m)
    float grow;      ///< croissance de la taille (m/s)
    float inten;     ///< intensité lumineuse (pour EMBER : index de couleur dans la palette)
    int kind;        ///< aspect (namespace ParticleKind)
};

/// Goutte d'eau qui tombe d'une stalactite.
struct Drop {
    vec3 p;      ///< position
    float vy;    ///< vitesse verticale
    int tube;    ///< galerie
    int ring;    ///< anneau (pour trouver le sol)
    float age;   ///< temps de chute
};
