// =============================================================================
//  render/mesh.h — maillages : sommets, sous-blocs (chunks) et planches
//
//  Un Mesh est construit côté CPU (tableaux `v` et `idx`), puis envoyé au GPU par
//  upload(). Il peut être découpé en « chunks » (portions contiguës d'indices avec
//  une sphère englobante) pour ne dessiner que ce qui est proche de la caméra ou
//  d'une lumière.
//
//  Un Mesh peut aussi enregistrer des « planches » (Board) : des boîtes orientées
//  destructibles qui servent à la fois à l'affichage, aux collisions et aux coups
//  de pioche (planches de soutènement, échelles, blocs d'éboulis…).
// =============================================================================
#pragma once

#include <GL/glew.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "core/math_utils.h"

/// Format de sommet commun à toute la géométrie du jeu (44 octets).
struct Vertex {
    vec3 p;       ///< position (monde, ou repère local pour les objets animés)
    vec3 n;       ///< normale
    vec2 uv;      ///< coordonnées de texture (sens dépendant du matériau)
    float mat;    ///< numéro de matériau (voir render/materials.h, namespace Mat)
    float seed;   ///< paramètre libre du matériau (variation, teinte, intensité…)
    float biome = 0.0f;  ///< biome (0..6, fractionnaire dans les transitions) : teinte, humidité, mousse
};

/// Portion contiguë d'indices d'un Mesh, avec sa sphère englobante (pour le culling).
struct Chunk {
    uint32_t first, count;  ///< premier indice et nombre d'indices
    vec3 c;                 ///< centre de la sphère englobante
    float r;                ///< rayon de la sphère englobante
};

/// Planche (ou bloc) : boîte orientée destructible, avec collision.
///
/// Repère local : `dl` = axe de la longueur, `dw` = axe de la largeur, `dt` = axe de
/// l'épaisseur ; `hl`, `hw`, `ht` sont les demi-dimensions correspondantes.
struct Board {
    vec3 c, dl, dw, dt;          ///< centre et axes (unitaires)
    float hl = 0, hw = 0, ht = 0;  ///< demi-longueur, demi-largeur, demi-épaisseur
    uint32_t i0 = 0;             ///< premier indice de sa géométrie dans le Mesh
    bool alive = true;           ///< false une fois cassée (géométrie effacée, plus de collision)
    int hp = 1;                  ///< nombre de coups de pioche avant rupture
    int frame = -1;              ///< cadre de soutènement auquel elle appartient (-1 : aucun)
    /// Rôle : 0 planche libre, 1 poteau, 2 linteau, 3 contrefiche, 7 bloc d'éboulis, 9 porte (indestructible).
    int role = 0;
    /// Poteaux / contrefiches : côté (0 gauche, 1 droite).
    /// Blocs d'éboulis : bit0 = touche le plafond, bit1 = posé au sol.
    int side = 0;
    int tube = 0;                ///< galerie dans laquelle elle se trouve
    uint32_t n = 36;             ///< nombre d'indices de sa géométrie (0 = invisible, portée par un modèle)
    int group = -1;              ///< objet monobloc (échelle / barricade) auquel elle appartient
    int wall = -1;               ///< éboulis auquel appartient le bloc (rôle 7)
};

/// Maillage indexé (géométrie CPU + tampons GPU).
struct Mesh {
    std::vector<Vertex> v;        ///< sommets
    std::vector<uint32_t> idx;    ///< indices (triangles)
    std::vector<Chunk> chunks;    ///< sous-blocs pour le culling
    std::vector<Board> boards;    ///< planches enregistrées (si `track`)
    bool track = false;           ///< si vrai, addBoard() enregistre chaque planche dans `boards`
    /// Géométrie produite par addBoard() : 0 boîte, 1 modèle de planche (plank.obj),
    /// 2 aucune (planche invisible, collision seule — la géométrie vient d'un modèle).
    int geom = 0;
    GLuint vao = 0, vbo = 0, ebo = 0;  ///< objets OpenGL (0 tant que non créés)
    size_t cIdx0 = 0, cVert0 = 0;      ///< début du chunk en cours (voir beginChunk)

    /// Commence un chunk : mémorise la position courante dans `idx` et `v`.
    void beginChunk() { cIdx0 = idx.size(); cVert0 = v.size(); }

    /// Termine le chunk commencé par beginChunk() (englobe les sommets ajoutés depuis).
    void endChunk() { endChunk(cVert0, v.size()); }

    /// Termine le chunk en calculant la sphère englobante des sommets [v0, v1).
    /// Ne fait rien si aucun indice n'a été ajouté.
    void endChunk(size_t v0, size_t v1);

    /// Crée le VAO et les tampons s'ils n'existent pas encore, et décrit le format Vertex.
    void ensure();

    /// Envoie les sommets et indices au GPU (GL_STATIC_DRAW par défaut, GL_STREAM_DRAW
    /// pour les maillages reconstruits à chaque image).
    void upload(GLenum usage = GL_STATIC_DRAW);

    /// Vide la géométrie CPU (les tampons GPU sont conservés).
    void clearCpu();

    /// Efface `n` indices à partir de `i0` (triangles dégénérés), côté CPU et GPU.
    /// Sert à faire disparaître une planche cassée sans tout renvoyer au GPU.
    void killIndices(uint32_t i0, uint32_t n);

    /// Libère les objets OpenGL.
    void destroy();

    /// Dessine les chunks pour lesquels `keep` renvoie vrai (culling par sphère).
    void draw(const std::function<bool(const Chunk&)>& keep) const;

    /// Dessine tout le maillage en un seul appel.
    void drawAll() const;
};
