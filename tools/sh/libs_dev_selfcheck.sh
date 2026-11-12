#!/usr/bin/env bash
# libs_dev_selfcheck.sh — GATE del gruppo `dev` (F3 del piano Toolchain-Zig).
#
# PERCHÉ ESISTE. `dev` non è un bundle di librerie da caricare: è il payload per COMPILARE
# (`include/**` di terze parti + header nostri + `lib/*.a`). `libs_bundle_selfcheck.sh` guarda le
# dipendenze dinamiche e su `dev` non misura nulla (0 dylib). Il criterio del piano (§4.2) è:
# *ogni `#include <…>` dei sorgenti runtime deve risolvere dentro `dev`, ogni `-l` della riga di
# link deve avere la sua libreria nel bundle*.
#
# COME MISURA. Non si greppa il testo: si chiede al PREPROCESSORE della triple di elencare le
# dipendenze reali di ogni sorgente (`zig cc -target … -M`, solo preprocessing, nessun oggetto).
# Così contano solo gli include che QUEL target prende davvero: `<winsock2.h>` dentro
# `#ifdef _WIN32` non è richiesto su macOS, `<mach/mach.h>` dentro `#ifdef __APPLE__` non è
# richiesto su Windows. (Greppando a testo si ottenevano 16-18 falsi mancanti per RID, misurato
# il 2026-11-11.) L'unico giudizio di "mancante" è `fatal error: '…' file not found`, cioè
# esattamente ciò che impedisce di compilare.
#
# CLASSIFICAZIONE delle dipendenze risolte:
#   bundle     `<dir>/include/…`                       → è ciò che F3 deve distribuire
#   toolchain  sysroot/libc di zig, SDK macOS           → libc/POSIX del target
#   locale     albero del monorepo (header `"…"` dei progetti)
#   LLMEnv     checkout LLMEnv affiancato               → D15: NON si vendorizza, non è un buco
#   altro      fuori da tutte le precedenti             → da classificare (fallisce)
#
# Le `-l` vengono dal runner C (l'unica riga di link). Nel conteggio NON entrano:
#   - le librerie di SISTEMA (`-lm`, `-lpthread`, `-lcompression`, …);
#   - le librerie di RUNTIME/ENGINE (`-llmcodec_*`, `-llmtoken`, `-lgmb_*`): sono il gruppo
#     `runtime`/`engine`, non il payload di compilazione — chiederle qui renderebbe `dev`
#     impossibile per costruzione (misurato il 2026-11-11: era l'unico residuo).
# Restano richieste le terze parti (SDL2, SDL2_ttf, curl, sqlite3, zstd, brotli, lz4, lzma,
# pcre2, crypto, ssl, freetype): senza, una macchina pulita non compila e non linka.
#
# USO
#   tools/sh/libs_dev_selfcheck.sh <cartella> [--rid RID] [--src DIR] [--jobs N] [--quiet]
#     <cartella>  staging del bundle (`include/` e `lib/` al primo livello)
#     --rid RID   osx-arm64|osx-x64|linux-x64|linux-arm64|win-x64|win-arm64 (default: host)
#     --src DIR   sorgenti da censire (default: GMBasic/native/gmb_runtime_c/src)
#     --jobs N    parallelismo del censimento (default 8)
#     --strict    anche gli header OPT-IN (Vulkan) devono essere nel bundle
#     --quiet     solo la riga di esito e gli errori
#   exit 0 = il bundle basta · 1 = manca qualcosa · 2 = uso/argomenti
#
# NESSUNA variabile d'ambiente controlla il comportamento (AGENTS): solo argomenti.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"            # radice del monorepo
ZW="$SCRIPT_DIR/zigcc.sh"                          # UNICO punto di risoluzione della toolchain (F1)

DIR=""; RID=""; SRCDIR=""; QUIET=0; JOBS=8; STRICT=0
while [ $# -gt 0 ]; do
  case "$1" in
    --rid)   RID="${2:-}"; shift 2 ;;
    --src)   SRCDIR="${2:-}"; shift 2 ;;
    --jobs)  JOBS="${2:-8}"; shift 2 ;;
    --strict) STRICT=1; shift ;;
    --quiet) QUIET=1; shift ;;
    -h|--help) sed -n '2,47p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
    *)  [ -z "$DIR" ] && DIR="$1" || { echo "argomento inatteso: $1" >&2; exit 2; }; shift ;;
  esac
done
[ -n "$DIR" ] && [ -d "$DIR" ] || { echo "libs_dev_selfcheck: manca la cartella del bundle" >&2; exit 2; }
[ -n "$SRCDIR" ] || SRCDIR="$ROOT/GMBasic/native/gmb_runtime_c/src"
[ -d "$SRCDIR" ] || { echo "libs_dev_selfcheck: sorgenti non trovati: $SRCDIR" >&2; exit 2; }

