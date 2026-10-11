// =============================================================================
//  render/renderer.cpp — création des ressources GPU
// =============================================================================
#include "render/renderer.h"

#include <vector>

#include "render/shader.h"
#include "render/textures.h"

void RenderResources::init() {
    // --- programmes GLSL
    progScene = loadProgram("scene.vert", "scene.frag");
    progShadow = loadProgram("shadow.vert", "shadow.frag");
    progDust = loadProgram("dust.vert", "dust.frag");
    progPart = loadProgram("particles.vert", "particles.frag");
    progSky = loadProgram("fullscreen.vert", "sky.frag");
    progDown = loadProgram("fullscreen.vert", "bloom_down.frag");
    progUp = loadProgram("fullscreen.vert", "bloom_up.frag");
    progComp = loadProgram("fullscreen.vert", "compose.frag");
    ui.init();

    // --- textures procédurales 512 x 512 (couleur sRGB + normales)
    {
        std::vector<uint8_t> c, n;
        generateRockTexture(512, c, n);
        texRockC = createTexture2D(512, c, true);
        texRockN = createTexture2D(512, n, false);
        generateWoodTexture(512, c, n);
        texWoodC = createTexture2D(512, c, true);
        texWoodN = createTexture2D(512, n, false);
    }

    // --- ombres omnidirectionnelles : SHADOW_SLOTS cube maps de distance
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, shadowTex);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_R32F, SHADOW_RES, SHADOW_RES, 6 * SHADOW_SLOTS);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &shadowDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, shadowDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SHADOW_RES, SHADOW_RES);
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, shadowDepth);

    // --- VAO des particules (8 flottants par point), de la poussière et du plein écran
    glGenVertexArrays(1, &partVao);
    glBindVertexArray(partVao);
    glGenBuffers(1, &partVbo);
    glBindBuffer(GL_ARRAY_BUFFER, partVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 32, (void*)16);
    glGenVertexArrays(1, &dustVao);
    glGenVertexArrays(1, &quadVao);
}
