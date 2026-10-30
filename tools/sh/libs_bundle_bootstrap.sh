#!/usr/bin/env bash
# libs_bundle_bootstrap.sh — prepara la CACHE di un RID con le terze parti pubblicate.
#
# PERCHÉ. Il build delle nostre librerie (e il CMake di `gmb_runtime_c`) vuole le terze parti
# già presenti nella cache canonica `~/.gmbasic/libs/<rid>/`: senza, si ferma con
# `GMB_CRYPTO_LIB non trovata`. Su una macchina di sviluppo la cache c'è; in un container
# (build Linux) è vuota — e il compilatore C# (`gmb install`) non è disponibile lì.
#
# Questo script scarica l'INDICE PIATTO (`libs-index`) e porta in cache i file del RID,
# verificando lo SHA-256. È l'unico punto che sa scaricare senza il compilatore.
#
# USO
#   tools/sh/libs_bundle_bootstrap.sh <rid> [--groups runtime,support] [--out DIR]
#
#   --scopes   quali scope scaricare (5ª colonna dell'indice: runtime|dev|component:*).
#              Default: tutto ciò che l'indice elenca per il RID.
#   --out      dove scrivere (default: ~/.gmbasic/libs/<rid>)
set -uo pipefail

RID="${1:-}"
[ -n "$RID" ] || { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
shift || true

SCOPES=""; OUT="$HOME/.gmbasic/libs/$RID"
while [ $# -gt 0 ]; do
  case "$1" in
    --scopes) SCOPES="${2:-}"; shift 2 ;;
    --out)    OUT="${2:-}"; shift 2 ;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done

IDX_URL="https://github.com/c-vision/GMBasicModules/releases/download/libs-index/index-$RID.txt"
TMP="$(mktemp "${TMPDIR:-/tmp}/gmbidx.XXXXXX")"
trap 'rm -f "$TMP"' EXIT

command -v curl >/dev/null || { echo "curl richiesto" >&2; exit 1; }
curl -fsSL --max-time 120 "$IDX_URL" -o "$TMP" || { echo "indice non scaricabile: $IDX_URL" >&2; exit 1; }

mkdir -p "$OUT"
ok=0; skip=0; bad=0; miss=0
while IFS=$'\t' read -r name sha size url scope; do
  case "$name" in \#*|"") continue ;; esac
  if [ -n "$SCOPES" ]; then
    case ",$SCOPES," in *",$scope,"*) ;; *) continue ;; esac
  fi
  dest="$OUT/$name"
  if [ -f "$dest" ]; then skip=$((skip+1)); continue; fi
  tmp="$dest.part"
  if ! curl -fsSL --max-time 300 -o "$tmp" "$url"; then
    echo "  ERRORE download: $name" >&2; rm -f "$tmp"; miss=$((miss+1)); continue
  fi
  if command -v sha256sum >/dev/null; then
    got=$(sha256sum "$tmp" | cut -d' ' -f1)
  else
    got=$(shasum -a 256 "$tmp" | cut -d' ' -f1)
  fi
  if [ "$got" = "$sha" ]; then mv "$tmp" "$dest"; ok=$((ok+1));
  else echo "  SHA-256 DIVERSO: $name (atteso $sha, ottenuto $got)" >&2; rm -f "$tmp"; bad=$((bad+1)); fi
done < "$TMP"

# I bundle pubblicano i file VERSIONATI (`libcrypto.so.3`), ma `find_library`/il linker cercano
# il nome NON versionato (`libcrypto.so`): senza questi symlink il CMake si ferma con
# "GMB_CRYPTO_LIB non trovata" anche avendo la libreria in cache. Su macOS la cosa non si vede
# perché il nome è già non versionato (libcrypto.3.dylib è cercato con quel nome).
linked=0
for f in "$OUT"/*.so.*; do
  [ -e "$f" ] || continue
  base="${f%%.so.*}.so"
  [ -e "$base" ] || { ln -sf "$(basename "$f")" "$base" && linked=$((linked+1)); }
done

echo "bootstrap $RID → $OUT: $ok scaricati, $skip già presenti, $bad sha errati, $miss errori, $linked symlink .so"
[ "$bad" = "0" ] && [ "$miss" = "0" ]
