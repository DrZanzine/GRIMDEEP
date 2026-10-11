// =============================================================================
//  game/game.h — la classe Game : état de la partie et boucle principale
//
//  Game possède tout : la fenêtre, le son, les ressources GPU, le monde, le joueur,
//  les ennemis, les effets… Une image (tickFrame) se déroule toujours dans cet ordre :
//
//     1. temps et entrées système (Echap, F11, menus)      game.cpp / game_player.cpp
//     2. entrées de jeu, déplacement du joueur, caméra      game_player.cpp
//     3. pioche, ennemis, « la Dame », dynamite             game_combat.cpp / game_ambient.cpp
//     4. objets, sortie, gouttes d'eau                      game_player.cpp / game_ambient.cpp
//     5. débris, rats, ragdolls, maillages dynamiques       game_ambient.cpp / game_render.cpp
//     6. lumières et ombres                                 game_lighting.cpp
//     7. rendu : ombres, scène HDR, particules, objets en main, post-traitement
//                                                           game_render.cpp
//     8. interface : HUD et minicarte, menus                game_hud.cpp / game_menus.cpp
//     9. capture d'écran éventuelle, affichage              game.cpp
//
//  L'ordre est important : plusieurs générateurs aléatoires sont partagés entre ces
//  étapes, et la partie est entièrement reproductible pour une graine donnée.
// =============================================================================
#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/random.h"
#include "entities/dynamite.h"
#include "entities/enemy.h"
#include "entities/lady.h"
#include "entities/particles.h"
#include "entities/ragdoll.h"
#include "entities/rat.h"
#include "game/options.h"
#include "game/settings.h"
#include "render/renderer.h"
#include "world/world.h"

/// Le jeu complet.
struct Game {
    /// Prépare le jeu avec les options de lancement (rien n'est créé avant run()).
    explicit Game(const LaunchOptions& options);

    /// Crée la fenêtre et les ressources, joue jusqu'à la fermeture, puis libère tout.
    /// Renvoie le code de sortie du programme.
    int run();

    // =========================================================================
    //  Types internes
    // =========================================================================

    /// État de la partie.
    enum PlayState { PLAY, DEAD, WIN };

    /// Écran affiché.
    enum Screen { SC_TITLE, SC_PLAY, SC_PAUSE, SC_SETTINGS, SC_CONTROLS };

    /// Apparitions de « la Dame » : 3 événements (0 entrée de la mine, 1 couloir des
    /// catacombes, 2 près de la sortie), chacun joué une seule fois.
    struct Haunt {
        int st[3] = {0, 0, 0};       ///< état de chaque événement : 0 à venir, 1 en cours, 2 terminé
        int ev = -1;                 ///< événement en cours (-1 : aucun)
        vec3 p = vec3(0);            ///< position de ses pieds
        vec3 f = vec3(0, 0, 1);      ///< direction de son regard
        float dis = -1;              ///< >= 0 : en train de se dissoudre (secondes écoulées)
        float lookT = 0;             ///< temps passé à la regarder (événement 2)
    };

    /// Lumière retenue pour l'image en cours.
    struct LightInfo {
        vec3 p, c;     ///< position, couleur x intensité
        float dist;    ///< distance à la caméra (tri)
        /// Source : index dans World::lights, ou identifiant spécial : 100000 lanterne,
        /// 300000+ orbes du camp, 310000+ projecteurs, 400000 la Dame, 500000+ explosions,
        /// 510000+ mèches de dynamite.
        int src;
        float inten;   ///< (inutilisé, toujours 1)
        int biome;     ///< biome / index de couleur (choix de la couleur des flammes)
    };

    /// Souris dans les menus (coordonnées du framebuffer).
    struct MouseState { float x = 0, y = 0; bool down = false, click = false, moved = false; };

    // =========================================================================
    //  Configuration et ressources
    // =========================================================================
    LaunchOptions opt;            ///< options de lancement
    uint32_t seed;                ///< graine de la grotte actuelle
    Settings cfg;                 ///< réglages du joueur
    WindowedState wst;            ///< fenêtre restaurée en quittant le plein écran
    GLFWwindow* win = nullptr;    ///< fenêtre GLFW
    aud::Engine audio;            ///< son
    RenderResources gfx;          ///< ressources GPU
    Mesh axeMesh;                 ///< pioche (vue en main)
    Mesh lanternMesh;             ///< lanterne (vue en main)
    Mesh enemyMesh;               ///< ennemis et ragdolls (reconstruit à chaque image)
    Mesh propsMesh;               ///< clés, fioles, dynamite (reconstruit à chaque image)
    LadyModel lady;               ///< « la Dame »
    World world;                  ///< le monde
    // palettes envoyées aux shaders (biomes, couleurs des flammes)
    GLfloat palTint[21], palAmb[21], palWet[7], palMoss[7];
    GLfloat palFlame[39] = {};    ///< 13 couleurs : 0..6 torches des biomes, 7..11 cristaux du camp, 12 yeux de la Dame

