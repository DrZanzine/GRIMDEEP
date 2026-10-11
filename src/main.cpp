// =============================================================================
//  GRIMDEEP — A Dwarf in a Cave
//  Jeu d'exploration souterraine en OpenGL 4.3 (C++17, GLFW, GLEW, GLM, WinMM).
//
//  Point d'entrée : lit les options de la ligne de commande, localise le dossier
//  assets/, puis confie tout le reste à la classe Game (src/game/game.h).
//  Pour une vue d'ensemble du code, voir docs/ARCHITECTURE.md.
// =============================================================================
#include <memory>

#include "core/paths.h"
#include "game/game.h"
#include "game/options.h"

int main(int argc, char** argv) {
    initPaths();
    LaunchOptions options = parseOptions(argc, argv);
    // Game est volumineux (tampons, moteur audio…) : on l'alloue sur le tas.
    std::unique_ptr<Game> game(new Game(options));
    return game->run();
}
