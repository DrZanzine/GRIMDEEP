// =============================================================================
//  world/world.h — le monde du jeu : la mine, ses décors et le camp de base
//
//  World contient tout ce qui est généré à partir de la graine :
//    - les galeries (Tube) et leur maillage de roche ;
//    - le boisage (cadres, coffrage, échelles, barricades), les torches, les toiles,
//      les flaques, les trous de rats, les décors propres à chaque biome ;
//    - les éboulis (murs de blocs à abattre), la grande porte et la sortie ;
//    - les emplacements des clés, fioles et ennemis ;
//    - le camp de base en surface (relief, montagne, forêt, matériel de mine).
//  Il gère aussi les interactions physiques avec ce décor : collisions du joueur,
//  coups de pioche, planches qui cassent, débris qui tombent.
//
//  L'implémentation est répartie par thème :
//    world_build.cpp     génération des galeries et du maillage de roche
//    world_timber.cpp    boisage, torches, échelles, barricades + aménagement des galeries
//    world_rockfall.cpp  éboulis (murs de blocs de pierre)
//    world_decor.cpp     biomes, lumières, flaques, trous de rats, cristaux, ossements…
//    world_entities.cpp  grande porte, sortie, clés, fioles et ennemis
//    world_hub.cpp       camp de base (surface)
//    world_physics.cpp   collisions, coups, casse, débris
//
//  Repère : la mine part vers -Z depuis l'anneau 0 de la galerie principale ; le camp
//  est du côté +Z. Les distances sont en mètres.
// =============================================================================
#pragma once

#include <cstdint>
#include <vector>

#include "core/random.h"
#include "render/mesh.h"
#include "world/biomes.h"
#include "world/tube.h"

/// Source de lumière fixe du décor (torche, flaque de lave, cristal, lanterne…).
struct WorldLight {
    vec3 pos;                ///< position
    vec3 col;                ///< couleur
    float inten;             ///< intensité
    /// Biome de la lumière : 0..5 souterrain, 6 camp, 7..11 cristaux géants du camp
    /// (couleur animée, une teinte par valeur).
    int biome;
    bool flame;              ///< vraie flamme (dessinée avec des particules de feu)
    bool needsDoor = false;  ///< allumée seulement une fois la grande porte ouverte (zone de sortie)
};

/// Objet à ramasser.
struct Pickup {
    vec3 p;       ///< position
    int kind;     ///< 0 = clé, 1 = fiole de soin
    bool alive;   ///< faux une fois ramassé
    float phase;  ///< décalage de l'animation (rotation, flottement)
};

/// Point d'apparition d'un ennemi.
struct Spawn {
    int type;  ///< 0 rôdeur, 1 araignée, 2 golem de lave
    vec3 pos;  ///< position (hauteur des yeux)
    int tube;  ///< galerie
};

/// Stalactite qui laisse tomber des gouttes d'eau.
struct Drip {
    vec3 tip;     ///< pointe de la stalactite
    int tube;     ///< galerie
    int ring;     ///< anneau le plus proche
    float timer;  ///< temps avant la prochaine goutte
};

/// Toile d'araignée (un triangle dans webMesh).
struct Web {
    vec3 c;       ///< centre approximatif
    float r;      ///< rayon (pour les coups de pioche)
    uint32_t i0;  ///< premier indice dans webMesh
    bool alive;   ///< faux une fois déchirée
    int frame;    ///< cadre de soutènement auquel elle est accrochée (-1 : paroi)
};

/// Cadre de soutènement : deux poteaux (doublés), un linteau, des contrefiches.
/// Si tous les poteaux d'un côté sont cassés, le reste du cadre s'effondre.
struct TimberFrame {
    int tube = 0;                 ///< galerie
    std::vector<int> post[2];     ///< planches des poteaux (gauche, droite)
    std::vector<int> beam;        ///< planches du linteau
    std::vector<int> brace[2];    ///< contrefiches (gauche, droite)
    std::vector<int> webs;        ///< toiles accrochées au cadre
    bool fell[2] = {false, false};  ///< côté déjà effondré
};

