// =============================================================================
//  core/png_writer.h — écriture d'images PNG sans dépendance externe
//
//  Utilisé pour les captures d'écran (F12 et option `--shot`). Les données sont
//  stockées sans compression (blocs deflate « stored ») : le fichier est plus gros
//  mais l'encodeur tient en quelques dizaines de lignes.
// =============================================================================
#pragma once

#include <cstdint>
#include <vector>

/// Écrit `rgb` (w x h pixels, 3 octets par pixel, première ligne = haut de l'image)
/// dans le fichier PNG `path`. Ne fait rien si le fichier ne peut pas être créé.
void writePNG(const char* path, const std::vector<uint8_t>& rgb, int w, int h);