host_rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)   echo osx-arm64 ;;
    Darwin-x86_64)  echo osx-x64 ;;
    Linux-x86_64)   echo linux-x64 ;;
    Linux-aarch64|Linux-arm64) echo linux-arm64 ;;
    MINGW*|MSYS*|CYGWIN*) case "$(uname -m)" in arm64|aarch64) echo win-arm64 ;; *) echo win-x64 ;; esac ;;
    *) echo "" ;;
  esac
}
rid_triple() {   # tabella RID → triple del piano (§17.1): RID ≠ triple
  case "$1" in
    osx-arm64)   echo aarch64-macos ;;
    osx-x64)     echo x86_64-macos ;;
    linux-x64)   echo x86_64-linux-gnu ;;
    linux-arm64) echo aarch64-linux-gnu ;;
    win-x64)     echo x86_64-windows-gnu ;;
    win-arm64)   echo aarch64-windows-gnu ;;
    *) return 1 ;;
  esac
}
[ -n "$RID" ] || RID="$(host_rid)"
TRIPLE="$(rid_triple "$RID")" || { echo "libs_dev_selfcheck: RID non valido: '$RID'" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/gmbdevchk.XXXXXX")" || exit 2
trap 'rm -rf "$TMP"' EXIT
say() { [ "$QUIET" = "1" ] || echo "$@"; }

# ── 0) struttura ────────────────────────────────────────────────────────────────────────────
for sub in include lib; do
  [ -d "$DIR/$sub" ] || { echo "libs_dev_selfcheck: manca $DIR/$sub (bundle dev malformato)" >&2; exit 2; }
done
DIR_REAL="$(cd "$DIR" && pwd -P)"
INC_N=$(find "$DIR/include" -type f 2>/dev/null | wc -l | tr -d ' ')
LIB_N=$(find "$DIR/lib" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' ')
say "libs_dev_selfcheck: rid=$RID ($TRIPLE) · $INC_N header · $LIB_N archivi in $DIR"

# ── 1) include esterni: LLMEnv (D15), progetti del monorepo, sysroot macOS ──────────────────
LLM="${LLMENV:-}"
if [ -z "$LLM" ]; then
  for c in "$HOME/Documents/Projects/LLMEnv" "$ROOT/../LLMEnv" "$ROOT/../../LLMEnv"; do
    [ -d "$c/LLMEngine" ] && { LLM="$c"; break; }
  done
fi
: > "$TMP/inc.txt"
LLM_REAL=""
if [ -n "$LLM" ] && [ -d "$LLM/LLMEngine" ]; then
  LLM_REAL="$(cd "$LLM" && pwd -P)"
  for d in LLMCodec/include LLMEngine/include LLMEngine/moe/include LLMEngine/asr/include \
           LLMEngine/Options LLMEngine/plugin LLMDesc LLMToken LLMToken/include LLMEngine/mlx LLMJinja; do
    [ -d "$LLM/$d" ] && printf -- '-I %s\n' "$LLM/$d" >> "$TMP/inc.txt"
  done
  say "   LLMEnv: $LLM (header esterni, non nel bundle — D15)"
else
  say "   LLMEnv: assente (gli include LLMEnv risulteranno mancanti)"
fi
# header INTERNI dei progetti del monorepo: nel build reale sono `-I` del CMake, non payload
# di `dev` (stessa lista di tools/c/build-windows.sh e del gate F2 di §9.6)
for d in GMBasic/gmbllm/include GMBasic/gmbllm/include/quant GMBasic/gmbllm/include/train \
         GMBasic/gmbllm/include/lora GMBasic/native/gmb_doc_c/include GMBasic/native/gmb_chart/include \
         GMBasic/native/gmb_agent_runtime/include GMBasic/native/gmb_agent_runtime \
         GMBasic/native/gmb_compiler_c/include GMBasic/native/gmb_db_c/include \
         GMBasic/native/gmb_torch/include GMBasic/native/gmb_runtime_c/src; do
  [ -d "$ROOT/$d" ] && printf -- '-I %s\n' "$ROOT/$d" >> "$TMP/inc.txt"
done
SDK=""
case "$RID" in
  osx-*)
    SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"
    if [ -n "$SDK" ]; then printf -- '-isysroot %s\n' "$SDK" >> "$TMP/inc.txt"; say "   SDK macOS: $SDK"; fi ;;
