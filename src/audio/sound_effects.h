// =============================================================================
//  audio/sound_effects.h — effets sonores synthétisés
//
//  Chaque effet est une petite formule (oscillateurs + bruit filtré + enveloppe)
//  évaluée échantillon par échantillon jusqu'à ce qu'il s'éteigne.
// =============================================================================
#pragma once

#include "audio/synth.h"

namespace aud {

/// Effets sonores disponibles (argument de Engine::sfx).
enum {
    SF_DRIP,       ///< goutte d'eau (désactivé : ignoré par Engine::sfx)
    SF_SWING,      ///< sifflement de la pioche / du lancer de dynamite
    SF_HITWOOD,    ///< coup sur du bois
    SF_BREAK,      ///< planche qui casse
    SF_PICKUP,     ///< objet ramassé (deux notes)
    SF_DOOR,       ///< grande porte qui s'ouvre (grondement)
    SF_HURT,       ///< joueur touché
    SF_DIE,        ///< mort (joueur ou ennemi)
    SF_ROAR,       ///< cri d'un ennemi qui s'éveille / attaque
    SF_CLICK,      ///< clic de menu (var > 0,5 : plus aigu)
    SF_HEART,      ///< battement de cœur (vie basse)
    SF_ROCK,       ///< pioche sur la pierre
    SF_ROCKBREAK,  ///< bloc de pierre qui éclate
    SF_FLESH,      ///< coup qui porte sur un ennemi
    SF_STEP,       ///< pas (var > 0,5 : sur l'herbe)
    SF_GHOST,      ///< apparition de « la Dame » qui s'évanouit
    SF_BOOM,       ///< explosion
    SF_RING,       ///< sifflement d'oreilles après l'explosion
    SF_FUSE,       ///< crépitement de la mèche
    SF_COUNT
};

/// Effet sonore en cours de lecture.
struct Sfx {
    bool on = false;      ///< actif
    int type = 0;         ///< effet (SF_*)
    float t = 0;          ///< temps écoulé (s)
    float vol = 1;        ///< volume (0..1)
    float var = 0;        ///< variante (0..1), sens propre à chaque effet
    float lp = 0;         ///< état du filtre passe-bas (bruit filtré)
    double ph = 0, ph2 = 0;  ///< phases des oscillateurs
};

/// Calcule l'échantillon suivant de l'effet `s` (désactive l'effet quand il est fini).
/// `rnd` fournit le bruit.
float sfxSample(Sfx& s, Rnd& rnd);

}  // namespace aud
