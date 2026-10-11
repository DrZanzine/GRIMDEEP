# Fiche de publication YouTube — GRIMDEEP

Tout ce qu'il faut pour mettre en ligne la présentation, dans l'ordre des écrans de YouTube Studio.

## Fichiers du dossier

| Fichier | Usage |
|---|---|
| `../GRIMDEEP_presentation_finale.mp4` | **La vidéo à mettre en ligne** (4:59.8, 1080p, avec ta voix, −17 LUFS) |
| `description.txt` | Description complète, à coller telle quelle (1 573 caractères sur 5 000 max) |
| `tags.txt` | Tags, à coller dans le champ « Tags » (315 caractères sur 500 max) |
| `sous-titres_fr.srt` | Sous-titres français corrigés, calés sur ta voix (58 sous-titres) |
| `miniature_1_golem.jpg` | Miniature recommandée (1280×720) |
| `miniature_2_dame.jpg` | Variante « mystère / horreur » |
| `miniature_3_camp.jpg` | Variante « vitrine » colorée |

## 1. Détails

**Titre** (100 caractères max). Propositions, de la plus sobre à la plus accrocheuse :

1. `GRIMDEEP — Un jeu 3D en C++ et OpenGL 4.3 | Présentation technique` ← recommandé
2. `J'ai codé un jeu de mine en 3D sans moteur (C++ / OpenGL)` (à éviter si le code n'est pas entièrement de toi)
3. `GRIMDEEP : éclairage, ombres, shaders et monstres en OpenGL pur`
4. `Un jeu 3D en OpenGL de A à Z : modèles, lumières, effets | GRIMDEEP`

**Description** : coller `description.txt`. Les chapitres (0:00, 0:07, …) deviennent automatiquement des chapitres cliquables dans la barre de lecture : il faut que la première ligne soit `0:00`, qu'il y en ait au moins 3 et qu'ils durent chacun au moins 10 s, ce qui est le cas.
Si le code source est publié (GitHub…), ajouter une ligne `💻 Code source : <lien>` sous « TECHNOLOGIES ».

**Miniature** : importer `miniature_1_golem.jpg`. (Une miniature personnalisée demande un compte validé par téléphone, dans Studio → Paramètres → Chaîne → Éligibilité aux fonctionnalités.) YouTube permet aussi le **test A/B de miniatures** (« Tester et comparer ») : y mettre les trois.

**Playlist** : facultatif, par exemple « Projets OpenGL » ou « Projets de cours ».

## 2. Public — « Est-ce conçu pour les enfants ? »

**Réponse : « Non, elle n'est pas conçue pour les enfants ».**

« Conçu pour les enfants » (loi américaine COPPA) signifie que la vidéo **vise** les enfants comme public principal (personnages enfantins, jouets, comptines…). Ce n'est pas le cas ici : c'est une présentation technique de programmation pour un public étudiant ou adulte.
Cocher « Oui » par erreur aurait des conséquences : commentaires désactivés, pas de miniature dans les notifications, pas de fiches ni d'écran de fin, et pas de mini-lecteur.

**Restriction d'âge (18+) : non, inutile.** La vidéo contient des monstres (araignée géante, golem de lave, rôdeur), des combats à la pioche, une explosion et une apparition inquiétante (la Dame). Mais il n'y a ni sang, ni gore, ni violence réaliste, ni langage grossier. C'est comparable à un jeu PEGI 7–12. Rien ne justifie une limite d'âge selon les règles de YouTube.

## 3. Plus d'options

| Réglage | Valeur |
|---|---|
| Contenu modifié ou synthétique | **Non.** La case concerne les contenus réalistes qui pourraient tromper (vraie personne, vrai lieu, faux événement). Des images de jeu vidéo ne sont pas concernées. |
| Promotion payante | Non |
| Sous-titres | Importer `sous-titres_fr.srt` (voir §5) |
| Langue de la vidéo | Français |
| Certification des sous-titres | « Ce contenu n'a jamais été diffusé à la télévision aux États-Unis » |
| Date d'enregistrement | Facultatif |
| Licence | Licence YouTube standard (ou Creative Commons – Attribution si tu veux autoriser la réutilisation) |
| Autoriser l'intégration | Oui (utile pour l'intégrer dans un rapport ou un portfolio) |
| Publier dans le fil Abonnements | Oui |
| Shorts remixing | Au choix |
| Catégorie | **Science et technologie** (recommandé) ou **Jeux vidéo** |
| Commentaires | Activés, avec « Retenir les commentaires potentiellement inappropriés » |

