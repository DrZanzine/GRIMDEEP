// =============================================================================
//  render/materials.h — identifiants de matériaux et palette de couleurs
//
//  Chaque sommet porte un numéro de matériau (Vertex::mat). Le fragment shader
//  (assets/shaders/scene.frag) lit ce numéro et choisit l'ombrage correspondant
//  (roche triplanaire, bois, eau, cristal…). Les numéros sont des flottants car ils
//  transitent par un attribut de sommet ; le shader les compare par intervalles.
//
//  Les matériaux >= 21 sont « génériques » : leur couleur diffuse et émissive est
//  lue dans une palette (uniformes uMdlA / uMdlE). La palette est remplie au
//  démarrage par les matériaux des modèles OBJ et par quelques couleurs du camp.
// =============================================================================
#pragma once

#include <vector>

#include "core/math_utils.h"

/// Numéros de matériaux reconnus par scene.frag.
namespace Mat {
constexpr float ROCK = 0.0f;            ///< roche des parois (textures triplanaires, humidité, mousse)
constexpr float ROCK_CORRIDOR = 0.2f;   ///< pierre lisse du couloir des catacombes (Vertex::seed = intensité)
constexpr float STONE_BLOCK = 0.4f;     ///< bloc d'éboulis (roche teintée selon Vertex::seed)
constexpr float WOOD = 1.0f;            ///< bois (texture de planche + carte de normales)
constexpr float TAR = 2.0f;             ///< étoupe goudronnée des torches (braises)
constexpr float WEB = 3.0f;             ///< toile d'araignée (motif procédural, alpha-to-coverage)
constexpr float WATER = 4.0f;           ///< flaque d'eau (reflets, rides)
constexpr float STEEL = 5.0f;           ///< acier (brides, clous, outils)
constexpr float FUR = 6.0f;             ///< fourrure de rat (Vertex::seed choisit queue / oreilles)
constexpr float HOLE = 7.0f;            ///< intérieur noir d'un trou (trous de rats, orbites)
constexpr float RED_GLOW = 8.0f;        ///< rouge émissif (yeux de rat, lit de braises de la forge)
constexpr float CRYSTAL = 9.0f;         ///< cristal luminescent des grottes (Vertex::seed = teinte)
constexpr float LAVA = 10.0f;           ///< lave émissive (flaques de la forge)
constexpr float BONE = 11.0f;           ///< ossements des catacombes
constexpr float MUSHROOM = 12.0f;       ///< champignon (Vertex::seed > 0.5 : chapeau luminescent)
constexpr float LANTERN_GLASS = 13.0f;  ///< verre de lanterne (émissif chaud)
constexpr float DARK_CLOTH = 14.0f;     ///< étoffe / cuir sombre
constexpr float CHITIN = 15.0f;         ///< chitine d'araignée (noire, très brillante)
constexpr float GOLEM = 16.0f;          ///< roche de golem veinée de lave
constexpr float EXIT_SKY = 17.0f;       ///< lumière du jour au fond de la sortie
constexpr float GOLD = 18.0f;           ///< or (clés, pépites)
constexpr float POTION = 19.0f;         ///< fiole de soin (rouge émissif)
constexpr float YELLOW_GLOW = 20.0f;    ///< jaune-orangé émissif (braises, yeux des ennemis de secours)
constexpr float MODEL_BASE = 21.0f;     ///< premier matériau générique de la palette
constexpr float LADY_SKIN = 90.0f;      ///< « la Dame » : peau marbrée
constexpr float LADY_DRESS = 91.0f;     ///< « la Dame » : robe déchirée
constexpr float LADY_HAIR = 92.0f;      ///< « la Dame » : cheveux
constexpr float LADY_EYE = 93.0f;       ///< « la Dame » : yeux rouges lumineux
constexpr float LADY_DARK = 94.0f;      ///< « la Dame » : orbites, bouche, ongles
constexpr float GRASS = 100.0f;         ///< herbe et chemin de terre du camp
constexpr float CHARRED_WOOD = 120.0f;  ///< bois calciné (Vertex::seed = 20 + intensité des braises x 10)
}  // namespace Mat

/// Nombre maximal de matériaux génériques (taille des tableaux uniformes du shader).
constexpr int MAX_MODEL_MATERIALS = 48;

/// Palette des matériaux génériques (numéros Mat::MODEL_BASE + index).
struct MaterialPalette {
    std::vector<vec4> diffuse;   ///< (couleur diffuse Kd, exposant spéculaire Ns)
    std::vector<vec4> emissive;  ///< (couleur émise Ke, brillance ; brillance < -0.5 = ombrage « cristal »)

    /// Ajoute un matériau et renvoie son numéro. Si la palette est pleine, renvoie Mat::STEEL.
    float append(vec3 kd, float ns, vec3 ke, float gloss);

    /// Comme append(), mais réutilise un matériau identique s'il existe déjà.
    float findOrAppend(vec3 kd, float ns, vec3 ke, float gloss);
};

/// Palette globale, envoyée au shader à chaque image (elle est animée : cristaux du camp).
extern MaterialPalette gMaterials;

/// Matériaux colorés enregistrés dans la palette par registerCustomMaterials().
/// Valeur par défaut 5 (= acier) tant qu'ils ne sont pas enregistrés.
struct CustomMaterials {
    float leaf[3] = {5, 5, 5};               ///< feuillages (vert foncé, vert clair, roux)
    float pine = 5;                          ///< aiguilles de pin
    float bark = 5;                          ///< écorce
    float sack = 5;                          ///< toile de jute des sacs
    float canvas = 5;                        ///< toile de tente
    float cloth = 5;                         ///< tissu rouge (porte de la tente)
    float crystal[5] = {5, 5, 5, 5, 5};      ///< cristaux géants du camp (une teinte chacun)
    float flash = 5;                         ///< éclair blanc-rouge d'un ennemi touché
    float dynamite = 5;                      ///< papier rouge des bâtons de dynamite
};

/// Numéros des matériaux colorés (remplis par registerCustomMaterials()).
extern CustomMaterials gCustomMats;

/// Couleurs de base des 5 cristaux géants du camp (magenta, cyan, ambre, vert, violet).
extern const vec3 CRYSTAL_COLORS[5];

/// Enregistre les matériaux colorés du camp, de la dynamite et de l'éclair d'impact.
/// À appeler avant le chargement des modèles (l'ordre fixe les numéros).
void registerCustomMaterials();
