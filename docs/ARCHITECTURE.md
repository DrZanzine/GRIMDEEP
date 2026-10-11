# Architecture du code de GRIMDEEP

Ce document explique comment le jeu est organisé, pour pouvoir le reprendre sans tout lire.
Chaque fichier source commence par un en-tête qui décrit son rôle, et chaque fonction est
documentée dans son fichier `.h` (ou juste au-dessus de sa définition pour les fonctions internes).

## Sommaire

1. [Vue d'ensemble](#1-vue-densemble)
2. [Les modules](#2-les-modules)
3. [Déroulement d'une image](#3-déroulement-dune-image)
4. [Conventions](#4-conventions)
5. [Génération du monde](#5-génération-du-monde)
6. [Rendu](#6-rendu)
7. [Son](#7-son)
8. [Déterminisme et tests de non-régression](#8-déterminisme-et-tests-de-non-régression)
9. [Recettes : modifier ou ajouter quelque chose](#9-recettes--modifier-ou-ajouter-quelque-chose)

---

## 1. Vue d'ensemble

Tout est **procédural** : la grotte, le camp de base, les textures, la musique et les effets
sonores sont calculés à partir d'une graine (`--seed N`). Les seuls fichiers lus sur le disque
sont les shaders GLSL et six modèles 3D (dans `assets/`).

```
main.cpp ──► Game (src/game) ──┬──► World (src/world)       la mine, le camp, la physique du décor
                               ├──► entities (src/entities) ennemis, ragdolls, rats, la Dame, dynamite…
                               ├──► RenderResources          shaders, textures, ombres, tampons (src/render)
                               ├──► UiRenderer (src/ui)      HUD et menus
                               └──► aud::Engine (src/audio)  musique et effets, thread temps réel
```

Les dépendances vont toujours dans le même sens : `core` ← `render` ← `world` ← `entities` ← `game`.
`audio` et `ui` ne dépendent de rien d'autre que `core` (et d'OpenGL pour `ui`).

## 2. Les modules

| Dossier | Fichiers | Rôle |
|---|---|---|
| `src/core` | `math_utils.h`, `random.h`, `noise.*`, `paths.*`, `png_writer.*` | Alias GLM, interpolations, générateur aléatoire `Rng`, bruit de valeur / fBm, recherche du dossier `assets/`, écriture PNG |
| `src/render` | `mesh.*` | `Vertex`, `Chunk`, `Board` (planche destructible), `Mesh` (tampons GPU + culling par chunks) |
| | `geometry.*` | Primitives : grilles, ellipsoïdes, cylindres, planches, surfaces lissées, splines |
| | `materials.*` | Numéros de matériaux (`Mat::ROCK`, `Mat::WOOD`…), palette des matériaux génériques |
| | `model_loader.*` | Lecteur OBJ/MTL, bibliothèque `gModels`, recopie animée d'un modèle dans un `Mesh` |
| | `textures.*` | Textures procédurales de roche et de bois (+ cartes de normales) |
| | `shader.*` | Chargement et compilation des fichiers de `assets/shaders/` |
| | `render_targets.*`, `renderer.*` | Tampons hors écran (HDR, MSAA, bloom) et ressources GPU partagées |
| `src/world` | `world.h` | Classe `World` et ses données (galeries, lumières, planches, objets, débris…) |
| | `tube.*` | Une galerie : anneaux, section, requêtes « où est le sol / le mur ? » |
| | `biomes.*` | Table des 7 biomes (couleurs, ambiance) |
| | `world_build.cpp` | Tracé des galeries, maillage de roche, stalactites, `World::build()` |
| | `world_timber.cpp` | Boisage, échelles, barricades, torches, aménagement complet des galeries |
| | `world_rockfall.cpp` | Éboulis (murs de blocs de pierre) |
| | `world_decor.cpp` | Biomes, flaques, trous de rats, cristaux, champignons, ossements |
| | `world_entities.cpp` | Grande porte, sortie, clés, fioles, points d'apparition des ennemis |
| | `world_hub.cpp` | Camp de base (relief, montagne, forêt, forge, cristaux géants…) |
| | `world_physics.cpp` | Collisions, coups de pioche, casse des planches, débris |
| `src/entities` | `enemy.*`, `ragdoll.*`, `rat.*`, `lady.*`, `props.*`, `dynamite.h`, `particles.h` | Données et maillages des entités (l'intelligence est dans `game`) |
| `src/audio` | `synth.h`, `music.*`, `sound_effects.*`, `reverb.*`, `audio_engine.*` | Synthétiseur : 6 morceaux, 18 effets, réverbération, sortie WinMM |
| `src/ui` | `bitmap_font.*`, `ui_renderer.*` | Police pixel 5x7 et dessin 2D par lots |
| `src/game` | `game.h` | Classe `Game` : tout l'état de la partie |
| | `game.cpp` | Démarrage, boucle, nouvelle partie, capture d'écran |
| | `game_player.cpp` | Entrées, déplacement, caméra, pioche, objets à ramasser |
| | `game_combat.cpp` | Coups, ennemis, dynamite, explosions |
| | `game_ambient.cpp` | La Dame, gouttes d'eau, rats, débris, poussière |
| | `game_lighting.cpp` | Choix des 32 lumières de l'image et des 4 ombres |
| | `game_render.cpp` | Passes de rendu (ombres, scène, particules, objets en main, post-traitement) |
| | `game_hud.cpp`, `game_menus.cpp` | HUD, minicarte, menus |
| | `options.*`, `settings.*` | Ligne de commande, `settings.ini`, plein écran |

## 3. Déroulement d'une image

`Game::tickFrame()` (`src/game/game.cpp`) appelle, **toujours dans cet ordre** :

1. `beginFrameTiming()` — pas de temps (borné à 50 ms ; arrêt sur image et ralenti après un impact ou une explosion).
2. `handleSystemKeys()` — Echap, F11, navigation clavier des menus. Dans un menu, `dt = 0` : le jeu est figé.
3. `updateScreenState()` — atténuation de la musique, curseur, horloges.
4. `handleGameplayInput()` puis `updateMovement()` — souris, touches, déplacement lissé, saut, collisions, balancement de la caméra.
5. `revealMap()`, `updateTitleCamera()`, `updatePickaxe()`, `updateBiome()`.
6. `updateEnemies()`, `updateLady()`, `updateDynamite()`, `updatePickups()`, `updateDrips()`.
7. `prepareFrame()` — matrices de la caméra (si la fenêtre est réduite, le rendu est sauté).
8. `updateDebrisAndRats()`, `buildDynamicMeshes()` — maillages reconstruits à chaque image (débris, rats, ennemis, objets).
9. `buildLights()`, `renderShadows()`, `renderScene()`, `renderParticles()`, `renderHeldItems()`, `postProcess()`.
10. `drawInterface()` — HUD et menus.
11. `handleCapture()` (F12 ou `--shot`), `present()`.

> **Important :** plusieurs générateurs aléatoires (`prng`, `grng`, `World::debrisRng`…) sont
> partagés entre ces étapes. Changer l'ordre des appels change la suite des tirages, donc le
> comportement exact du jeu (voir §8).

## 4. Conventions

- **Unités** : mètres et secondes. L'axe Y est vertical.
- **Repère du monde** : la mine part de l'origine vers **-Z** ; le camp de base est du côté **+Z**
  (`World::hubFrontZ` = Z de l'entrée de la mine). `World::inHub(p)` indique si un point est au camp.
- **Position du joueur et des ennemis** : `pos` est à **hauteur des yeux**, 1,65 m au-dessus du sol.
- **Repère d'une galerie** : chaque anneau (`Ring`) a une tangente `T`, une droite `R` et un haut `U`.
  Les fonctions de `Tube` travaillent en coordonnées locales `(x, y, t)` : `toWorld()`, `floorY()`,
  `ceilY()`, `wallRange()`… Les anneaux sont espacés de `ds = 0,3 m`.
- **Galeries** : `tubes[0]` est la galerie principale ; les suivantes sont des culs-de-sac branchés dessus.
- **Biomes** : 0 galeries boisées, 1 grottes humides, 2 antre des cristaux, 3 catacombes,
  4 forge infernale, 5 la sortie, 6 le camp de base (`src/world/biomes.*`).
- **Planches (`Board`)** : boîtes orientées (axes `dl`, `dw`, `dt`, demi-tailles `hl`, `hw`, `ht`)
  enregistrées dans `World::woodMesh`. Elles portent la collision et la destructibilité de tout le
  boisage, des échelles, des barricades, des blocs d'éboulis (rôle 7) et de la porte (rôle 9).
- **Matériaux** : chaque sommet porte un numéro (`Vertex::mat`, constantes `Mat::` dans
  `src/render/materials.h`) que `scene.frag` interprète. Les numéros ≥ 21 lisent leur couleur dans
  la palette `gMaterials`.
- **Nommage** : types en `PascalCase`, fonctions et variables en `camelCase`, constantes en
  `MAJUSCULES`. Les commentaires et l'interface sont en français (le texte affiché est sans accents :
  la police pixel n'en contient pas).

## 5. Génération du monde

`World::build(seed)` (`src/world/world_build.cpp`) :

1. **Galerie principale** (`generateTube`) : ~900 anneaux. Le cap dérive selon un bruit 1D, rappelé
   vers le cap initial ; on fait jusqu'à 40 essais et on garde le plus long tracé qui ne se recoupe pas.
   Un tronçon droit est réservé au **couloir des catacombes**. La **grande porte** est à 92 % du trajet.
2. **Section** (`computeSection`) : superellipse aplatie en bas, déformée par du bruit fBm et des crêtes.
3. **Galeries latérales** : jusqu'à 4 culs-de-sac (une clé au fond de chacun).
4. **Maillage de roche** (`buildRockMesh`) : ouvertures aux jonctions, fonds en dôme, lumière du jour au bout.
5. **Stalactites** (qui gouttent) et stalagmites.
6. **Aménagement** (`furnishGalleries`) : éboulis aux points les plus étroits, cadres de soutènement,
   coffrage, torches, échelles, planches au sol, trous de rats, flaques, décors de biome, porte,
   sortie, clés, fioles, ennemis, toiles.
7. **Camp de base** (`buildHub`), puis attribution des biomes aux sommets et envoi au GPU.

## 6. Rendu

- **Scène** en HDR (`RGBA16F`) avec MSAA x4. Jusqu'à **32 lumières** ponctuelles par image ; les 4
  plus proches ont une **ombre** (cube maps de distance, échantillonnage doux). Les maillages sont
  découpés en *chunks* et seuls ceux proches de la caméra (ou d'une lumière, pour les ombres) sont dessinés.
- **Particules** : un seul programme (`particles.*`), le type choisit l'aspect (flamme, braise, fumée…).
  La fumée est triée et mélangée en alpha, tout le reste est additif.
- **Post-traitement** : bloom en pyramide (6 niveaux), tonemapping ACES, vignettage, grain.
- **Interface** : triangles colorés en 2D (`UiRenderer`), police 5x7 dessinée en petits carrés.

## 7. Son

`aud::Engine` (`src/audio/audio_engine.*`) tourne dans son propre thread et remplit des tampons de
1024 échantillons. Le jeu ne fait qu'envoyer des demandes : `audio.sfx(aud::SF_..., volume, variante)`,
`audio.setBiome(b)`, `audio.setVolumes(...)`. La musique change avec un fondu enchaîné de 4 s.
Rien n'est chargé depuis le disque : chaque instrument et chaque effet est une petite formule.

## 8. Déterminisme et tests de non-régression

En mode capture (`--shot fichier.png`), le pas de temps est fixe (1/30 s) et tout part de la graine :
deux exécutions identiques produisent **la même image au pixel près** et le même journal.
`tools/regression_test.sh` s'en sert pour vérifier qu'une modification ne change rien :

```bash
tools/regression_test.sh render  tests_avant      # avant la modification
# ... modifier, puis build.bat ...
tools/regression_test.sh render  tests_apres
tools/regression_test.sh compare tests_avant tests_apres
```

Pour une modification volontaire du comportement, les images différeront évidemment : l'outil sert
alors à vérifier que *seuls* les scénarios attendus changent.

Points d'attention pour garder ce déterminisme :

- ne pas changer l'ordre des appels qui consomment un générateur aléatoire ;
- dans une expression qui appelle plusieurs fois `rng.r(...)` (par exemple `vec3(rng.r(), rng.r(), rng.r())`),
  l'ordre d'évaluation des arguments n'est pas garanti par le C++ : ne pas « factoriser » ces appels
  dans des variables intermédiaires si l'on veut un résultat identique ;
- ne pas activer d'options de compilation qui changent le calcul flottant (`-ffast-math`, `-march=native`…).

## 9. Recettes : modifier ou ajouter quelque chose

**Régler un ennemi** (vitesse, PV, dégâts…) : table `ENEMY_STATS` dans `src/entities/enemy.cpp`.
Leur répartition par biome : `enemyTypeFor()` et `placeEntities()` dans `src/world/world_entities.cpp`.

**Ajouter un matériau** :
1. choisir un numéro libre et l'ajouter au namespace `Mat` (`src/render/materials.h`) ;
2. ajouter la branche correspondante dans `assets/shaders/scene.frag` (les tests sont des intervalles :
   `vMat > n - 0.5 && vMat < n + 0.5`) ;
3. ou plus simplement, pour une couleur unie : `gMaterials.findOrAppend(couleur, brillance, émission, gloss)`.

**Ajouter un effet sonore** : ajouter une valeur à l'énumération de `src/audio/sound_effects.h`, sa
formule dans `sfxSample()` (`sound_effects.cpp`), puis l'appeler avec `audio.sfx(aud::SF_MON_EFFET)`.

**Modifier un modèle 3D** : éditer `tools/generate_models.py`, puis `python tools/generate_models.py`
(les fichiers de `assets/models/` sont réécrits). Les groupes `g` et les commentaires `# pivot`
servent à l'animation (voir `appendEnemyMesh()`).

**Ajouter une option de test** : un champ dans `LaunchOptions` (`src/game/options.h`), sa lecture dans
`parseOptions()`, puis son utilisation dans `Game`. Penser à la documenter dans le README.

**Ajouter un écran de menu** : une valeur dans `Game::Screen`, une fonction `drawXxxScreen()` dans
`game_menus.cpp` (avec `menuButton`, `menuSlider`, `menuToggle`) et son appel dans `drawInterface()`.
