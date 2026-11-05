#!/usr/bin/env bash
# Costruisce il pacchetto hdf5: shim `libgmbhdf5.dylib` + la dylib HDF5 che porta con sé.
# Prerequisito: HDF5 installato (brew install hdf5) per header e libhdf5.
# La dylib di HDF5 viene COPIATA nel pacchetto e rinominata con install-name @rpath, così il
# pacchetto è relocabile (@loader_path) e non dipende dal percorso di brew a runtime.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
H5=$(brew --prefix hdf5 2>/dev/null || true)
[ -n "$H5" ] && [ -f "$H5/include/hdf5.h" ] || { echo "hdf5 non trovato (brew install hdf5)"; exit 1; }
OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT"
H5DYLIB=$(ls "$H5"/lib/libhdf5.[0-9]*.dylib 2>/dev/null | head -1)
[ -n "$H5DYLIB" ] || { echo "libhdf5.*.dylib assente in $H5/lib"; exit 1; }
cp -f "$H5DYLIB" "$OUT/libhdf5.dylib"
install_name_tool -id @rpath/libhdf5.dylib "$OUT/libhdf5.dylib"
rm -f "$OUT/libgmbhdf5.dylib"
cc -dynamiclib -O2 -std=c99 \
   -I"$H5/include" -I"$HERE/include" \
   "$HERE/src/gmbhdf5.c" "$HERE/src/hdf5_ffi.c" \
   -o "$OUT/libgmbhdf5.dylib" \
   -L"$OUT" -lhdf5 -Wl,-rpath,@loader_path
# macOS: install_name_tool invalida la firma della dylib copiata -> ri-firmare entrambe
# (senza firma valida il caricamento viene ucciso con SIGKILL).
codesign --force --sign - "$OUT/libhdf5.dylib" 2>/dev/null || true
codesign --force --sign - "$OUT/libgmbhdf5.dylib" 2>/dev/null || true
echo "ok: $OUT/libgmbhdf5.dylib (+ libhdf5.dylib, firmate)"