/// Paire de trous de rats reliés (les rats courent de l'un à l'autre).
struct HolePair {
    vec3 pA, nA, pB, nB;  ///< position et normale de la paroi de chaque trou
    int tube;             ///< galerie
    int iA, iB;           ///< anneaux des deux trous
    float xA, xB;         ///< abscisse locale des deux trous
};

/// Vie d'un débris de bois (secondes) : intact jusqu'à WOOD_CHAR, il noircit et braise
/// jusqu'à WOOD_ASH, puis se réduit en cendres et disparaît à WOOD_GONE.
constexpr float WOOD_CHAR = 9.0f, WOOD_ASH = 13.0f, WOOD_GONE = 17.0f;

/// Débris physique : morceau de planche, éclat de bois ou de pierre.
/// Simulé comme une boîte orientée qui tombe, rebondit et finit par s'immobiliser.
struct Debris {
    vec3 p, v;          ///< position et vitesse
    vec3 ax, ay, az;    ///< axes de la boîte
    vec3 h;             ///< demi-dimensions
    vec3 w;             ///< vitesse angulaire (axe * vitesse)
    float seed;         ///< variation de texture
    float sleepT = 0;   ///< temps passé presque immobile au sol
    float age = 0;      ///< temps de simulation active
    float life = 0;     ///< âge total (sert au noircissement / à l'effritement)
    float unseen = 0;   ///< temps passé hors du champ de vision (pierres)
    float die = -1;     ///< >= 0 : en train de s'effriter (pierres)
    bool sleep = false; ///< immobile : plus simulé
    bool stone = false; ///< pierre (sinon bois)
    int tube = 0;       ///< galerie (pour les collisions)
    int ring = -1;      ///< anneau le plus proche (cache de recherche)
};

/// Le monde complet (voir l'en-tête du fichier).
struct World {
    // ---------------------------------------------------------------- maillages
    Mesh rockMesh;    ///< parois, stalactites, décors minéraux, champignons, ossements, cristaux
    Mesh woodMesh;    ///< boisage, échelles, barricades, porte, éboulis (planches enregistrées)
    Mesh torchMesh;   ///< supports de torches
    Mesh webMesh;     ///< toiles d'araignée
    Mesh waterMesh;   ///< flaques d'eau et de lave
    Mesh debrisMesh;  ///< débris (reconstruit à chaque image)
    Mesh ratMesh;     ///< rats (reconstruit à chaque image)
    Mesh hubMesh;     ///< camp de base

    // ---------------------------------------------------------------- contenu de la mine
    std::vector<Tube> tubes;           ///< galeries (0 = principale)
    std::vector<WorldLight> lights;    ///< lumières fixes
    std::vector<TimberFrame> frames;   ///< cadres de soutènement
    std::vector<Web> webs;             ///< toiles
    std::vector<HolePair> holes;       ///< trous de rats
    std::vector<Debris> debris;        ///< débris en cours de simulation
    std::vector<Pickup> pickups;       ///< clés et fioles
    std::vector<Spawn> spawns;         ///< ennemis à créer
    std::vector<Drip> drips;           ///< stalactites qui gouttent

    uint32_t seed = 1;                 ///< graine du monde
    Rng debrisRng{99};                 ///< aléa des débris (indépendant de la génération)
    std::vector<vec3> ashEvents;       ///< positions où émettre de la cendre (consommées par le jeu à chaque image)

    // ---------------------------------------------------------------- porte et sortie
    int doorRing = 0;                  ///< anneau de la grande porte (galerie principale)
    vec3 doorPos = vec3(0);            ///< position de la porte (pour l'interaction E)
    vec3 exitPos = vec3(0);            ///< bout de la galerie : atteindre ce point = victoire
    bool doorOpen = false;             ///< porte ouverte
    int keysNeeded = 0;                ///< nombre de clés à trouver (3 en général)
    std::vector<int> doorBoards;       ///< planches de la porte

    // ---------------------------------------------------------------- camp de base
    /// Obstacle cylindrique du camp (rocher, arbre, caisse…), dans le plan XZ.
    struct HubSolid { float x, z, r; };
    std::vector<HubSolid> hubSolids;   ///< obstacles du camp
    float hubGroundY = 0;              ///< altitude de référence du sol (seuil de la mine)
    float hubFrontZ = 0;               ///< Z de l'entrée de la mine (le camp est au-delà)
    vec3 hubSpawn = vec3(0);           ///< point de départ du joueur dans le camp

