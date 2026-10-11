// =============================================================================
//  render/render_targets.cpp — création des framebuffers
// =============================================================================
#include "render/render_targets.h"

#include <algorithm>

void RenderTargets::destroy() {
    if (!msFbo) return;
    glDeleteFramebuffers(1, &msFbo);
    glDeleteRenderbuffers(1, &msColor);
    glDeleteRenderbuffers(1, &msDepth);
    glDeleteFramebuffers(1, &resFbo);
    glDeleteTextures(1, &resTex);
    glDeleteFramebuffers(BLOOM_LEVELS, blFbo);
    glDeleteTextures(BLOOM_LEVELS, blTex);
    msFbo = 0;
}

void RenderTargets::create(int W, int H) {
    destroy();
    w = W;
    h = H;
    // --- tampon MSAA x4 : couleur HDR + profondeur 32 bits flottante
    glGenFramebuffers(1, &msFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
    glGenRenderbuffers(1, &msColor);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
    glGenRenderbuffers(1, &msDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT32F, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);

    // Crée une texture RGBA16F filtrée et son framebuffer.
    auto makeColorTarget = [&](GLuint& tex, GLuint& fbo, int tw, int th) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, tw, th, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    };
    makeColorTarget(resTex, resFbo, W, H);
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        blW[i] = std::max(1, W >> (i + 1));
        blH[i] = std::max(1, H >> (i + 1));
        makeColorTarget(blTex[i], blFbo[i], blW[i], blH[i]);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
