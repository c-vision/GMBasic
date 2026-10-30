#!/usr/bin/env bash
# libs_cache_headers_linux.sh — mette gli HEADER di sviluppo nella cache del RID (dentro un
# container Ubuntu). Serve al build delle nostre librerie: i loro CMake leggono gli header da
# `<cache>/<rid>/include` (gruppo `dev`) e si fermano con "zstd.h non trovato".
#
# Non pubblica nulla: popola la cache locale del container, che è un ambiente di BUILD.
# Uso (dentro il container): libs_cache_headers_linux.sh <rid>
set -euo pipefail
RID="${1:?rid}"
DEST="$HOME/.gmbasic/libs/$RID/include"
export DEBIAN_FRONTEND=noninteractive

apt-get install -y -qq --no-install-recommends \
  zlib1g-dev libzstd-dev liblz4-dev liblzma-dev libbrotli-dev libpcre2-dev \
  libsqlite3-dev libcurl4-openssl-dev libssl-dev libsdl2-dev libsdl2-ttf-dev \
  libfreetype-dev >/dev/null

mkdir -p "$DEST"
# Header "piatti" e cartelle di dominio: si copia ciò che i CMake cercano davvero.
for h in zstd.h zdict.h zstd_errors.h lz4.h lz4frame.h lz4hc.h lzma.h zlib.h zconf.h \
         pcre2.h sqlite3.h ft2build.h; do
  [ -f "/usr/include/$h" ] && cp -f "/usr/include/$h" "$DEST/"
done
for d in lzma brotli curl openssl SDL2 freetype2; do
  [ -d "/usr/include/$d" ] && cp -R "/usr/include/$d" "$DEST/"
done
# Alcune distro mettono gli header per-arch in /usr/include/<triple>.
for d in /usr/include/*-linux-gnu; do
  [ -d "$d" ] || continue
  for h in zlib.h zconf.h; do [ -f "$d/$h" ] && cp -f "$d/$h" "$DEST/"; done
done

echo "header nella cache: $(find "$DEST" -type f | wc -l) file → $DEST"
