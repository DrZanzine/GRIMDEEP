// =============================================================================
//  game/settings.h — réglages du joueur (settings.ini) et plein écran
//
//  settings.ini est un simple fichier « clé=valeur » à la racine du projet :
//      master=0.800   music=0.700   sfx=0.800   sens=1.000   fullscreen=1
//  Il est relu au démarrage et réécrit en quittant l'écran des paramètres et à la
//  fermeture du jeu.
// =============================================================================
#pragma once

struct GLFWwindow;

/// Réglages modifiables dans le menu « Paramètres ».
struct Settings {
    float master = 0.8f;      ///< volume général (0..1)
    float music = 0.7f;       ///< volume de la musique (0..1)
    float sfx = 0.8f;         ///< volume des effets (0..1)
    float sens = 1.0f;        ///< sensibilité de la souris (0,2..2,5)
    bool fullscreen = true;   ///< plein écran
};

/// Lit settings.ini (s'il existe) et borne les valeurs.
void loadSettings(Settings& st);

/// Écrit settings.ini.
void saveSettings(const Settings& st);

/// Position et taille de la fenêtre en mode fenêtré (restaurées en quittant le plein écran).
struct WindowedState { int x = 100, y = 100, w = 1600, h = 900; };

/// Passe la fenêtre en plein écran (écran principal) ou la restaure en mode fenêtré.
void applyFullscreen(GLFWwindow* win, bool fs, WindowedState& ws);