    /// Altitude du sol du camp au point (x, z).
    float hubGround(float x, float z) const;
    /// Contraint une position au terrain jouable du camp (murs invisibles, obstacles, sol).
    vec3 constrainHub(vec3 p, bool fly) const;
    /// Vrai si `p` est dans le camp (devant l'entrée de la mine).
    bool inHub(vec3 p) const { return !hubMesh.v.empty() && p.z > hubFrontZ + 0.25f; }

    // ---------------------------------------------------------------- objets monoblocs
    /// Échelle ou barricade : un modèle 3D visible + des planches invisibles (collision).
    /// Le premier coup qui casse une de ses planches fait s'effondrer tout l'objet.
    struct ModelProp {
        uint32_t i0 = 0, n = 0;   ///< indices du modèle dans woodMesh
        std::vector<int> boards;  ///< planches de collision
        bool alive = true;        ///< faux une fois effondré
        int kind = 0;             ///< 0 échelle, 1 barricade
        vec3 stand = vec3(0);     ///< (tests) où se placer pour le regarder
        vec3 look = vec3(0);      ///< (tests) point à regarder
    };
    std::vector<ModelProp> props;  ///< échelles et barricades

    // ---------------------------------------------------------------- éboulis
    /// Mur de blocs de pierre qui bouche toute la galerie : il faut l'abattre à la pioche.
    struct Rockfall {
        vec3 c = vec3(0);          ///< centre
        int tube = 0, ring = 0;    ///< emplacement
        std::vector<int> blocks;   ///< planches (rôle 7) qui forment les blocs
        bool told = false;         ///< (réservé) message déjà affiché
    };
    std::vector<Rockfall> rockfalls;  ///< éboulis

    // ---------------------------------------------------------------- couloir des catacombes
    /// Long couloir lisse et droit des catacombes (anneaux [corridorStart, corridorEnd]
    /// de la galerie principale ; -1 s'il n'existe pas). Lieu d'une apparition de « la Dame ».
    int corridorStart = -1, corridorEnd = -1;
    /// Poids 0..1 du couloir à l'anneau `i` de la galerie `ti` (1 = pleinement dans le couloir,
    /// avec des transitions de 12 anneaux de part et d'autre).
    float corridorWeight(int ti, int i) const {
        if (ti != 0 || corridorStart < 0) return 0.0f;
        return smoothStep((float)corridorStart - 12.0f, (float)corridorStart, (float)i) *
               (1.0f - smoothStep((float)corridorEnd, (float)corridorEnd + 12.0f, (float)i));
    }
    /// Vrai si l'anneau `i` est dans le couloir ou à moins de `m` anneaux de ses transitions.
    bool nearCorridor(int ti, int i, int m) const {
        return ti == 0 && corridorStart >= 0 && i > corridorStart - 12 - m && i < corridorEnd + 12 + m;
    }

    // ---------------------------------------------------------------- caméra
    vec3 camPos = vec3(0), camFwd = vec3(0, 0, -1);  ///< caméra courante (les gravats s'effritent hors du champ)

    // ---------------------------------------------------------------- génération
    /// Génère tout le monde à partir de la graine `sd` (détruit l'ancien) et l'envoie au GPU.
    void build(uint32_t sd);
    /// Libère les maillages GPU.
    void destroy();

    // ---------------------------------------------------------------- biomes
    /// Biome (fractionnaire dans les transitions) à l'anneau `i` de la galerie `ti`.
    float biomeAt(int ti, int i) const;
    /// Biome à une position quelconque (camp compris).
    float biomeAtPos(vec3 p) const;

