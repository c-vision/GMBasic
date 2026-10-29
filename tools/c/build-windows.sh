#!/usr/bin/env bash
# build-windows.sh — compila il runtime C su Windows (ARM64 o x64) DA macOS/Linux con Zig.
#
# NIENTE MSVC, niente admin, niente VM. Si CROSS-COMPILA: `zig cc -target <triple>` produce
# oggetti/DLL COFF; l'esecuzione (se serve) avviene su una macchina Windows.
#
# Cosa fa:
#   1. genera il toolchain CMake per il target (compilatore, archiver, resource compiler,
#      flag, librerie di sistema Win32);
#   2. configura `native/gmb_runtime_c` e costruisce i target;
#   3. lascia gli artefatti in `<build>` (libgmb_runtime.a + libgmb_*.dll).
#
# PREREQUISITI (una volta sola):
#   tools/c/build-deps.sh --target aarch64-windows-gnu      # terze parti
#   (poi copiare/collegare include+lib nella cache RID usata da CMake:
#    `GMBasic/build/libs/win-arm64` oppure `~/.gmbasic/libs/win-arm64`)
#
# USO:
#   tools/c/build-windows.sh                               # win-arm64, tutti i target runtime
#   tools/c/build-windows.sh --rid win-x64 --triple x86_64-windows-gnu
#   tools/c/build-windows.sh --targets "gmb_gfx gmb_pkg"   # solo alcuni target
#   tools/c/build-windows.sh --config-only                 # genera solo il toolchain
#
# NOTE (scoperte sul campo, non teoria):
#   - `zig cc` NON è un compilatore "completo" da solo: va dato `cc -target …`, quindi CMake
#     vuole un WRAPPER come CMAKE_C_COMPILER (passare a zig i flag di CMake fallisce con
#     "unknown command: -target");
#   - `zig rc` serve come CMAKE_RC_COMPILER (senza, freetype/SDL si fermano su windres);
#   - le lib statiche di pcre2/curl vogliono `PCRE2_STATIC` / `CURL_STATICLIB`, altrimenti
#     gli header dichiarano `__declspec(dllimport)` e il link cerca una DLL che non esiste;
#   - `shared/include/win32` va force-incluso (`-include gmb_win32.h`) e messo in `-I`;
#   - le librerie di sistema Win32 (winmm, setupapi, imm32, dinput8, dxguid, crypt32, …) non
#     sono deducibili: senza, il link di gmb_gfx/gmb_pkg si ferma su simboli "dllimport".
set -eu

RID="win-arm64"
TRIPLE="aarch64-windows-gnu"
ARCH="arm64"
BUILD="$PWD/GMBasic/build/cmake-$RID"
DEPS="$PWD/GMBasic/build/libs"          # contiene <rid>/{include,lib} (cache RID del progetto)
ZIG_BIN="${ZIG:-$HOME/ai/tools/zig-aarch64-macos-0.17.0/zig}"
RUNTIME_SRC="$PWD/GMBasic/native/gmb_runtime_c"
TARGETS="gmb_runtime gmb_chart gmb_tray gmb_boot gmb_compress gmb_gfx gmb_grep gmb_pkg"
CONFIG_ONLY=0

while [ $# -gt 0 ]; do
  case "$1" in
    --rid)        RID="$2"; shift 2 ;;
    --triple)     TRIPLE="$2"; shift 2 ;;
    --arch)       ARCH="$2"; shift 2 ;;
    --build)      BUILD="$2"; shift 2 ;;
    --deps)       DEPS="$2"; shift 2 ;;
    --zig)        ZIG_BIN="$2"; shift 2 ;;
    --targets)    TARGETS="$2"; shift 2 ;;
    --config-only) CONFIG_ONLY=1; shift ;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done

[ -x "$ZIG_BIN" ] || { echo "zig non trovato: $ZIG_BIN (ZIG=... o --zig)" >&2; exit 2; }
case "$TRIPLE" in
  aarch64-windows-gnu) ARCH="arm64" ;;
  x86_64-windows-gnu)  ARCH="amd64" ;;
  *) echo "triple non Windows: $TRIPLE" >&2; exit 2 ;;
esac

WINC="$PWD/shared/include/win32"
RID_LIBS="$DEPS/$RID"
# La cache RID del progetto tiene gli archivi PIATTI accanto a include/ (`<rid>/*.a`);
# l'output di build-deps.sh ha invece `<dir>/lib`. Si accettano entrambi.
LIBDIR="$RID_LIBS"
[ -d "$RID_LIBS/lib" ] && LIBDIR="$RID_LIBS/lib"
[ -d "$LIBDIR" ] || { echo "mancano le terze parti in $RID_LIBS (tools/c/build-deps.sh --target $TRIPLE)" >&2; exit 2; }

mkdir -p "$BUILD"
TC="$BUILD/toolchain-$TRIPLE.cmake"