    // =========================================================================
    //  État de la partie
    // =========================================================================
    int state = PLAY;             ///< PLAY, DEAD ou WIN
    float hp = 100;               ///< points de vie (100 = 10 cœurs)
    float hurtFlash = 0;          ///< rouge à l'écran après un coup reçu
    float playTime = 0;           ///< durée de la partie (affichée à la victoire)
    int keys = 0;                 ///< clés ramassées
    int kills = 0;                ///< ennemis tués
    std::string msg;              ///< message affiché au centre de l'écran
    float msgT = 0;               ///< temps d'affichage restant du message
    std::vector<Enemy> enemies;   ///< ennemis
    std::vector<Drop> drops;      ///< gouttes d'eau en chute
    std::vector<Particle> parts;  ///< particules
    std::vector<Ragdoll> rags;    ///< corps des ennemis tués (10 au maximum)
    std::vector<Dynamite> dyns;   ///< bâtons de dynamite (6 au maximum)
    std::vector<Blast> blasts;    ///< explosions récentes
    std::vector<Rat> rats;        ///< rats en vadrouille
    float ratTimer = 0;           ///< temps avant le prochain rat
    int curBiome = -1;            ///< biome courant (-1 : à déterminer)
    float biomeBanner = 0;        ///< affichage du nom du biome (s)
    Rng grng{4242};               ///< aléa du jeu (rats, objets lâchés)
    Rng prng{777};                ///< aléa des particules
    Rng drng2{31337};             ///< aléa des gouttes d'eau
    Rng rrng{2024};               ///< aléa des ragdolls
    Haunt hn;                     ///< apparitions de la Dame
    int hauntHint = -1;           ///< anneau du joueur (recherche rapide pour la Dame)
    bool wallHint = false;        ///< message « un éboulis barre la galerie » déjà affiché

    // --- joueur
    vec3 pos = vec3(0);           ///< position des yeux (avant balancement)
    float yaw = 0, pitch = 0;     ///< orientation du regard
    bool fly = false;             ///< vol libre (touche F)
    std::vector<int> hints;       ///< anneaux courants (World::constrain)
    float walked = 0;             ///< distance parcourue à pied (rythme des pas)
    vec3 pvel = vec3(0);          ///< vitesse lissée
    float sprintAmt = 0;          ///< 0..1 : course (élargit le champ de vision)
    float bobAmt = 0;             ///< 0..1 : amplitude du balancement de la tête
    float groundEye = -1e9f;      ///< hauteur des yeux lissée (suit le relief)
    float jumpOff = 0, jumpV = 0; ///< hauteur et vitesse du saut
    int lastStepIndex = 0;        ///< dernier pas joué (son des pas)

    // --- pioche et effets de caméra
    float swingT = -1.0f;         ///< temps dans l'animation de la pioche (-1 : au repos)
    bool swingHit = false;        ///< le coup de l'animation en cours a déjà porté
    float camShake = 0;           ///< secousse de la caméra
    float camKick = 0;            ///< inclinaison de la caméra pendant le coup
    float hitStop = 0;            ///< arrêt sur image (temps ralenti à 10 %)
    float slowMo = 0;             ///< ralenti après une explosion
    float flashW = 0;             ///< flash blanc de l'explosion
    float fovPunch = 0;           ///< élargissement du champ de vision (explosion)
    float muffle = 0;             ///< oreilles qui sifflent (musique atténuée)

    // --- carte : anneaux découverts (+ bords gauche / droit en XZ) et cases du camp (80 x 60 cases de 4 m)
    std::vector<std::vector<uint8_t>> mapSeen;
    std::vector<std::vector<vec4>> mapEdge;
    std::vector<uint8_t> hubSeen = std::vector<uint8_t>(80 * 60, 0);

    // --- temps
    double lastT = 0, fpsT = 0;   ///< horloges (temps réel)
    int fpsN = 0;                 ///< images comptées pour le titre de la fenêtre
    double simT = 0;              ///< temps de jeu simulé
    double menuT = 0;             ///< temps passé dans les menus
    double tStart = 0;            ///< début de la boucle (mesure des performances en capture)
    int frame = 0;                ///< numéro de l'image

