#!/usr/bin/env bash
# libs_cross_build.sh — archivi delle NOSTRE librerie per un RID diverso dall'host (F5).
#
# Perché CMake e non un glob di sorgenti: la selezione NON è "tutti i .c". `gmb_runtime_c`
# sceglie le implementazioni UI per piattaforma (`gmb_ui_stub.c` debole PRIMO membro,
# `gmb_ui_gfx.c`, `gmb_ui_null.c`), il backend Vulkan solo se l'header esiste, e `APPLE`/
# `WIN32` escludono interi file (AX/CG). Compilando tutto si ottengono DUE definizioni degli
# stessi simboli: misurato, `duplicate symbol: gmb_gfx_cls`, `gmb_widget_style_current`, `main`.
# CMake fa quella selezione e la fa per il target giusto, quindi qui si usa lui.
#
# Due trappole misurate su macOS (2026-11-12):
#   1. `ar` di Apple RIFIUTA oggetti non-Mach-O («archive member … not a mach-o file») e scrive
#      un archivio con ZERO membri: il link poi dichiara undefined OGNI simbolo. Serve `zig ar`.
#      CMake non lo accetta via -DCMAKE_AR (resta l'`ar` di sistema): quindi gli archivi si
#      RICREANO qui dagli oggetti che CMake ha prodotto — la selezione è sua, l'archiviazione
#      nostra.
#   2. `$CACHE/include` NON va messo nel percorso di include: contiene header di piattaforma
#      (`sys/types.h`, `bits/types/time_t.h`) che, davanti a quelli di zig, rompono tutto con
#      «unknown type name '__time64_t'» (311/419 sorgenti). Si linkano solo i pezzi che servono.
#
# Uso: tools/sh/libs_cross_build.sh <rid> [--zig <path>] [--out <dir>] [--jobs N] [--force]
#
# NIENTE variabili d'ambiente: RID, toolchain, destinazione e parallelismo sono argomenti.
set -uo pipefail

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="$MONO/GMBasic"
# Toolchain: NESSUN percorso cablato. Se non arriva `--zig`, la risolve
# `tools/sh/zigcc.sh --print-zig` (ordine canonico del piano F1: --zig → <ver>-<rid> →
# <ver>-* → PATH). Prima qui c'era "0.17.0-osx-arm64": su un host Linux/Windows o con
# un'altra versione il tool moriva.
ZIG=""
RID=""; OUT=""; JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
while [ $# -gt 0 ]; do
  case "$1" in
    --zig)   ZIG="${2:-}"; shift 2 ;;
    --zig=*) ZIG="${1#--zig=}"; shift ;;
    --out)   OUT="${2:-}"; shift 2 ;;
    --out=*) OUT="${1#--out=}"; shift ;;
    --jobs)  JOBS="${2:-}"; shift 2 ;;
    --force) shift ;;
    -h|--help) sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "libs_cross_build: opzione sconosciuta: $1" >&2; exit 2 ;;
    *)  RID="$1"; shift ;;
  esac
done
[ -n "$RID" ] || { echo "libs_cross_build: serve un RID (linux-x64, linux-arm64, osx-x64, win-x64, win-arm64)" >&2; exit 2; }
if [ -z "$ZIG" ]; then
  ZIG="$(bash "$MONO/tools/sh/zigcc.sh" --print-zig)" || {
    echo "libs_cross_build: nessuna toolchain zig (usa --zig <path>, oppure gmbadmin first-use)" >&2
    exit 2
  }
fi
[ -x "$ZIG" ] || { echo "libs_cross_build: zig non eseguibile: $ZIG" >&2; exit 2; }

TRIPLE="$(bash "$MONO/tools/sh/gmb_link_set.sh" --rid "$RID" --print triple)" || exit 2
OS="$(bash "$MONO/tools/sh/gmb_link_set.sh" --rid "$RID" --print os)" || exit 2
CACHE="$HOME/.gmbasic/libs/$RID"
OUT="${OUT:-$MONO/build/libs/$RID}"
mkdir -p "$OUT"
CROSS_OUT="$OUT/cmake"

# ── Include delle terze parti: solo i file che servono (trappola 2) ──────────────────────────
INC_TP="$OUT/inc"
rm -rf "$INC_TP"; mkdir -p "$INC_TP"
for _h in sqlite3.h sqlite3ext.h zlib.h zstd.h pcre2.h lzma.h lz4.h bzlib.h curl.h SDL2; do
  [ -e "$CACHE/include/$_h" ] && ln -sfn "$CACHE/include/$_h" "$INC_TP/$_h"
