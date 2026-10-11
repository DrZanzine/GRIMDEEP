// =============================================================================
//  render/model_loader.h — chargement des modèles 3D Wavefront (.obj + .mtl)
//
//  Les modèles sont dans assets/models/ et sont générés par tools/generate_models.py.
//  Conventions lues dans les fichiers :
//    - chaque groupe `g` est une partie animable (jambe, bras, patte…) ;
//    - un commentaire `# pivot <groupe> x y z` donne le point d'articulation du groupe ;
//    - les matériaux nommés wood, steel, steel_dark, nail, leather, chitin, chitin_dark,
//      fang sont remplacés par les matériaux procéduraux du jeu ; les autres sont ajoutés
//      à la palette (couleur et émission du .mtl).
//  Si un modèle est absent, `ok` reste faux et le jeu utilise des formes procédurales.
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "render/mesh.h"

/// Modèle chargé en mémoire (prêt à être recopié dans un Mesh par appendModel()).
struct ModelData {
    std::vector<Vertex> v;            ///< sommets (matériau dans Vertex::mat)
    std::vector<int> vg;              ///< groupe (partie animable) de chaque sommet
    std::vector<uint32_t> idx;        ///< indices des triangles
    std::vector<std::string> gname;   ///< nom de chaque groupe (le groupe 0 est "body")
    std::vector<vec3> gpivot;         ///< point d'articulation de chaque groupe
    bool ok = false;                  ///< vrai si le chargement a réussi

    /// Index du groupe nommé `n`, ou -1 s'il n'existe pas.
    int group(const std::string& n) const {
        for (size_t i = 0; i < gname.size(); i++)
            if (gname[i] == n) return (int)i;
        return -1;
    }
};

/// Tous les modèles utilisés par le jeu.
struct ModelLibrary {
    ModelData barricade;  ///< planks_barricade : barricade qui barre une galerie
    ModelData ladder;     ///< ladder : échelle appuyée contre la paroi
    ModelData plank;      ///< plank : planche clouée (remplace les boîtes du coffrage et des débris)
    ModelData mummy;      ///< mummy : le « rôdeur »
    ModelData spider;     ///< giant_spider : araignée géante
    ModelData golem;      ///< lava_golem : golem de lave
};

/// Bibliothèque globale, remplie par loadAllModels().
extern ModelLibrary gModels;

/// Charge `assets/models/<name>.obj` et son `.mtl` dans `md`. Renvoie md.ok.
/// Les matériaux non standard sont ajoutés à la palette gMaterials.
bool loadModel(const std::string& name, ModelData& md);

/// Charge tous les modèles de gModels (l'ordre fixe les numéros de matériaux).
void loadAllModels();

/// Recopie le modèle `md` dans le maillage `m`.
///   - `W`     : matrice de placement (modèle -> monde) ;
///   - `seed`  : valeur écrite dans Vertex::seed (variation de texture) ;
///   - `uvOff` : décalage ajouté aux coordonnées de texture ;
///   - `gx`    : matrices optionnelles par groupe, appliquées avant W (animation des membres).
void appendModel(Mesh& m, const ModelData& md, const mat4& W, float seed, vec2 uvOff = vec2(0),
                 const std::vector<mat4>* gx = nullptr);

/// Ajoute une planche de jeu (boîte orientée c, dl, dt, demi-dimensions hl, hw, ht)
/// représentée par le modèle plank.obj (x = longueur, y = épaisseur, z = largeur).
void addPlankModel(Mesh& m, vec3 c, vec3 dl, vec3 dt, float hl, float hw, float ht, float seed);