    // --- entrées (état précédent des touches, pour détecter les appuis)
    double mx0 = 0, my0 = 0;      ///< position précédente de la souris
    bool firstMouse = true;       ///< ignorer le premier déplacement (recentrage)
    bool keyF = false, keyR = false, keyP = false, mouseL = false, keyE = false, keyEnter = false;
    bool keySpace = false, keyG = false;
    bool keyEsc = false, keyF11 = false, keyUp = false, keyDn = false, keyLf = false, keyRt = false, keyRet = false;

    // --- menus
    Screen screen = SC_TITLE;     ///< écran courant
    Screen backTo = SC_TITLE;     ///< écran de retour depuis Paramètres / Commandes
    bool worldDirty = false;      ///< une partie a déjà été jouée : « Jouer » génère une nouvelle grotte
    bool cursorFree = true;       ///< curseur visible (menus)
    float heartT = 0;             ///< temps avant le prochain battement de cœur (vie basse)
    MouseState ms;                ///< souris
    int btnCount = 0;             ///< nombre de widgets dessinés dans l'image
    int menuSel = 0;              ///< widget sélectionné (clavier / survol)
    int menuCountPrev = 1;        ///< nombre de widgets de l'image précédente (navigation clavier)
    int dragId = -1;              ///< curseur en cours de glissement
    bool uiEnter = false, uiLeft = false, uiRight = false;  ///< appuis Entrée / flèches dans les menus

    // --- lumières
    std::vector<LightInfo> lv;    ///< lumières de l'image (conservées : utilisées par lightAt())
    int slotLight[SHADOW_SLOTS];  ///< source affectée à chaque cube map d'ombre (-1 : libre)
    int nl = 0;                   ///< nombre de lumières envoyées au shader
    GLfloat lpos[MAX_LIGHTS * 3] = {}, lcol[MAX_LIGHTS * 3] = {};  ///< positions et couleurs
    GLint lsh[MAX_LIGHTS];        ///< cube map d'ombre de chaque lumière (-1 : sans ombre)
    vec3 showCol[5];              ///< couleurs animées des cristaux / orbes du camp
    mat4 Mlan = mat4(1.0f);       ///< matrice de la lanterne tenue en main
    size_t lastDebrisCount = 0;   ///< nombre de débris à l'image précédente (bouffée de poussière)

    // =========================================================================
    //  Valeurs de l'image en cours
    // =========================================================================
    double now = 0;               ///< horloge
    float dt = 0;                 ///< pas de temps de simulation (0 dans les menus, ralenti possible)
    float rdt = 0;                ///< pas de temps réel (non ralenti)
    float T = 0;                  ///< temps des animations (shaders, flammes…)
    bool live = false;            ///< on joue (écran SC_PLAY)
    bool titleCam = false;        ///< caméra cinématique de l'écran titre
    vec3 front = vec3(0, 0, -1);  ///< direction du regard
    vec3 flat = vec3(0, 0, -1);   ///< direction du regard projetée à l'horizontale
    vec3 rightv = vec3(1, 0, 0);  ///< droite de la caméra
    vec3 eye = vec3(0);           ///< position de la caméra (balancement, secousses compris)
    int fbw = 0, fbh = 0;         ///< taille du framebuffer
    float aspect = 1;             ///< rapport largeur / hauteur
    vec3 frontV = vec3(0, 0, -1); ///< direction de visée finale (avec l'inclinaison du coup)
    mat4 proj, view, VP;          ///< matrices de projection, de vue et leur produit

    // =========================================================================
    //  game.cpp : cycle de vie
    // =========================================================================
    bool init();                  ///< fenêtre, OpenGL, son, ressources, monde, position de départ
    bool tickFrame();             ///< une image complète ; renvoie faux pour quitter
    void shutdown();              ///< sauvegarde des réglages, arrêt du son, fermeture
    void beginFrameTiming();      ///< calcule now / dt / rdt (arrêt sur image, ralenti)
    void updateScreenState();     ///< son, curseur, horloges selon l'écran courant
    void placePlayer(float meters);  ///< place le joueur dans une galerie à `meters` mètres
    void placeHub();              ///< place le joueur au départ du camp
    void initEntities();          ///< crée les ennemis à partir des points d'apparition
    void resetMap();              ///< efface la carte découverte
    void restart(bool newSeed);   ///< nouvelle partie (nouvelle grotte si newSeed)
    void startPlaying();          ///< bouton « Jouer » de l'écran titre
    void say(const std::string& s);  ///< affiche un message au centre de l'écran
    bool handleCapture();         ///< F12 / --shot ; renvoie vrai pour quitter (capture automatique faite)
    void present();               ///< échange les tampons, compte les images