# ── wrapper del compilatore/archiver/resource compiler ───────────────────────────────────────
printf '#!/bin/sh\nexec "%s" cc -target %s "$@"\n' "$ZIG_BIN" "$TRIPLE" > "$BUILD/cc.sh"
printf '#!/bin/sh\nexec "%s" ar "$@"\n'             "$ZIG_BIN"          > "$BUILD/ar.sh"
printf '#!/bin/sh\nexec "%s" ar s "$@"\n'           "$ZIG_BIN"          > "$BUILD/ranlib.sh"
printf '#!/bin/sh\nexec "%s" rc "$@"\n'             "$ZIG_BIN"          > "$BUILD/rc.sh"
# CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ha bisogno di leggere i simboli dagli oggetti: senza
# CMAKE_OBJDUMP/CMAKE_DLLTOOL CMake non riesce e la shared lib esporta ZERO simboli.
printf '#!/bin/sh\nexec "%s" objdump "$@"\n'       "$ZIG_BIN"          > "$BUILD/objdump.sh"
printf '#!/bin/sh\nexec "%s" dlltool "$@"\n'       "$ZIG_BIN"          > "$BUILD/dlltool.sh"
chmod +x "$BUILD/cc.sh" "$BUILD/ar.sh" "$BUILD/ranlib.sh" "$BUILD/rc.sh" \
         "$BUILD/objdump.sh" "$BUILD/dlltool.sh"

# Librerie di sistema Win32 usate dal runtime/grafica (dx/dinput/imm sono del present SDL).
WIN_SYSTEM_LIBS="-lwinmm -lsetupapi -limm32 -ldinput8 -ldxguid -lole32 -loleaut32 -luuid \
-lversion -lavrt -lws2_32 -lshell32 -luser32 -lgdi32 -ladvapi32 -loleacc -lcrypt32 -lbcrypt \
-lsecur32 -lncrypt -lwintrust -lfreetype -lz -lbrotlienc -lbrotlidec -lbrotlicommon -lzstd \
-llz4 -llzma"

cat > "$TC" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR $ARCH)
set(CMAKE_C_COMPILER $BUILD/cc.sh)
set(CMAKE_AR $BUILD/ar.sh)
set(CMAKE_RANLIB $BUILD/ranlib.sh)
set(CMAKE_RC_COMPILER $BUILD/rc.sh)
set(CMAKE_OBJDUMP $BUILD/objdump.sh)
set(CMAKE_DLLTOOL $BUILD/dlltool.sh)
set(CMAKE_C_COMPILER_AR $BUILD/ar.sh)
set(CMAKE_C_COMPILER_RANLIB $BUILD/ranlib.sh)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# Le shared lib dei progetti (llmtoken/llmcodec/…) non hanno __declspec(dllexport): su macOS
# esportano tutto, su Windows non esportano NULLA. CMake genera un .def con tutti i simboli.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_C_FLAGS "-DPCRE2_STATIC -DCURL_STATICLIB -DSDL_MAIN_HANDLED -Wno-incompatible-pointer-types -Wno-int-conversion -I$WINC -include $WINC/gmb_win32.h" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS    "-L$LIBDIR $WIN_SYSTEM_LIBS" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "-L$LIBDIR -Wl,--export-all-symbols $WIN_SYSTEM_LIBS" CACHE STRING "" FORCE)
set(CMAKE_PREFIX_PATH "$LIBDIR;$RID_LIBS;$HOME/gmb-deps/$TRIPLE")
EOF

echo "toolchain: $TC"
[ "$CONFIG_ONLY" = "1" ] && exit 0

cmake -S "$RUNTIME_SRC" -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$TC" \
      -DCMAKE_BUILD_TYPE=Release -DLLMENV="${LLMENV:-$HOME/Documents/Projects/LLMEnv}" \
      > "$BUILD/configure.log" 2>&1 || { echo "configure FALLITA: $BUILD/configure.log" >&2; exit 1; }

# xargs: un solo make con la lista dei target (falliscono insieme, non uno alla volta)
make -C "$BUILD" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 8)" $TARGETS 2>&1 | tee "$BUILD/build.log" | \
  grep -E "error:|undefined symbol|Built target" | tail -40
# ── verifica del backend Vulkan (opt-in) ────────────────────────────────────────────────────
# `gmb_gfx_vulkan.c` include `gmb_gfx_spirv.h`, che NON esiste nel repo: lo genera CMake dai
# due .spv precompilati. Qui lo si genera con lo stesso script e si COMPILA il file (senza
# linkarlo: il present Vulkan è opt-in e richiede il loader). Serve a non lasciare il file
# fuori da ogni verifica.
SPIRV_H="$BUILD/gmb_gfx_spirv.h"
cmake -DOUT="$SPIRV_H" \
      -DIN1="$RUNTIME_SRC/shaders/prebuilt/sprite.vert.spv" -DNAME1=sprite_vert_spv \
      -DIN2="$RUNTIME_SRC/shaders/prebuilt/sprite.frag.spv" -DNAME2=sprite_frag_spv \
      -P "$RUNTIME_SRC/cmake/embed_spirv.cmake" >/dev/null
VK_INC=(-I "$RUNTIME_SRC/include" -I "$BUILD" -I "$RID_LIBS/include" -I "$WINC"
        -I "$RUNTIME_SRC/../gmb_chart/include" -I "$HOME/Documents/Projects/LLMEnv/LLMToken")
if "$BUILD/cc.sh" -c -o "$BUILD/gmb_gfx_vulkan.o" -DPCRE2_STATIC -DCURL_STATICLIB \
     -include "$WINC/gmb_win32.h" "${VK_INC[@]}" "$RUNTIME_SRC/src/gmb_gfx_vulkan.c" 2>"$BUILD/vulkan.log"; then
  echo "backend Vulkan (opt-in): gmb_gfx_vulkan.c COMPILA"
else
  echo "backend Vulkan (opt-in): non compila — vedi $BUILD/vulkan.log"
fi

echo "artefatti in $BUILD:"
ls -l "$BUILD"/*.dll "$BUILD"/*.a 2>/dev/null | awk '{printf "  %-34s %6.1f MB\n", $9, $5/1048576}'
