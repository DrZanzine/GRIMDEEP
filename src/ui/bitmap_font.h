// =============================================================================
//  ui/bitmap_font.h — police de caractères pixel 5 x 7
//
//  Chaque caractère est décrit par 7 lignes de 5 pixels ("1" = allumé), séparées par
//  des espaces : "01110 10001 ...". Seules les majuscules, les chiffres et quelques
//  signes sont définis (le texte est mis en majuscules avant affichage ; les accents
//  ne sont pas gérés, d'où les textes du jeu sans accents).
// =============================================================================
#pragma once

/// Motif 5 x 7 du caractère `c` (35 chiffres + 6 espaces), ou nullptr s'il n'existe pas.
const char* glyphPattern(char c);
