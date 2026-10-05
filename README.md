# GRIMDEEP — A Dwarf in a Cave (OpenGL 4.3)

Compilation : `build.bat` (g++ MSYS2 + GLFW + GLEW + GLM + WinMM). Lancement : `run.bat [options]`.

## But
Partir du **camp de base** (plaine, forêt, forge, chariot…), entrer dans la mine à flanc de montagne, traverser les 5 biomes, trouver les **3 clés** cachées au fond des galeries latérales (gardées par des ennemis),
ouvrir la **grande porte** à la fin du tunnel (touche E), puis rejoindre la lumière de la **sortie**.

## Contrôles
| Touche | Action |
|---|---|
| ZQSD / WASD, souris | se déplacer, regarder |
| Maj | courir (accélération progressive, FOV qui s'ouvre, pas sonores) |
| Clic gauche | coup de **pioche** (portée 3,2 m) : abat les blocs d'éboulis et les planches, repousse et blesse les monstres |
| E | interagir (ouvrir la porte avec 3 clés) |
| F | marche / vol libre |
| Entrée | recommencer (mort / victoire) |
| R | nouvelle grotte |
| F12 | capture PNG |
| Echap | pause (reprendre, paramètres, commandes, menu principal, quitter) |
| F11 | plein écran / fenêtré |

## Contenu
- **Menu titre en 3D** (le camp au couchant, montagne et cristaux géants, braises), **menu pause** (Echap), **paramètres**
  (volume général / musique / effets, sensibilité souris, plein écran — enregistrés dans `settings.ini`) et écran des **commandes**.
- **Plein écran** par défaut (F11 pour basculer).
- **HUD** : 10 cœurs (10 PV chacun) en haut à gauche, **minicarte** en bas à droite qui se dessine à mesure qu'on explore
  et indique les **clés** (même hors de la zone explorée, avec une flèche) et la porte. Quand on est touché : son + rouge
  sur le pourtour de l'écran (pulsation et battement de cœur à vie basse).
- **Musique** : 6 morceaux composés (un par biome + camp/menu/sortie), sans bruit blanc ; aucun son de goutte d'eau.
- Le **bois détruit** noircit au bout de quelques secondes, braise puis se réduit en cendres et disparaît.
- **5 biomes** + zone de sortie, avec transitions : Galeries boisées, Grottes humides (mousse, champignons
  luminescents, torches bleues), Antre des cristaux (cristaux émissifs), Catacombes (ossements, torches vertes),
  Forge infernale (lave, torches rouges). Chaque biome a sa palette, son ambiance et **sa musique générée**
  (synthé temps réel, gammes différentes) avec **fondu enchaîné** de 4 s au changement de biome.
- **Camp de base au crépuscule avec un vrai jeu de lumière** : la palette des cristaux géants dérive lentement sur tout le spectre et une onde de luminosité parcourt la montagne ;
  5 orbes de couleur dérivent au-dessus de la place (avec leurs étincelles) et 3 projecteurs balaient la face de la montagne ; étoiles qui scintillent, feux qui « respirent ».
  Dans la mine, les lumières restent **fixes** (aucun scintillement), ombres omnidirectionnelles. **Lanterne** compacte dans la main gauche.
- **Éboulis** : des murs de blocs de pierre (plusieurs épaisseurs, appareil en quinconce) bouchent toute la galerie à plusieurs endroits (5 sur la galerie principale, 1 dans chaque galerie latérale).
  Il faut les abattre à la pioche (2 coups par bloc, les coups touchent aussi les blocs voisins) : les blocs éclatent en morceaux, et un bloc qui perd son appui s'écroule.
- **Combat** : arrêt sur image et secousse de caméra à l'impact, ennemi qui clignote et recule (repoussement + état sonné), **attaque en 3 temps** (armement, fente, récupération :
  on peut esquiver), et **ragdoll** à la mort (corps rigide qui bascule et rebondit, membres qui pendent et traînent au sol).
- **Montagne du camp** : la façade est ramenée devant les galeries (elle ne les chevauche plus), plus de collines parasites derrière elle, texture à grande échelle vue de loin (plus de motif d'écailles),
  herbe au pied et neige au sommet.
- **Ennemis** : rôdeurs (3 PV), araignées géantes (2 PV, rapides), golems de lave (6 PV, très dangereux).
  Ils s'éveillent à portée. 100 PV de départ, fioles de soin rouges (+35 PV), parfois lâchées par les ennemis.
- **Gouttes d'eau** qui tombent des stalactites (avec leur son), flaques, toiles, rats, trous de rats.
- Planches destructibles avec collision, effondrement des cadres, échelles (une échelle frappée s'effondre d'un bloc), **barricades** qui barrent la galerie (2 coups de pioche pour les abattre), impasses.
- **Modèles 3D** chargés depuis `models/` (OBJ + MTL, générés par `models/generate_models.py`) : échelle, planches,
  momie (le « rôdeur »), araignée géante et golem de lave, animés par parties (jambes, bras, pattes). La pioche est générée dans le code. Si un modèle manque,
  le jeu retombe sur les formes procédurales d'origine. Le jeu doit être lancé depuis le dossier qui contient `models/`.

## Options de test
`--seed N` `--biome B` `--at METRES` `--look DEG` `--pitch DEG` `--branch K` `--door` `--exit` `--open`
`--keys N` `--hp N` `--enemy TYPE` `--ladder` `--barricade` `--hub` `--hubpos X Z` `--menu` `--pause` `--settings` `--controls` `--windowed` `--noenemies` `--noaudio` `--shot f.png [--frames N]` `--w` `--h`

Outils de test : `--god` (invulnérable), `--noclip`, `--campos X Y Z` (caméra libre, avec `--look`/`--pitch`), `--eyeoff D H R` (décale la caméra pour observer un ennemi), `--swing T` (fige la pioche à T secondes de l'animation), `--break` (coups automatiques), `--axe`.
