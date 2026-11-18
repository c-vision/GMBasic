#!/usr/bin/env bash
# tools/sh/libs_fetch_rid.sh — popola la cache di un RID dalle release, SENZA il CLI C.
#
# PERCHÉ SERVE. `gmbadmin libs ensure` prende il RID dall'host (`gmb_libs_rid()`): non ha un
# `--rid`, quindi da macOS non può riempire `~/.gmbasic/libs/linux-x64/`. Per il gate di F5
# (link per `linux-x64` da macOS) quella cache deve esistere, e la fonte autorevole è l'indice
# `shared/settings/libs-index-<rid>.tsv` (nome<TAB>sha256<TAB>size<TAB>url, già proiettato dal
# job `index`). Qui si scarica SOLO ciò che manca o ha sha diverso.
#
# Uso:
#   tools/sh/libs_fetch_rid.sh <rid>            # scarica ciò che manca
#   tools/sh/libs_fetch_rid.sh <rid> --list     # elenca senza scaricare
#   tools/sh/libs_fetch_rid.sh <rid> --force    # riscarica tutto
#
# NIENTE variabili d'ambiente: RID e cache sono argomenti (`--cache` per una destinazione diversa).
set -uo pipefail

RID=""; MODE="fetch"; CACHE=""; JOBS=8
# ── Modalità interna `--one <dest> <nome> <sha> <url>`: un file, usata dai worker paralleli ──
# (bash 3.2 non ha `wait -n` né process substitution affidabile per un pool: `xargs -P` sì).
if [ "${1:-}" = "--one" ]; then
  dest="$2/$3"; sha="$4"; url="$5"
  mkdir -p "$(dirname "$dest")"
  tmp="$dest.part"
  if ! curl -sSfL --retry 2 --connect-timeout 20 --max-time 120 -o "$tmp" "$url"; then
    rm -f "$tmp"; echo "ERR download $3" >&2; exit 1; fi
  got="$(shasum -a 256 "$tmp" 2>/dev/null | cut -d' ' -f1)"
  if [ "$got" != "$sha" ]; then rm -f "$tmp"; echo "ERR sha $3" >&2; exit 1; fi
  mv -f "$tmp" "$dest"; echo "OK $3"; exit 0
fi
while [ $# -gt 0 ]; do
  case "$1" in
    --list)  MODE="list"; shift ;;
    --force) MODE="force"; shift ;;
    --cache) CACHE="${2:-}"; shift 2 ;;
    --cache=*) CACHE="${1#--cache=}"; shift ;;
    -h|--help) sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "libs_fetch_rid: opzione sconosciuta: $1" >&2; exit 2 ;;
    *) [ -z "$RID" ] && { RID="$1"; shift; } || { echo "libs_fetch_rid: troppi argomenti ($1)" >&2; exit 2; } ;;
  esac
done

die() { echo "libs_fetch_rid: $*" >&2; exit 2; }
[ -n "$RID" ] || die "serve il RID (osx-arm64|osx-x64|linux-x64|linux-arm64|win-x64|win-arm64)"

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INDEX="$MONO/shared/settings/libs-index-$RID.tsv"
[ -f "$INDEX" ] || die "indice assente: $INDEX (rigeneralo con tools/sh/libs_index_publish.sh)"
[ -n "$CACHE" ] || CACHE="$HOME/.gmbasic/libs/$RID"
mkdir -p "$CACHE"

command -v curl >/dev/null 2>&1 || die "curl assente"

sha_of() { shasum -a 256 "$1" 2>/dev/null | cut -d' ' -f1; }
n_ok=0; n_get=0; n_bad=0; n_todo=0
TODO="$(mktemp "${TMPDIR:-/tmp}/libsfetch.XXXXXX")"
trap 'rm -f "$TODO"' EXIT

