#!/usr/bin/env bash
# libs_support_linux.sh — completa il set `support` di un RID Linux dentro un container Ubuntu.
#
# PERCHÉ. Il runtime C richiede zstd/brotli/lzma/lz4/pcre2/sqlite3/curl (+ dipendenze), ma il
# bundle `support` pubblicato per Linux conteneva solo crypto/ssl/freetype/SDL/hdf5/webp…:
# senza quei file il configure di `gmb_runtime_c` si ferma su `GMB_ZSTD_LIB non trovata` e il
# runtime Linux non si può costruire.
#
# COSA FA. In un container della STESSA architettura del RID: installa i pacchetti runtime,
# copia le `.so` in una cartella di staging, crea i symlink non versionati (`libzstd.so` →
# `libzstd.so.1`, che è ciò che cerca `find_library`) e chiude le dipendenze con `ldd`.
# La logica interna sta in `libs_support_linux_inner.sh` (un file vero, non una stringa passata
# a `bash -c`: gli apici dentro quella stringa vengono interpretati dallo script esterno).
#
# USO
#   tools/sh/libs_support_linux.sh <linux-arm64|linux-x64> [--out DIR]
#
# Poi: tools/sh/libs_publish_files_gh.sh support <rid> --source <out> --tag support-v2 --yes
set -uo pipefail

RID="${1:-}"; shift || true
case "$RID" in
  linux-arm64) PLATFORM="linux/arm64" ;;
  linux-x64)   PLATFORM="linux/amd64" ;;
  *) echo "RID non Linux: '$RID' (linux-arm64 | linux-x64)" >&2; exit 2 ;;
esac
OUT="${HOME}/gmb-bundles/${RID}/support"
while [ $# -gt 0 ]; do
  case "$1" in --out) OUT="${2:-}"; shift 2 ;; *) echo "opzione sconosciuta: $1" >&2; exit 2 ;; esac
done
mkdir -p "$OUT"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MONO="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Librerie richieste dal runtime (nomi dei pacchetti Debian) e pattern dei file.
PKGS="libzstd1 liblz4-1 liblzma5 libbrotli1 libpcre2-8-0 libsqlite3-0 libcurl4 zlib1g libtiff5 liblerc3 libwebp7 libsharpyuv0"
PATTERNS="libzstd.so liblz4.so liblzma.so libbrotlienc.so libbrotlidec.so libbrotlicommon.so libpcre2-8.so libsqlite3.so libcurl.so libz.so.1"

docker run --rm --platform "$PLATFORM" \
  -v "$OUT":/out -v "$MONO":/src:ro \
  -e OUT=/out -e PKGS="$PKGS" -e PATTERNS="$PATTERNS" \
  ubuntu:22.04 bash /src/tools/sh/libs_support_linux_inner.sh || { echo "container fallito" >&2; exit 1; }

echo "staging: $OUT"
ls -1 "$OUT" | head -20
