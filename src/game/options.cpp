// =============================================================================
//  game/options.cpp — analyse de la ligne de commande
// =============================================================================
#include "game/options.h"

#include <chrono>
#include <cstdlib>

LaunchOptions parseOptions(int argc, char** argv) {
    LaunchOptions o;
    o.seed = (uint32_t)std::chrono::system_clock::now().time_since_epoch().count() % 100000;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) o.seed = (uint32_t)std::atoi(argv[++i]);
        else if (a == "--shot" && i + 1 < argc) o.shot = argv[++i];
        else if (a == "--at" && i + 1 < argc) { o.startAt = (float)std::atof(argv[++i]); o.startAtGiven = true; }
        else if (a == "--look" && i + 1 < argc) o.lookOff = (float)std::atof(argv[++i]);
        else if (a == "--pitch" && i + 1 < argc) o.lookPitch = (float)std::atof(argv[++i]);
        else if (a == "--w" && i + 1 < argc) o.winW = std::atoi(argv[++i]);
        else if (a == "--h" && i + 1 < argc) o.winH = std::atoi(argv[++i]);
        else if (a == "--frames" && i + 1 < argc) o.shotFrames = std::atoi(argv[++i]);
        else if (a == "--branch" && i + 1 < argc) o.startBranch = std::atoi(argv[++i]);
        else if (a == "--biome" && i + 1 < argc) o.startBiome = std::atoi(argv[++i]);
        else if (a == "--enemy" && i + 1 < argc) o.testEnemy = std::atoi(argv[++i]);
        else if (a == "--fly") o.startFly = true;
        else if (a == "--ladder") o.atLadder = true;
        else if (a == "--barricade") o.atBarricade = true;
        else if (a == "--hub") o.startHub = true;
        else if (a == "--noclip") o.noclip = true;
        else if (a == "--god") o.god = true;
        else if (a == "--corr") o.corridorTest = true;
        else if (a == "--dyn" && i + 1 < argc) o.dynFrame = std::atoi(argv[++i]);
        else if (a == "--dame" && i + 1 < argc) o.ladyDist = (float)std::atof(argv[++i]);
        else if (a == "--eyeoff" && i + 3 < argc) {
            o.eyeOff = vec3((float)std::atof(argv[i + 1]), (float)std::atof(argv[i + 2]), (float)std::atof(argv[i + 3]));
            i += 3;
        } else if (a == "--campos" && i + 3 < argc) {
            o.camGiven = true;
            o.camPos = vec3((float)std::atof(argv[i + 1]), (float)std::atof(argv[i + 2]), (float)std::atof(argv[i + 3]));
            i += 3;
        } else if (a == "--menu") o.menu = true;
        else if (a == "--pause") o.pause = true;
        else if (a == "--settings") o.settings = true;
        else if (a == "--controls") o.controls = true;
        else if (a == "--windowed") o.windowed = true;
        else if (a == "--hubpos" && i + 2 < argc) {
            o.startHub = true;
            o.hubX = (float)std::atof(argv[++i]);
            o.hubZ = (float)std::atof(argv[++i]);
        } else if (a == "--axe") o.forceAxe = true;
        else if (a == "--swing" && i + 1 < argc) { o.forceAxe = true; o.forceSwing = (float)std::atof(argv[++i]); }
        else if (a == "--autowalk") o.autoWalk = true;
        else if (a == "--rat") o.forceRat = true;
        else if (a == "--ratshow") o.ratShow = true;
        else if (a == "--break") o.forceBreak = true;
        else if (a == "--door") o.atDoor = true;
        else if (a == "--exit") o.atExit = true;
        else if (a == "--noaudio") o.noAudio = true;
        else if (a == "--noenemies") o.noEnemies = true;
        else if (a == "--open") o.openDoor = true;
        else if (a == "--hp" && i + 1 < argc) o.hp = (float)std::atof(argv[++i]);
        else if (a == "--autoe") o.autoE = true;
        else if (a == "--keys" && i + 1 < argc) o.keys = std::atoi(argv[++i]);
    }
    return o;
}
