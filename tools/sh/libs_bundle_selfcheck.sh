#!/usr/bin/env bash
# libs_bundle_selfcheck.sh — GATE: un bundle deve CHIUDERE le sue dipendenze.
#
# TUTTO il necessario deve stare nel bundle (direttiva 2026-10-29): ogni libreria che un file
# del gruppo carica (`LC_LOAD_DYLIB` / `DT_NEEDED`) deve essere **nello stesso bundle** oppure
# essere una libreria di SISTEMA. Nessun gruppo può dipendere da un altro: se `core` richiede
# `libllmtoken` (gruppo `engine`), il gate FALLISCE e la correzione è pubblicare i due insieme
# (gruppo unico `runtime-core`).
#
# USO
#   tools/sh/libs_bundle_selfcheck.sh <cartella> [--quiet]
#     <cartella>  cartella con i file del bundle (dopo l'estrazione: piatta)
#   exit 0 = chiuso · 1 = dipendenze mancanti (stampa file → dipendenza)
#
# NOTA: elenca anche i file che *dipendono* da qualcosa fuori bundle, così il publish può
# fermarsi prima di caricare un archivio incompleto.
set -uo pipefail
DIR="${1:-}"; [ -n "$DIR" ] && [ -d "$DIR" ] || { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
QUIET=0; [ "${2:-}" = "--quiet" ] && QUIET=1

# nomi di librerie di sistema ammessi (POSIX + runtime C/C++ standard)
is_system() {
  case "$1" in
    /usr/lib/*|/System/*|/Library/Apple/*) return 0;;
    @rpath/libSystem*|@rpath/libobjc*|@rpath/libc++*) return 0;;
    libc.so*|ld-linux*|libm.so*|libpthread*|libdl*|librt*|libgcc_s*|libstdc++*|libresolv*|libutil*) return 0;;
  esac
  return 1
}

deps_of() { # stampa le dipendenze "bare" di un file
  case "$(uname -s)" in
    Darwin) otool -L "$1" 2>/dev/null | tail -n +2 | awk '{print $1}';;
    *)      readelf -d "$1" 2>/dev/null | sed -n 's/.*\[\(.*\)\]/\1/p';;
  esac
}

ERR=0; N=0
while IFS= read -r f; do
  [ -f "$f" ] || continue
  N=$((N+1))
  base="$(basename "$f")"
  while IFS= read -r dep; do
    [ -n "$dep" ] || continue
    is_system "${dep%% *}" && continue
    dep_base="$(basename "${dep%% *}")"
    # presente nel bundle (nome esatto o con alias lib<base>)?
    if [ -e "$DIR/$dep_base" ] || [ -e "$DIR/lib$dep_base" ]; then continue; fi
    printf "  %-34s → %s (non nel bundle)\n" "$base" "$dep"
    ERR=$((ERR+1))
  done < <(deps_of "$f")
done < <(find "$DIR" -maxdepth 1 -type f \( -name "*.dylib" -o -name "*.so" -o -name "*.so.*" -o -name "*.dll" \) | sort)

[ "$QUIET" = "1" ] || echo "libs_bundle_selfcheck: $N file, $ERR dipendenze fuori bundle"
[ "$ERR" = "0" ] || exit 1
exit 0