done
[ -d "$CACHE/include/openssl" ] && ln -sfn "$CACHE/include/openssl" "$INC_TP/openssl"
INC=(-I "$INC_TP")
INC+=(-I "$MONO/shared/include" -I "$ROOT/native/gmb_runtime_c/include" -I "$ROOT/native/gmb_runtime_c/src")
INC+=(-I "$ROOT/native/gmb_chart/include" -I "$ROOT/native/gmb_compiler_c/include")
INC+=(-I "$ROOT/native/gmb_compiler_c/frontend/include" -I "$ROOT/native/gmb_bytecode/include")
INC+=(-I "$ROOT/native/gmb_c99native/include" -I "$ROOT/native/gmb_db_c/include" -I "$ROOT/native/gmb_db_c/src")
INC+=(-I "$ROOT/native/gmb_doc_c/include" -I "$ROOT/native/gmb_torch/include")
INC+=(-I "$ROOT/native/gmb_agent_runtime" -I "$ROOT/gmbllm/include")

_CMAKE_SYS=()
case "$OS" in
  Linux)  _CMAKE_SYS=(-DCMAKE_SYSTEM_NAME=Linux)
          [ "${TRIPLE%%-*}" = x86_64 ] && _CMAKE_SYS+=(-DCMAKE_SYSTEM_PROCESSOR=x86_64) || _CMAKE_SYS+=(-DCMAKE_SYSTEM_PROCESSOR=aarch64) ;;
  Darwin) _CMAKE_SYS=(-DCMAKE_SYSTEM_NAME=Darwin) ;;
  Windows) _CMAKE_SYS=(-DCMAKE_SYSTEM_NAME=Windows) ;;
esac

# ── `ar`/`ranlib` per il target: wrapper su `zig ar` (llvm-ar) ────────────────────────────────
# L'`ar` di Apple non capisce la sintassi `@response-file` che CMake usa nel ramo Windows: il
# try_compile del compilatore muore con «ar: @CMakeFiles/cmTC_xxx.dir/objects1.rsp: No such file»
# e CMake conclude «will not be able to correctly generate this project». NON è un problema di
# compilatore: `CMAKE_C_COMPILER_WORKS` non serve, serve un `ar` che parli la stessa lingua.
# Gli stessi wrapper servono anche per l'archiviazione finale (oggetti ELF/COFF, non Mach-O).
mkdir -p "$OUT/bin"
printf '#!/bin/sh\nexec "%s" ar "$@"\n' "$ZIG" > "$OUT/bin/zig-ar"
printf '#!/bin/sh\nexec "%s" ranlib "$@"\n' "$ZIG" > "$OUT/bin/zig-ranlib"
chmod +x "$OUT/bin/zig-ar" "$OUT/bin/zig-ranlib"

