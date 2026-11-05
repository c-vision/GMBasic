#!/usr/bin/env bash
# libs_verify_load.sh — GATE di caricabilità: prova a fare `dlopen` di OGNI libreria della cache
# (`~/.gmbasic/libs/<rid>/`) in un PROCESSO SEPARATO, con il path dinamico puntato alla cache stessa
# — cioè come farà il runtime. Scova la classe di bug "dipendenza mancante dal bundle" (un file
# linkato con `LC_LOAD_DYLIB` non presente → il dlopen fallisce, ma nessun `sha256` se ne accorge).
#
# PERCHÉ ESISTE. Caso reale (2026-10-28): `libgmb_ai_mlx` dipendeva da `libllm_ai_mlx`, che NON era
# né nel manifest né nel bundle → l'engine MLX non partiva, e `libs check` era verde (gli sha dei
# file presenti erano corretti). Gli sha dicono "file integro", non "grafo delle dipendenze completo".
#
# USO
#   tools/sh/libs_verify_load.sh [DIR] [--quiet]
#     DIR      cartella da verificare (default: ~/.gmbasic/libs/<rid della macchina>)
#     --quiet  stampa solo il riepilogo e i fallimenti
#
# EXIT: 0 = tutte caricabili · 1 = almeno una NON caricabile (stampa nome + dipendenza mancante).
#
# NOTA: è uno strumento di sviluppo/CI, non del runtime. Carica le dylib per davvero: alcune
# inizializzano sottosistemi (SDL, torch) — per questo ogni lib gira in un processo figlio isolato.
set -euo pipefail

DIR="${1:-}"; [ "${DIR:-}" = "--quiet" ] && { DIR=""; QUIET=1; }
QUIET="${QUIET:-0}"
[ "${2:-}" = "--quiet" ] && QUIET=1

default_rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) echo "osx-arm64";; Darwin-x86_64) echo "osx-x64";;
    Linux-x86_64) echo "linux-x64";;  Linux-aarch64|Linux-arm64) echo "linux-arm64";;
    MINGW*|MSYS*|CYGWIN*) echo "win-x64";; *) echo "osx-arm64";;
  esac
}
[ -n "$DIR" ] || DIR="$HOME/.gmbasic/libs/$(default_rid)"
[ -d "$DIR" ] || { echo "cartella non trovata: $DIR" >&2; exit 1; }

case "$(uname -s)" in
  Darwin) VAR=DYLD_LIBRARY_PATH ;;
  *)      VAR=LD_LIBRARY_PATH ;;
esac
# dlopen isolato SENZA Python: un mini-programma C compilato al volo con `cc` (già dipendenza
# della toolchain). Stampa la causa se il caricamento fallisce, exit 1.
CC_BIN="${CC:-cc}"
command -v "$CC_BIN" >/dev/null || { echo "serve un compilatore C (cc) per il dlopen isolato" >&2; exit 1; }
WORKD="$(mktemp -d "${TMPDIR:-/tmp}/gmb-verify-load.XXXXXX")"
trap 'rm -rf "$WORKD"' EXIT
cat > "$WORKD/dlopen_probe.c" <<'PROBE'
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    /* Argomenti 2..N = dipendenze `@rpath/…` presenti nella stessa cartella: si caricano PRIMA
     * (RTLD_GLOBAL). Con flat namespace un simbolo fornito da un'altra nostra lib gia' caricata
     * risultava "symbol not found in flat namespace" — falso positivo (caso reale:
     * libgmb_fonts → _gmb_alert_box_centered, definito in libgmb_gfx/libgmb_compress). */
    for (int i = 2; i < argc; i++) (void)dlopen(argv[i], RTLD_NOW | RTLD_GLOBAL);
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    if (!h) { printf("%s\n", dlerror()); return 1; }
    dlclose(h);
    return 0;
}
PROBE
"$CC_BIN" -O0 -w "$WORKD/dlopen_probe.c" -o "$WORKD/dlopen_probe" $( [ "$(uname -s)" = "Linux" ] && echo "-ldl" ) \
  || { echo "compilazione del probe dlopen fallita" >&2; exit 1; }

