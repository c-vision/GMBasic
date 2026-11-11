#!/usr/bin/env bash
# libs_stage_system_deps.sh — porta le terze parti di SISTEMA dove i CMake di GMBasic le cercano.
#
# PERCHÉ ESISTE. `gmb_runtime_c/CMakeLists.txt` risolve le terze parti con
# `find_library(... PATHS "${GMB_RID_RES}" NO_DEFAULT_PATH)`: la cache del RID
# (`~/.gmbasic/libs/<rid>` oppure `build/libs/<rid>`) e NIENT'ALTRO, di proposito (un fallback
# sulla directory del builder cucirebbe un path assoluto nei binari). Su un runner pulito quella
# cartella non esiste e il configure si ferma con "GMB_CRYPTO_LIB non trovata": questo script la
# popola dalle librerie installate dal sistema (brew su macOS, apt su Linux).
#
# Uso: tools/sh/libs_stage_system_deps.sh <rid> <cartella-di-destinazione>
#      es: tools/sh/libs_stage_system_deps.sh linux-x64 GMBasic/build/libs/linux-x64
#
# Esce 1 solo se manca una libreria OBBLIGATORIA (`gmb_need_lib` = FATAL_ERROR nel CMake),
# elencandole: le opzionali vengono segnalate e il configure degrada da sé (build headless).
set -uo pipefail
RID="${1:-}"; OUT="${2:-}"
if [ -z "$RID" ] || [ -z "$OUT" ]; then
  sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 2
fi
mkdir -p "$OUT" || exit 1

# `gmb_need_lib`: se manca, il configure si FERMA.
REQUIRED=(crypto ssl zstd brotlienc brotlidec lzma lz4 pcre2-8)
# `find_library` senza obbligo: se manca, il CMake riduce la build (es. SDL2 -> headless).
OPTIONAL=(SDL2 SDL2_ttf sqlite3 jpeg png tiff webp openblas)
NAMES=("${REQUIRED[@]}" "${OPTIONAL[@]}")

formula() {
  case "$1" in
    crypto|ssl)          echo openssl@3;;
    brotlienc|brotlidec) echo brotli;;
    lzma)                echo xz;;
    SDL2)                echo sdl2;;
    SDL2_ttf)            echo sdl2_ttf;;
    jpeg)                echo jpeg-turbo;;
    png)                 echo libpng;;
    tiff)                echo libtiff;;
    sqlite3)             echo sqlite;;
    *)                   echo "$1";;
  esac
}

dirs=(); INC=""
case "$RID" in
  osx-*)
    prefix="$(brew --prefix 2>/dev/null || echo /usr/local)"
    for n in "${NAMES[@]}"; do dirs+=("$prefix/opt/$(formula "$n")/lib"); done
    dirs+=("$prefix/lib"); INC="$prefix/include";;
  *)
    dirs=(/usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib)
    INC=/usr/include;;
esac

have=(); for d in "${dirs[@]}"; do [ -d "$d" ] && have+=("$d"); done
[ "${#have[@]}" -gt 0 ] || { echo "ERRORE: nessuna directory di librerie trovata per $RID" >&2; exit 1; }

is_required() { local x; for x in "${REQUIRED[@]}"; do [ "$x" = "$1" ] && return 0; done; return 1; }

copied=0; miss_req=(); miss_opt=()
for n in "${NAMES[@]}"; do
  found=""
  # prima il nome NON versionato (è quello che `find_library(NAMES n)` cerca), poi i versionati
  for d in "${have[@]}"; do
    for f in "$d/lib$n.dylib" "$d/lib$n.so" "$d/lib$n.a" \
             "$d/lib$n".*.dylib "$d/lib$n.so".* "$d"/lib"$n"-*.dylib; do
      [ -f "$f" ] && { found="$f"; break; }
    done
    [ -n "$found" ] && break
  done
  if [ -n "$found" ]; then
    # -L: si copia il FILE, non il symlink (find_library deve vedere una libreria vera)
    cp -fL "$found" "$OUT/" && copied=$((copied+1))
  elif is_required "$n"; then
    miss_req+=("$n")
  else
    miss_opt+=("$n")
  fi
done

# Normalizzazione: se è arrivata solo la versione versionata (`libjpeg.so.8.2.2`), `find_library`
# con NAMES `jpeg` non la vede. Si crea il nome canonico come symlink, senza duplicare i byte.
for n in "${NAMES[@]}"; do
  case "$RID" in osx-*) base="$OUT/lib$n.dylib";; *) base="$OUT/lib$n.so";; esac
  [ -e "$base" ] && continue
  for cand in "$OUT/lib$n".*.dylib "$OUT/lib$n.so".* "$OUT"/lib"$n"-*.dylib; do
    [ -f "$cand" ] && { ln -sf "$(basename "$cand")" "$base"; break; }
  done
done

# Header: le terze parti non sono nel path di default del compilatore (brew su macOS).
if [ ! -e "$OUT/include" ] && [ -d "$INC" ]; then ln -sfn "$INC" "$OUT/include"; fi

echo "terze parti di sistema per $RID: $copied librerie copiate in $OUT"
[ "${#miss_opt[@]}" -gt 0 ] && echo "  opzionali assenti (il CMake degrada da sé): ${miss_opt[*]}"
if [ "${#miss_req[@]}" -gt 0 ]; then
  echo "ERRORE: mancano librerie OBBLIGATORIE: ${miss_req[*]}" >&2
  echo "  macOS: brew install cmake openssl@3 zstd curl brotli lz4 xz pcre2 sdl2 sdl2_ttf jpeg-turbo libpng libtiff webp sqlite" >&2
  echo "  Linux: apt-get install libssl-dev libzstd-dev libbrotli-dev liblz4-dev liblzma-dev libpcre2-dev libsdl2-dev libsdl2-ttf-dev libjpeg-dev libpng-dev libtiff-dev libwebp-dev libsqlite3-dev libopenblas-dev" >&2
  exit 1
fi
