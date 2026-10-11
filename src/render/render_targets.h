// =============================================================================
//  render/render_targets.h — tampons de rendu hors écran
//
//  Chaîne de rendu d'une image :
//    1. la scène est dessinée en HDR (RGBA16F) dans un tampon multi-échantillonné
//       (MSAA x4) : `msFbo` ;
//    2. ce tampon est « résolu » dans une texture simple : `resTex` ;
//    3. le bloom réduit cette texture par moitiés successives (`blTex[0..5]`), puis
//       remonte en additionnant les niveaux ;
//    4. compose.frag mélange `resTex` et `blTex[0]` vers l'écran.
//  Les tampons sont recréés quand la taille de la fenêtre change.
// =============================================================================
#pragma once

#include <GL/glew.h>

/// Nombre de niveaux de la pyramide de bloom.
constexpr int BLOOM_LEVELS = 6;

/// Ensemble des framebuffers et textures intermédiaires.
struct RenderTargets {
    int w = 0, h = 0;                    ///< taille actuelle (0 = pas encore créés)
    GLuint msFbo = 0;                    ///< framebuffer multi-échantillonné (scène HDR)
    GLuint msColor = 0, msDepth = 0;     ///< ses renderbuffers couleur et profondeur
    GLuint resFbo = 0, resTex = 0;       ///< scène résolue (sans MSAA)
    GLuint blTex[BLOOM_LEVELS] = {};     ///< textures de la pyramide de bloom
    GLuint blFbo[BLOOM_LEVELS] = {};     ///< framebuffers associés
    int blW[BLOOM_LEVELS] = {}, blH[BLOOM_LEVELS] = {};  ///< tailles des niveaux

    /// Libère tous les objets (sans effet s'ils n'existent pas).
    void destroy();

    /// (Re)crée tous les tampons pour une image de W x H pixels.
    void create(int W, int H);
};
