// =============================================================================
//  render/geometry.h — primitives géométriques procédurales
//
//  Fonctions qui ajoutent des formes à un Mesh : grilles, ellipsoïdes, cylindres,
//  planches (boîtes légèrement irrégulières), surfaces lissées le long d'un chemin…
//  Tout le décor du jeu (galeries, soutènements, camp, ennemis de secours, objets
//  tenus en main, « la Dame ») est assemblé à partir de ces briques.
//
//  Paramètres communs :
//    - `mat`  : numéro de matériau (voir render/materials.h) ;
//    - `seed` : valeur écrite dans Vertex::seed (variation du matériau).
// =============================================================================
#pragma once

#include <functional>
#include <vector>

#include "core/random.h"
#include "render/mesh.h"

/// Ajoute une grille de `rows` x `cols` points (`P`, rangée par rangée), refermée sur
/// elle-même dans le sens des colonnes (la dernière colonne rejoint la première).
/// Les normales sont lissées puis orientées vers `ref[i]` si `refSign` < 0 (surface
/// vue de l'extérieur, ex. ellipsoïde) ou à l'opposé si `refSign` > 0 (surface vue
/// de l'intérieur, ex. paroi de galerie).
void addGrid(Mesh& m, const std::vector<vec3>& P, const std::vector<vec3>& ref, int rows, int cols, float refSign,
             float mat, float seed);

/// Ajoute un ellipsoïde de centre `c`, d'axes `ax`, `ay`, `az` et de rayons `rad` (7 x 10 sommets).
void addEllipsoid(Mesh& m, vec3 c, vec3 ax, vec3 ay, vec3 az, vec3 rad, float mat, float seed);

/// Ajoute une boîte orientée aux faces planes dont les 8 coins sont légèrement
/// décalés au hasard (amplitude `jit`), pour un aspect de bois brut.
/// Les UV suivent les dimensions réelles (la texture garde la même échelle partout).
void addBoardBox(Mesh& m, vec3 c, vec3 dl, vec3 dw, vec3 dt, float hl, float hw, float ht, float mat, float seed,
                 Rng& rng, float jit);

/// Ajoute une planche (voir Board) : géométrie selon `m.geom` (boîte, modèle plank.obj
/// ou rien), puis enregistrement dans `m.boards` si `m.track` est vrai.
void addBoard(Mesh& m, vec3 c, vec3 dl, vec3 dw, vec3 dt, float hl, float hw, float ht, float mat, float seed, Rng& rng,
              float jit = 0.012f);

/// Ajoute une planche allant du point `a` au point `b`. `hint` indique la direction
/// de l'épaisseur (projetée perpendiculairement à la planche). `extend` rallonge la
/// planche à chaque bout. Renvoie faux si a et b sont confondus.
bool addBoardBetween(Mesh& m, vec3 a, vec3 b, vec3 hint, float hw, float ht, float mat, Rng& rng, float extend = 0.0f,
                     float jit = 0.012f);

/// Ajoute un cylindre (ou cône tronqué) lisse de `p0` (rayon r0) à `p1` (rayon r1),
/// à `seg` côtés, avec des disques de fermeture optionnels à chaque bout.
/// UV : u = angle (0..1), v = 0 en p0 et 1 en p1.
void addCylinder(Mesh& m, vec3 p0, vec3 p1, float r0, float r1, int seg, float mat, float seed, bool cap0 = true,
                 bool cap1 = true);

/// Comme addGrid(), mais avec UV et matériau par sommet ; normales orientées à
/// l'opposé de `ref` (surface vue de l'extérieur). Utilisé pour « la Dame ».
void addSurface(Mesh& m, const std::vector<vec3>& P, const std::vector<vec3>& ref, const std::vector<vec2>& UV,
                const std::vector<float>& MT, int rows, int cols);

/// Spline de Catmull-Rom passant par les points de contrôle `cp`, avec `per` points
/// par segment (le dernier point de contrôle est ajouté à la fin).
std::vector<vec3> catmullRomSpline(const std::vector<vec3>& cp, int per);

/// Tube lisse le long d'un chemin (repères à rotation minimale). `rad(u)` donne le
/// rayon en fonction de l'abscisse u (0 au début, 1 à la fin) ; `flat` aplatit la
/// section (1 = ronde).
void addLoft(Mesh& m, const std::vector<vec3>& path, const std::function<float(float)>& rad, int seg, float mat,
             float flat = 1.0f);

/// Ellipsoïde finement subdivisé (`rows` x `cols`), orienté par la matrice `B`.
void addEllipsoidHiRes(Mesh& m, vec3 c, const mat3& B, vec3 r, float mat, int rows = 16, int cols = 24);
