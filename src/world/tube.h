// =============================================================================
//  world/tube.h — une galerie de la mine (« tube »)
//
//  Une galerie est une suite d'anneaux (Ring) espacés de `ds` mètres le long d'une
//  courbe. Chaque anneau a un repère local :
//      T = tangente (sens de la galerie), R = droite, U = haut ;
//  et une section : `M` points 2D (x le long de R, y le long de U) qui décrivent la
//  paroi autour du centre. Toutes les requêtes géométriques (où est le sol ? où sont
//  les murs à telle hauteur ?) se font dans ce repère local (x, y, t).
//
//  La galerie 0 est la galerie principale ; les suivantes sont des galeries
//  latérales en cul-de-sac (`parent` = 0, `junction` = anneau de raccordement).
// =============================================================================
#pragma once

#include <vector>

#include "core/math_utils.h"

/// Anneau de galerie : centre, repère local et dimensions de la section.
struct Ring {
    vec3 c;            ///< centre de l'anneau
    vec3 T, R, U;      ///< tangente, droite, haut (repère orthonormé)
    float w, h;        ///< demi-largeur et demi-hauteur de la section avant déformation
    float yaw, pitch;  ///< cap et pente de la galerie à cet endroit
};

/// Galerie : anneaux + section déformée + informations de raccordement.
struct Tube {
    std::vector<Ring> rings;  ///< anneaux successifs
    std::vector<vec2> loc;    ///< section : M points (x, y) par anneau, repère local de l'anneau
    std::vector<vec3> P;      ///< mêmes points en coordonnées monde
    std::vector<char> cut;    ///< quads retirés de la paroi (ouverture vers une autre galerie)
    int N = 0;                ///< nombre d'anneaux
    int M = 56;               ///< nombre de points par section
    float ds = 0.3f;          ///< espacement des anneaux (mètres)
    int vBase = 0;            ///< index du premier sommet de cette galerie dans World::rockMesh
    bool isMain = false;      ///< vrai pour la galerie principale
    int parent = -1;          ///< galerie sur laquelle elle se branche (-1 : aucune)
    int junction = -1;        ///< anneau de raccordement dans la galerie parente
    int side = 0;             ///< côté du branchement (+1 droite, -1 gauche)

    /// Convertit des coordonnées locales de l'anneau `i` (x à droite, y en haut,
    /// t le long de la galerie) en position monde.
    vec3 toWorld(int i, float x, float y, float t = 0) const;

    /// Trouve les parois gauche (`xl`) et droite (`xr`) de l'anneau `i` à la hauteur
    /// locale `y`. Renvoie faux si cette hauteur est hors de la section.
    bool wallRange(int i, float y, float& xl, float& xr) const;

    /// Hauteur locale du plafond (`top`) ou du sol (!`top`) de l'anneau `i` à
    /// l'abscisse `x` (±1e9 si la verticale ne coupe pas la section).
    float vertRange(int i, float x, bool top) const;

    /// Hauteur locale du sol à l'abscisse `x` (-1 par défaut si introuvable).
    float floorY(int i, float x) const;

    /// Hauteur locale du plafond à l'abscisse `x` (2 par défaut si introuvable).
    float ceilY(int i, float x) const;

    /// Distance du centre à la paroi dans la direction d'angle `phi` (repère local).
    float radiusAt(int i, float phi) const;

    /// Anneau le plus proche de `p`. Avec un indice de départ `hint` >= 0, la recherche
    /// est limitée à ±40 anneaux autour (rapide, pour suivre un objet qui se déplace).
    int locate(vec3 p, int hint) const;
};
