#!/usr/bin/env bash
# build-deps.sh — costruisce le TERZE PARTI del runtime per PIÙ target, in locale.
#
# NON si esegue nulla: si COMPILA soltanto. Nessun MSVC, nessun admin, nessuna VM,
# nessuna Rosetta (macOS Intel si compila, non si esegue su questa macchina).
#
# TARGET (triple accettate):
#   aarch64-windows-gnu   Windows ARM64        (zig cc)
#   x86_64-windows-gnu    Windows x86-64       (zig cc)
#   aarch64-macos         macOS Apple Silicon   (Apple clang)
#   x86_64-macos          macOS Intel          (Apple clang -target x86_64)
#
# LIBRERIE:
#   zlib sqlite3 zstd lz4 brotli   — sorgenti compilati a mano (niente configure)
#   lzma pcre2                     — CMake, cross-toolchain generata qui
#   openssl                        — Configure + perl (statica, no-shared, no-asm)
#   freetype sdl2 sdl2_ttf         — catena grafica (CMake; freetype usa zlib+brotli di $OUT)
#   curl                           — HTTP client (OpenSSL/zlib/brotli/zstd di $OUT)
#
# USO:
#   tools/c/build-deps.sh --all-targets [--out-base DIR] [lib...]
#   tools/c/build-deps.sh --target <triple> [--out DIR] [lib...]
#
# Esempio:
#   tools/c/build-deps.sh --all-targets zlib sqlite3 zstd lz4 brotli lzma pcre2 openssl freetype sdl2 sdl2_ttf
#
# Risultato: <out>/{include,lib} — header e archivi statici per QUEL target.
set -eu

ZIG_BIN="${ZIG:-$HOME/ai/tools/zig-aarch64-macos-0.17.0/zig}"
OUT_BASE="$HOME/gmb-deps"
SRC_BASE="$HOME/gmb-deps/src"
ALL=0; TARGET=""; OUT=""; LIBS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --all-targets) ALL=1; shift ;;
    --target)      TARGET="$2"; shift 2 ;;
    --out)         OUT="$2"; shift 2 ;;
    --out-base)    OUT_BASE="$2"; shift 2 ;;
    --src-base)    SRC_BASE="$2"; shift 2 ;;
    *)             LIBS="$LIBS $1"; shift ;;
  esac
done
[ -n "$LIBS" ] || LIBS="zlib sqlite3 zstd lz4 brotli lzma pcre2 openssl freetype sdl2 sdl2_ttf curl"
[ -x "$ZIG_BIN" ] || { echo "zig non trovato: $ZIG_BIN (ZIG=... )" >&2; exit 2; }
SDK="$(xcrun --sdk macosx --show-sdk-path)"
mkdir -p "$SRC_BASE"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmbdeps.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

fetch() { [ -f "$2" ] || curl -sSL --max-time 1200 -o "$2" "$1"; }
untar() { tar -xzf "$1" -C "$2" 2>/dev/null || true; }

# ── preparazione per target: compilatore, archiver, toolchain CMake ───────────────────────────
# Imposta: CC_CMD, AR_CMD, RANLIB_CMD, TOOLCHAIN, NEED_RANLIB, EXT
prepare() { # $1=triple
  local t="$1"
  case "$t" in
    *-windows-gnu)
      local arch="arm64"; [ "$t" = "x86_64-windows-gnu" ] && arch="amd64"
      printf '#!/bin/sh\nexec "%s" cc -target %s "$@"\n' "$ZIG_BIN" "$t" > "$WORK/cc.sh"
      printf '#!/bin/sh\nexec "%s" ar "$@"\n' "$ZIG_BIN" > "$WORK/ar.sh"
      printf '#!/bin/sh\nexec "%s" ar s "$@"\n' "$ZIG_BIN" > "$WORK/ranlib.sh"
      printf '#!/bin/sh\nexec "%s" rc "$@"\n' "$ZIG_BIN" > "$WORK/rc.sh"
      chmod +x "$WORK/cc.sh" "$WORK/ar.sh" "$WORK/ranlib.sh" "$WORK/rc.sh"
      CC_CMD="$WORK/cc.sh"; AR_CMD="$WORK/ar.sh"; RANLIB_CMD="true"
      NEED_RANLIB=0; EXT=".a"; OSKIND=windows
      cat > "$WORK/toolchain.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR $arch)
