// =============================================================================
//  audio/audio_engine.h — moteur audio temps réel (Windows WinMM / waveOut)
//
//  Un thread dédié remplit en continu 6 tampons de 1024 échantillons stéréo 16 bits
//  et les envoie à la carte son. Chaque échantillon mélange :
//    - la musique du biome courant (fondu enchaîné de 4 s au changement de biome),
//      atténuée par le « duck » (menus, oreilles qui sifflent après une explosion) ;
//    - une réverbération de Schroeder sur la musique ;
//    - les effets sonores en cours (24 au maximum) ;
//    - un adoucissement des aigus et une saturation douce (tanh) en sortie.
//  Le jeu communique avec le thread audio sans verrou : demandes d'effets via une
//  file circulaire atomique, volumes / biome / duck via des variables atomiques.
// =============================================================================
#pragma once

#include <atomic>
#include <memory>

#include "audio/music.h"
#include "audio/reverb.h"
#include "audio/sound_effects.h"

namespace aud {

/// Moteur audio. Utilisation : start() au démarrage, sfx() / setBiome() / setVolumes()
/// pendant la partie, stop() à la fin. Si start() échoue, les appels sont sans effet.
struct Engine {
    static const int NB = 6;      ///< nombre de tampons envoyés à la carte son
    static const int BS = 1024;   ///< échantillons stéréo par tampon (~23 ms)

    std::atomic<bool> run{false};                   ///< le thread audio tourne
    std::atomic<int> target{0};                     ///< morceau demandé (biome)
    std::atomic<unsigned> qhead{0}, qtail{0};       ///< file des effets demandés (écriture / lecture)
    std::atomic<unsigned> q[64];                    ///< file : type | volume << 8 | variante << 16
    std::atomic<float> vMaster{0.8f}, vMusic{0.7f}, vSfx{0.8f};  ///< volumes (0..1)
    std::atomic<float> duck{1.0f};                  ///< atténuation de la musique (1 = aucune)
    Track tracks[NTRACKS];                          ///< les 6 morceaux (tous synthétisés pendant les fondus)
    Sfx sfxs[24];                                   ///< effets en cours
    Rnd rnd;                                        ///< bruit des effets
    Reverb rv[2];                                   ///< réverbération gauche / droite
    float lpL = 0, lpR = 0;                         ///< filtres de sortie
    float duckS = 1.0f;                             ///< duck lissé
    bool ok = false;                                ///< moteur démarré

    Engine();
    ~Engine();

    /// Ouvre la sortie audio et lance le thread. Renvoie faux si aucun périphérique n'est disponible.
    bool start();
    /// Arrête le thread et ferme la sortie audio.
    void stop();
    /// Choisit la musique : numéro de biome 0..5 (le camp de base utilise le morceau 5).
    void setBiome(int b) { target = b < 0 ? 0 : (b >= NTRACKS ? NTRACKS - 1 : b); }
    /// Règle les volumes général, musique et effets (0..1).
    void setVolumes(float master, float music, float sfxv) { vMaster = master; vMusic = music; vSfx = sfxv; }
    /// Règle l'atténuation de la musique (1 = normale).
    void setDuck(float d) { duck = d; }
    /// Demande la lecture d'un effet (SF_*) avec un volume 0..1 et une variante 0..255.
    void sfx(int type, float vol = 1.0f, int var = 0);

    /// Initialise les morceaux et la réverbération (appelé par start()).
    void initSynthesis();
    /// Calcule `frames` échantillons stéréo entrelacés dans `out` (cœur du mixage).
    void mix(short* out, int frames);

private:
    struct Device;                  ///< ressources WinMM (définies dans le .cpp)
    std::unique_ptr<Device> dev;    ///< périphérique de sortie

    /// Point d'entrée du thread audio (signature imposée par CreateThread : DWORD WINAPI f(LPVOID)).
    static unsigned long __stdcall threadMain(void* p);
    /// Boucle du thread audio : renvoie chaque tampon dès que la carte son l'a joué.
    void loop();
    /// Démarre un effet à partir d'une demande de la file.
    void spawnSfx(unsigned v);
};

}  // namespace aud
