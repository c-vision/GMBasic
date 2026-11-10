#!/usr/bin/env bash
# tools/sh/libs_toolchain_pack.sh — GRUPPO `toolchain`: il COMPILATORE come artefatto per RID.
#
# PERCHÉ ESISTE. zig è l'unico compilatore del progetto (F1 del piano Toolchain-Zig), ma finora
# non era un artefatto distribuito: si assumeva un zig nel PATH. Qui la distribuzione ufficiale
# (già firmata/rilasciata da ziglang.org) diventa un bundle per RID, nella stessa forma degli
# altri gruppi: `<nome><TAB>sha256<TAB>size<TAB>url`, consumabile dal downloader C e dal manifest.
#
# COSA PRODUCE, per RID (uno per riga in uscita):
#   zig-<versione>-<rid>.tar.xz   radice unica `zig-<versione>-<rid>/` con il compilatore,
#                                 `LICENSE` e `THIRD_PARTY_NOTICES` generato dal contenuto reale.
# Il RID è quello del progetto (GmbRid.cmake): osx-arm64, osx-x64, linux-x64, linux-arm64,
# win-x64, win-arm64. Le distribuzioni Windows sono `.zip`: vengono ri-impacchettate in `.tar.xz`
# per avere UN formato di artefatto (il downloader non deve conoscere due formati).
#
# NON compila nulla e NON pubblica: la pubblicazione passa dal canale unico degli asset
# (`libs_publish_files_gh.sh`, D13 = un solo writer dell'indice). Il tag di release è un
# parametro (`--tag`, default `toolchain-v1`) e finisce solo nell'URL stampato.
#
# USO
#   tools/sh/libs_toolchain_pack.sh [--rid RID|--all] [opzioni]
#     --version V        versione zig da impacchettare (default 0.17.0)
#     --rid RID          un RID (default: RID della macchina)
#     --all              tutti e 6 i RID
#     --tag TAG          tag della release per l'URL stampato (default toolchain-v1)
#     --out DIR          dove scrivere gli artefatti (default <repo>/build/libs/toolchain)
#     --cache DIR        cache delle distribuzioni scaricate (default ~/.gmbasic/toolchains/dist)
#     --from FILE        usa QUESTA distribuzione per il RID singolo (niente download;
#                        utile offline e per rigiocare un artefatto già scaricato)
#     --dry-run          stampa piano + URL + nomi, non scarica e non scrive
#     --keep-work        non cancella la cartella di lavoro (diagnosi)
#     -h|--help
#
# GATE (eseguito in automatico quando il RID è quello della macchina): l'artefatto viene
# estratto in una cartella temporanea e `zig version` deve stampare la versione attesa. Sui RID
# non-host il gate possibile è strutturale: radice unica, `zig` eseguibile presente, licenze
# incluse, sha256 registrato.
#
# NESSUNA variabile d'ambiente controlla il comportamento (AGENTS): solo argomenti.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"                 # repo/tools
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"            # radice del monorepo (quella con build/)
. "$SCRIPT_DIR/libs_groups.sh"

ZIG_VERSION="0.17.0"
TAG="toolchain-v1"
RID=""
ALL=0
DRY=0
FROM=""
KEEP_WORK=0
OUT_DIR="$PROJECT_ROOT/build/libs/toolchain"
CACHE_DIR="$HOME/.gmbasic/toolchains/dist"

usage() { sed -n '2,45p' "$0" | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
  case "$1" in
    --version) ZIG_VERSION="${2:-}"; shift 2 ;;
    --rid)     RID="${2:-}"; shift 2 ;;
    --all)     ALL=1; shift ;;
    --tag)     TAG="${2:-}"; shift 2 ;;
    --out)     OUT_DIR="${2:-}"; shift 2 ;;
    --cache)   CACHE_DIR="${2:-}"; shift 2 ;;
    --from)    FROM="${2:-}"; shift 2 ;;
    --dry-run) DRY=1; shift ;;
    --keep-work) KEEP_WORK=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done

host_rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)             echo osx-arm64 ;;
    Darwin-x86_64)            echo osx-x64 ;;
    Linux-x86_64|Linux-amd64)  echo linux-x64 ;;
    Linux-aarch64|Linux-arm64) echo linux-arm64 ;;
    MINGW*|MSYS*|CYGWIN*)     case "$(uname -m)" in aarch64|arm64) echo win-arm64 ;; *) echo win-x64 ;; esac ;;
    *) echo "" ;;
  esac
}