    // ---------------------------------------------------------------- interactions
    /// Contraint `p` à l'espace jouable : dans une galerie, à hauteur des yeux au-dessus du
    /// sol (ou n'importe où dans la section en vol libre) ; dans le camp, voir constrainHub().
    /// `hints` mémorise l'anneau courant de chaque galerie (accélère la recherche).
    vec3 constrain(vec3 p, bool fly, std::vector<int>& hints) const;
    /// Repousse une position hors des planches vivantes (le joueur est un empilement de sphères).
    void collideBoards(vec3& p, bool fly) const;
    /// Lance un rayon contre les planches ; renvoie l'index touché (ou -1) et la distance.
    int rayBoard(vec3 o, vec3 d, float maxT, float& tHit) const;
    /// Liste les planches (hors porte) à moins de `rad` du point `pt`.
    void boardsNear(vec3 pt, float rad, std::vector<int>& out) const;
    /// Applique un coup à une planche (perte d'un PV, casse éventuelle, éclats).
    void hitBoard(int bi, vec3 hit, vec3 push);
    /// Casse une planche : elle disparaît et se change en débris (entière ou en deux morceaux).
    void breakBoard(int bi, vec3 hit, vec3 push, bool whole);
    /// Effondre un côté d'un cadre dont tous les poteaux de ce côté sont cassés.
    void checkCollapse(int frameId);
    /// Déchire les toiles le long d'un coup ; renvoie le nombre de toiles détruites.
    int clearWebs(vec3 o, vec3 d, float reach);
    /// Crée un débris : la portion [s0, s1] de la planche `b` le long de sa longueur.
    void spawnPiece(const Board& b, float s0, float s1, vec3 vel);
    /// Crée `n` petits éclats (bois ou pierre) projetés depuis `at`.
    void spawnChips(vec3 at, vec3 push, int n, bool stone = false);
    /// Casse un bloc d'éboulis en 4 morceaux puis fait tomber les blocs devenus sans appui.
    void breakStone(int bi, vec3 hit, vec3 push);
    /// Fait tomber les blocs de l'éboulis `wid` qui ne sont plus soutenus.
    void settleStone(int wid);
    /// Ouvre la grande porte (ses planches disparaissent).
    void openDoor();
    /// Fait avancer la simulation des débris (gravité, rebonds, mise en sommeil, cendres).
    void stepDebris(float dt);
    /// Reconstruit le maillage des débris (à appeler après stepDebris).
    void buildDebrisMesh();

private:
    // --- world_build.cpp : galeries
    void generateTube(Tube& tb, int Nmax, vec3 p0, float yaw0, float pitch0, bool isMain, Rng& rng, int parent, int pj,
                      float branchW);
    int validTubeLength(const std::vector<Ring>& rs, int parent, int pj) const;
    void computeSection(size_t ti);
    void buildRockMesh();
    void addTubeEnd(int ti, int ring, int dir, float mat = 0.0f);
    void addCone(vec3 base, vec3 axis, float len, float r0, Rng& rng);
    void addStalactites(Rng& rng);
    // --- world_timber.cpp : boisage et aménagement
    void furnishGalleries(Rng& rng);
    int addTimber(int ti, int fid, int role, int side, vec3 a, vec3 b, vec3 hint, float hw, float ht, Rng& rng,
                  float ext = 0.0f, float jit = 0.012f);
    void addWeb(int fid, vec3 corner, vec3 A, vec3 B, float size);
    void addFrame(int ti, int i, Rng& rng, bool doorway);
    void addLagging(int ti, int i, int j, int side, Rng& rng);
    void addTorch(int ti, int i, int side, Rng& rng);
    void addLadder(int ti, int i, int side, Rng& rng);
    bool addBarricade(int ti, int i, Rng& rng);
    // --- world_rockfall.cpp
    void addRockfall(int ti, int i, int layers, Rng& rng);
    // --- world_decor.cpp
    void addPuddles(int ti, int count, Rng& rng);
    void addRatHoles(int ti, int i, int k, Rng& rng);
    void assignBiomes(Mesh& m);
    void addLight(vec3 pos, vec3 col, float inten, int biome, bool flame);
    void addCrystal(vec3 base, vec3 axis, float len, float rad, float hue);
    void addDecor(int ti, Rng& rng);
    // --- world_entities.cpp
    void addDoor(Rng& rng);
    void addDoorAndExit(Rng& rng);
    void placeEntities(Rng& rng);
    // --- world_hub.cpp
    void buildHub();
};
