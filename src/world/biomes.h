// =============================================================================
//  world/biomes.h — définition des biomes
//
//  La galerie principale traverse 5 biomes successifs, suivis de la zone de sortie.
//  Le camp de base (en surface) est le biome 6. Chaque biome fixe la teinte de la
//  roche, la lumière ambiante, la couleur des torches, l'humidité et la mousse ;
//  ces valeurs sont envoyées au shader de scène (uniformes uTint, uAmbB, uWetB, uMossB).
//  Le numéro de biome sert aussi à choisir la musique (src/audio/music.cpp ; le camp utilise le morceau 5).
// =============================================================================
#pragma once

#include "core/math_utils.h"

/// Paramètres d'ambiance d'un biome.
struct BiomeDef {
    const char* name;  ///< nom affiché à l'écran en entrant dans le biome
    vec3 tint;         ///< teinte multipliée à la couleur de la roche
    vec3 amb;          ///< lumière ambiante
    vec3 torch;        ///< couleur des flammes de torches
    float torchI;      ///< intensité des torches
    float wet;         ///< humidité des parois (0 sec .. 1 ruisselant)
    float moss;        ///< quantité de mousse (0 .. 1)
};

/// Nombre total de biomes (5 souterrains + sortie + camp).
constexpr int BIOME_COUNT = 7;

/// Numéro du biome « zone de sortie » (après la grande porte).
constexpr int EXIT_BIOME = 5;

/// Numéro du biome « camp de base » (surface).
constexpr int HUB_BIOME = 6;

/// Table des biomes, indexée par numéro :
/// 0 galeries boisées, 1 grottes humides, 2 antre des cristaux, 3 catacombes,
/// 4 forge infernale, 5 la sortie, 6 le camp de base.
extern const BiomeDef BIOMES[BIOME_COUNT];
