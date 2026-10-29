#!/usr/bin/env bash
# libs_index_lib.sh — INDICE TSV delle librerie native per il downloader C (UNA implementazione).
#
# Sorgente condivisa da:
#   tools/sh/gmb_c99native_run.sh        (sample/eseguibili: `<exe>.libs.txt` accanto all'exe)
#   tools/sh/test_minitests_c99native    (runner dei MiniTest C99Native)
#
# PERCHÉ UN FILE SOLO. Il formato `nome<TAB>sha256<TAB>size<TAB>url` è il contratto del downloader
# runtime (`gmb_libs_fetch.c` → `gmb_libs_autoensure`). Se ogni script lo costruisse per conto suo,
# il primo fix resterebbe in uno solo dei due (era già successo con la chiusura transitiva delle
# dipendenze, presente in un runner e non nell'altro).
#
# L'indice NON si ricava dal JSON a runtime: è la PROIEZIONE `shared/settings/libs-index-<rid>.tsv`
# scritta da `tools/sh/libs_manifest_gh.sh build` (niente Python, niente parser JSON nella catena C).
#
# CONTRATTO col chiamante (variabili d'ambiente della shell, non process env):
#   ROOT        radice del checkout GMBasic (es. .../GMBasic)
#   LIBS_CACHE  cache delle lib per il RID (default: ~/.gmbasic/libs/<rid>)
#   LIBS_INDEX  (opzionale) percorso della proiezione TSV; default calcolato da ROOT+RID
#   RID         (opzionale) osx-arm64|osx-x64|linux-*|win-x64; default: dedotto dalla macchina
#
# API:
#   emit_libs_index <out>                 copia TUTTA la proiezione (tutti i file per-file del RID)
#   emit_exe_libs <exe> <out> [c-generato] solo le lib da cui l'exe DIPENDE (+ le FFI dichiarate)
set -u

li_rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)             echo "osx-arm64" ;;
    Darwin-x86_64)            echo "osx-x64" ;;
    Linux-x86_64)             echo "linux-x64" ;;
    Linux-aarch64|Linux-arm64) echo "linux-arm64" ;;
    MINGW*|MSYS*|CYGWIN*)     echo "win-x64" ;;
    *)                        echo "osx-arm64" ;;
  esac
}

li_init() {
  : "${RID:=$(li_rid)}"
  : "${LIBS_CACHE:=$HOME/.gmbasic/libs/$RID}"
  : "${LIBS_INDEX:=$ROOT/../shared/settings/libs-index-$RID.tsv}"
}

# Tutti i file pubblicati per-file per questo RID (indice completo del compilatore).
emit_libs_index() { # $1 = file di output
  li_init
  if [ -f "$LIBS_INDEX" ]; then cp -f "$LIBS_INDEX" "$1"; else : > "$1"; fi
}

# `<exe>.libs.txt`: le lib dell'indice da cui l'eseguibile DIPENDE (letto dal binario: otool -L /
# readelf -d, con chiusura TRANSITIVA) PIÙ le lib FFI dichiarate nel sorgente (`DECLARE … LIB`),
# che non sono dipendenze di link: il runtime le carica con `dlopen`, quindi i nomi si leggono dal
# C GENERATO (`gmb_ffi_dl_open("<nome>")`, scritto dall'emitter).
emit_exe_libs() { # $1 = eseguibile, $2 = output, $3 = C generato (opzionale, per le FFI)
  li_init
  local exe="$1" out="$2" cfile="${3:-}"
  [ -f "$LIBS_INDEX" ] || return 0
  local names=""
  case "$(uname -s)" in
    Darwin) names=$(otool -L "$exe" 2>/dev/null | tail -n +2 | awk '{print $1}' | sed 's|.*/||') ;;
    *)      names=$(readelf -d "$exe" 2>/dev/null | sed -n 's/.*\[\(.*\)\]/\1/p') ;;
  esac
  # FFI (`DECLARE … LIB "<nome>"`): NON è una dipendenza di LINK — il runtime fa `dlopen` — quindi
  # `otool -L`/`readelf -d` non la vedono. Senza questo blocco un eseguibile che fa `REQUIRE amqp`
  # non porta `libgmbamqp.dylib` nel suo libs.txt → su una macchina senza la lib in libs/ e con la
  # cache vuota il `dlopen` fallisce.
  if [ -n "$cfile" ] && [ -f "$cfile" ]; then
    local lext ffilib
    case "$(uname -s)" in Darwin) lext=dylib;; Linux) lext=so;; *) lext=dll;; esac
    for ffilib in $(sed -n 's/.*gmb_ffi_dl_open("\([^"]*\)").*/\1/p' "$cfile" | sort -u); do
      case "$ffilib" in
        lib*) names="$names ${ffilib}.${lext}" ;;
        *)    names="$names lib${ffilib}.${lext} ${ffilib}.${lext}" ;;
      esac
    done
  fi
  [ -n "$names" ] || return 0
  # CHIUSURA TRANSITIVA: una lib dell'indice può dipendere da un'ALTRA lib dell'indice che
  # l'eseguibile non linka direttamente (caso reale: libbrotlidec → libbrotlicommon). Senza
  # questo, l'exe parte e muore su "Library not loaded: libbrotlicommon.1.dylib".
  local queue="$names" seen=" " n f dep d
  while [ -n "$queue" ]; do
    local next=""
    for n in $queue; do
      case "$seen" in *" $n "*) continue;; esac
      seen="$seen$n "
      f=""
      for d in "$LIBS_CACHE" "$ROOT/build/libs/$RID"; do
        [ -f "$d/$n" ] && { f="$d/$n"; break; }
      done
      [ -n "$f" ] || continue
      for dep in $(otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}' | sed 's|.*/||'); do
        case "$seen" in *" $dep "*) ;; *) next="$next $dep";; esac
      done
    done
    queue="$next"
    names="$names $next"
  done
  # shellcheck disable=SC2086
  local wantdir; wantdir="$(mktemp -d "${TMPDIR:-/tmp}/gmblibswant.XXXXXX")"
  printf '%s\n' $names | sed '/^[[:space:]]*$/d' | sort -u > "$wantdir/want"
  : > "$out"
  grep '^#' "$LIBS_INDEX" >> "$out" 2>/dev/null || true
  awk -F'\t' -v OFS='\t' 'NR==FNR{w[$1]=1;next} /^#/{next} ($1 in w)' \
      "$wantdir/want" "$LIBS_INDEX" >> "$out"
  rm -rf "$wantdir"
}
