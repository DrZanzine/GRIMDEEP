// =============================================================================
//  game/game.cpp — cycle de vie du jeu et déroulement d'une image
// =============================================================================
#include "game/game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/png_writer.h"
#include "entities/props.h"
#include "render/materials.h"
#include "render/model_loader.h"
#include "render/shader.h"

Game::Game(const LaunchOptions& options) : opt(options), seed(options.seed) { fly = options.startFly; }

int Game::run() {
    if (!init()) return 1;
    while (!glfwWindowShouldClose(win)) {
        if (!tickFrame()) break;
    }
    shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
//  Démarrage
// ---------------------------------------------------------------------------

bool Game::init() {
    // --- fenêtre OpenGL 4.3 (plein écran au démarrage, sauf en mode capture)
    if (!glfwInit()) { std::fprintf(stderr, "GLFW init failed\n"); return false; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    loadSettings(cfg);
    if (opt.windowed) cfg.fullscreen = false;
    wst.w = opt.winW;
    wst.h = opt.winH;
    GLFWmonitor* mon0 = glfwGetPrimaryMonitor();
    const GLFWvidmode* vm0 = glfwGetVideoMode(mon0);
    if (cfg.fullscreen && opt.shot.empty()) {
        glfwWindowHint(GLFW_RED_BITS, vm0->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, vm0->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, vm0->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, vm0->refreshRate);
        win = glfwCreateWindow(vm0->width, vm0->height, "GRIMDEEP", mon0, nullptr);
    } else win = glfwCreateWindow(opt.winW, opt.winH, "GRIMDEEP", nullptr, nullptr);
    if (!win) { std::fprintf(stderr, "Impossible de creer la fenetre OpenGL 4.3\n"); return false; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(opt.shot.empty() ? 1 : 0);  // synchro verticale (désactivée en capture)
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::fprintf(stderr, "GLEW init failed\n"); return false; }
    std::printf("GL: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    // --- son (pas de son en mode capture)
    if (opt.shot.empty() && !opt.noAudio) {
        if (!audio.start()) std::printf("Audio indisponible\n");
    }
    audio.setVolumes(cfg.master, cfg.music, cfg.sfx);

    // --- ressources : shaders, textures, matériaux, modèles, objets construits en code
    gfx.init();
    registerCustomMaterials();
    loadAllModels();
    glUseProgram(gfx.progScene);
    if (!gMaterials.diffuse.empty()) {
        glUniform4fv(uniformLoc(gfx.progScene, "uMdlA"), (GLsizei)gMaterials.diffuse.size(), glm::value_ptr(gMaterials.diffuse[0]));
        glUniform4fv(uniformLoc(gfx.progScene, "uMdlE"), (GLsizei)gMaterials.emissive.size(), glm::value_ptr(gMaterials.emissive[0]));
    }
    buildPickaxe(axeMesh);
    axeMesh.upload();
    buildLady(lady);
    lady.mesh.upload();
    buildLantern(lanternMesh);
    lanternMesh.upload();
    world.build(seed);

    // --- palettes de biomes pour les shaders
    for (int b = 0; b < 7; b++)
        for (int c = 0; c < 3; c++) {
            palTint[b * 3 + c] = BIOMES[b].tint[c];
            palAmb[b * 3 + c] = BIOMES[b].amb[c];
            palFlame[b * 3 + c] = BIOMES[b].torch[c];
        }
    for (int b = 0; b < 5; b++)
        for (int c = 0; c < 3; c++) palFlame[(7 + b) * 3 + c] = CRYSTAL_COLORS[b][c];
    palFlame[36] = 1.0f;  // 12 : rouge des yeux de « la Dame »
    palFlame[37] = 0.05f;
    palFlame[38] = 0.03f;
    for (int b = 0; b < 7; b++) {
        palWet[b] = BIOMES[b].wet;
        palMoss[b] = BIOMES[b].moss;
    }

    // --- position de départ (camp par défaut, ou selon les options de test)
    resetMap();
    float startMeters = opt.startAt;
    if (opt.startBiome >= 0) startMeters = (opt.startBiome + 0.5f) * world.doorRing / 5.0f * 0.3f;
    if (opt.atDoor) startMeters = (world.doorRing - 9) * 0.3f;
    if (opt.atExit) startMeters = (world.tubes[0].N - 14) * 0.3f;
    bool tubeStart = opt.startAtGiven || opt.startBiome >= 0 || opt.atDoor || opt.atExit || opt.startBranch > 0 || opt.atLadder || opt.atBarricade;
    if (tubeStart) placePlayer(startMeters);
    else placeHub();
    if ((opt.atLadder || opt.atBarricade) && !world.props.empty()) {  // test : se place face à la première échelle / barricade
        const World::ModelProp* pL = &world.props[0];
        for (const auto& l : world.props)
            if (l.kind == (opt.atBarricade ? 1 : 0)) { pL = &l; break; }
        const auto& L = *pL;
        vec3 d = L.look - L.stand;
        pos = L.stand;
        yaw = std::atan2(d.z, d.x);
        pitch = 0.0f;
    }
    if (opt.startHub) {
        pos = vec3(opt.hubX, world.hubGround(opt.hubX, world.hubFrontZ + opt.hubZ) + 1.65f, world.hubFrontZ + opt.hubZ);
        yaw = -PI * 0.5f + glm::radians(opt.lookOff);
        pitch = glm::radians(opt.lookPitch);
    }
    if (opt.corridorTest && world.corridorStart >= 0) {
        opt.startBranch = 0;
        placePlayer((world.corridorStart - 30) * 0.3f);
    }
    if (opt.camGiven) {
        pos = opt.camPos;
        yaw = glm::radians(opt.lookOff);
        pitch = glm::radians(opt.lookPitch);
        fly = true;
    }
    initEntities();
    keys = opt.keys;
    hp = opt.hp;
    if (opt.openDoor) world.openDoor();

    // --- horloges, écran de départ
    ratTimer = grng.r(14.0f, 30.0f);
    for (int s = 0; s < SHADOW_SLOTS; s++) slotLight[s] = -1;
    lastT = glfwGetTime();
    fpsT = lastT;
    tStart = glfwGetTime();
    screen = !opt.shot.empty() ? SC_PLAY : SC_TITLE;
    backTo = SC_TITLE;
    if (opt.menu) screen = SC_TITLE;
    if (opt.pause) screen = SC_PAUSE;
    if (opt.settings) { screen = SC_SETTINGS; backTo = SC_TITLE; }
    if (opt.controls) { screen = SC_CONTROLS; backTo = SC_TITLE; }
    std::printf("ZQSD/WASD deplacement | Maj courir | clic gauche pioche | E interagir | F vol | R nouvelle grotte | F12 capture | Echap quitter\n");
    return true;
}

void Game::shutdown() {
    saveSettings(cfg);
    audio.stop();
    glfwTerminate();
}

// ---------------------------------------------------------------------------
//  Une image
// ---------------------------------------------------------------------------

bool Game::tickFrame() {
    glfwPollEvents();
    beginFrameTiming();
    handleSystemKeys();
    live = (screen == SC_PLAY);
    titleCam = (screen == SC_TITLE) || ((screen == SC_SETTINGS || screen == SC_CONTROLS) && backTo == SC_TITLE);
    if (!live) dt = 0.0f;  // le jeu est figé dans les menus
    else menuSel = 0;
    if (glfwWindowShouldClose(win)) return false;
    updateScreenState();

    // --- simulation
    handleGameplayInput();
    updateMovement();
    if (live) revealMap();
    if (titleCam) updateTitleCamera();
    updatePickaxe();
    if (glm::length(opt.eyeOff) > 0) eye += rightv * opt.eyeOff.x + vec3(0, opt.eyeOff.y, 0) - flat * opt.eyeOff.z;  // (test) caméra décalée
    updateBiome();
    updateEnemies();
    updateLady();
    updateDynamite();
    if (!wallHint)
        for (const auto& w : world.rockfalls)
            if (glm::length(pos - w.c) < 8.0f) {
                wallHint = true;
                say("UN EBOULIS BARRE LA GALERIE : ABATS LES BLOCS A LA PIOCHE !");
                break;
            }
    updatePickups();
    updateDrips();

    // --- rendu
    if (!prepareFrame()) return true;  // fenêtre réduite : on saute le rendu
    updateDebrisAndRats();
    buildDynamicMeshes();
    buildLights();
    renderShadows();
    renderScene();
    renderParticles();
    renderHeldItems();
    postProcess();
    drawInterface();
    if (handleCapture()) return false;
    present();
    return true;
}

void Game::beginFrameTiming() {
    now = glfwGetTime();
    dt = (float)std::min(now - lastT, 0.05);  // pas de temps borné (pas de saut après un ralentissement)
    lastT = now;
    if (!opt.shot.empty()) dt = 1.0f / 30.0f;  // capture : temps fixe, résultat reproductible
    rdt = dt;
    if (hitStop > 0.0f) {  // arrêt sur image au moment de l'impact
        hitStop -= rdt;
        dt *= 0.1f;
    } else if (slowMo > 0.0f) {  // ralenti après l'explosion
        slowMo -= rdt;
        dt *= 0.3f + 0.7f * std::max(0.0f, 1.0f - slowMo / 0.75f);
    }
}

void Game::updateScreenState() {
    audio.setDuck(live ? std::max(0.2f, 1.0f - muffle) : 0.5f);  // musique atténuée dans les menus
    muffle = std::max(0.0f, muffle - rdt * 0.3f);
    audio.setVolumes(cfg.master, cfg.music, cfg.sfx);
    if (opt.shot.empty() && (screen != SC_PLAY) != cursorFree) {  // curseur visible dans les menus seulement
        cursorFree = !cursorFree;
        glfwSetInputMode(win, GLFW_CURSOR, cursorFree ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }
    simT += dt;
    if (!live) menuT += rdt;
    T = (float)(simT + menuT) + 3.0f;
    if (state == PLAY) playTime += dt;
}

// ---------------------------------------------------------------------------
//  Placement et nouvelle partie
// ---------------------------------------------------------------------------

void Game::placePlayer(float meters) {
    const Tube* tb = &world.tubes[0];
    int i = std::min(tb->N - 12, std::max(6, (int)(meters / tb->ds)));
    if (opt.startBranch > 0 && opt.startBranch < (int)world.tubes.size()) {
        tb = &world.tubes[opt.startBranch];
        i = std::min(tb->N - 8, std::max(6, (int)(meters / tb->ds)));
    }
    const Ring& r = tb->rings[i];
    pos = tb->toWorld(i, 0.0f, tb->floorY(i, 0.0f) + 1.65f);
    hints.assign(world.tubes.size(), -1);
    yaw = std::atan2(r.T.z, r.T.x) + glm::radians(opt.lookOff);  // regard dans le sens de la galerie
    pitch = glm::radians(opt.lookPitch);
}

void Game::placeHub() {
    hn = Haunt();
    pos = world.hubSpawn;
    yaw = -PI * 0.5f;  // face à la montagne
    pitch = 0.0f;
    hints.assign(world.tubes.size(), -1);
}

void Game::initEntities() {
    enemies.clear();
    for (auto& sp : world.spawns) {
        Enemy e;
        e.type = sp.type;
        e.pos = sp.pos;
        e.hp = ENEMY_STATS[sp.type].hp;
        e.hints.assign(world.tubes.size(), -1);
        enemies.push_back(e);
    }
    if (opt.noEnemies) enemies.clear();
    if (opt.testEnemy >= 0) {  // (test) un ennemi juste devant le joueur, qui le regarde
        Enemy e;
        e.type = opt.testEnemy % 3;
        vec3 fl = glm::normalize(vec3(std::cos(yaw), 0, std::sin(yaw)));
        e.pos = pos + fl * 2.3f;
        e.face = -fl;
        e.hp = ENEMY_STATS[e.type].hp;
        e.hints.assign(world.tubes.size(), -1);
        enemies.push_back(e);
    }
}

void Game::resetMap() {
    mapSeen.assign(world.tubes.size(), {});
    mapEdge.assign(world.tubes.size(), {});
    for (size_t t = 0; t < world.tubes.size(); t++) {
        mapSeen[t].assign((size_t)world.tubes[t].N, 0);
        mapEdge[t].assign((size_t)world.tubes[t].N, vec4(0));
    }
    std::fill(hubSeen.begin(), hubSeen.end(), 0);
}

void Game::restart(bool newSeed) {
    if (newSeed) {  // générateur congruentiel : nouvelle graine dérivée de l'ancienne
        seed = seed * 1664525u + 1013904223u;
        seed %= 100000;
    }
    world.build(seed);
    hints.clear(); parts.clear(); drops.clear(); rags.clear(); dyns.clear(); blasts.clear();
    hp = 100; keys = 0; kills = 0; state = PLAY; playTime = 0; hurtFlash = 0;
    msg.clear();
    for (int b = 0; b < 7; b++)
        for (int c = 0; c < 3; c++) palFlame[b * 3 + c] = BIOMES[b].torch[c];
    placeHub();
    initEntities();
    curBiome = -1;
    resetMap();
}

void Game::startPlaying() {
    // première partie : on garde la grotte déjà générée ; ensuite, une nouvelle à chaque fois
    if (worldDirty) restart(true);
    else {
        hp = 100; keys = 0; kills = 0; state = PLAY; playTime = 0; hurtFlash = 0;
        msg.clear();
        placeHub();
        curBiome = -1;
    }
    worldDirty = true;
    screen = SC_PLAY;
    menuSel = 0;
    for (int s2 = 0; s2 < SHADOW_SLOTS; s2++) slotLight[s2] = -1;
    rats.clear();
    say("TROUVE LES 3 CLES DANS LA MINE, OUVRE LA GRANDE PORTE ET SORS !");
}

void Game::say(const std::string& s) {
    msg = s;
    msgT = 3.5f;
}

// ---------------------------------------------------------------------------
//  Capture et affichage
// ---------------------------------------------------------------------------

bool Game::handleCapture() {
    bool kP = glfwGetKey(win, GLFW_KEY_F12) == GLFW_PRESS;
    bool autoShot = !opt.shot.empty() && frame == opt.shotFrames;
    if ((kP && !keyP) || autoShot) {
        // lecture du tampon arrière, retourné verticalement (OpenGL : origine en bas)
        std::vector<uint8_t> px8((size_t)fbw * fbh * 3), flip((size_t)fbw * fbh * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, px8.data());
        for (int y = 0; y < fbh; y++) std::memcpy(&flip[(size_t)y * fbw * 3], &px8[(size_t)(fbh - 1 - y) * fbw * 3], (size_t)fbw * 3);
        std::string name = autoShot ? opt.shot : "capture_" + std::to_string((int)now) + ".png";
        writePNG(name.c_str(), flip, fbw, fbh);
        std::printf("Capture : %s", name.c_str());
        std::puts("");
        if (autoShot) {  // résumé de l'état de la partie (utile pour les tests automatiques)
            size_t dead = 0;
            for (auto& bd : world.woodMesh.boards)
                if (!bd.alive) dead++;
            std::printf("biome %d (%s) | PV %.0f | tues %d | cles %d/%d | planches detruites %zu | debris %zu | rats %zu | ennemis vivants %zu | gouttes %zu",
                        curBiome, BIOMES[std::min(6, std::max(0, curBiome))].name, hp, kills, keys, world.keysNeeded, dead, world.debris.size(), rats.size(),
                        (size_t)std::count_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; }), drops.size());
            std::puts("");
            std::printf("%.1f ms/frame", (glfwGetTime() - tStart) * 1000.0 / frame);
            std::puts("");
            return true;
        }
    }
    keyP = kP;
    return false;
}

void Game::present() {
    glfwSwapBuffers(win);
    frame++;
    fpsN++;
    if (now - fpsT > 1.0 && opt.shot.empty()) {  // titre de la fenêtre : graine, images/s, mode
        char title[160];
        std::snprintf(title, sizeof title, "GRIMDEEP - A Dwarf in a Cave | seed %u | %d FPS | %s", seed, fpsN, fly ? "vol" : "marche");
        glfwSetWindowTitle(win, title);
        fpsN = 0;
        fpsT = now;
    }
}
