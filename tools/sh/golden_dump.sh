#!/usr/bin/env bash
# tools/sh/golden_dump.sh — GOLDEN DUMP del mondo C (TODO §6, F0).
#
#   tools/sh/golden_dump.sh            # verifica i dump contro tools/golden/expected
#   tools/sh/golden_dump.sh --update   # rigenera i riferimenti (dopo una modifica VOLUTA)
#
# Per ogni sorgente in tools/golden/src/<nome>.bas produce:
#   <nome>.ast     `gmb dump ast`
#   <nome>.disasm  `gmb dump disasm` (bytecode A1/GMBC)
#   <nome>.c       `gmb dump c --no-exe` (C99Native)
# e li confronta con tools/golden/expected/. Le differenze sono regressioni (o modifiche da
# approvare con --update): il test è deterministico, nessuna esecuzione del programma.
# I path assoluti nei dump sono normalizzati in "<nome>.bas" (il checkout cambia).
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
gmb=${GMB_CLI:-$root/GMBasic/native/gmb_compiler_c/build/gmb}
src="$root/tools/golden/src"
exp="$root/tools/golden/expected"
update=0
[ "${1:-}" = "--update" ] && update=1

[ -x "$gmb" ] || { echo "?gmb non costruito: $gmb" >&2; echo "  cmake -S GMBasic/native/gmb_compiler_c -B GMBasic/native/gmb_compiler_c/build && cmake --build GMBasic/native/gmb_compiler_c/build" >&2; exit 2; }
[ -d "$src" ] || { echo "?manca $src" >&2; exit 2; }
mkdir -p "$exp"

tmp=$(mktemp -d /tmp/gmb_golden.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

fails=0; total=0
for f in "$src"/*.bas; do
    n=$(basename "$f" .bas)
    # [dump] file atteso, comando
    for kind in ast disasm c; do
        out="$tmp/$n.$kind"
        case $kind in
            ast)    "$gmb" dump ast "$f" > "$out" 2>/dev/null ;;
            disasm) "$gmb" dump disasm "$f" --quiet > "$out" 2>/dev/null ;;
            c)      "$gmb" dump c "$f" --out "${out}.src" --quiet >/dev/null 2>&1 && mv "${out}.src" "$out" ;;
        esac
        # normalizza il path assoluto del sorgente
        if [ -f "$out" ]; then
            sed -e "s|$f|$n.bas|g" -e "s|$src/||g" "$out" > "$out.norm" && mv "$out.norm" "$out"
        fi
        total=$((total+1))
        if [ "$update" = 1 ]; then
            cp "$out" "$exp/$n.$kind"
            continue
        fi
        if ! diff -q "$exp/$n.$kind" "$out" >/dev/null 2>&1; then
            echo "DIFF  $n.$kind"
            diff -u "$exp/$n.$kind" "$out" 2>/dev/null | head -20 | sed 's/^/      /'
            fails=$((fails+1))
        fi
    done
done

if [ "$update" = 1 ]; then
    echo "golden aggiornati: $total dump in $exp"
    exit 0
fi
if [ "$fails" -eq 0 ]; then
    echo "GOLDEN OK ($total dump)"
    exit 0
fi
echo "GOLDEN: $fails/$total differenze (rilancia con --update se la modifica è voluta)"
exit 1
