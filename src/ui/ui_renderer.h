// =============================================================================
//  ui/ui_renderer.h — mini moteur de dessin 2D (HUD et menus)
//
//  Principe : entre begin() et flush(), chaque appel (rect, text, heart…) ajoute des
//  triangles colorés à un tableau ; flush() les envoie au GPU et les dessine en une
//  seule fois. Coordonnées en pixels, origine en haut à gauche de l'écran.
//  Aucune texture : le texte est fait de petits carrés (police 5 x 7).
// =============================================================================
#pragma once

#include <GL/glew.h>

#include <string>
#include <vector>

/// Lot de triangles 2D colorés + programme de dessin associé.
struct UiRenderer {
    GLuint prog = 0, vao = 0, vbo = 0;  ///< objets OpenGL
    std::vector<float> v;               ///< sommets en attente : (x, y, r, g, b, a) par sommet
    int W = 1, H = 1;                   ///< taille de l'écran en pixels

    /// Compile le shader 2D et crée le VAO / VBO. À appeler une fois après la création du contexte.
    void init();

    /// Commence une nouvelle image de `w` x `h` pixels (vide le lot).
    void begin(int w, int h);

    /// Rectangle plein (x, y = coin haut-gauche).
    void rect(float x, float y, float w, float h, float r, float g, float b, float a);

    /// Rectangle à dégradé : couleurs RGBA aux 4 coins (haut-gauche, haut-droit, bas-droit, bas-gauche).
    void rectGrad(float x, float y, float w, float h, const float* c00, const float* c10, const float* c11, const float* c01);

    /// Quadrilatère quelconque (4 sommets dans l'ordre).
    void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float r, float g, float b, float a);

    /// Triangle.
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a);

    /// Segment épais de `th` pixels.
    void line(float x0, float y0, float x1, float y1, float th, float r, float g, float b, float a);

    /// Disque (polygone à `seg` côtés).
    void disc(float cx, float cy, float rad, float r, float g, float b, float a, int seg = 16);

    /// Cœur en pixels (7 x 7 cases de `sc` pixels) : `fill` = 0 vide, 1 moitié, 2 plein.
    void heart(float x, float y, float sc, int fill, float alpha = 1.0f);

    /// Largeur en pixels du texte `s` à l'échelle `sc` (6 pixels de police par caractère).
    float textWidth(const std::string& s, float sc);

    /// Texte d'une couleur, chaque pixel de police mesurant `sc` pixels.
    void text(float x, float y, float sc, const std::string& s, float r, float g, float b, float a);

    /// Texte à dégradé vertical (couleur RGB `top` en haut -> `bot` en bas).
    void textGrad(float x, float y, float sc, const std::string& s, const float* top, const float* bot, float a);

    /// Halo lumineux autour d'un texte (copies décalées peu opaques, dans 8 directions).
    void textGlow(float x, float y, float sc, const std::string& s, float r, float g, float b, float spread, float a);

    /// Comme flush(), mais limite le dessin au rectangle donné (coordonnées écran).
    void flushClip(float x, float y, float w, float h);

    /// Dessine tout le lot en attente (mélange alpha, sans profondeur) puis le vide.
    void flush();
};
