#!/usr/bin/env bash
# GMBASIC — freschezza delle lib dei pacchetti rispetto ai sorgenti C del pacchetto.
#
# PERCHE'. Una `lib/<rid>/lib<nome>.*` PIU' VECCHIA dei sorgenti C e' una lib NON ricostruita:
# dopo l'ultima modifica al pacchetto non e' stato rieseguito `build.sh`. Il sintomo e' subdolo
# — l'API BASIC c'e' (l'`.inc` aggiornato) ma il simbolo nuovo manca, oppure il comportamento
# e' quello vecchio — e l'errore arriva lontano, come "simbolo non trovato" o come valore
# sbagliato. Meglio bloccarsi al commit che inseguire un falso bug del compilatore.
#
# COSA FA. Per ogni pacchetto del workspace con sorgenti C (`src/*.c`, `include/*.h`) confronta
# il mtime di OGNI lib (`*.dylib|*.so|*.dll` sotto `lib/`) con il PIU' RECENTE fra i sorgenti:
# lib piu' vecchia = STALE. Sorgenti ma nessuna lib buildata = NOLIB (non e' un errore: la lib
# si costruisce quando serve). Pacchetti senza sorgenti C (solo BASIC) sono ignorati.
#
# Uso:
#   tools/sh/gmb_pkg_libcheck.sh                   # tutti i pacchetti del workspace
#   tools/sh/gmb_pkg_libcheck.sh webp avif         # solo i pacchetti indicati (per nome)
#   tools/sh/gmb_pkg_libcheck.sh --quiet webp      # solo le righe STALE
#   tools/sh/gmb_pkg_libcheck.sh --root <dir>      # radice alternativa (default: repo)
#
# Output (TSV, una riga per evento; path relativi alla cartella del pacchetto):
#   CHECKED <pkg/ver> <n-lib>
#   NOLIB   <pkg/ver> <n-sorgenti>
#   STALE   <pkg/ver> <lib> <sorgente-piu-recente>
#
# Exit: 0 = nessuna lib stale · 1 = almeno una lib da ricostruire · 2 = uso errato.
set -u

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SELF_DIR/../.." && pwd)"
QUIET=0
PKGS=()

usage() { awk 'NR>1 && /^#/ {sub(/^# ?/, ""); print; next} NR>1 {exit}' "$0"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --quiet|-q) QUIET=1; shift ;;
        --root)     [ $# -ge 2 ] || { echo "--root richiede una directory" >&2; exit 2; }
                    ROOT="$2"; shift 2 ;;
        -h|--help)  usage; exit 0 ;;
        --)         shift; break ;;
        -*)         echo "opzione sconosciuta: $1" >&2; exit 2 ;;
        *)          break ;;
    esac
done
PKGS=("$@")

# want <nome-pacchetto>  — nessun filtro = tutti; altrimenti solo i nomi richiesti.
want() {
    [ "${#PKGS[@]}" -eq 0 ] && return 0
    local w
    for w in "${PKGS[@]}"; do [ "$w" = "$1" ] && return 0; done
    return 1
}

base="$ROOT/packages"
if [ ! -d "$base" ]; then
    echo "ERRORE: cartella packages/ assente in $ROOT" >&2
    exit 2
fi

checked=0
stale=0
for pdir in "$base"/*/*/; do
    [ -d "$pdir" ] || continue
    ver="$(basename "$pdir")"
    case "$ver" in [0-9]*.[0-9]*) ;; *) continue ;; esac
    rel="${pdir#"$base"/}"
    rel="${rel%/}"
    want "${rel%%/*}" || continue

    srcn="$(find "$pdir/src" "$pdir/include" -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null | wc -l | tr -d ' ')"
    [ "$srcn" -gt 0 ] || continue

    libs="$(find "$pdir/lib" -type f \( -name '*.dylib' -o -name '*.so' -o -name '*.dll' \) 2>/dev/null | sort)"
    if [ -z "$libs" ]; then
        [ "$QUIET" -eq 1 ] || printf 'NOLIB\t%s\t%s\n' "$rel" "$srcn"
        continue
    fi

    nlib=0
    while IFS= read -r lib; do
        [ -n "$lib" ] || continue
        nlib=$((nlib + 1))
        newf="$(find "$pdir/src" "$pdir/include" -type f \( -name '*.c' -o -name '*.h' \) -newer "$lib" 2>/dev/null | head -1)"
        [ -n "$newf" ] || continue
        stale=$((stale + 1))
        printf 'STALE\t%s\t%s\t%s\n' "$rel" "${lib#"$pdir"/}" "${newf#"$pdir"/}"
    done <<< "$libs"

    checked=$((checked + 1))
    [ "$QUIET" -eq 1 ] || printf 'CHECKED\t%s\t%s\n' "$rel" "$nlib"
done

[ "$stale" -gt 0 ] && exit 1
exit 0
