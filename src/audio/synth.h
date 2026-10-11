// =============================================================================
//  audio/synth.h — constantes et briques de base du synthétiseur
//
//  Tout le son du jeu est calculé en temps réel (aucun fichier audio) : musique et
//  effets sont synthétisés échantillon par échantillon à 44,1 kHz, en stéréo.
// =============================================================================
#pragma once

#include <cmath>
#include <cstdint>

namespace aud {

/// Fréquence d'échantillonnage (Hz).
static const int SR = 44100;

/// Nombre de morceaux de musique (un par biome souterrain + camp/menu/sortie).
static const int NTRACKS = 6;

/// Petit générateur pseudo-aléatoire xorshift32 propre à l'audio
/// (indépendant de celui du jeu : le son n'influence jamais la partie).
struct Rnd {
    uint32_t s = 2463534242u;  ///< état interne
    /// Réel uniforme dans [0, 1).
    float f() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / 16777216.0f; }
    /// Réel uniforme dans [a, b).
    float r(float a, float b) { return a + (b - a) * f(); }
    /// Entier uniforme dans [0, n).
    int i(int n) { return (int)(f() * n) % n; }
};

/// Fréquence (Hz) d'une note MIDI (69 = la 440 Hz, +1 = un demi-ton).
static inline float midi(float m) { return 440.0f * std::pow(2.0f, (m - 69.0f) / 12.0f); }

/// Onde en dents de scie (-1..1) pour une phase exprimée en tours.
static inline float sawc(double ph) { return (float)(2.0 * (ph - std::floor(ph + 0.5))); }

/// Sinusoïde pour une phase exprimée en tours.
static inline float sinc(double ph) { return (float)std::sin(6.283185307179586 * ph); }

}  // namespace aud
