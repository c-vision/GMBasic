#!/usr/bin/env bash
# Costruisce il pacchetto heif: shim dylib SELF-CONTAINED. libheif + libde265 + x265 sono
# COMPILATE DAI SORGENTI e linkate staticamente -> nessuna lib di sistema a runtime.
# Il package manager esegue questo build all'install (contentType c-ffi-source).
# Prerequisiti di build: git, cmake, compilatore C/C++.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD="$HERE/.build"
OUT="$HERE/lib/osx-arm64"
mkdir -p "$BUILD" "$OUT" "$HERE/include"
J="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

# ── libde265 (HEVC decode) ──
DE265="$BUILD/libde265"
[ -d "$DE265" ] || git clone --depth 1 https://github.com/strukturag/libde265.git "$DE265"
cmake -S "$DE265" -B "$DE265/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DENABLE_SDL=OFF >/dev/null
cmake --build "$DE265/build" -j"$J" >/dev/null
cmake --install "$DE265/build" --prefix "$BUILD/prefix" >/dev/null
DE265A=$(find "$BUILD/prefix" -name "libde265.a" | head -1)

# ── x265 (HEVC encode) ──
X265="$BUILD/x265"
[ -d "$X265" ] || git clone --depth 1 https://bitbucket.org/multicoreware/x265_git.git "$X265"
cmake -S "$X265/source" -B "$X265/build" -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_SHARED=OFF -DENABLE_CLI=OFF -DENABLE_ASSEMBLY=OFF >/dev/null
cmake --build "$X265/build" -j"$J" >/dev/null
cmake --install "$X265/build" --prefix "$BUILD/prefix" >/dev/null
X265A=$(find "$BUILD/prefix" -name "libx265.a" | head -1)

# ── libheif (container), statica, collegata a de265+x265 ──
HEIF="$BUILD/libheif"
[ -d "$HEIF" ] || git clone --depth 1 https://github.com/strukturag/libheif.git "$HEIF"
cmake -S "$HEIF" -B "$HEIF/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DWITH_LIBDE265=ON -DWITH_LIBDE265_PLUGIN=OFF -DWITH_X265=ON \
  -DWITH_DAV1D=OFF -DWITH_RAV1E=OFF -DWITH_SVTAV1=OFF -DWITH_AOM_DECODER=OFF -DWITH_AOM_ENCODER=OFF \
  -DWITH_JPEG_DECODER=OFF -DWITH_JPEG_ENCODER=OFF -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF \
  -DWITH_UNCOMPRESSED_CODEC=OFF -DWITH_EXAMPLES=OFF -DWITH_GDK_PIXBUF=OFF \
  -DCMAKE_PREFIX_PATH="$BUILD/prefix" \
  -DLIBDE265_INCLUDE_DIR="$BUILD/prefix/include" -DLIBDE265_LIBRARY="$DE265A" \
  -DX265_INCLUDE_DIR="$BUILD/prefix/include" -DX265_LIBRARY="$X265A" >/dev/null
cmake --build "$HEIF/build" -j"$J" >/dev/null
HEIFA=$(find "$HEIF/build" -name "libheif.a" | head -1)

[ -n "$DE265A" ] && [ -n "$X265A" ] && [ -n "$HEIFA" ] || { echo "statiche non prodotte (de265=$DE265A x265=$X265A heif=$HEIFA)"; exit 1; }

rm -f "$OUT/libgmbheif.dylib"
cc -dynamiclib -O2 -w -I"$HEIF/libheif/api" -I"$HEIF/build" -I"$HERE/include" "$HERE/src/gmbheif.c" \
   -o "$OUT/libgmbheif.dylib" \
   -Wl,-force_load,"$HEIFA" "$DE265A" "$X265A" -lc++ -lpthread -lm
echo "ok: $OUT/libgmbheif.dylib (self-contained)"