set(CMAKE_C_COMPILER $WORK/cc.sh)
set(CMAKE_AR $WORK/ar.sh)
set(CMAKE_RANLIB $WORK/ranlib.sh)
set(CMAKE_C_COMPILER_AR $WORK/ar.sh)
set(CMAKE_C_COMPILER_RANLIB $WORK/ranlib.sh)
set(CMAKE_RC_COMPILER $WORK/rc.sh)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
EOF
      ;;
    *-macos)
      local arch="arm64"; [ "$t" = "x86_64-macos" ] && arch="x86_64"
      printf '#!/bin/sh\nexec clang -target %s-apple-darwin -isysroot "%s" "$@"\n' "$arch" "$SDK" > "$WORK/cc.sh"
      chmod +x "$WORK/cc.sh"
      CC_CMD="$WORK/cc.sh"; AR_CMD="ar"; RANLIB_CMD="ranlib"
      NEED_RANLIB=1; EXT=".a"; OSKIND=macos
      cat > "$WORK/toolchain.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR $arch)
set(CMAKE_OSX_ARCHITECTURES $arch)
set(CMAKE_OSX_SYSROOT $SDK)
set(CMAKE_C_COMPILER $WORK/cc.sh)
set(CMAKE_AR ar)
set(CMAKE_RANLIB ranlib)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
EOF
      ;;
    *) echo "target sconosciuto: $t" >&2; return 1 ;;
  esac
  TOOLCHAIN="$WORK/toolchain.cmake"
}

# archive <file.a> <oggetti...> — per macOS l'indice (ranlib) si fa a parte.
archive() {
  local a="$1"; shift
  if [ "$NEED_RANLIB" = "1" ]; then
    $AR_CMD rc "$a" "$@" || return 1     # ar di Apple: 's' non è un modificatore di -r
    ranlib "$a"
  else
    $AR_CMD rcs "$a" "$@" || return 1    # LLVM archiver (zig ar): archivi COFF
  fi
  return 0
}

# ── lib = $OUT corriponde alla cartella di quel target ───────────────────────────────────────
build_zlib() {
  echo "  [zlib 1.3.1]"
  fetch "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz" "$SRC_BASE/zlib.tar.gz"
  [ -d "$SRC_BASE/zlib-1.3.1" ] || untar "$SRC_BASE/zlib.tar.gz" "$SRC_BASE"
  local d="$SRC_BASE/zlib-1.3.1" o="$WORK/o_zlib"; mkdir -p "$o"; local objs=()
  for f in adler32 compress crc32 deflate infback inffast inflate inftrees trees uncompr zutil; do
    $CC_CMD -O2 -c -o "$o/$f.o" "$d/$f.c" 2>/dev/null || true
    [ -f "$o/$f.o" ] && objs+=("$o/$f.o")
  done
  archive "$OUT/lib/libz.a" "${objs[@]}"
  cp "$d/zlib.h" "$d/zconf.h" "$OUT/include/"
}

