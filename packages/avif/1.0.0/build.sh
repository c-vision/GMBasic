#!/usr/bin/env bash
# Costruisce il pacchetto avif: shim dylib SELF-CONTAINED. libavif e' COMPILATA DAI SORGENTI
# (git) e linkata staticamente insieme a libaom.a + libvmaf.a (statiche) -> nessuna lib di
# sistema a runtime. Il package manager esegue questo build all'install (contentType c-ffi-source).
# Prerequisiti di build: git, cmake, brew aom/libvmaf (statiche). AV1 decode/encode via aom
# (non serve dav1d).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD="$HERE/.build"; SRC="$BUILD/libavif"
AOM=$(brew --prefix aom 2>/dev/null || true)
VMAF=$(brew --prefix libvmaf 2>/dev/null || true)
[ -n "$AOM" ] && [ -f "$AOM/lib/libaom.a" ] || { echo "aom statica non trovata (brew install aom)"; exit 1; }
[ -n "$VMAF" ] && [ -f "$VMAF/lib/libvmaf.a" ] || { echo "libvmaf statica non trovata (brew install libvmaf)"; exit 1; }
OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT" "$HERE/include" "$BUILD"

if [ ! -d "$SRC" ]; then
  git clone --depth 1 --branch v1.2.1 https://github.com/AOMediaCodec/libavif.git "$SRC" 2>/dev/null \
    || git clone --depth 1 https://github.com/AOMediaCodec/libavif.git "$SRC"
fi

cmake -S "$SRC" -B "$SRC/build" \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DAVIF_CODEC_AOM=SYSTEM -DAVIF_LOCAL_AOM=OFF \
  -DAOM_INCLUDE_DIR="$AOM/include" -DAOM_LIBRARY="$AOM/lib/libaom.a" \
  -DAVIF_LIBYUV=OFF -DAVIF_BUILD_APPS=OFF -DAVIF_BUILD_TESTS=OFF -DAVIF_ENABLE_WERROR=OFF >/dev/null
cmake --build "$SRC/build" -j8 >/dev/null

AVIFA=$(find "$SRC/build" -name "libavif.a" | head -1)
[ -n "$AVIFA" ] || { echo "libavif.a non prodotta"; exit 1; }

rm -f "$OUT/libgmbavif.dylib"
cc -dynamiclib -O2 -I"$AOM/include" -I"$SRC/include" -I"$HERE/include" "$HERE/src/gmbavif.c" \
   -o "$OUT/libgmbavif.dylib" \
   "$AVIFA" "$AOM/lib/libaom.a" "$VMAF/lib/libvmaf.a" -lc++ -lpthread -lm
echo "ok: $OUT/libgmbavif.dylib (self-contained)"
