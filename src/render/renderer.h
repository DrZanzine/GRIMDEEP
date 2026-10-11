// =============================================================================
//  render/renderer.h — ressources GPU partagées par toutes les passes de rendu
//
//  Regroupe ce qui est créé une fois au démarrage : programmes GLSL, textures
//  procédurales, cartes d'ombre, VAO des particules / de la poussière / du triangle
//  plein écran, tampons hors écran et moteur d'interface 2D. Les passes elles-mêmes
//  (ombres, scène, particules, post-traitement) sont dans game/game_render.cpp.
// =============================================================================
#pragma once

#include <GL/glew.h>

#include "render/render_targets.h"
#include "ui/ui_renderer.h"

/// Nombre de lumières qui projettent des ombres en même temps (cube maps).
constexpr int SHADOW_SLOTS = 4;
/// Résolution d'une face de cube map d'ombre (pixels).
constexpr int SHADOW_RES = 384;
/// Portée des cartes d'ombre (m).
constexpr float SHADOW_FAR = 40.0f;
/// Rayon d'influence d'une lumière ponctuelle dans les shaders (m).
constexpr float LIGHT_RADIUS = 19.0f;
/// Nombre maximal de lumières envoyées au shader (MAXL dans les shaders).
constexpr int MAX_LIGHTS = 32;

/// Ressources GPU.
struct RenderResources {
    // programmes (voir render/shader.h pour la liste des fichiers)
    GLuint progScene = 0, progShadow = 0, progDust = 0, progPart = 0, progSky = 0;
    GLuint progDown = 0, progUp = 0, progComp = 0;
    // textures procédurales
    GLuint texRockC = 0, texRockN = 0, texWoodC = 0, texWoodN = 0;
    // ombres : tableau de cube maps (distance à la lumière, R32F) + profondeur
    GLuint shadowTex = 0, shadowFbo = 0, shadowDepth = 0;
    // géométrie dynamique
    GLuint partVao = 0, partVbo = 0;  ///< particules : (position, taille) + (type, graine, vie, intensité)
    GLuint dustVao = 0;               ///< poussière (positions calculées dans le shader)
    GLuint quadVao = 0;               ///< triangle plein écran (positions calculées dans le shader)
    RenderTargets targets;            ///< tampons hors écran (recréés au redimensionnement)
    UiRenderer ui;                    ///< interface 2D

    /// Compile les programmes et crée toutes les ressources. Contexte OpenGL requis.
    void init();
};