# RID → nome della distribuzione UFFICIALE (macOS/Linux .tar.xz, Windows .zip).
dist_name() {
  case "$1" in
    osx-arm64)   echo "zig-aarch64-macos-${ZIG_VERSION}.tar.xz" ;;
    osx-x64)     echo "zig-x86_64-macos-${ZIG_VERSION}.tar.xz" ;;
    linux-x64)   echo "zig-x86_64-linux-${ZIG_VERSION}.tar.xz" ;;
    linux-arm64) echo "zig-aarch64-linux-${ZIG_VERSION}.tar.xz" ;;
    win-x64)     echo "zig-x86_64-windows-${ZIG_VERSION}.zip" ;;
    win-arm64)   echo "zig-aarch64-windows-${ZIG_VERSION}.zip" ;;
    *) return 1 ;;
  esac
}

all_rids() { echo "osx-arm64 osx-x64 linux-x64 linux-arm64 win-x64 win-arm64"; }

# sha256 portatile: Git for Windows non ha sempre `shasum` (è uno script Perl), ma ha `sha256sum`
# o `openssl`. Senza questo il CI Windows scriverebbe un indice con lo sha VUOTO.
# Opzioni di impacchettamento IDENTICHE fra bsdtar (macOS/Windows) e GNU tar (Linux). Senza,
# l'archivio porta il NOME dell'utente del runner (`gaetano`/`runner`, `staff`/`root`) e lo stesso
# contenuto produce sha diversi — misurato 2026-11-10 su osx-arm64: 19.506 file identici, sha
# diverso solo per `uname`/`gname` e per il fuso dell'mtime.
tar_opts() {
  # NIENTE `--format=pax`: misurato il 2026-11-10, i pax header per file portano l'artefatto da
  # 57 MB a 185 MB (3,2x sul download di ogni RID) in cambio di una identita byte-per-byte che
  # comunque non copre GNU tar vs bsdtar. Formato di default (restricted pax → ustar sui nomi
  # normali): si tengono solo i campi che non costano nulla.
  if tar --version 2>/dev/null | head -1 | grep -qi 'gnu tar'; then
    printf '%s\n' --owner=0 --group=0 --numeric-owner --sort=name
  else
    printf '%s\n' --uid 0 --gid 0 --uname '' --gname ''
  fi
}

sha256_of() { # $1 = file
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}';
  elif command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}';
  else openssl dgst -sha256 "$1" | awk '{print $NF}';
  fi
}

[ "$ALL" = "1" ] || [ -n "$RID" ] || RID="$(host_rid)"
[ "$ALL" = "1" ] || [ -n "$RID" ] || { echo "RID non riconosciuto: passare --rid o --all" >&2; exit 2; }

if [ "$ALL" = "1" ]; then RIDS="$(all_rids)"; else RIDS="$RID"; fi
if [ -n "$FROM" ] && [ "$(echo "$RIDS" | wc -w)" -gt 1 ]; then
  echo "--from vale per un solo RID (non con --all)" >&2; exit 2
fi

# ── piano ────────────────────────────────────────────────────────────────────────────────────
echo "== libs_toolchain_pack: zig $ZIG_VERSION · tag $TAG · dry=$DRY"
for r in $RIDS; do
  d="$(dist_name "$r")" || { echo "RID non valido: $r" >&2; exit 2; }
  echo "   $r -> $d  ->  zig-${ZIG_VERSION}-${r}.tar.xz"
done
[ "$DRY" = "1" ] && { echo "(dry-run: non scarico, non scrivo)"; exit 0; }

mkdir -p "$OUT_DIR" || { echo "impossibile creare $OUT_DIR" >&2; exit 1; }

# ── THIRD_PARTY_NOTICES dal contenuto REALE del pacchetto ────────────────────────────────────
write_notices() { # $1 = radice estratta, $2 = rid, $3 = file di uscita
  local src="$1" rid="$2" out="$3" rel n=0
  {
    echo "THIRD_PARTY NOTICES — zig ${ZIG_VERSION} · RID ${rid}"
    echo "Generato da tools/sh/libs_toolchain_pack.sh (artefatto deterministico: nessun timestamp)."
    echo "Elenco dei file di licenza presenti nella distribuzione ufficiale, testo integrale."
    echo
  } > "$out"
  while IFS= read -r rel; do
    n=$((n + 1))
    {
      echo "=================================================================="
      echo "== ${rel}"
      echo "=================================================================="
      cat "$src/$rel"
      echo
    } >> "$out"
  # `LC_ALL=C` è obbligatorio: con il locale del runner l'ordine dei file cambia (stessa dimensione,
  # byte diversi) e i notices — parte dell'artefatto — non sarebbero più riproducibili.
  done < <(cd "$src" && find . \( -iname 'LICENSE*' -o -iname 'COPYING*' -o -iname 'NOTICE*' \) -type f | sed 's|^\./||' | LC_ALL=C sort)
  echo "   notices: $n file di licenza"
  [ "$n" -gt 0 ] || { echo "ERRORE: nessuna licenza trovata nella distribuzione" >&2; return 1; }
}

