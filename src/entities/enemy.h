// =============================================================================
//  entities/enemy.h — ennemis : caractéristiques, état et maillage animé
//
//  Trois types : 0 rôdeur (momie), 1 araignée géante, 2 golem de lave.
//  L'intelligence (poursuite, attaque) est dans game/game_combat.cpp ; ce module
//  décrit les données et construit le maillage animé de chaque ennemi à chaque image.
//
//  Attaque en 3 temps (esquivable) :
//    armement (durée `wind` du type) -> frappe (ATK_STRIKE) -> récupération (ATK_RECOVER).
//  Les dégâts sont infligés à la fin de l'armement si le joueur est encore à portée.
// =============================================================================
#pragma once

#include <vector>

#include "render/mesh.h"
#include "render/model_loader.h"

/// Caractéristiques d'un type d'ennemi.
struct EnemyStat {
    float speed;    ///< vitesse de marche (m/s)
    float hp;       ///< points de vie (coups de pioche)
    float dmg;      ///< dégâts d'une attaque (PV du joueur)
    float cd;       ///< délai entre deux attaques (s)
    float reach;    ///< portée d'attaque (m)
    float aggro;    ///< distance à laquelle il s'éveille (m)
    float radius;   ///< rayon de la zone touchable par la pioche (m)
    float centerH;  ///< hauteur du centre du corps au-dessus des pieds (m)
    float knock;    ///< vitesse de recul quand il est touché (m/s)
    float stun;     ///< durée d'étourdissement quand il est touché (s)
    float wind;     ///< durée de l'armement de l'attaque (s)
};

/// Statistiques par type : 0 rôdeur, 1 araignée, 2 golem de lave.
extern const EnemyStat ENEMY_STATS[3];

/// Durée de la frappe (fente) et de la récupération après une attaque (s).
constexpr float ATK_STRIKE = 0.14f, ATK_RECOVER = 0.5f;

/// Échelle appliquée au modèle 3D de chaque type.
extern const float ENEMY_SCALE[3];

/// État d'un ennemi.
struct Enemy {
    int type = 0;                  ///< 0 rôdeur, 1 araignée, 2 golem
    vec3 pos = vec3(0);            ///< position « des yeux » (pieds = pos - 1,65 m), comme le joueur
    vec3 face = vec3(1, 0, 0);     ///< direction du regard (horizontale)
    float hp = 1;                  ///< points de vie restants
    float cd = 0;                  ///< temps avant de pouvoir attaquer à nouveau
    float anim = 0;                ///< phase de l'animation de marche
    float hurt = 0;                ///< > 0 juste après un coup reçu (éclair blanc-rouge)
    bool awake = false;            ///< poursuit le joueur
    bool alive = true;             ///< faux une fois tué
    std::vector<int> hints;        ///< anneaux courants (accélère World::constrain)
    vec3 vel = vec3(0);            ///< repoussement (m/s), s'éteint vite
    float stun = 0;                ///< sonné : ne bouge ni n'attaque
    float hurtLean = 0;            ///< recul visuel après un coup (1 -> 0)
    float atkT = -1;               ///< temps écoulé dans l'attaque (-1 : pas d'attaque)
    bool atkHit = false;           ///< dégâts de l'attaque en cours déjà appliqués
};

/// Modèle 3D associé à un type d'ennemi (momie, araignée, golem).
const ModelData& enemyModel(int type);

/// Ajoute l'ennemi `e` au maillage `m`, animé (marche, attaque, recul, tremblement).
/// `time` = temps de jeu (sert au tremblement d'un ennemi touché).
/// Sans modèle 3D, l'ennemi est dessiné avec des formes procédurales simples.
void appendEnemyMesh(Mesh& m, const Enemy& e, float time);
