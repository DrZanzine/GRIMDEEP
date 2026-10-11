// =============================================================================
//  game/options.h — options de la ligne de commande
//
//  En jeu normal, aucune option n'est nécessaire. Les options servent surtout aux
//  tests et aux captures d'écran automatiques (voir README, « Options de test »).
//  Exemple : grimdeep.exe --seed 42 --biome 2 --shot cristaux.png --frames 60
// =============================================================================
#pragma once

#include <cstdint>
#include <string>

#include "core/math_utils.h"

/// Toutes les options reconnues, avec leur valeur par défaut.
struct LaunchOptions {
    // --- monde et fenêtre
    uint32_t seed = 0;            ///< --seed N : graine de la grotte (par défaut : dérivée de l'horloge)
    int winW = 1600, winH = 900;  ///< --w / --h : taille de la fenêtre (mode fenêtré, captures)
    bool windowed = false;        ///< --windowed : ignore le plein écran des réglages
    bool noAudio = false;         ///< --noaudio : pas de son

    // --- capture automatique
    std::string shot;             ///< --shot f.png : fenêtre, temps fixe de 1/30 s par image ; capture l'image `shotFrames` puis quitte
    int shotFrames = 25;          ///< --frames N : numéro de l'image capturée

    // --- point de départ
    float startAt = 3.0f;         ///< --at M : départ à M mètres dans la galerie (au lieu du camp)
    bool startAtGiven = false;    ///< vrai si --at a été donné
    int startBiome = -1;          ///< --biome B : départ au milieu du biome B (0..4)
    int startBranch = 0;          ///< --branch K : départ dans la galerie latérale K
    bool atDoor = false;          ///< --door : départ devant la grande porte
    bool atExit = false;          ///< --exit : départ près de la sortie
    bool atLadder = false;        ///< --ladder : face à la première échelle
    bool atBarricade = false;     ///< --barricade : face à la première barricade
    bool corridorTest = false;    ///< --corr : devant le couloir des catacombes
    bool startHub = false;        ///< --hub / --hubpos X Z : départ dans le camp
    float hubX = 0, hubZ = 100;   ///< position dans le camp (Z relatif à l'entrée de la mine)
    float lookOff = 0.0f;         ///< --look DEG : décalage du cap de la caméra
    float lookPitch = 0.0f;       ///< --pitch DEG : inclinaison de la caméra
    bool camGiven = false;        ///< --campos X Y Z : caméra libre à une position donnée
    vec3 camPos = vec3(0);        ///< position de --campos
    bool startFly = false;        ///< --fly : départ en vol libre

    // --- écrans
    bool menu = false;            ///< --menu : écran titre
    bool pause = false;           ///< --pause : menu pause
    bool settings = false;        ///< --settings : écran des paramètres
    bool controls = false;        ///< --controls : écran des commandes

    // --- état de jeu
    int keys = 0;                 ///< --keys N : nombre de clés au départ
    float hp = 100.0f;            ///< --hp N : points de vie au départ
    bool openDoor = false;        ///< --open : grande porte déjà ouverte
    bool noEnemies = false;       ///< --noenemies : aucun ennemi
    int testEnemy = -1;           ///< --enemy TYPE : un ennemi (0 rôdeur, 1 araignée, 2 golem) à 2,3 m devant soi

    // --- outils de test
    bool god = false;             ///< --god : invulnérable
    bool noclip = false;          ///< --noclip : pas de collisions
    vec3 eyeOff = vec3(0);        ///< --eyeoff D H R : décale la caméra (observer un ennemi)
    bool forceAxe = false;        ///< --axe : pioche figée (voir forceSwing)
    float forceSwing = 0.22f;     ///< --swing T : fige la pioche à T secondes de l'animation
    bool autoWalk = false;        ///< --autowalk : avance tout seul
    bool forceBreak = false;      ///< --break : coups de pioche automatiques (images 3, 8, 13, 18), portée 12 m
    bool autoE = false;           ///< --autoe : appuie sur E à l'image 3
    bool forceRat = false;        ///< --rat : fait sortir un rat 45 images avant la capture
    bool ratShow = false;         ///< --ratshow : montre un rat devant la caméra
    float ladyDist = -1.0f;       ///< --dame D : fait apparaître la Dame à D mètres devant soi
    int dynFrame = -1;            ///< --dyn F : lance une dynamite à l'image F
};

/// Lit les options de `argv`. Les options inconnues sont ignorées.
LaunchOptions parseOptions(int argc, char** argv);
