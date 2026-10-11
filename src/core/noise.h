// =============================================================================
//  core/noise.h — fonctions de hachage et de bruit procédural (côté CPU)
//
//  Le bruit sert à déformer les parois des galeries, à moduler le relief du camp,
//  à varier les décors… Toutes les fonctions sont déterministes : mêmes entrées,
//  même graine `s`  =>  même résultat.
// =============================================================================
#pragma once

#include <cstdint>

#include "core/math_utils.h"

/// Mélange les bits d'un entier 32 bits (fonction de hachage « avalanche »).
uint32_t hash32(uint32_t x);

/// Hache une position entière 3D et une graine en un réel pseudo-aléatoire dans [0, 1).
float hash3D(int x, int y, int z, uint32_t s);

/// Bruit de valeur 3D : interpolation lisse de valeurs aléatoires placées aux
/// sommets d'une grille entière. Résultat dans [0, 1].
float valueNoise3D(float x, float y, float z, uint32_t s);

/// Bruit fractal (fBm) : somme de `oct` octaves de bruit de valeur, chaque octave
/// ayant une fréquence ~2x plus élevée et une amplitude 2x plus faible. Résultat dans [0, 1].
float fbm3D(vec3 p, int oct, uint32_t s);

/// Bruit de valeur 1D (interpolation cubique entre valeurs aléatoires). Résultat dans [0, 1].
/// Utilisé pour faire serpenter les galeries (cap, pente, largeur).
float valueNoise1D(float x, uint32_t s);