pack_one() { # $1 = rid
  local rid="$1" dist dist_path work top archive sha size zurl found_ver
  dist="$(dist_name "$rid")" || return 1
  archive="zig-${ZIG_VERSION}-${rid}.tar.xz"

  # 1) distribuzione: --from, oppure cache, oppure download
  mkdir -p "$CACHE_DIR"
  if [ -n "$FROM" ]; then
    [ -f "$FROM" ] || { echo "ERRORE: --from '$FROM' non esiste" >&2; return 1; }
    dist_path="$FROM"
  else
    dist_path="$CACHE_DIR/$dist"
    if [ ! -f "$dist_path" ]; then
      zurl="https://ziglang.org/download/${ZIG_VERSION}/${dist}"
      echo "-> download $zurl"
      curl -fL --retry 3 --retry-delay 2 --max-time 1800 -o "$dist_path.part" "$zurl" \
        || { echo "ERRORE: download fallito ($zurl)" >&2; rm -f "$dist_path.part"; return 1; }
      mv -f "$dist_path.part" "$dist_path"
    else
      echo "-> cache: $dist_path"
    fi
  fi

  # 2) estrazione (tar.xz oppure zip) in una radice con il nome definitivo
  work="$(mktemp -d "${TMPDIR:-/tmp}/gmbtc.XXXXXX")" || return 1
  case "$dist" in
    # Windows: la distribuzione ufficiale è uno zip. `unzip` non esiste su tutti i runner: si usa
    # bsdtar (libarchive legge lo zip) come fallback, così il packer gira anche su Windows.
    *.zip)
      if command -v unzip >/dev/null 2>&1; then
        unzip -q "$dist_path" -d "$work" || { echo "ERRORE: unzip fallito" >&2; return 1; }
      else
        tar -xf "$dist_path" -C "$work" || { echo "ERRORE: estrazione zip fallita" >&2; return 1; }
      fi ;;
    *)     tar -xf "$dist_path" -C "$work"    || { echo "ERRORE: estrazione fallita" >&2; return 1; } ;;
  esac
  top="$(find "$work" -maxdepth 1 -mindepth 1 -type d | head -1)"
  [ -n "$top" ] || { echo "ERRORE: distribuzione senza cartella radice" >&2; return 1; }

  # 3) radice col nome del nostro artefatto + notices
  mv "$top" "$work/zig-${ZIG_VERSION}-${rid}" || return 1
  top="$work/zig-${ZIG_VERSION}-${rid}"
  if ! write_notices "$top" "$rid" "$top/THIRD_PARTY_NOTICES"; then
    [ "$KEEP_WORK" = "1" ] || rm -rf "$work"; return 1
  fi
  # DETERMINISMO: due run sullo stesso input devono produrre lo STESSO sha256, perché l'indice
  # pubblica un hash per file e il downloader lo verifica. Misurato 2026-11-10: con un timestamp
  # nei notices due run davano 57.400.160 e 57.419.744 byte (hash diversi → artefatto non
  # riproducibile). mtime fisso su notices e radice: l'artefatto dipende solo dal contenuto.
  # `TZ=UTC0`: `touch -t` interpreta l'ora nel fuso locale; su un runner UTC e su una macchina CET
  # lo stesso comando scriveva 1980-01-01 00:00 e 1979-12-31 23:00 → tar diversi.
  TZ=UTC0 touch -t 198001010000 "$top/THIRD_PARTY_NOTICES" 2>/dev/null || true
  TZ=UTC0 touch -t 198001010000 "$top" 2>/dev/null || true
  [ -x "$top/zig" ] || [ -x "$top/zig.exe" ] || {
    echo "ERRORE: compilatore assente nella distribuzione ($rid)" >&2
    [ "$KEEP_WORK" = "1" ] || rm -rf "$work"; return 1; }

  # 4) impacchettamento: un solo formato (.tar.xz) per tutti i RID
  echo "-> comprimo $archive"
  local -a TOPTS=()
  while IFS= read -r o; do TOPTS+=("$o"); done < <(tar_opts)
  # ELENCO ORDINATO: tar archivia nell'ordine in cui riceve i path, e l'ordine di `readdir`
  # dipende dal filesystem. Due macchine con contenuto identico producevano flussi tar diversi
  # (misurato 2026-11-10: 19.506 file identici byte per byte, tar decompresso diverso).
  local LIST="$work/.filelist"
  ( cd "$work" && find "zig-${ZIG_VERSION}-${rid}" | LC_ALL=C sort > "$LIST" )
  if ! tar ${TOPTS[@]+"${TOPTS[@]}"} -cJf "$OUT_DIR/$archive" -C "$work" -T "$LIST" 2>/dev/null; then
    # Runner il cui tar non ha xz integrato (alcuni Windows): si comprime a parte. Contenuto e
    # formato identici; se manca anche `xz` l'errore è esplicito, mai un archivio corrotto.
    if ! command -v xz >/dev/null 2>&1; then
      echo "ERRORE: il tar locale non supporta -J e non c'è xz" >&2
      rm -f "$OUT_DIR/$archive"; [ "$KEEP_WORK" = "1" ] || rm -rf "$work"; return 1
    fi
    tar ${TOPTS[@]+"${TOPTS[@]}"} -cf - -C "$work" -T "$LIST" | xz -6 > "$OUT_DIR/$archive" \
      || { echo "ERRORE: compressione xz fallita" >&2; rm -f "$OUT_DIR/$archive"; return 1; }
  fi

  # 5) gate: sul RID host l'artefatto DEVE eseguire e dichiarare la versione attesa
  if [ "$rid" = "$(host_rid)" ]; then
    check="$(mktemp -d "${TMPDIR:-/tmp}/gmbtcchk.XXXXXX")"
    tar -xf "$OUT_DIR/$archive" -C "$check" || { echo "ERRORE: riestrazione fallita" >&2; return 1; }
    found_ver="$("$check/zig-${ZIG_VERSION}-${rid}/zig" version 2>/dev/null || true)"
    if [ "$found_ver" = "$ZIG_VERSION" ]; then
      echo "   gate: zig version = $found_ver (OK)"
    else
      echo "ERRORE: gate versione fallito: '$found_ver' != '$ZIG_VERSION'" >&2
      rm -rf "$check"; return 1
    fi
    rm -rf "$check"
  else
    echo "   gate: RID non-host — controllo strutturale (radice, eseguibile, licenze, sha256)"
  fi

  # 6) sha256 + riga d'indice (stesso formato di libs-index-<rid>.tsv)
  sha="$(sha256_of "$OUT_DIR/$archive")"
  size="$(wc -c < "$OUT_DIR/$archive" | tr -d ' ')"
  # Formato CANONICO di `shasum -a 256 -c` (`<hash>  <file>`): un sidecar non verificabile con gli
  # strumenti standard non serve a nulla (prima scriveva `sha256 <hash>`).
  printf '%s  %s\n' "$sha" "$archive" > "$OUT_DIR/$archive.sha256"
  printf '   %s  %s byte\n' "$archive" "$size"
  # Una riga per RID in un file separato: l'indice finale si compone DOPO il loop (vedi sotto).
  printf '%s\t%s\t%s\t%s\n' "$archive" "$sha" "$size" \
    "https://github.com/c-vision/GMBasicModules/releases/download/${TAG}/${archive}" \
    > "$OUT_DIR/.idx-${rid}"

  [ "$KEEP_WORK" = "1" ] || rm -rf "$work"
  return 0
}

