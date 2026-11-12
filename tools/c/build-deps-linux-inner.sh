#!/usr/bin/env bash
# build-deps-linux-inner.sh — terze parti per un RID Linux, DENTRO un container Ubuntu.
#
# PERCHÉ ESISTE. `tools/c/build-deps.sh` compila le terze parti con la toolchain dell'host
# (zig per Windows, clang per macOS) e non ha un ramo Linux: le uniche librerie che mancavano a
# F3 erano proprio `x86_64-linux-gnu` e `aarch64-linux-gnu`. Qui si usa la via più corta e
# riproducibile: un container della STESSA architettura del RID, con i pacchetti `-dev` di
# Ubuntu 24.04, e si copia solo il payload che serve a `dev` (header delle terze parti +
# archivi statici). Non si compila nulla a mano: le versioni sono quelle del distro e il RID
# è garantito dall'immagine (`--platform linux/amd64|linux/arm64`).
#
# USO (invocato da build-deps.sh, non a mano):
#   bash /repo/tools/c/build-deps-linux-inner.sh <triple> <lib...>      OUT montato su /out
#     triple: x86_64-linux-gnu | aarch64-linux-gnu
#   exit 0 = payload completo · 1 = qualcosa manca (l'elenco dei mancanti va su stderr)
#
# NOTA: `libcompression`/`libbz2` NON sono richieste su Linux (Apple-only la prima, inutile la
# seconda: il runtime comprime con zlib/zstd/brotli/lzma/lz4), quindi il gate le tratta come
# librerie di sistema fuori Darwin.
set -uo pipefail

TRIPLE="${1:-}"; shift || true
LIBS="${*:-zlib sqlite3 zstd lz4 brotli lzma pcre2 openssl freetype sdl2 sdl2_ttf curl}"
case "$TRIPLE" in
  x86_64-linux-gnu)  DEBARCH=amd64 ;;
  aarch64-linux-gnu) DEBARCH=arm64 ;;
  *) echo "build-deps-linux-inner: triple non gestito: '$TRIPLE'" >&2; exit 2 ;;
esac
OUT=/out
mkdir -p "$OUT/include" "$OUT/lib" || exit 2

echo "  [container] $TRIPLE ($DEBARCH) · libs: $LIBS"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null 2>&1 || { echo "apt-get update fallito" >&2; exit 1; }
# `--no-install-recommends`: nessun tool grafico/X11 che non fa parte del payload di compilazione.
apt-get install -y -qq --no-install-recommends \
  zlib1g-dev libsqlite3-dev libzstd-dev liblz4-dev libbrotli-dev liblzma-dev \
  libpcre2-dev libssl-dev libfreetype-dev libsdl2-dev libsdl2-ttf-dev libcurl4-openssl-dev \
  >/tmp/apt.log 2>&1 || { tail -20 /tmp/apt.log >&2; exit 1; }

# ── header: SOLO quelli delle terze parti (non /usr/include intero: porterebbe la libc) ─────
# `-L` (dereferenzia): su Ubuntu gli header arch-specifici sono SYMLINK dentro /usr/include
# (es. `SDL2/SDL_platform.h` → `../x86_64-linux-gnu/SDL2/SDL_platform.h`). Con `cp -a` il
# bundle riceve il link e resta un buco che sembra un header mancante (misurato 2026-11-11).
copy_hdr() { for p in "$@"; do [ -e "/usr/include/$p" ] && cp -aL "/usr/include/$p" "$OUT/include/" 2>/dev/null; done; }
copy_hdr zlib.h zconf.h sqlite3.h zstd.h zstd_errors.h lz4.h lz4frame.h lz4hc.h lzma.h lzma \
         brotli pcre2.h pcre2posix.h openssl opensslv.h ossl_typ.h curl SDL2 ft2build.h freetype2 \
         ftd2xx.h jpeglib.h png.h webp
# `freetype2/` è l'albero che include/ft2build.h cerca; esposto anche piatto per chi fa <ft2build.h>.
[ -d /usr/include/freetype2/freetype ] && cp -aL /usr/include/freetype2/freetype "$OUT/include/" 2>/dev/null
# Ubuntu tiene gli header ARCH-SPECIFICI in /usr/include/<triple> (es. `openssl/opensslconf.h`,
# `SDL2/_real_SDL_config.h`): senza, il bundle sembra completo e il preprocessore si ferma.
# Misurato il 2026-11-11: erano gli UNICI 2 header mancanti su linux-x64 e linux-arm64 (5 sorgenti).
if [ -d "/usr/include/$TRIPLE" ]; then
  cp -aL "/usr/include/$TRIPLE/." "$OUT/include/" 2>/dev/null
fi

# ── archivi statici: il payload di LINK del profilo `dev` ────────────────────────────────────
LIBDIR="/usr/lib/$TRIPLE"
[ -d "$LIBDIR" ] || LIBDIR="/usr/lib/${DEBARCH}-linux-gnu"
: > /tmp/want.txt
case " $LIBS " in *" zlib "*)    echo libz.a          >> /tmp/want.txt ;; esac
case " $LIBS " in *" sqlite3 "*) echo libsqlite3.a    >> /tmp/want.txt ;; esac
case " $LIBS " in *" zstd "*)    echo libzstd.a       >> /tmp/want.txt ;; esac
case " $LIBS " in *" lz4 "*)     echo liblz4.a        >> /tmp/want.txt ;; esac
case " $LIBS " in *" brotli "*)  printf '%s\n' libbrotlicommon.a libbrotlidec.a libbrotlienc.a >> /tmp/want.txt ;; esac
case " $LIBS " in *" lzma "*)    echo liblzma.a       >> /tmp/want.txt ;; esac
case " $LIBS " in *" pcre2 "*)   echo libpcre2-8.a    >> /tmp/want.txt ;; esac
case " $LIBS " in *" openssl "*) printf '%s\n' libssl.a libcrypto.a >> /tmp/want.txt ;; esac
case " $LIBS " in *" freetype "*) echo libfreetype.a  >> /tmp/want.txt ;; esac
case " $LIBS " in *" sdl2 "*)    echo libSDL2.a       >> /tmp/want.txt ;; esac
case " $LIBS " in *" sdl2_ttf "*) echo libSDL2_ttf.a  >> /tmp/want.txt ;; esac
case " $LIBS " in *" curl "*)    echo libcurl.a       >> /tmp/want.txt ;; esac

missing=0
while IFS= read -r a; do
  [ -n "$a" ] || continue
  f="$(find "$LIBDIR" -maxdepth 1 -name "$a" 2>/dev/null | head -1)"
  if [ -n "$f" ]; then cp -f "$f" "$OUT/lib/"; else echo "  MANCA: $a (in $LIBDIR)" >&2; missing=$((missing+1)); fi
done < /tmp/want.txt

n_inc=$(find "$OUT/include" -type f | wc -l | tr -d ' ')
n_lib=$(find "$OUT/lib" -type f | wc -l | tr -d ' ')
echo "  [container] $TRIPLE: $n_inc header, $n_lib archivi"
[ "$n_inc" -gt 0 ] && [ "$n_lib" -gt 0 ] || { echo "payload vuoto" >&2; exit 1; }
[ "$missing" -eq 0 ] || { echo "  $missing archivi mancanti dai pacchetti -dev" >&2; exit 1; }
echo "  [container] $TRIPLE OK"
exit 0
