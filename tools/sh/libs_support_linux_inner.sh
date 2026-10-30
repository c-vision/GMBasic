#!/usr/bin/env bash
# libs_support_linux_inner.sh — parte che gira DENTRO il container (invocata da
# libs_support_linux.sh). Non si esegue a mano: dipende da /out, $PKGS e $PATTERNS.
#
# Perché un file separato: l'alternativa era un `docker run ... bash -c "<script>"`, e ogni
# apice inverso/doppio apice dentro quella stringa veniva interpretato dallo script esterno
# (bug reale: due run falliti in silenzio). Così il codice è un normale file bash.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

: "${OUT:?OUT non impostata}"
: "${PKGS:?PKGS non impostata}"
: "${PATTERNS:?PATTERNS non impostata}"

apt-get update -qq
# I nomi dei pacchetti cambiano fra release: si installa il set e, se un nome non esiste in
# questa immagine, si segnala e si prosegue (le dipendenze mancanti emergono da `ldd`).
# shellcheck disable=SC2086
apt-get install -y -qq --no-install-recommends $PKGS 2>/dev/null || {
  for p in $PKGS; do
    apt-get install -y -qq --no-install-recommends "$p" >/dev/null 2>&1 || echo "  (pacchetto assente in questa immagine: $p)"
  done
}

LIBDIR=$(dirname "$(ldconfig -p | awk '/libzstd\.so\.1 /{print $NF; exit}')")
echo "libdir=$LIBDIR"

# Dipendenze del SISTEMA (non vanno nel bundle): libc, loader, runtime gcc.
is_system() {
  case "$(basename "$1")" in
    libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|ld-linux*|linux-vdso*|libgcc_s.so*|libstdc++.so*) return 0 ;;
    *) return 1 ;;
  esac
}

stage() {
  local f="$1" b
  [ -e "$f" ] || return 0
  b=$(basename "$f")
  is_system "$f" && return 0
  [ -e "$OUT/$b" ] && return 0
  cp -L "$f" "$OUT/$b"
  # chiusura ricorsiva: ogni dipendenza non di sistema
  local deps
  deps=$(ldd "$f" 2>/dev/null | awk '{print $3}' | grep '^/' || true)
  local d
  for d in $deps; do stage "$d"; done
}

for pat in $PATTERNS; do
  for f in "$LIBDIR"/$pat*; do
    [ -e "$f" ] || continue
    case "$(basename "$f")" in *.so) continue ;; esac   # solo i file versionati veri
    stage "$f"
  done
done

# Seconda passata: chiude le dipendenze anche dei file che erano GIÀ nel bundle (quelli
# ereditati da support-v1: libtiff.so.6 chiedeva libLerc.so.4, libwebp.so.7 chiedeva
# libsharpyuv.so.0 — e nessuno dei due era pubblicato: il gate di caricabilità falliva).
close_deps() {
  local f="$1" d
  for d in $(ldd "$f" 2>/dev/null | awk '{print $3}' | grep '^/' || true); do stage "$d"; done
}
for f in "$OUT"/*; do [ -f "$f" ] && close_deps "$f"; done

# `find_library` cerca il nome NON versionato (`libzstd.so`), il loader il SONAME versionato
# (`libzstd.so.1`). Nel bundle servono ENTRAMBI come FILE VERI: il publisher salta i symlink e la
# cache del client non li sa creare, quindi si materializza una copia (non un link).
for f in "$OUT"/*.so.*; do
  [ -e "$f" ] || continue
  base="${f%%.so.*}.so"
  [ -e "$base" ] || cp -L "$f" "$base"
done

# ── SDL2 / SDL2_ttf dai SORGENTI ──────────────────────────────────────────────────────────────
# In Ubuntu 24.04 `libsdl2-2.0-0` è **sdl2-compat**: un adattatore che implementa l'API SDL2
# sopra SDL3 e al `dlopen` pretende `libSDL3.so.0` (assente in 24.04). Risultato: ogni nostra
# libreria che linka SDL2 falliva il caricamento con "Failed loading SDL3 library.".
# Si compila la SDL2 vera (2.30.9) e SDL2_ttf (2.22.0) e si installano nel bundle.
if [ "${SDL_FROM_SOURCE:-1}" = "1" ]; then
  apt-get install -y -qq --no-install-recommends cmake make gcc g++ pkg-config libfreetype-dev curl tar gzip ca-certificates libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libxss-dev libxkbcommon-dev >/dev/null
  cd /tmp
  [ -d SDL2-2.30.9 ] || { curl -fsSL -o sdl2.tgz https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz && tar -xzf sdl2.tgz; }
  cmake -S SDL2-2.30.9 -B b_sdl2 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/out \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF >/dev/null
  cmake --build b_sdl2 -j4 >/dev/null && cmake --build b_sdl2 --target install >/dev/null
  [ -d SDL2_ttf-2.22.0 ] || { curl -fsSL -o sdttf.tgz https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.22.0/SDL2_ttf-2.22.0.tar.gz && tar -xzf sdttf.tgz; }
  cmake -S SDL2_ttf-2.22.0 -B b_ttf -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/out \
        -DBUILD_SHARED_LIBS=ON -DSDL2TTF_VENDORED=OFF -DSDL2TTF_HARFBUZZ=OFF \
        -DSDL2_INCLUDE_DIR="$OUT/include/SDL2" -DSDL2_LIBRARY="$OUT/libSDL2.so" -DCMAKE_PREFIX_PATH=/out >/dev/null
  cmake --build b_ttf -j4 >/dev/null && cmake --build b_ttf --target install >/dev/null
  rm -rf /out/lib/cmake /out/share 2>/dev/null || true
  echo "SDL2/SDL2_ttf dai sorgenti: $(ls /out/libSDL2-2.0.so.0 /out/libSDL2_ttf-2.0.so.0 2>/dev/null | wc -l) file"
  for f in /out/libSDL2-2.0.so.0 /out/libSDL2_ttf-2.0.so.0; do [ -f "$f" ] && close_deps "$f"; done
fi

# ALIAS: il CMake cerca `libSDL2.so` / `libSDL2_ttf.so` (find_library NAMES SDL2_ttf), ma su
# Linux i file si chiamano `libSDL2_ttf-2.0.so.0`. Senza questi nomi il link di gmb_gfx non
# trovava SDL2_ttf e lasciava TTF_* indefiniti (dlopen KO su Linux, tollerato su macOS).
[ -f "$OUT/libSDL2-2.0.so.0" ] && [ ! -e "$OUT/libSDL2.so" ] && cp -L "$OUT/libSDL2-2.0.so.0" "$OUT/libSDL2.so"
[ -f "$OUT/libSDL2_ttf-2.0.so.0" ] && [ ! -e "$OUT/libSDL2_ttf.so" ] && cp -L "$OUT/libSDL2_ttf-2.0.so.0" "$OUT/libSDL2_ttf.so"

echo "file raccolti: $(ls "$OUT" | wc -l)"
