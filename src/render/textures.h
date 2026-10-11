// =============================================================================
//  render/textures.h — textures procédurales (roche et bois)
//
//  Aucune image n'est chargée depuis le disque : la roche et le bois sont calculés
//  au démarrage (bruit périodique + cellules de Voronoï/Worley). Les textures sont
//  « tileables » (elles se répètent sans couture), et chacune est accompagnée d'une
//  carte de normales déduite d'une carte de hauteur.
// =============================================================================
#pragma once

#include <GL/glew.h>

#include <cstdint>
#include <vector>

/// Génère la texture de roche N x N : `col` = couleur sRGB + brillance dans l'alpha,
/// `nrm` = carte de normales. Gros blocs en marches, facettes inclinées, fractures,
/// grain et paillettes minérales.
void generateRockTexture(int N, std::vector<uint8_t>& col, std::vector<uint8_t>& nrm);

/// Génère la texture de bois N x N (u = le long du fil) : cernes déformés, stries,
/// fentes ; brillance dans l'alpha, plus la carte de normales.
void generateWoodTexture(int N, std::vector<uint8_t>& col, std::vector<uint8_t>& nrm);

/// Crée une texture OpenGL 2D RGBA8 (ou SRGB8_ALPHA8 si `srgb`) répétée, avec mipmaps
/// et filtrage anisotrope x8.
GLuint createTexture2D(int N, const std::vector<uint8_t>& px, bool srgb);
