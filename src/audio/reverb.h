// =============================================================================
//  audio/reverb.h — réverbération de Schroeder (ambiance de caverne)
//
//  4 filtres en peigne amortis en parallèle, puis 2 filtres passe-tout en série.
//  Une instance par canal (gauche / droite), avec des longueurs légèrement
//  différentes pour élargir l'image stéréo.
// =============================================================================
#pragma once

#include <cstddef>
#include <vector>

namespace aud {

/// Réverbération d'un canal.
struct Reverb {
    /// Filtre en peigne : ligne à retard + passe-bas dans la boucle de réinjection.
    struct Comb { std::vector<float> b; size_t p = 0; float lp = 0; };
    Comb c[4];                 ///< filtres en peigne
    std::vector<float> a[2];   ///< lignes à retard des passe-tout
    size_t ap[2] = {0, 0};     ///< positions de lecture des passe-tout

    /// Alloue les lignes à retard ; `seed` = 0 (canal gauche) ou 1 (canal droit).
    void init(int seed);

    /// Traite un échantillon et renvoie le signal réverbéré (sans le signal direct).
    float process(float x);
};

}  // namespace aud