fail=0
rm -f "$OUT_DIR"/.idx-*
for r in $RIDS; do
  echo "-- $r"
  pack_one "$r" || { echo "FALLITO: $r" >&2; fail=$((fail + 1)); }
done

# INDICE ACCUMULATIVO. Un'invocazione per RID non deve cancellare le righe degli altri RID:
# prima l'indice veniva troncato a ogni run (`: > file`) e restava una sola riga (misurato
# 2026-11-10). Merge per nome, l'ultima riga vince, ordine stabile.
IDX="$OUT_DIR/libs-index-toolchain.tsv"
{
  [ -f "$IDX" ] && cat "$IDX"
  cat "$OUT_DIR"/.idx-* 2>/dev/null || true
} | awk -F'\t' 'NF>=4 && $1!="" {a[$1]=$0} END {for (k in a) print a[k]}' | sort > "$IDX.new"
mv -f "$IDX.new" "$IDX"
rm -f "$OUT_DIR"/.idx-*

echo
echo "== indice ($IDX)"
cat "$IDX"
echo
echo "== pubblicazione (canale unico degli asset, D13)"
echo "   tools/sh/libs_publish_files_gh.sh toolchain <rid> --source $OUT_DIR"
[ "$fail" -eq 0 ] || { echo "== $fail RID falliti" >&2; exit 1; }
echo "== OK"