    // =========================================================================
    //  game_player.cpp : entrées, déplacement, caméra, objets
    // =========================================================================
    void handleSystemKeys();      ///< Echap, F11, navigation clavier des menus
    void handleGameplayInput();   ///< souris, F, R, Entrée, clic, E
    void updateMovement();        ///< déplacement lissé, saut, collisions, balancement
    void revealMap();             ///< découvre la carte autour du joueur
    void updateTitleCamera();     ///< plan cinématique de l'écran titre
    void updatePickaxe();         ///< animation de la pioche et moment de l'impact
    void updateBiome();           ///< biome courant (musique, bannière) et minuteries
    void updatePickups();         ///< clés et fioles, sortie

    // =========================================================================
    //  game_combat.cpp : combat et dynamite
    // =========================================================================
    void hurtPlayer(float dmg);                 ///< inflige des dégâts au joueur
    void killEnemy(Enemy& e, vec3 dir);         ///< tue un ennemi (ragdoll, fiole éventuelle)
    void axeHit(vec3 eyePos, vec3 dir);         ///< résout un coup de pioche
    void explode(vec3 C);                       ///< explosion de dynamite en C
    void updateEnemies();                       ///< poursuite, attaque, repoussement
    void updateDynamite();                      ///< lancer, rebonds, mèche, explosion

    // =========================================================================
    //  game_ambient.cpp : ambiance (la Dame, gouttes, rats, débris, poussière)
    // =========================================================================
    float lightAt(vec3 p) const;                ///< luminosité approximative en p (intensité des particules)
    void puff(vec3 at, int n, float strength);  ///< bouffée de poussière
    bool spawnRat(float speed);                 ///< fait sortir un rat d'un trou proche
    void updateLady();                          ///< apparitions de la Dame
    mat4 ladyMatrix() const;                    ///< placement de la Dame
    void updateDrips();                         ///< gouttes d'eau des stalactites
    void updateDebrisAndRats();                 ///< débris, cendres, rats, ragdolls

    // =========================================================================
    //  game_lighting.cpp
    // =========================================================================
    void buildLights();           ///< choisit les lumières de l'image et les cubes d'ombre

    // =========================================================================
    //  game_render.cpp
    // =========================================================================
    bool prepareFrame();          ///< taille du framebuffer, tampons, matrices ; faux si fenêtre réduite
    void buildDynamicMeshes();    ///< maillages des ennemis, ragdolls, objets, dynamite
    void renderShadows();         ///< cartes d'ombre des lumières affectées à un cube
    void renderScene();           ///< ciel et scène HDR
    void renderParticles();       ///< poussière, flammes, particules (mise à jour comprise)
    void renderHeldItems();       ///< pioche et lanterne par-dessus la scène
    void postProcess();           ///< résolution MSAA, bloom, composition à l'écran

    // =========================================================================
    //  game_hud.cpp / game_menus.cpp : interface
    // =========================================================================
    void drawInterface();         ///< HUD + écrans de menu
    void drawHud();               ///< cœurs, clés, messages, effets d'écran, carte, mort / victoire
    void drawMinimap();           ///< minicarte en bas à droite
    void drawKeyIcon(float x, float y, float s, bool gold);                    ///< icône de clé
    void drawVignette(float a, float r, float g, float b, float k);            ///< assombrissement du pourtour
    void drawPanel(float x, float y, float w, float h, float a);               ///< cadre de menu
    bool menuButton(const std::string& label, float x, float y, float w, float h);  ///< bouton ; vrai si activé
    void menuSlider(const char* label, float& v, float lo, float hi, float step, float x, float y, float w, float h, bool pct);  ///< curseur
    void menuToggle(const char* label, bool& v, float x, float y, float w, float h);  ///< interrupteur OUI / NON
    bool mouseInside(float x, float y, float w, float h) const;               ///< souris dans le rectangle
    void drawLogo(float yTop, float scl);  ///< logo « GRIMDEEP »
    void drawTitleScreen();       ///< écran titre
    void drawPauseScreen();       ///< menu pause
    void drawSettingsScreen();    ///< paramètres
    void drawControlsScreen();    ///< liste des commandes

    // tailles d'interface de l'image en cours (calculées dans drawInterface)
    float uiW = 0, uiH = 0;       ///< taille de l'écran
    float uiSc = 2, uiCx = 0, uiCy = 0;  ///< échelle du texte du HUD, centre de l'écran
    float uiTs = 2, uiBt = 2;     ///< échelle du petit texte et du texte des boutons
};
