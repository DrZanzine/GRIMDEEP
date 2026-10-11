// =============================================================================
//  audio/music.h — musique générative : 6 morceaux composés, joués en temps réel
//
//    0 galeries boisées    : luth et flûte, ré dorien, folk de nains (76 bpm)
//    1 grottes humides     : harpe et nappes, fa lydien, ambiant aquatique (58 bpm)
//    2 antre des cristaux  : boîte à musique à 3 temps, mi mineur, scintillant
//    3 catacombes          : bourdon, chœur en cluster, glas, battement de cœur (44 bpm)
//    4 forge infernale     : percussions, basse martelée, cuivres, ré phrygien (104 bpm)
//    5 camp / menu / sortie: cordes et flûte héroïque, sol majeur (84 bpm)
//
//  Chaque morceau (Track) avance par « pas » de double-croche : à chaque pas,
//  onStep() déclenche des notes (voix). Les voix sont ensuite synthétisées à chaque
//  échantillon (enveloppe attaque / décroissance / relâchement + timbre). Aucun
//  bruit blanc : uniquement des timbres harmoniques.
// =============================================================================
#pragma once

#include <vector>

#include "audio/synth.h"

namespace aud {

/// Instruments (timbres) disponibles pour une voix.
enum { V_PLUCK, V_BELL, V_PAD, V_STRINGS, V_FLUTE, V_CELESTA, V_CHOIR, V_THUMP, V_CLANG, V_BASS, V_BRASS, V_TICK };

/// Voix de synthèse (une note en cours).
struct Voice {
    bool on = false;                 ///< voix active
    int type = 0;                    ///< instrument (V_*)
    double ph = 0, ph2 = 0, ph3 = 0; ///< phases des oscillateurs (en tours)
    float f = 100;                   ///< fréquence (Hz)
    float amp = 0.1f;                ///< amplitude
    float pan = 0;                   ///< panoramique (-1 gauche .. +1 droite)
    float age = 0;                   ///< temps écoulé depuis le début de la note (s)
    float dur = 1;                   ///< durée avant relâchement (s)
    float atk = 0.01f;               ///< durée de l'attaque (s)
    float rel = 0.3f;                ///< durée du relâchement (s)
    float dec = 0;                   ///< constante de décroissance exponentielle (0 = tenue)
    float lp = 0;                    ///< état du filtre passe-bas
    float lpc = 0.1f;                ///< coefficient du filtre passe-bas
};

/// Note de mélodie : pas de départ, hauteur MIDI, durée en pas.
struct Note { int step; float m; int len; };

/// Un morceau de musique et ses voix.
struct Track {
    int id = 0;                  ///< numéro du morceau (0..5)
    Rnd rnd;                     ///< aléa (ornements)
    std::vector<Voice> vs;       ///< réserve de voix (72)
    double acc = 0;              ///< temps accumulé depuis le dernier pas
    int step = 0;                ///< numéro du prochain pas
    float gain = 0;              ///< volume de fondu enchaîné (0..1)
    float time = 0;              ///< temps écoulé (LFO, vibrato)
    float stepSec = 0.2f;        ///< durée d'un pas (s)

    // ---------- création de voix
    /// Voix libre (ou la première si toutes sont occupées).
    Voice* alloc();
    /// Démarre une voix avec tous ses paramètres et la renvoie.
    Voice* voice_(int type, float m, float amp, float dur, float dec, float atk, float rel, float pan, float lpc);
    /// Nappe tenue (attaque et relâchement lents).
    void pad(int type, float m, float amp, float dur, float atk = 2.5f, float rel = 3.0f, float pan = 0, float lpc = 0.12f);
    /// Corde pincée (luth, harpe).
    void pluck(float m, float amp, float dec, float pan = 0);
    /// Cloche (synthèse FM).
    void bell(float m, float amp, float dec, float pan = 0);
    /// Célesta / boîte à musique.
    void celesta(float m, float amp, float dec, float pan = 0);
    /// Flûte (avec vibrato).
    void flute(float m, float amp, float dur, float pan = 0);
    /// Cuivre (filtre qui s'ouvre).
    void brass(float m, float amp, float dur, float pan = 0);
    /// Basse saturée.
    void bass(float m, float amp, float dur, float lpc = 0.1f);
    /// Percussion grave (fréquence en Hz, qui chute).
    void thump(float hz, float amp, float dec);
    /// Choc métallique (enclume).
    void clang(float m, float amp, float dec);
    /// Petit clic rythmique.
    void tick(float m, float amp, float pan = 0);
    /// Joue les notes de la mélodie `n` qui commencent au pas `pos`.
    void melody(const std::vector<Note>& n, int pos, int type, float amp, float pan = 0);

    /// Prépare le morceau `t` (tempo, nappes de fond permanentes).
    void init(int t);
    /// Intervalles d'un accord : 0 mineur, 1 majeur, 2 mineur 7, 3 majeur 7.
    static const int* chordI(int q);
    /// Partition : déclenche les notes du pas courant.
    void onStep();
    /// Synthétise un échantillon de la voix `v` ; renvoie la valeur, `panOut` reçoit le panoramique.
    float voice(Voice& v, float& panOut);
    /// Ajoute un échantillon du morceau à (L, R) et fait avancer le temps de `dt`.
    void render(float& L, float& R, float dt);
};

}  // namespace aud
