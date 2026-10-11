#!/bin/bash
# =============================================================================
#  tools/regression_test.sh — test de non-régression visuel et fonctionnel
#
#  Le jeu est entièrement déterministe en mode capture (--shot : graine fixe, pas de
#  temps fixe de 1/30 s). On peut donc rendre une série de scénarios AVANT et APRÈS
#  une modification, puis comparer les images pixel à pixel et les journaux.
#
#  Utilisation (Git Bash ou terminal MSYS2, depuis la racine du projet) :
#     1. avant la modification :  tools/regression_test.sh render  tests_avant
#     2. modifier le code, relancer build.bat
#     3. après la modification :  tools/regression_test.sh render  tests_apres
#     4. comparaison          :  tools/regression_test.sh compare tests_avant tests_apres
#
#  Chaque scénario produit <nom>.png (capture 480 x 270) et <nom>.log (sortie console
#  normalisée : génération du monde + état final de la partie : PV, ennemis tués,
#  planches détruites, débris…). Une modification purement « cosmétique » du code doit
#  donner 100 % d'images et de journaux identiques.
# =============================================================================
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXE="$ROOT/build/grimdeep.exe"
export PATH=/c/msys64/mingw64/bin:$PATH

# nom | options du jeu
SCENARIOS='
hub|--seed 123
menu|--seed 123 --menu --frames 60
pause|--seed 123 --pause
settings|--seed 123 --settings
controls|--seed 123 --controls
b0|--seed 123 --biome 0
b1|--seed 123 --biome 1
b2|--seed 123 --biome 2
b3|--seed 123 --biome 3
b4|--seed 123 --biome 4
door|--seed 123 --door --keys 3 --autoe --frames 60
exit|--seed 123 --exit --open --frames 40
walk0|--seed 123 --biome 0 --autowalk --frames 240
walk4|--seed 4242 --biome 4 --autowalk --frames 300 --god
en1|--seed 123 --biome 1 --enemy 1 --frames 80
en2|--seed 123 --biome 4 --enemy 2 --frames 80 --hp 30
kill0|--seed 123 --biome 0 --enemy 0 --break --pitch -10 --frames 300
swing|--seed 123 --biome 2 --swing 0.27
dyn|--seed 123 --biome 0 --dyn 3 --frames 70
killdyn|--seed 123 --biome 4 --enemy 2 --dyn 1 --pitch -30 --frames 420 --god
dame|--seed 123 --biome 0 --dame 6 --frames 8
corr|--seed 4242 --corr --autowalk --frames 320 --noenemies
barricade|--seed 123 --barricade --break --frames 40
ladder|--seed 123 --ladder --break --frames 40
rockfall|--seed 123 --at 31.5 --break --frames 400
rat|--seed 123 --biome 0 --rat --frames 90 --noenemies
hubpos|--seed 123 --hubpos 0 40 --look 90 --pitch 10
fly|--seed 123 --fly --biome 1 --autowalk --frames 100 --noclip
'

render() {
    local out="$1"
    [ -x "$EXE" ] || { echo "Executable introuvable : $EXE (lancez build.bat)"; exit 1; }
    mkdir -p "$out"
    out="$(cd "$out" && pwd)"
    # le jeu est lancé dans un dossier temporaire (copie de assets/) pour ne pas modifier settings.ini
    local run; run="$(mktemp -d)"
    cp -r "$ROOT/assets" "$run/"
    echo "$SCENARIOS" | while IFS='|' read -r name args; do
        [ -z "$name" ] && continue
        echo "  $name"
        (cd "$run" && "$EXE" --w 480 --h 270 $args --shot "$out/$name.png" > "$out/$name.raw" 2>&1)
        grep -v -e "ms/frame" -e "^Capture" -e "^GL:" "$out/$name.raw" > "$out/$name.log"
        rm -f "$out/$name.raw"
    done
    rm -rf "$run"
    echo "Rendus dans $out"
}

compare() {
    local a="$1" b="$2" same=0 diff=0
    for f in "$a"/*.png; do
        local n; n="$(basename "$f" .png)"
        if cmp -s "$f" "$b/$n.png" && cmp -s "$a/$n.log" "$b/$n.log"; then
            same=$((same + 1))
        else
            diff=$((diff + 1))
            echo "DIFFERENT : $n"
        fi
    done
    echo "Scenarios identiques : $same   differents : $diff"
    [ "$diff" -eq 0 ]
}

case "${1:-}" in
    render)  [ $# -eq 2 ] || { echo "usage : $0 render <dossier>"; exit 1; }; render "$2" ;;
    compare) [ $# -eq 3 ] || { echo "usage : $0 compare <dossier_avant> <dossier_apres>"; exit 1; }; compare "$2" "$3" ;;
    *) echo "usage : $0 render <dossier> | compare <avant> <apres>"; exit 1 ;;
esac
