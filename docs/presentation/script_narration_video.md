# Script de voix off — GRIMDEEP

**Vidéo à utiliser pour enregistrer votre voix :** `GRIMDEEP_presentation_voixoff.mp4` (son du jeu baissé).
**Version avec le son du jeu à plein volume :** `GRIMDEEP_presentation.mp4`.
**Durée : 4 min 59 s.** Un chapitre par critère d'évaluation, dans l'ordre demandé.

## Repères rapides

| Critère | Début |
|---|---|
| 1 · Présentation et démonstration technique | 00:00 |
| 2 · Chargement de modèles 3D | 00:32 |
| 3 · Système d'éclairage | 01:10 |
| 4 · Caméra virtuelle | 01:40 |
| 5 · Textures et matériaux | 02:05 |
| 6 · Composition de la scène | 02:35 |
| 7 · Animation et interactivité | 03:07 |
| 8 · Structure et propreté du code | 03:55 |
| 9 · Effets spéciaux | 04:12 |

## Conseils d'enregistrement

- **Chaque réplique tient dans son plan** (environ 2,4 mots par seconde, débit posé). Si vous parlez plus vite, laissez un silence à la fin du plan ; ne débordez pas sur le suivant.
- Sur les **cartons noirs** (3 s), lisez seulement le titre du chapitre.
- **[code à l'écran]** : un extrait du vrai code apparaît à droite (fichier, fonction, numéros de ligne). Le texte explique ce qu'il fait.
- Matériel : Audacity, OBS ou le Dictaphone de Windows suffisent. Montez ensuite la voix sur la vidéo « voixoff » (Clipchamp, fourni avec Windows, ou DaVinci Resolve), en calant le début de la voix sur 00:00.
- Deux passages sont à dire avec vos propres mots ; ils sont signalés par **[À vous]**.

---

## 1 · Présentation du jeu et démonstration technique

**00:00 – 00:07 · Écran titre**
> Voici GRIMDEEP, un jeu d'exploration souterraine en 3D, programmé en C++ avec OpenGL 4.3.

**00:07 – 00:10 · Carton**
> Premier point : le jeu.

**00:10 – 00:21 · Survol du camp de base**
> On part d'un camp de base au crépuscule. Tout le décor est généré à partir d'une graine : chaque partie donne une mine différente.

**00:21 – 00:32 · Entrée dans la mine — [code à l'écran : `Game::tickFrame()`, src/game/game.cpp]**
> Le but : trouver trois clés et sortir. À chaque image, tickFrame simule le jeu, puis dessine en cinq passes, des ombres jusqu'au post-traitement.

## 2 · Chargement de modèles 3D

**00:32 – 00:35 · Carton**
> Deuxième critère : le chargement de modèles 3D.

**00:35 – 00:45 · Le rôdeur — [code : `loadModel()`, src/render/model_loader.cpp]**
> Les personnages sont des fichiers OBJ, lus par un chargeur écrit sans bibliothèque. Il partage les sommets identiques et découpe les faces en triangles.

**00:45 – 00:54 · L'araignée — [code : `appendModel()`]**
> Chaque modèle est divisé en groupes articulés. Le jeu applique une matrice par groupe, ce qui anime les pattes.

**00:54 – 01:03 · Le golem de lave**
> Les six modèles sont générés par un script Python. S'il en manque un, des formes procédurales le remplacent.

**01:03 – 01:10 · Barricade**
> Barricades, échelles et planches sont aussi des modèles OBJ, doublés de boîtes invisibles pour les collisions.

## 3 · Système d'éclairage

**01:10 – 01:13 · Carton**
> Troisième critère : l'éclairage.

**01:13 – 01:23 · Torches et ombres — [code : boucle d'éclairage, assets/shaders/scene.frag]**
> L'éclairage est calculé par pixel dans le fragment shader : jusqu'à trente-deux lumières, en Blinn-Phong avec un terme de Fresnel.

**01:23 – 01:32 · Lumières colorées — [code : `shadowAt()`, scene.frag]**
> Les quatre lumières les plus proches projettent des ombres dans toutes les directions, grâce à des cartes d'ombre en cube.

**01:32 – 01:40 · Le camp au crépuscule**
> Au camp s'ajoute un soleil couchant. L'image est calculée en haute dynamique, puis adoucie d'un halo lumineux.

## 4 · Caméra virtuelle

**01:40 – 01:43 · Carton**
> Quatrième critère : la caméra.

**01:43 – 01:55 · Vue subjective — [code : `Game::prepareFrame()`, src/game/game_render.cpp]**
> La caméra est à hauteur des yeux. Une matrice de perspective à soixante-seize degrés et une matrice de vue la définissent. La tête se balance en marchant.

**01:55 – 02:05 · Vol libre — [code : `Game::handleGameplayInput()`, src/game/game_player.cpp]**
> La souris règle le lacet et le tangage, limité pour ne pas se retourner. La touche F active le vol libre.

## 5 · Textures et matériaux

**02:05 – 02:08 · Carton**
> Cinquième critère : textures et matériaux.

**02:08 – 02:15 · Roche — [code : `tri()`, scene.frag]**
> Aucune image n'est chargée : la roche est calculée au démarrage, puis projetée depuis trois axes.

**02:15 – 02:22 · Bois — [code : `createTexture2D()`, src/render/textures.cpp]**
> Le bois est généré de la même façon. Chaque texture reçoit des mipmaps et un filtrage anisotrope.

**02:22 – 02:29 · Cristaux**
> Le shader compte plus de vingt-cinq matériaux, comme ce cristal au cœur lumineux.

**02:29 – 02:35 · Lave**
> La lave, elle, est émissive et animée par du bruit.

## 6 · Composition de la scène

**02:35 – 02:38 · Carton**
> Sixième critère : la composition de la scène.

**02:38 – 02:47 · Le camp vu de haut — [code : `World::build()`, src/world/world_build.cpp]**
> Le monde est assemblé dans un ordre fixe : galeries, roche, boisage, décors, objets et ennemis, puis le camp.

**02:47 – 03:07 · Les cinq biomes — [code : `World::biomeAt()`, src/world/world_decor.cpp]**
> La galerie traverse cinq biomes : galeries boisées, grottes humides, antre des cristaux, catacombes et forge infernale. Chacun a sa teinte, ses torches, son humidité et sa musique, avec de courtes transitions.

## 7 · Animation et interactivité

**03:07 – 03:10 · Carton**
> Septième critère : animation et interactivité.

**03:10 – 03:19 · Les rats — [code : `ratPositionAt()`, src/entities/rat.cpp]**
> Le décor vit : chaque rat suit un trajet entre deux trous, sa position interpolée selon la distance parcourue.

**03:19 – 03:29 · Pioche contre l'éboulis — [code : `World::hitBoard()`, src/world/world_physics.cpp]**
> Le décor est destructible : planches et blocs ont des points de vie. Un bloc cassé éclate, et ceux sans appui s'écroulent.

**03:29 – 03:35 · Barricade détruite**
> Une barricade résiste deux coups, puis tombe d'un bloc.

**03:35 – 03:45 · Combat — [code : `appendEnemyModel()` puis `stepRagdoll()`]**
> Les ennemis attaquent en trois temps : on peut esquiver. À leur mort, les membres pendent, simulés par intégration de Verlet.

**03:45 – 03:55 · La grande porte**
> Avec les trois clés, la touche E ouvre la grande porte, et la lumière de la sortie s'allume.

## 8 · Structure et propreté du code

**03:55 – 03:58 · Carton**
> Huitième critère : la structure du code.

**03:58 – 04:12 · Arborescence du projet, puis terminal**
> Le code est réparti en sept modules documentés, aux dépendances à sens unique. Un test de non-régression compare les rendus, pixel par pixel, avant et après une modification.

> **[À vous]** *Si les règles du cours le demandent, précisez ici comment le code a été produit ou restructuré (par exemple avec l'aide d'un assistant). Le plan dure 14 s : il reste environ 2 s pour une courte phrase, ou dites-la sur l'écran de fin.*

## 9 · Effets spéciaux

**04:12 – 04:15 · Carton**
> Dernier critère : les effets spéciaux.

**04:15 – 04:25 · Reflets et gouttes d'eau — [code : branche « eau », scene.frag]**
> Les flaques renvoient le reflet de chaque lumière, sur une surface ridée par des ondes. Ce n'est pas un miroir, mais l'illusion fonctionne.

**04:25 – 04:31 · Flammes et braises — [code : `Game::renderParticles()`]**
> Flammes, braises, étincelles et fumée sont des particules, chacune avec son propre mouvement.

**04:31 – 04:41 · Dynamite**
> La dynamite combine tout : des centaines de particules, une onde de choc, une lumière qui projette des ombres, un ralenti et une secousse.

**04:41 – 04:51 · La Dame — [code : dissolution, scene.frag]**
> Enfin, un secret des catacombes : quand on s'approche, elle se dissout. Le shader efface ses pixels selon un motif de bruit.

**04:51 – 04:59 · Écran titre**
> **[À vous]** *Une phrase de conclusion personnelle, par exemple ce que ce projet vous a appris.* Merci de votre attention.

---

## Ce que dit le script, et où le vérifier dans le code

| Affirmation | Fichier / fonction |
|---|---|
| Cinq passes de rendu par image | `src/game/game.cpp` — `Game::tickFrame()` |
| Chargeur OBJ sans bibliothèque, sommets partagés, faces en triangles | `src/render/model_loader.cpp` — `loadModel()` |
| Une matrice par groupe articulé | `src/render/model_loader.cpp` — `appendModel()` |
| Six modèles générés par un script, formes de remplacement | `tools/generate_models.py`, `src/entities/enemy.cpp` — `appendEnemyMesh()` |
| 32 lumières, Blinn-Phong et Fresnel | `assets/shaders/scene.frag`, `src/render/renderer.h` (`MAX_LIGHTS`) |
| Ombres pour les 4 lumières les plus proches (cube maps) | `scene.frag` — `shadowAt()`, `src/game/game_lighting.cpp` |
| HDR et halo lumineux (bloom) | `assets/shaders/bloom_down.frag`, `bloom_up.frag`, `compose.frag` |
| Perspective 76°, matrice de vue, balancement | `src/game/game_render.cpp` — `prepareFrame()`, `game_player.cpp` — `updateMovement()` |
| Lacet / tangage limités, vol libre (F) | `src/game/game_player.cpp` — `handleGameplayInput()` |
| Textures calculées, triplanaire, mipmaps, anisotrope | `src/render/textures.cpp`, `scene.frag` — `tri()` |
| Plus de 25 matériaux | `src/render/materials.h` (namespace `Mat`) |
| Ordre d'assemblage du monde | `src/world/world_build.cpp` — `World::build()` |
| Cinq biomes et transitions | `src/world/world_decor.cpp` — `World::biomeAt()`, `src/world/biomes.cpp` |
| Trajet des rats | `src/entities/rat.cpp` — `ratPositionAt()` |
| Points de vie, blocs qui s'écroulent | `src/world/world_physics.cpp` — `hitBoard()`, `world_rockfall.cpp` — `settleStone()` |
| Attaque en trois temps, ragdoll de Verlet | `src/entities/enemy.cpp`, `src/entities/ragdoll.cpp` — `stepRagdoll()` |
| Porte à trois clés, lumières de sortie | `src/game/game_player.cpp` — `handleGameplayInput()`, `src/world/world_entities.cpp` |
| Sept modules, test de non-régression | `src/`, `tools/regression_test.sh`, `docs/ARCHITECTURE.md` |
| Reflets sur l'eau (pas un miroir) | `scene.frag`, branche « eau » |
| Particules | `src/game/game_render.cpp` — `renderParticles()`, `assets/shaders/particles.frag` |
| Explosion | `src/game/game_combat.cpp` — `explode()` |
| Dissolution de la Dame | `scene.frag` (uniforme `uGhost`) |

## À savoir sur le tournage (à mentionner si on vous le demande)

La vidéo a été tournée dans le moteur du jeu, image par image, avec une caméra libre scriptée. Certains
événements ont été déclenchés pour le tournage : rats lâchés, gouttes ajoutées au-dessus d'une flaque,
araignée placée devant la caméra, ennemis retirés des plans calmes, porte ouverte par script. Les
séquences « vue subjective » et « combat » utilisent les vraies commandes du moteur (marche, saut,
coups de pioche, ennemi qui attaque).
