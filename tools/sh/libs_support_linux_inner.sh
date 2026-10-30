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

echo "file raccolti: $(ls "$OUT" | wc -l)"