**Droits d'auteur / Content ID** : aucun risque. Toute la musique et tous les bruitages sont **synthétisés par le jeu** (`src/audio/`) ; aucun extrait sous licence n'est utilisé. La vérification automatique (« Vérifications ») devrait donc ressortir sans problème.

## 4. Éléments vidéo

**Écran de fin** (15 à 20 dernières secondes, de 4:40 à 4:59) : la dernière section (« la Dame », puis le carton « Fin ») s'y prête. Ajouter un élément « S'abonner » et « Vidéo : la plus récente » ou une playlist. Le placer en bas pour ne pas cacher le titre final.

**Fiches** (cartes « i ») : une fiche à 3:55 (« Structure du code ») vers le dépôt du code, si tu as une vidéo ou une playlist à lier (YouTube n'autorise les liens externes qu'aux chaînes du Programme Partenaire).

## 5. Sous-titres

Studio → Sous-titres → Français → Ajouter → **Importer un fichier → « Avec minutage »** → `sous-titres_fr.srt`.

Le fichier part de la transcription automatique de ta voix (horaires exacts), avec le texte corrigé : GRIMDEEP, Blinn-Phong, mipmaps, « groupes articulés », « flaques »… Il améliore l'accessibilité et le référencement, puisque YouTube indexe le texte des sous-titres.

⚠️ **Une phrase à vérifier en réécoutant vers 1:24.** La transcription a entendu « projettent des **ondes** dans toutes les directions » là où le script dit « des **ombres** ». Le sous-titre écrit « ombres ». Si tu as bien dit « ombres », tout va bien. Sinon, le sous-titre corrige le lapsus à l'écrit, et la vidéo reste compréhensible.

## 6. Visibilité

| Cas | Choix |
|---|---|
| Rendu pour le cours, seul le prof doit voir | **Non répertoriée** : seules les personnes qui ont le lien la voient, et elle n'apparaît ni dans les recherches ni sur la chaîne |
| Portfolio public | **Publique** |
| Pour relire avant | **Privée**, puis passage en « Non répertoriée » ou « Publique » |

Conseil : mettre d'abord en **Privée**, attendre la fin du traitement HD (la 1080p arrive quelques minutes après la SD), vérifier les chapitres, les sous-titres et la miniature, puis changer la visibilité.

## 7. Commentaire épinglé (à publier puis épingler)

```
Merci d'avoir regardé ! 🎮
Les points abordés suivent les critères du projet : modèles 3D, éclairage, caméra, textures, composition, animation, structure du code et effets spéciaux (chapitres dans la description).
Une question sur une technique (ombres en cube maps, particules, ragdoll…) ? Posez-la ici 👇
```

## 8. Transparence

La vidéo affiche à l'écran des extraits du vrai code. Si une partie du code a été écrite avec un assistant d'IA, c'est **à toi de décider**, selon les règles de ton cours, s'il faut l'indiquer dans la description (par exemple : « Code réalisé avec l'aide d'un assistant IA »). YouTube ne l'exige pas pour ce type de contenu. Le titre n°2 (« J'ai codé… ») est à éviter si l'affirmation n'est pas exacte.

## 9. Liste de vérification avant de publier

- [ ] Fichier `GRIMDEEP_presentation_finale.mp4` importé (pas les versions sans voix)
- [ ] Titre, description, tags collés
- [ ] Miniature importée
- [ ] « Non, elle n'est pas conçue pour les enfants » coché
- [ ] Catégorie et langue réglées
- [ ] Sous-titres `.srt` importés « avec minutage »
- [ ] Chapitres visibles dans la barre de lecture
- [ ] Écran de fin ajouté
- [ ] Phrase « ombres / ondes » réécoutée vers 1:24
- [ ] Visibilité choisie (Non répertoriée pour un rendu de cours)