# Flag di compilazione per RID. Windows (mingw) ha bisogno delle estensioni GNU e di tre header
# forzati, altrimenti i sorgenti — scritti per POSIX — non compilano:
#   · `-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D__USE_MINGW_ANSI_STDIO=1` → `strcasestr`,
#     `gmtime_r`, `snprintf` dichiarati;
#   · `-include math.h` → i sorgenti AMOS definiscono `abs()` come macro PRIMA di includere
#     math.h e ne rompono le dichiarazioni (`expected identifier or '('`);
#   · `-include unistd.h` → `usleep` (gmb_console.c lo usa senza includerlo su `_WIN32`);
#   · `-include direct.h` → `_mkdir` (gmb_fetch.c).
# `-DPCRE2_STATIC` è obbligatorio: senza, la dichiarazione di pcre2 diventa il simbolo letterale
# `__declspec(dllimport) pcre2_compile_8` e il link non lo trova.
_CFLAGS="-target $TRIPLE"
if [ "$OS" = Windows ]; then
  # `-fms-extensions`: senza, clang (in modalità gnu) non riconosce `__declspec` e le funzioni
  # WinCrypt dichiarate con `__declspec(dllimport)` finiscono nel binario come riferimenti dal
  # NOME LETTERALE (`undefined symbol: __declspec(dllimport) CertFreeCertificateContext`).
  _CFLAGS="$_CFLAGS -fms-extensions -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D__USE_MINGW_ANSI_STDIO=1 -DPCRE2_STATIC -include math.h -include unistd.h -include direct.h"
  # Il bundle Windows tiene le librerie in `<cache>/lib`, ma il CMake del progetto le cerca nella
  # RADICE della cache (`find_library(... PATHS ${GMB_RID_RES} NO_DEFAULT_PATH)`): si espongono
  # con symlink (reversibili, nessun file toccato).
  for _a in "$CACHE"/lib/*.a; do
    [ -e "$_a" ] || continue
    _b="$(basename "$_a")"; [ -e "$CACHE/$_b" ] || ln -s "lib/$_b" "$CACHE/$_b"
  done
fi

echo "libs_cross_build: $RID ($TRIPLE) → $OUT"
cmake -S "$ROOT/native/gmb_runtime_c" -B "$CROSS_OUT" \
  "${_CMAKE_SYS[@]}" \
  -DCMAKE_C_COMPILER="$ZIG" -DCMAKE_C_COMPILER_ARG1=cc \
  -DCMAKE_C_FLAGS="$_CFLAGS" \
  -DCMAKE_AR="$OUT/bin/zig-ar" -DCMAKE_RANLIB="$OUT/bin/zig-ranlib" \
  -DCMAKE_C_COMPILER_AR="$OUT/bin/zig-ar" -DCMAKE_C_COMPILER_RANLIB="$OUT/bin/zig-ranlib" \
  > "$OUT/cmake-configure.log" 2>&1 \
  || { echo "  configure FALLITO (log: $OUT/cmake-configure.log)" >&2; tail -5 "$OUT/cmake-configure.log" >&2; exit 1; }

if [ "$OS" = Windows ]; then
  # Il target `gmb_runtime` trascina i target di LLMEnv (`llmcodec`, `llmdesc`, `llmengine_plugin`)
  # che su Windows NON compilano (usano `pwrite`/`sysconf`/`sys/mman.h`/`dlfcn.h` senza guardie):
  # si compilano gli OGGETTI dei nostri tre object-library con le regole di `make`, saltando le
  # dipendenze di target. La selezione dei sorgenti resta quella di CMake.
  for _t in gmb_runtime_core_obj gmb_runtime_ui_obj gmb_runtime_sql_obj; do
    _objs="$(grep -oE "CMakeFiles/$_t\.dir/[^ :]+\.(c|cc)\.obj" "$CROSS_OUT/CMakeFiles/$_t.dir/build.make" | sort -u)"
    make -C "$CROSS_OUT" -f "CMakeFiles/$_t.dir/build.make" -k -j "$JOBS" $_objs \
      > "$OUT/build-$_t.log" 2>&1 || true
    _bad="$(grep -c 'Error 1' "$OUT/build-$_t.log" 2>/dev/null || echo 0)"
    echo "  $_t: $(echo "$_objs" | wc -w | tr -d ' ') sorgenti, $_bad non compilano"
  done
  # Il chart è un target autonomo (non dipende dalle LLM): si costruisce normalmente.
  cmake --build "$CROSS_OUT" --target gmb_chart -j "$JOBS" > "$OUT/build-gmb_chart.log" 2>&1 \
    || echo "  gmb_chart: build FALLITA (log: $OUT/build-gmb_chart.log)" >&2
else
  cmake --build "$CROSS_OUT" --target gmb_runtime -j "$JOBS" > "$OUT/cmake-build.log" 2>&1 \
    || { echo "  build FALLITO (log: $OUT/cmake-build.log)" >&2; grep -m5 'error:' "$OUT/cmake-build.log" >&2; exit 1; }
fi

# ── Archivi, ricreati con `zig ar` (trappola 1) ──────────────────────────────────────────────
_arch() {  # _arch <nome archivio> <dir…>
  local name="$1"; shift
  local objs=()
  while IFS= read -r -d '' o; do objs+=("$o"); done < <(find "$@" \( -name '*.o' -o -name '*.obj' \) -print0 2>/dev/null)
  [ ${#objs[@]} -gt 0 ] || { echo "  $name: nessun oggetto" >&2; return 1; }
  rm -f "$OUT/$name"
  "$ZIG" ar rcs "$OUT/$name" "${objs[@]}" || { echo "  $name: zig ar fallito" >&2; return 1; }
  echo "  $name: ${#objs[@]} oggetti"
}
_arch libgmb_runtime.a "$CROSS_OUT/CMakeFiles/gmb_runtime_core_obj.dir" "$CROSS_OUT/CMakeFiles/gmb_runtime_sql_obj.dir" || exit 1
_arch libgmb_ui.a      "$CROSS_OUT/CMakeFiles/gmb_runtime_ui_obj.dir" || exit 1
_arch libgmb_chart.a   "$CROSS_OUT/gmb_chart_build" || exit 1

# `libgmb_vm/compiler/db/doc` NON servono al percorso C99Native (il driver è host e il C emesso
# chiama solo il runtime); per gli altri target del monorepo si costruiscono con lo stesso CMake
# quando avranno un target autonomo per RID.
printf 'rid=%s\ntriple=%s\n' "$RID" "$TRIPLE" > "$OUT/TARGET"
echo "libs_cross_build: $RID → $OUT (runtime, ui, chart)"
