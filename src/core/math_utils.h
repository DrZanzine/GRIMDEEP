// =============================================================================
//  core/math_utils.h — petites fonctions mathématiques utilisées partout
//
//  Ce fichier regroupe les alias GLM et les fonctions scalaires élémentaires
//  (interpolation, lissage, courbes d'accélération). Tout est `inline` : aucune
//  unité de compilation associée.
// =============================================================================
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

// Alias courts pour les types GLM, utilisés dans tout le projet.
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

/// Constante pi en simple précision.
static const float PI = 3.14159265358979f;

/// Interpolation linéaire : renvoie a quand t = 0 et b quand t = 1.
static inline float mixf(float a, float b, float t) { return a + (b - a) * t; }

/// Équivalent du `smoothstep` GLSL : 0 avant `a`, 1 après `b`, courbe en S entre les deux.
static inline float smoothStep(float a, float b, float x) {
    float t = std::min(std::max((x - a) / (b - a), 0.0f), 1.0f);
    return t * t * (3 - 2 * t);
}

/// Courbe de fondu quintique de Perlin (6t^5 - 15t^4 + 10t^3) : dérivées première et
/// seconde nulles en 0 et 1, ce qui évite les cassures visibles dans le bruit.
static inline float quinticFade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }

/// Accélération « ease-out » : démarre vite et ralentit en arrivant à 1.
static inline float easeOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

/// Accélération « ease-in » : démarre lentement et accélère jusqu'à 1.
static inline float easeIn(float t) { return t * t; }
