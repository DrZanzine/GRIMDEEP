// =============================================================================
//  render/shader.h — chargement et compilation des programmes GLSL
//
//  Les sources GLSL sont dans assets/shaders/ (un fichier par étage) :
//    scene.vert / scene.frag        géométrie éclairée (tous les matériaux)
//    shadow.vert / shadow.frag      cartes d'ombre omnidirectionnelles (distance à la lumière)
//    dust.vert / dust.frag          poussière en suspension autour de la caméra
//    particles.vert / particles.frag flammes, braises, étincelles, fumée, feu d'explosion
//    fullscreen.vert                triangle plein écran (post-traitements)
//    sky.frag                       ciel du crépuscule (camp de base)
//    bloom_down.frag / bloom_up.frag halo lumineux (bloom) par pyramide de mipmaps
//    compose.frag                   composition finale (tonemapping ACES, vignettage, grain)
//  En cas d'erreur (fichier absent, compilation, édition des liens), le message est
//  affiché et le programme s'arrête.
// =============================================================================
#pragma once

#include <GL/glew.h>

/// Charge `assets/shaders/<vsFile>` et `assets/shaders/<fsFile>`, les compile et les
/// lie en un programme. Quitte le jeu avec un message si quelque chose échoue.
GLuint loadProgram(const char* vsFile, const char* fsFile);

/// Raccourci pour glGetUniformLocation.
inline GLint uniformLoc(GLuint program, const char* name) { return glGetUniformLocation(program, name); }
