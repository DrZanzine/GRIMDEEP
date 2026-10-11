// =============================================================================
//  core/paths.h — localisation des fichiers du jeu (assets, réglages)
//
//  Le jeu a besoin du dossier `assets/` (shaders et modèles 3D). Pour que
//  l'exécutable fonctionne qu'on le lance depuis la racine du projet (run.bat),
//  depuis build/ ou par double-clic, on cherche la « racine » du projet : le
//  premier dossier contenant `assets/shaders/` parmi le dossier courant, le
//  dossier de l'exécutable et ses deux parents.
// =============================================================================
#pragma once

#include <string>

/// Détermine la racine du projet. À appeler une fois au démarrage, avant tout chargement.
void initPaths();

/// Chemin d'un fichier du dossier assets, ex. `assetPath("shaders/scene.frag")`.
std::string assetPath(const std::string& relative);

/// Chemin du fichier de réglages `settings.ini` (à la racine du projet).
std::string settingsPath();