esac
WINC=""
case "$RID" in
  win-*)
    WINC="$DIR/include/gmb_win32.h"
    [ -f "$WINC" ] || { echo "libs_dev_selfcheck: manca lo shim win32 ($WINC)" >&2; exit 2; } ;;
esac

# ── 2) dipendenze REALI per la triple: `-M` per sorgente, in parallelo, nessun oggetto ──────
mkdir -p "$TMP/deps" "$TMP/err" "$TMP/work"
{
  printf '#!/usr/bin/env bash\nsrc="$1"; b="$(basename "$src" .c)"\ncd "%s" || exit 1\n' "$TMP/work"
  printf 'exec "%s" -target %s -M -I "%s" ' "$ZW" "$TRIPLE" "$DIR/include"
  while IFS= read -r l; do [ -n "$l" ] && printf '%s ' "$l"; done < "$TMP/inc.txt"
  [ -n "$WINC" ] && printf -- '-include "%s" ' "$WINC"
  printf '"$src"\n'
} > "$TMP/probe.sh"
chmod +x "$TMP/probe.sh"
find "$SRCDIR" -type f -name '*.c' -print0 2>/dev/null | xargs -0 -n 1 -P "$JOBS" \
  bash -c '"$0" "$1" > "'"$TMP"'/deps/$(basename "${1%.c}").d" 2> "'"$TMP"'/err/$(basename "${1%.c}").err" || true' \
  "$TMP/probe.sh"
N_SRC=$(find "$SRCDIR" -type f -name '*.c' | wc -l | tr -d ' ')

cat "$TMP"/deps/*.d 2>/dev/null | tr ' \\' '\n\n' | grep -E '^/' | sort -u > "$TMP/headers_raw.txt"

IN=0; SYS=0; LLMN=0; LOC=0; OTHER_N=0
: > "$TMP/other.txt"
while IFS= read -r p; do
  [ -n "$p" ] || continue
  case "$p" in
    "$DIR"/*|"$DIR_REAL"/*)                 IN=$((IN+1)); continue ;;
    */toolchains/zig/*/lib/*|*/lib/clang/*) SYS=$((SYS+1)); continue ;;
    "$ROOT"/*)                              LOC=$((LOC+1)); continue ;;
  esac
  if [ -n "$SDK" ] && [ "${p#"$SDK"/}" != "$p" ]; then SYS=$((SYS+1)); continue; fi
  if [ -n "$LLM_REAL" ] && [ "${p#"$LLM_REAL"/}" != "$p" ]; then LLMN=$((LLMN+1)); continue; fi
  printf '%s\n' "$p" >> "$TMP/other.txt"; OTHER_N=$((OTHER_N+1))
done < "$TMP/headers_raw.txt"

# mancanti = errori del preprocessore (unico giudizio affidabile)
: > "$TMP/missing.txt"
grep -h 'fatal error:' "$TMP"/err/*.err 2>/dev/null \
  | sed -E "s/.*fatal error: '([^']+)' file not found.*/\1/" | grep -v 'fatal error:' \
  | sort -u > "$TMP/missing.txt"
