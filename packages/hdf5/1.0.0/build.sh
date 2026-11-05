#!/usr/bin/env bash
# Costruisce il pacchetto hdf5: shim `libgmbhdf5.dylib` + la dylib HDF5 che porta con sé.
#
# Versione: HDF5 2.2.0 (l'ultima), quella di Homebrew. La dylib NON è linkata col nome
# standard: nel pacchetto si chiama `libhdf5.320.dylib` (soname della serie 2.x). Motivo: il
# runtime GMBasic pubblica nella cache una sua libhdf5 (soname 310, HDF5 1.14.6) con
# install-name `@rpath/libhdf5.dylib`, e la VM/C la precarica. dyld deduplica per install-name:
# chiedendo `@rpath/libhdf5.dylib` il backend VM/C avrebbe riusato la 1.14.6 (verificato:
# H5VERSION dava 2.2.0 su IL/PCode/C99 e 1.14.6 su VM/C). Con un install-name distinto, e con
# il binding a due livelli (default di Mach-O), le chiamate dello shim sono legate alla NOSTRA
# copia: H5VERSION = 2.2.0 su tutti e quattro i backend.
#
# Prerequisiti:
#   - brew install hdf5   (dylib 2.2.0 + header)
#   - brew install libaec (libsz.2, dipendenza di libhdf5: viene copiata e resa relocabile)
#
# Output in lib/<rid>/: libgmbhdf5.dylib + libhdf5.320.dylib + libsz.2.dylib + libaec.0.dylib.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)

case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) RID=osx-arm64 ;;
    Darwin-x86_64) RID=osx-x64 ;;
    *) echo "piattaforma non gestita: $(uname -s)-$(uname -m)"; exit 1 ;;
esac

H5=$(brew --prefix hdf5 2>/dev/null || true)
[ -n "$H5" ] && [ -f "$H5/include/hdf5.h" ] || { echo "hdf5 non trovato (brew install hdf5)"; exit 1; }
SZ=$(brew --prefix libaec 2>/dev/null || true)
[ -n "$SZ" ] && [ -f "$SZ/lib/libsz.2.dylib" ] || { echo "libsz non trovata (brew install libaec)"; exit 1; }

OUT="$HERE/lib/$RID"
mkdir -p "$OUT"

# 1) HDF5 2.2.0 con install-name proprio (@rpath/libhdf5.320.dylib)
cp -fL "$H5/lib/libhdf5.320.dylib" "$OUT/libhdf5.320.dylib"
chmod u+w "$OUT/libhdf5.320.dylib"
install_name_tool -id @rpath/libhdf5.320.dylib "$OUT/libhdf5.320.dylib"

# 2) libsz.2 (compressione szip) accanto, con install-name @rpath e riferimento aggiornato
cp -fL "$SZ/lib/libsz.2.dylib" "$OUT/libsz.2.dylib"
chmod u+w "$OUT/libsz.2.dylib"
install_name_tool -id @rpath/libsz.2.dylib "$OUT/libsz.2.dylib"
install_name_tool -change "$SZ/lib/libsz.2.dylib" @rpath/libsz.2.dylib "$OUT/libhdf5.320.dylib"
# libaec.0 e' un'altra dipendenza di libhdf5 (codec lossless usato da szip): stessa cura
cp -fL "$SZ/lib/libaec.0.dylib" "$OUT/libaec.0.dylib"
chmod u+w "$OUT/libaec.0.dylib"
install_name_tool -id @rpath/libaec.0.dylib "$OUT/libaec.0.dylib"
install_name_tool -change "$SZ/lib/libaec.0.dylib" @rpath/libaec.0.dylib "$OUT/libhdf5.320.dylib"
# esplicita il rpath anche sulla dylib HDF5 (risoluzione di @rpath/libsz.2 anche se caricata
# direttamente, senza passare per lo shim)
install_name_tool -add_rpath @loader_path "$OUT/libhdf5.320.dylib" 2>/dev/null || true

# 3) shim FFI, legato alla NOSTRA dylib (soname 320) e con rpath @loader_path
rm -f "$OUT/libgmbhdf5.dylib"
cc -dynamiclib -O2 -std=c99 \
   -I"$H5/include" -I"$HERE/include" \
   "$HERE/src/gmbhdf5.c" "$HERE/src/hdf5_ffi.c" \
   -o "$OUT/libgmbhdf5.dylib" \
   -L"$OUT" -lhdf5.320 -Wl,-rpath,@loader_path

# 4) macOS: install_name_tool invalida la firma -> ri-firmare tutte (senza firma valida il
#    caricamento viene ucciso con SIGKILL).
for f in "$OUT/libsz.2.dylib" "$OUT/libaec.0.dylib" "$OUT/libhdf5.320.dylib" "$OUT/libgmbhdf5.dylib"; do
    codesign --force --sign - "$f" 2>/dev/null || true
done
echo "ok: $OUT/libgmbhdf5.dylib (+ libhdf5.320.dylib, libsz.2.dylib, libaec.0.dylib — HDF5 2.2.0, firmate)"
