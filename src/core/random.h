// =============================================================================
//  core/random.h — générateur pseudo-aléatoire déterministe
//
//  Toute la génération procédurale (grotte, camp, décors, ennemis…) dépend d'une
//  graine. Avec la même graine, on obtient exactement le même monde : c'est ce qui
//  permet l'option `--seed N` et les captures reproductibles.
// =============================================================================
#pragma once

#include <cstdint>

/// Générateur xorshift64 : rapide, sans état global, reproductible.
/// Chaque système qui a besoin d'aléa possède sa propre instance (et donc sa propre suite).
struct Rng {
    uint64_t s;  ///< état interne (jamais nul)

    /// Initialise l'état à partir d'une graine, puis « chauffe » le générateur
    /// (4 tirages) pour décorréler les premières valeurs de graines voisines.
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL) {
        for (int i = 0; i < 4; i++) next();
    }

    /// Tire un entier 64 bits uniforme.
    uint64_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }

    /// Tire un réel uniforme dans [0, 1) (24 bits de précision).
    float f() { return (next() >> 40) / 16777216.0f; }

    /// Tire un réel uniforme dans [a, b).
    float r(float a, float b) { return a + (b - a) * f(); }

    /// Renvoie vrai avec la probabilité p.
    bool chance(float p) { return f() < p; }
};
