#!/usr/bin/env bash
# Costruisce il pacchetto hdf5: shim `libgmbhdf5.dylib` + la dylib HDF5 che porta con sé.
#
# La dylib HDF5 NON è quella di Homebrew: è la build che il runtime GMBasic pubblica nella
# cache (`~/.gmbasic/libs/<rid>/libhdf5.310.dylib`, HDF5 1.14.6). La VM/C precarica le lib del
# runtime, quindi una build diversa (es. brew 2.2) risponderebbe solo sui backend che caricano
# la copia del pacchetto: `H5VERSION` divergerebbe fra backend (verificato). Con la build
# allineata la versione è identica su IL, PCode, VM/C e C99 nativo.
#
# Prerequisiti:
#   - header HDF5 per compilare: brew install hdf5 (solo header; la lib linkata è la nostra);
#   - cache runtime popolata: gmb libs ensure (oppure HDF5_DYLIB=/percorso/libhdf5.dylib).
#
# Output: lib/<rid>/libgmbhdf5.dylib + lib/<rid>/libhdf5.dylib (relocabile: @rpath/@loader_path).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)

case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) RID=osx-arm64 ;;
    Darwin-x86_64) RID=osx-x64 ;;
    *) echo "piattaforma non gestita: $(uname -s)-$(uname -m)"; exit 1 ;;
esac

H5=$(brew --prefix hdf5 2>/dev/null || true)
[ -n "$H5" ] && [ -f "$H5/include/hdf5.h" ] || { echo "header HDF5 non trovati (brew install hdf5)"; exit 1; }

OUT="$HERE/lib/$RID"
mkdir -p "$OUT"
H5LIB="$OUT/libhdf5.dylib"

if [ ! -f "$H5LIB" ]; then
    SRC="${HDF5_DYLIB:-}"
    if [ -z "$SRC" ]; then
        # preferisce il nome stabile 310, poi qualunque build 310.*
        for cand in "$HOME/.gmbasic/libs/$RID/libhdf5.310.dylib" "$HOME"/.gmbasic/libs/"$RID"/libhdf5.310*.dylib; do
            [ -f "$cand" ] && { SRC="$cand"; break; }
        done
    fi
    [ -n "$SRC" ] && [ -f "$SRC" ] || {
        echo "libhdf5 del runtime non trovata in ~/.gmbasic/libs/$RID/"
        echo "  esegui: gmb libs ensure"
        echo "  oppure: HDF5_DYLIB=/percorso/libhdf5.dylib bash build.sh"
        exit 1
    }
    cp -f "$SRC" "$H5LIB"
    install_name_tool -id @rpath/libhdf5.dylib "$H5LIB"
fi

rm -f "$OUT/libgmbhdf5.dylib"
cc -dynamiclib -O2 -std=c99 \
   -I"$H5/include" -I"$HERE/include" \
   "$HERE/src/gmbhdf5.c" "$HERE/src/hdf5_ffi.c" \
   -o "$OUT/libgmbhdf5.dylib" \
   -L"$OUT" -lhdf5 -Wl,-rpath,@loader_path

# macOS: install_name_tool invalida la firma della dylib copiata -> ri-firmare entrambe
# (senza firma valida il caricamento viene ucciso con SIGKILL).
codesign --force --sign - "$H5LIB" 2>/dev/null || true
codesign --force --sign - "$OUT/libgmbhdf5.dylib" 2>/dev/null || true
echo "ok: $OUT/libgmbhdf5.dylib (+ libhdf5.dylib, firmate)"