while IFS=$'\t' read -r name sha size url; do
  case "$name" in ''|\#*) continue ;; esac
  [ -n "$sha" ] && [ -n "$url" ] || continue
  dest="$CACHE/$name"
  if [ "$MODE" = list ]; then
    have="manca"; [ -f "$dest" ] && have="presente"
    printf '  %-46s %10s B  %s\n' "$name" "${size:-?}" "$have"
    continue
  fi
  if [ -f "$dest" ] && [ "$MODE" != force ] && [ "$(sha_of "$dest")" = "$sha" ]; then
    n_ok=$((n_ok+1)); continue
  fi
  printf '%s\t%s\t%s\n' "$name" "$sha" "$url" >> "$TODO"
  n_todo=$((n_todo+1))
done < "$INDEX"

[ "$MODE" = list ] && exit 0
[ "$n_todo" -eq 0 ] && { echo "libs_fetch_rid: $RID — cache già completa ($n_ok file)" >&2; exit 0; }

echo "libs_fetch_rid: $RID → $CACHE — $n_todo da scaricare ($n_ok già validi), $JOBS in parallelo" >&2
# Pool di worker: un `--one` per file, al massimo $JOBS in volo. Niente `xargs -I` + `bash -c`:
# su macOS la riga in input non arrivava integra al worker (campi tab fusi → url vuoto → 404 su
# TUTTI i file). Qui i campi li legge la stessa shell, che li passa come argomenti separati.
: > "$TODO.out"
pids=()
# TRE campi (nome, sha, url): è quello che scrive il writer qui sopra. Leggerne quattro
# lasciava `url` vuota → 404 su OGNI file (osservato: «errori 641» con url vuota).
while IFS=$'\t' read -r n s u; do
  [ -n "$n" ] || continue
  ( if bash "$0" --one "$CACHE" "$n" "$s" "$u" >> "$TODO.out" 2>&1; then :; fi ) &
  pids+=($!)
  if [ ${#pids[@]} -ge "$JOBS" ]; then wait "${pids[0]}" 2>/dev/null; pids=("${pids[@]:1}"); fi
done < "$TODO"
for _p in ${pids[@]+"${pids[@]}"}; do wait "$_p" 2>/dev/null; done
n_get="$(grep -c '^OK ' "$TODO.out" 2>/dev/null || true)"; n_get="${n_get:-0}"
n_bad="$(grep -c '^ERR ' "$TODO.out" 2>/dev/null || true)"; n_bad="${n_bad:-0}"
[ "$n_bad" -gt 0 ] && { echo "  falliti $n_bad:" >&2; grep '^ERR ' "$TODO.out" | head -8 | sed 's/^/    /' >&2; }
rm -f "$TODO.out"
# ── Shim di piattaforma nel bundle Linux: vanno MESSI DA PARTE (non cancellati). ──────────────
# Il bundle Linux porta, oltre agli header veri delle terze parti, un albero POSIX/mingw
# (`include/sys/types.h`, `include/bits/types/time_t.h`, `include/unistd.h`, `include/gmb_win32.h`…)
# che serve ai build Windows. Messo davanti agli header di zig su un target Linux li OMBREGGIA:
# `error: unknown type name '__time64_t'` su 311/419 sorgenti del runtime (misurato 2026-11-12),
# e sotto CMake la build della runtime si ferma subito. Su Linux quei file non servono: si
# spostano in `$CACHE/.shim-backup` (reversibile, nessun dato perso).
prune_platform_shims() {
  case "$RID" in linux-*) ;; *) return 0 ;; esac
  local inc="$CACHE/include" bk="$CACHE/.shim-backup" moved=0 _f
  [ -d "$inc" ] || return 0
  for _f in sys bits netinet arpa machine unistd.h dirent.h dlfcn.h netdb.h poll.h pwd.h \
            regex.h spawn.h termios.h gmb_win32.h; do
    [ -e "$inc/$_f" ] || continue
    mkdir -p "$bk"; rm -rf "$bk/$_f"; mv "$inc/$_f" "$bk/$_f" && moved=$((moved+1))
  done
  [ "$moved" -gt 0 ] && echo "  shim di piattaforma spostati in .shim-backup: $moved (ombreggiavano glibc)" >&2
  return 0
}

echo "libs_fetch_rid: $RID → $CACHE — scaricati $n_get, già validi $n_ok, errori $n_bad" >&2
prune_platform_shims
[ "$n_bad" -eq 0 ]