build_zstd() {
  echo "  [zstd 1.5.7]"
  fetch "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz" "$SRC_BASE/zstd.tar.gz"
  [ -d "$SRC_BASE/zstd-1.5.7" ] || untar "$SRC_BASE/zstd.tar.gz" "$SRC_BASE"
  local d="$SRC_BASE/zstd-1.5.7/lib" o="$WORK/o_zstd"; mkdir -p "$o"; local objs=()
  # NB: i .S ci sono solo su x86-64 (HUF_decompress*_asm_loop) e sono OBBLIGATORI: senza,
  # il C compilato per x86_64 li referenzia e il link si ferma su "undefined symbol
  # HUF_decompress4X2_usingDTable_internal_fast_asm_loop".
  for f in "$d"/common/*.c "$d"/compress/*.c "$d"/decompress/*.c "$d"/dictBuilder/*.c "$d"/decompress/*.S; do
    local b; b="$(basename "${f%.*}")"
    $CC_CMD -O2 -I"$d" -I"$d/common" -I"$d/legacy" -c -o "$o/$b.o" "$f" 2>/dev/null || true
    [ -f "$o/$b.o" ] && objs+=("$o/$b.o")
  done
  archive "$OUT/lib/libzstd.a" "${objs[@]}"
  cp "$d/zstd.h" "$d/zstd_errors.h" "$OUT/include/"
}

build_lz4() {
  echo "  [lz4 1.10.0]"
  fetch "https://github.com/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz" "$SRC_BASE/lz4.tar.gz"
  [ -d "$SRC_BASE/lz4-1.10.0" ] || untar "$SRC_BASE/lz4.tar.gz" "$SRC_BASE"
  local d="$SRC_BASE/lz4-1.10.0/lib" o="$WORK/o_lz4"; mkdir -p "$o"; local objs=()
  for f in lz4 lz4hc lz4frame xxhash; do
    $CC_CMD -O2 -I"$d" -c -o "$o/$f.o" "$d/$f.c" 2>/dev/null || true
    [ -f "$o/$f.o" ] && objs+=("$o/$f.o")
  done
  archive "$OUT/lib/liblz4.a" "${objs[@]}"
  cp "$d"/*.h "$OUT/include/" 2>/dev/null || true
}

# brotli: tre archivi come upstream (common / enc / dec) — un archivio unico crea doppioni
# quando il linker chiede sia -lbrotlienc sia -lbrotlidec.
build_brotli() {
  echo "  [brotli 1.1.0]"
  fetch "https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz" "$SRC_BASE/brotli.tar.gz"
  [ -d "$SRC_BASE/brotli-1.1.0" ] || untar "$SRC_BASE/brotli.tar.gz" "$SRC_BASE"
  local d="$SRC_BASE/brotli-1.1.0" o="$WORK/o_brotli"; mkdir -p "$o"; local common=() enc=() dec=()
  for f in "$d"/c/common/*.c; do
    local b; b="$(basename "$f" .c)"
    $CC_CMD -O2 -I"$d/c/include" -c -o "$o/c_$b.o" "$f" 2>/dev/null || true
    [ -f "$o/c_$b.o" ] && common+=("$o/c_$b.o")
  done
  for f in "$d"/c/enc/*.c; do
    local b; b="$(basename "$f" .c)"
    $CC_CMD -O2 -I"$d/c/include" -c -o "$o/e_$b.o" "$f" 2>/dev/null || true
    [ -f "$o/e_$b.o" ] && enc+=("$o/e_$b.o")
  done
  for f in "$d"/c/dec/*.c; do
    local b; b="$(basename "$f" .c)"
    $CC_CMD -O2 -I"$d/c/include" -c -o "$o/d_$b.o" "$f" 2>/dev/null || true
    [ -f "$o/d_$b.o" ] && dec+=("$o/d_$b.o")
  done
  archive "$OUT/lib/libbrotlicommon.a" "${common[@]}"
  archive "$OUT/lib/libbrotlienc.a" "${enc[@]}"
  archive "$OUT/lib/libbrotlidec.a" "${dec[@]}"
  mkdir -p "$OUT/include"; cp -R "$d/c/include/brotli" "$OUT/include/"
}

build_sqlite3() {
  echo "  [sqlite3 3.46.1 amalgamation]"
  fetch "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip" "$SRC_BASE/sqlite.zip"
  [ -d "$SRC_BASE/sqlite-amalgamation-3460100" ] || ( cd "$SRC_BASE" && unzip -q -o sqlite.zip )
  local d="$SRC_BASE/sqlite-amalgamation-3460100"
  $CC_CMD -O2 -c -o "$WORK/sqlite3.o" "$d/sqlite3.c"
  archive "$OUT/lib/libsqlite3.a" "$WORK/sqlite3.o"
  cp "$d/sqlite3.h" "$d/sqlite3ext.h" "$OUT/include/"
}

# xz/liblzma: CMake (il build manuale richiede config.h generato).
build_lzma() {
  echo "  [lzma 5.6.3 (cmake)]"
  fetch "https://github.com/tukaani-project/xz/releases/download/v5.6.3/xz-5.6.3.tar.gz" "$SRC_BASE/xz.tar.gz"
  [ -d "$SRC_BASE/xz-5.6.3" ] || untar "$SRC_BASE/xz.tar.gz" "$SRC_BASE"
  local b="$WORK/b_xz"; rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      "$SRC_BASE/xz-5.6.3" > "$b/cfg.log" 2>&1 && make -j8 liblzma > "$b/build.log" 2>&1 ) || {
      echo "    build lzma FALLITA (vedi $b/*.log)" >&2; return 1; }
  cp "$b/liblzma.a" "$OUT/lib/"
  cp "$SRC_BASE/xz-5.6.3/src/liblzma/api/lzma.h" "$OUT/include/"
  mkdir -p "$OUT/include/lzma"; cp "$SRC_BASE"/xz-5.6.3/src/liblzma/api/lzma/*.h "$OUT/include/lzma/"
}

build_pcre2() {
  echo "  [pcre2 10.44 (cmake)]"
  fetch "https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.gz" "$SRC_BASE/pcre2.tar.gz"
  [ -d "$SRC_BASE/pcre2-10.44" ] || untar "$SRC_BASE/pcre2.tar.gz" "$SRC_BASE"
  local b="$WORK/b_pcre2"; rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DPCRE2_BUILD_PCRE2_16=OFF -DPCRE2_BUILD_PCRE2_32=OFF \
      -DPCRE2_BUILD_TESTS=OFF -DPCRE2_BUILD_PCRE2GREP=OFF \
      "$SRC_BASE/pcre2-10.44" > "$b/cfg.log" 2>&1 && make -j8 > "$b/build.log" 2>&1 ) || {
      echo "    build pcre2 FALLITA (vedi $b/*.log)" >&2; return 1; }
  cp "$b/libpcre2-8.a" "$OUT/lib/"
  cp "$b/pcre2.h" "$b/pcre2posix.h" "$OUT/include/" 2>/dev/null || true
}

# openssl: Configure + perl, statica, senza asm (l'asm è per-arch e non serve qui).
build_openssl() {
  echo "  [openssl 3.5.4 (Configure, statica, no-asm)]"
  fetch "https://github.com/openssl/openssl/releases/download/openssl-3.5.4/openssl-3.5.4.tar.gz" "$SRC_BASE/openssl.tar.gz"
  [ -d "$SRC_BASE/openssl-3.5.4" ] || untar "$SRC_BASE/openssl.tar.gz" "$SRC_BASE"
  local b="$WORK/b_ssl"; rm -rf "$b"; mkdir -p "$b"
  local cfg
  case "$OSKIND" in
    windows) cfg="mingw64" ;;
    macos)   [ "${TARGET##*x86_64*}" = "" ] && cfg="darwin64-x86_64-cc" || cfg="darwin64-arm64-cc" ;;
  esac
  ( cd "$b" && CC="$CC_CMD" AR="$AR_CMD" RANLIB="$RANLIB_CMD" \
      perl "$SRC_BASE/openssl-3.5.4/Configure" "$cfg" no-shared no-tests no-apps no-docs no-asm \
        --prefix="$OUT" --openssldir="$OUT/ssl" > "$b/cfg.log" 2>&1 ) || {
      echo "    configure openssl FALLITO (vedi $b/cfg.log)" >&2; return 1; }
  ( cd "$b" && make -j8 build_libs > "$b/build.log" 2>&1 ) || {
      echo "    build openssl FALLITA (vedi $b/build.log)" >&2; return 1; }
  cp "$b/libcrypto.a" "$b/libssl.a" "$OUT/lib/"
  # header: quelli pubblici stanno nel SORGENTE, quelli generati (configuration.h, opensslv.h…)
  # nella build dir. Servono ENTRAMBI: copiare solo la build dir lascia un set incompleto
  # (28 header su ~200 → "openssl/macros.h not found").
  cp -R "$SRC_BASE/openssl-3.5.4/include/openssl" "$OUT/include/"
  cp -R "$b/include/openssl/." "$OUT/include/openssl/"
  rm -f "$OUT"/include/openssl/*.in
}

# freetype: servono i glifi della TTF (SDL_ttf → freetype). zlib/brotli vengono da $OUT.
# HarfBuzz/PNG/BZip2 disattivati: non servono al runtime e allungherebbero la catena.
build_freetype() {
  echo "  [freetype 2.13.3 (cmake)]"
  fetch "https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz" "$SRC_BASE/freetype.tar.gz"
  [ -d "$SRC_BASE/freetype-VER-2-13-3" ] || untar "$SRC_BASE/freetype.tar.gz" "$SRC_BASE"
  local b="$WORK/b_ft"; rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_BZIP2=ON \
      -DZLIB_INCLUDE_DIR="$OUT/include" -DZLIB_LIBRARY="$OUT/lib/libz.a" \
      -DBROTLIDEC_INCLUDE_DIRS="$OUT/include" -DBROTLIDEC_LIBRARIES="$OUT/lib/libbrotlidec.a" \
      -DBROTLICOMMON_LIBRARIES="$OUT/lib/libbrotlicommon.a" \
      -DCMAKE_PREFIX_PATH="$OUT" "$SRC_BASE/freetype-VER-2-13-3" > "$b/cfg.log" 2>&1 \
      && make -j8 freetype > "$b/build.log" 2>&1 ) || { echo "    freetype FALLITA ($b/*.log)" >&2; return 1; }
  cp "$b/libfreetype.a" "$OUT/lib/"
  mkdir -p "$OUT/include/freetype2"
  cp -R "$SRC_BASE/freetype-VER-2-13-3/include/." "$OUT/include/freetype2/"
  cp "$b/include/freetype/config/ftconfig.h" "$OUT/include/freetype2/freetype/config/" 2>/dev/null || true
  cp "$b/include/freetype/config/ftoption.h" "$OUT/include/freetype2/freetype/config/" 2>/dev/null || true
}

# SDL2: base grafica (finestra/input/renderer). Statica.
build_sdl2() {
  echo "  [SDL2 2.30.9 (cmake)]"
  fetch "https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz" "$SRC_BASE/sdl2.tar.gz"
  [ -d "$SRC_BASE/SDL2-2.30.9" ] || untar "$SRC_BASE/sdl2.tar.gz" "$SRC_BASE"
  local b="$WORK/b_sdl2"; rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
      -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON "$SRC_BASE/SDL2-2.30.9" > "$b/cfg.log" 2>&1 \
      && make -j8 SDL2-static > "$b/build.log" 2>&1 ) || { echo "    SDL2 FALLITA ($b/*.log)" >&2; return 1; }
  # CMake su Windows produce ".lib" (SDL2-static.lib); si normalizza al nome usato dai nostri link.
  local a=""
  for cand in "$b/libSDL2.a" "$b/SDL2-static.lib" "$b/libSDL2-static.a" "$b/SDL2.lib"; do
    [ -f "$cand" ] && { a="$cand"; break; }
  done
  [ -n "$a" ] || { echo "    SDL2: archivio non trovato in $b" >&2; return 1; }
  cp "$a" "$OUT/lib/libSDL2.a"
  cp "$a" "$OUT/lib/SDL2.lib"
  mkdir -p "$OUT/include/SDL2"
  cp "$b"/include/SDL2/*.h "$OUT/include/SDL2/"
  cp "$b"/include-config-*/SDL2/SDL_config.h "$OUT/include/SDL2/" 2>/dev/null || true
}