# ── CHIUSURA DELLE DIPENDENZE `@rpath/…` ───────────────────────────────────────────────────
# Regola: un bundle è valido solo se OGNI dipendenza `@rpath/<nome>` di OGNI sua libreria
# esiste NELLA STESSA CARTELLA (o è una libreria di sistema con path assoluto).
#
# PERCHÉ ESISTE. Lo `sha256` dice "file integro", il `dlopen` dice "si carica su questa
# macchina (con questa cache)": nessuno dei due dice "il set è completo nel bundle".
# Caso reale (2026-11-04): `libSDL2.dylib`/`libSDL3.dylib` erano SALTATI da questo ciclo
# (`continue`), quindi il caso "SDL3 mancante accanto a SDL2" non veniva mai visto qui ma
# arrivava all'utente come `Failed loading SDL3 library` a runtime. SDL2 e SDL3 sono un
# set INDIVISIBILE (la SDL2 distribuita può essere sdl2-compat, che pretende SDL3): lo
# verifichiamo per chiusura, che non richiede un display e quindi vale anche in CI.
lib_deps() { # $1 = libreria → una dipendenza per riga (senza header né install name)
  case "$(uname -s)" in
    Darwin) otool -L "$1" 2>/dev/null | tail -n +3 | awk '{print $1}' ;;
    *)      readelf -d "$1" 2>/dev/null | sed -n 's/.*\[\(.*\)\]/\1/p' ;;
  esac
}

closure_missing() { # $1 = libreria → nomi che mancano nella sua cartella (vuoto = chiusa)
  _dir="$(dirname "$1")"; _self="$(basename "$1")"; _out=""
  for _dep in $(lib_deps "$1"); do
    case "$_dep" in
      "$_self")       continue ;;
      @rpath/*)       _n="${_dep#@rpath/}" ;;
      @loader_path/*) _n="${_dep#@loader_path/}" ;;
      /*)             continue ;;   # /usr/lib, /System/...: di sistema, non del bundle
      *)              _n="$_dep" ;;
    esac
    [ -f "$_dir/$_n" ] || _out="$_out $_n"
  done
  printf '%s' "$_out"
}

total=0; bad=0; warn=0; sdl=0
for f in "$DIR"/*.dylib "$DIR"/*.so "$DIR"/*.so.* "$DIR"/*.dll; do
  [ -f "$f" ] || continue
  base="$(basename "$f")"
  # SDL2/SDL3: al `dlopen` inizializzano il video e senza display (container/CI) falliscono per
  # l'AMBIENTE, non per il bundle → su questi NON si fa dlopen, ma la CHIUSURA sì (era il buco).
  case "$base" in
    libSDL2*|libSDL3*|SDL2*.dll|SDL3*.dll)
      total=$((total+1)); sdl=$((sdl+1))
      miss="$(closure_missing "$f")"
      if [ -n "$miss" ]; then
        bad=$((bad+1))
        printf '  ✗ %-34s dipendenze mancanti:%s\n' "$base" "$miss"
      elif [ "$QUIET" = "1" ]; then :
      else printf '  ✓ %s (chiusura ok)\n' "$base"; fi
      continue ;;
  esac
  total=$((total+1))
  # Dipendenze sorelle gia' nella cartella: si pre-caricano nel probe (flat namespace).
  preload=()
  for _d in $(lib_deps "$f"); do
    case "$_d" in
      @rpath/*)       _n="$DIR/${_d#@rpath/}" ;;
      @loader_path/*) _n="$DIR/${_d#@loader_path/}" ;;
      *)              continue ;;
    esac
    [ "$_n" = "$f" ] || { [ -f "$_n" ] && preload+=("$_n"); }
  done
  err="$(env "$VAR=$DIR" "$WORKD/dlopen_probe" "$f" ${preload[@]+"${preload[@]}"} 2>&1)" || {
    # riga utile: quella con "Library not loaded"/"image not found" se c'è, altrimenti la prima
    msg="$(printf '%s' "$err" | grep -m1 -E 'Library not loaded|image not found|cannot open shared|undefined symbol|symbol not found' || printf '%s' "$err" | head -1)"
    case "$msg" in
      *'symbol not found in flat namespace'*)
        # NON è un buco del bundle: la lib usa un simbolo definito da una lib SORELLA che il
        # runtime carica prima con RTLD_GLOBAL (caso reale: libgmb_fonts → _gmb_alert_box_centered,
        # definito in libgmb_gfx/libgmb_compress, che libgmb_fonts non linka per design).
        # Contarlo come errore renderebbe il gate inutilizzabile (lo era: 151/152 con un falso rosso).
        warn=$((warn+1))
        printf '  ⚠ %-34s simbolo di una lib sorella (RTLD_GLOBAL nel runtime): %s\n' "$base" "$msg" ;;
      *)
        bad=$((bad+1))
        printf '  ✗ %-34s %s\n' "$base" "$msg" ;;
    esac
  }
  [ "$QUIET" = "1" ] || printf '  ✓ %s\n' "$base"
done

echo "[libs] caricabilità: $((total-bad))/$total OK in $DIR (di cui $sdl SDL verificati per chiusura, $warn avvisi)"
[ "$bad" = "0" ] || { echo "[libs] $bad librerie NON caricabili (dipendenza mancante dal bundle?)" >&2; exit 1; }