FAILED_SRC=$(grep -l 'fatal error:' "$TMP"/err/*.err 2>/dev/null | wc -l | tr -d ' ')
# OPT-IN (piano §2.5): il present Vulkan è opzionale e le sue header non viaggiano in `dev`.
# Restano un buco SOLO con `--strict`, così non mascherano mancanze vere (SDL, curl, …).
: > "$TMP/optin.txt"; : > "$TMP/missing_req.txt"
while IFS= read -r h; do
  [ -n "$h" ] || continue
  case "$h" in vulkan/*|vk_video/*) printf '%s\n' "$h" >> "$TMP/optin.txt" ;; *) printf '%s\n' "$h" >> "$TMP/missing_req.txt" ;; esac
done < "$TMP/missing.txt"
MISS_H=$(wc -l < "$TMP/missing_req.txt" | tr -d ' ')
OPTIN_N=$(wc -l < "$TMP/optin.txt" | tr -d ' ')
[ "$STRICT" = "1" ] && { MISS_H=$((MISS_H + OPTIN_N)); OPTIN_N=0; }

# ── 3) `-l` della riga di link ↔ archivi del bundle ─────────────────────────────────────────
RUNNER="$ROOT/tools/sh/gmb_c99native_run.sh"
# SOLO il blocco della riga di LINK (da `-o "$OUTBIN"` alla `fi` che lo chiude). Cercare le `-l`
# in tutto il file prende i token dei COMMENTI (`-linux`, `-linux-musl`) e l'operatore di test
# `[ "$_i" -lt … ]`: misurato il 2026-11-11, il gate segnalava librerie inesistenti `linux`,
# `linux-musl`, `t`. Dentro il blocco di link non c'è nessuno dei due.
awk '/-o "\$OUTBIN"/{f=1} f{print} f && /^fi$/{exit}' "$RUNNER" 2>/dev/null > "$TMP/link.txt"
grep -oE -- '-l[a-zA-Z0-9_+.-]+' "$TMP/link.txt" 2>/dev/null \
  | grep -vxE -- '-l(t|e)' | sed 's/^-l//' | sort -u > "$TMP/links_raw.txt"
# librerie di SISTEMA: non stanno nel bundle (libc, matematica, Win32). `compression` e `bz2`
# sono Apple-only / non usate dal runtime su Linux (`gmb_compress` usa zlib/zstd/brotli/lzma/lz4):
# fuori da Darwin non si possono chiedere a un bundle `.so`.
SYS_LIBS=" m pthread dl rt c crypt resolv util iconv z intl msvcrt kernel32 user32 gdi32 shell32 ole32 advapi32 ws2_32 "
SKIP_LIBS=""
case "$RID" in
  osx-*) SYS_LIBS="$SYS_LIBS compression bz2 " ;;
  # `-lcompression` è un framework di macOS: su Linux/Windows la voce non esiste e non è
  # assimilabile a una libreria del bundle → NON APPLICABILE, non «mancante».
  *)     SYS_LIBS="$SYS_LIBS bz2 "; SKIP_LIBS=" compression " ;;
esac
L_OK=0; L_SYS=0; L_ENG=0; L_NA=0; : > "$TMP/missing_l.txt"
while IFS= read -r n; do
  [ -n "$n" ] || continue
  case "$SYS_LIBS" in *" $n "*) L_SYS=$((L_SYS+1)); continue ;; esac
  case "$SKIP_LIBS" in *" $n "*) L_NA=$((L_NA+1)); continue ;; esac
  case "$n" in llmcodec_*|llmtoken|llmengine_*|gmb_*) L_ENG=$((L_ENG+1)); continue ;; esac
  found=0
  for cand in "lib$n.a" "lib$n.dylib" "lib$n.so" "lib$n.dll.a" "$n.lib" "lib$n.lib"; do
    [ -f "$DIR/lib/$cand" ] && { found=1; break; }
  done
  if [ "$found" = "0" ]; then
    for f in "$DIR/lib/lib$n".so.*; do [ -f "$f" ] && { found=1; break; }; done
  fi
  if [ "$found" = "1" ]; then L_OK=$((L_OK+1)); else printf '%s\n' "$n" >> "$TMP/missing_l.txt"; fi
done < "$TMP/links_raw.txt"
MISS_L=$(wc -l < "$TMP/missing_l.txt" | tr -d ' ')
L_WANT=$((L_OK + MISS_L))

# ── 4) esito ───────────────────────────────────────────────────────────────────────────────
if [ "$MISS_H" -gt 0 ]; then
  echo "libs_dev_selfcheck: $MISS_H header non risolti ($FAILED_SRC/$N_SRC sorgenti in errore):" >&2
  sed 's/^/    /' "$TMP/missing_req.txt" >&2
fi
if [ "$OPTIN_N" -gt 0 ]; then
  say "   opt-in (Vulkan) assenti dal bundle: $OPTIN_N — non bloccano (--strict per esigerli)"
fi
if [ "$MISS_L" -gt 0 ]; then
  echo "libs_dev_selfcheck: $MISS_L librerie di link assenti dal bundle:" >&2
  sed 's/^/    -l/' "$TMP/missing_l.txt" >&2
fi
if [ "$OTHER_N" -gt 0 ]; then
  echo "libs_dev_selfcheck: $OTHER_N header fuori da bundle/toolchain/LLMEnv:" >&2
  head -20 "$TMP/other.txt" | sed 's/^/    /' >&2
  [ "$OTHER_N" -gt 20 ] && echo "    … e altri $((OTHER_N-20))" >&2
fi

STATUS="PASS"; { [ "$MISS_H" -eq 0 ] && [ "$MISS_L" -eq 0 ] && [ "$OTHER_N" -eq 0 ]; } || STATUS="FAIL"
say "   sorgenti $N_SRC · header: bundle $IN · toolchain $SYS · LLMEnv $LLMN · monorepo $LOC · fuori $OTHER_N"
echo "libs_dev_selfcheck: rid=$RID · -l $L_OK/$L_WANT nel bundle ($L_SYS di sistema, $L_ENG runtime/engine, $L_NA non applicabili) · $STATUS"
[ "$STATUS" = "PASS" ] || exit 1
exit 0