# SDL2_ttf: render del testo TTF (usa freetype + SDL2). Solo la libreria statica.
build_sdl2_ttf() {
  echo "  [SDL2_ttf 2.22.0 (cmake)]"
  fetch "https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz" "$SRC_BASE/sdl2ttf.tar.gz"
  [ -d "$SRC_BASE/SDL2_ttf-2.22.0" ] || untar "$SRC_BASE/sdl2ttf.tar.gz" "$SRC_BASE"
  local b="$WORK/b_ttf"; rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF -DSDL2TTF_VENDORED=OFF -DSDL2TTF_HARFBUZZ=OFF \
      -DCMAKE_PREFIX_PATH="$OUT" -DSDL2_INCLUDE_DIR="$OUT/include/SDL2" \
      -DSDL2_LIBRARY="$OUT/lib/libSDL2.a" "$SRC_BASE/SDL2_ttf-2.22.0" > "$b/cfg.log" 2>&1 \
      && make -j8 SDL2_ttf > "$b/build.log" 2>&1 ) || { echo "    SDL2_ttf FALLITA ($b/*.log)" >&2; return 1; }
  local a=""
  for cand in "$b/libSDL2_ttf.a" "$b/SDL2_ttf.lib" "$b/libSDL2_ttf-static.a" "$b/SDL2_ttf-static.lib"; do
    [ -f "$cand" ] && { a="$cand"; break; }
  done
  [ -n "$a" ] || { echo "    SDL2_ttf: archivio non trovato in $b" >&2; return 1; }
  cp "$a" "$OUT/lib/libSDL2_ttf.a"
  cp "$a" "$OUT/lib/SDL2_ttf.lib"
  mkdir -p "$OUT/include/SDL2"
  cp "$SRC_BASE"/SDL2_ttf-2.22.0/*.h "$OUT/include/SDL2/" 2>/dev/null || true
  cp "$b"/include-config-*/SDL2_ttf/*.h "$OUT/include/SDL2/" 2>/dev/null || true
}

# curl: HTTP client del runtime (gmb_net_http_*). Statico, con OpenSSL/zlib/brotli/zstd di $OUT.
build_curl() {
  echo "  [curl 8.11.0 (cmake)]"
  fetch "https://github.com/curl/curl/releases/download/curl-8_11_0/curl-8.11.0.tar.gz" "$SRC_BASE/curl.tar.gz"
  [ -d "$SRC_BASE/curl-8.11.0" ] || untar "$SRC_BASE/curl.tar.gz" "$SRC_BASE"
  local b="$WORK/b_curl"; rm -rf "$b"; mkdir -p "$b"
  ( cd "$b" && cmake -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
      -DCURL_USE_OPENSSL=ON -DOPENSSL_ROOT_DIR="$OUT" -DOPENSSL_INCLUDE_DIR="$OUT/include" \
      -DCURL_ZLIB=ON -DZLIB_INCLUDE_DIR="$OUT/include" -DZLIB_LIBRARY="$OUT/lib/libz.a" \
      -DCURL_BROTLI=ON -DCURL_ZSTD=ON -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF \
      -DUSE_NGHTTP2=OFF -DUSE_LIBIDN2=OFF -DCURL_DISABLE_LDAP=ON -DCMAKE_PREFIX_PATH="$OUT" \
      "$SRC_BASE/curl-8.11.0" > "$b/cfg.log" 2>&1 \
      && make -j8 libcurl_static > "$b/build.log" 2>&1 ) || { echo "    curl FALLITA ($b/*.log)" >&2; return 1; }
  local a=""
  for cand in "$b/lib/libcurl.a" "$b/libcurl.a" "$b/lib/libcurl-d.a"; do
    [ -f "$cand" ] && { a="$cand"; break; }
  done
  [ -n "$a" ] || { echo "    curl: archivio non trovato in $b" >&2; return 1; }
  cp "$a" "$OUT/lib/libcurl.a"; cp "$a" "$OUT/lib/curl.lib" 2>/dev/null || true
  mkdir -p "$OUT/include/curl"; cp "$SRC_BASE"/curl-8.11.0/include/curl/*.h "$OUT/include/curl/"; cp "$b"/include/curl/curl.h "$OUT/include/curl/" 2>/dev/null || true
}

run_for() { # $1=triple $2=out
  prepare "$1"
  TARGET="$1"
  mkdir -p "$2/include" "$2/lib"
  echo "══ $1 → $2"
  for l in $LIBS; do
    case "$l" in
      zlib)    build_zlib ;;
      sqlite3) build_sqlite3 ;;
      zstd)    build_zstd ;;
      lz4)     build_lz4 ;;
      brotli)  build_brotli ;;
      lzma)    build_lzma ;;
      pcre2)   build_pcre2 ;;
      openssl) build_openssl ;;
      freetype) build_freetype ;;
      sdl2)    build_sdl2 ;;
      sdl2_ttf) build_sdl2_ttf ;;
      curl)    build_curl ;;
      *)       echo "  [$l] sconosciuta" >&2; continue ;;
    esac
  done
  printf '  → %s archivi in %s/lib\n' "$(ls -1 "$2/lib" | grep -c .)" "$2"
}

if [ "$ALL" = "1" ]; then
  for t in aarch64-windows-gnu x86_64-windows-gnu x86_64-macos aarch64-macos; do
    OUT="$OUT_BASE/$t"
    run_for "$t" "$OUT"
  done
else
  [ -n "$TARGET" ] || { echo "serve --target <triple> o --all-targets" >&2; exit 2; }
  [ -n "$OUT" ] || OUT="$OUT_BASE/$TARGET"
  run_for "$TARGET" "$OUT"
fi
