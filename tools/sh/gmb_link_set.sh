#!/usr/bin/env bash
# tools/sh/gmb_link_set.sh — IL SET DI LINK PER UN RID (fase F5 del piano toolchain-zig).
#
# PERCHÉ ESISTE. I runner del leg nativo (`gmb_c99native_run.sh`,
# `test_minitests_c99native`) avevano le liste di link CABLATE su Darwin/arm64:
# `-framework CoreGraphics … AppKit` sempre, `-lcompression` sempre (esiste solo su macOS),
# e le terze parti per NOME ESATTO di file (`libcrypto.3.dylib`, `libSDL2.dylib`, …) con
# l'estensione `.dylib` scritta dentro. Conseguenza: un link per `linux-x64` chiedeva
# framework Apple e file che su Linux non esistono — F5 non poteva funzionare, e ogni nuova
# libreria andava aggiunta a mano in due script.
#
# COSA FA. Dato un RID (i sei del piano) emette il set di link corretto per QUELLA piattaforma:
#   - mapping RID → triple `zig` (§17.1 del piano, UNA tabella);
#   - `-framework` SOLO su Darwin;
#   - librerie di sistema per OS (`-lcompression` esiste solo su macOS);
#   - terze parti per NOME LOGICO (`-lcrypto`, `-lSDL2`, …) da una `-L` sulla cache del RID:
#     l'estensione la sceglie il linker (`.dylib` / `.so` / `.dll.a`), non lo script;
#   - `libgmb_agent_runtime` solo su Darwin (i suoi oggetti usano AX/CG: su Linux non linkano);
#   - nomi ESATTI dei file da scaricare, letti dall'indice `shared/settings/libs-index-<rid>.tsv`
#     (nessun `.dylib` cablato nel downloader).
#
# Uso:
#   eval "$(bash tools/sh/gmb_link_set.sh --rid osx-arm64 [--cache <dir>])"
# Emette assegnazioni shell su stdout, diagnostica su stderr. NIENTE variabili d'ambiente: il RID
# e la cache sono ARGOMENTI.
#
# `LS_AGENT=0` non significa «l'agente non esiste»: significa che per quel RID il suo archivio non
# è linkabile qui (F4/F6 devono fornire la variante per la piattaforma). Il chiamante lo dichiara.
set -uo pipefail

RID=""; CACHE=""; PRINT=""
while [ $# -gt 0 ]; do
  case "$1" in
    --print) PRINT="${2:-}"; shift 2 ;;
    --print=*) PRINT="${1#--print=}"; shift ;;
    --rid)   RID="${2:-}";   shift 2 ;;
    --rid=*) RID="${1#--rid=}"; shift ;;
    --cache) CACHE="${2:-}"; shift 2 ;;
    --cache=*) CACHE="${1#--cache=}"; shift ;;
    -h|--help) sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "gmb_link_set: argomento sconosciuto: $1" >&2; exit 2 ;;
  esac
done

die() { echo "gmb_link_set: $*" >&2; exit 2; }
[ -n "$RID" ] || die "serve --rid <osx-arm64|osx-x64|linux-x64|linux-arm64|win-x64|win-arm64>"

# ── §17.1 del piano: l'UNICA tabella RID → triple. `arm64-macos` non esiste come triple. ──────
case "$RID" in
  osx-arm64)   LS_OS=Darwin; LS_ARCH=arm64;  LS_TRIPLE=aarch64-macos ;;
  osx-x64)     LS_OS=Darwin; LS_ARCH=x86_64; LS_TRIPLE=x86_64-macos ;;
  linux-x64)   LS_OS=Linux;  LS_ARCH=x86_64; LS_TRIPLE=x86_64-linux-gnu.2.28 ;;
  linux-arm64) LS_OS=Linux;  LS_ARCH=arm64;  LS_TRIPLE=aarch64-linux-gnu.2.28 ;;
  win-x64)     LS_OS=Windows; LS_ARCH=x86_64; LS_TRIPLE=x86_64-windows-gnu ;;
  win-arm64)   LS_OS=Windows; LS_ARCH=arm64; LS_TRIPLE=aarch64-windows-gnu ;;
  *) die "RID non riconosciuto: $RID (attesi i 6 del piano)" ;;
esac

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT="$MONO/GMBasic"
[ -n "$CACHE" ] || CACHE="$HOME/.gmbasic/libs/$RID"
INDEX="$MONO/shared/settings/libs-index-$RID.tsv"

# ── Flag di link dipendenti dall'OS (una sola fonte) ─────────────────────────────────────────
LS_TARGETFLAG="-target $LS_TRIPLE"
LS_STRIP=""; LS_RPATH=""; LS_UNDEF=""; LS_NM_GU="-g"
case "$LS_OS" in
  Darwin)
    LS_DYLIBEXT="dylib"
    LS_STRIP="-Wl,-dead_strip_dylibs"      # ld64: tiene solo le dylib davvero usate
    LS_RPATH="-Wl,-rpath,@loader_path"
    LS_UNDEF="-Wl,-undefined,dynamic_lookup"   # dylib con simboli risolti a load-time
    LS_NM_GU="-gU"
    ;;
  Linux)
    LS_DYLIBEXT="so"
    # LS_STRIP VUOTO DI PROPOSITO: `-Wl,--as-needed` non è nella specifica di F5 e non esiste
    # ancora un leg Linux che possa misurarne l'effetto. Si aggiunge quando c'è un gate che lo
    # verifica — non «per simmetria» con `-dead_strip_dylibs` di ld64.
    LS_STRIP=""
    LS_RPATH='-Wl,-rpath,$ORIGIN'
    ;;
  Windows)
    LS_DYLIBEXT="dll"                      # import library dei .dll mingw; `nm` non serve
    LS_NM_GU=""
    ;;
esac

# ── SDK macOS (piano L13): zig ACCETTA `-isysroot` ma non cerca l'SDK da sé. Senza, anche le
# librerie di sistema del target Apple (`-lcurl`, `-lz`, `-lcompression`, `-lbz2`) risultano
# «not found», e un target macOS diverso dall'host non linka affatto. `xcrun` c'è solo su macOS:
# altrove non si emette nulla (e la diagnostica lo dice).
LS_SDK=()
if [ "$LS_OS" = Darwin ]; then
  if command -v xcrun >/dev/null 2>&1; then
    _sdk="$(xcrun --show-sdk-path 2>/dev/null || true)"
    if [ -n "$_sdk" ] && [ -d "$_sdk" ]; then
      # `-isysroot` da solo NON basta: zig non cerca le librerie di sistema (`.tbd` di curl/z/
      # bz2/compression/sqlite3) dentro `usr/lib` dell'SDK — «unable to find dynamic system
      # library … searched paths: none». Con la `-L` esplicita e la `-F` dei framework il cross
      # macOS linka (provato: eseguibile Mach-O x86_64 prodotto da arm64).
      LS_SDK=(-isysroot "$_sdk" -L"$_sdk/usr/lib" -F"$_sdk/System/Library/Frameworks")
    else
      echo "gmb_link_set: $RID — SDK macOS non trovato (xcrun --show-sdk-path vuoto)" >&2
    fi
  else
    echo "gmb_link_set: $RID — xcrun assente: nessun -isysroot (link macOS solo se il host lo può)" >&2
  fi
fi

# ── Framework: SOLO Darwin. Su Linux/Windows non esistono e il link fallirebbe. ───────────────
LS_FRAMEWORKS=()
if [ "$LS_OS" = Darwin ]; then
  LS_FRAMEWORKS=(-framework CoreGraphics -framework ApplicationServices -framework ImageIO
                 -framework CoreServices -framework AppKit)
fi

# ── Librerie di SISTEMA per OS. `-lcompression` è macOS-only (su Linux è libz/liblzma). ───────
LS_SYS=()
case "$LS_OS" in
  Darwin)  LS_SYS=(-lcurl -lcompression -lz -lbz2 -lsqlite3) ;;
  Linux)   LS_SYS=(-lcurl -lz -lbz2 -lsqlite3 -lm -ldl -lpthread) ;;
  # Windows: insieme mingw minimo per curl/sqlite/openssl; NON verificato (F4 non ha ancora
  # prodotto il bundle `runtime` per i due RID Windows) — se il link chiede altro, si aggiunge qui.
  Windows) LS_SYS=(-lcurl -lz -lbz2 -lsqlite3 -lws2_32 -lbcrypt -luserenv -lole32 -luuid -lm) ;;
esac

# ── Terze parti: NOME LOGICO + `-L<cache>`; l'estensione la sceglie il linker. ────────────────
# Si emette una `-l` SOLO se nella cache esiste un file per quel nome: su un RID di cui non si è
# ancora scaricato il `support` la `-l` farebbe fallire il link con «cannot find», mentre il
# messaggio utile è «questa libreria manca nella cache del RID».
# MoltenVK esiste solo su macOS (Metal); MLX non è nel perimetro dei due RID Linux/Windows.
THIRD_LOGIC=(crypto ssl zstd brotlienc brotlidec pcre2-8 SDL2_mixer SDL2_ttf SDL2 lzma lz4)
[ "$LS_OS" = Darwin ] && THIRD_LOGIC=(crypto ssl zstd brotlienc brotlidec pcre2-8 SDL2_mixer SDL2_ttf SDL2 lzma lz4 MoltenVK)

LS_THIRD=()
LS_MISSING=()
for _l in "${THIRD_LOGIC[@]}"; do
  case "$LS_DYLIBEXT" in
    dylib) _pat="$CACHE/lib$_l.dylib" ;;   # `-l` risolve via symlink non versionato
    so)    _pat="$CACHE/lib$_l.so" ;;
    dll)   _pat="$CACHE/lib$_l.dll.a"; [ -f "$_pat" ] || _pat="$CACHE/lib$_l.dll" ;;
  esac
  if [ -f "$_pat" ]; then
    LS_THIRD+=("-l$_l")
  else
    LS_MISSING+=("$_l")
  fi
done
[ ${#LS_THIRD[@]} -gt 0 ] && LS_THIRD=(-L"$CACHE" "${LS_THIRD[@]}")

# ── agent_runtime: solo Darwin (i suoi oggetti usano AX/CG). ──────────────────────────────────
LS_AGENT=0
[ "$LS_OS" = Darwin ] && LS_AGENT=1

# ── File ESATTI da scaricare, dall'indice del RID (niente `.dylib` cablati). ──────────────────
# Per ogni nome logico si prende la riga dell'indice con basename `lib<l>` o `lib<l>.<…>`,
# preferendo il nome più corto (la variante non versionata, che nel bundle è un file reale).
LS_FETCH_NAMES=()
if [ -f "$INDEX" ]; then
  for _l in "${THIRD_LOGIC[@]}"; do
    # SOLO l'artefatto DINAMICO della piattaforma: nell'indice convivono `lib/libcrypto.a`
    # (statica, per il gruppo `dev`) e `libcrypto.so`/`libcrypto.3.dylib` (runtime). Prendere la
    # statica sarebbe un nome che al runtime non serve e su macOS nemmeno esiste per `-l`.
    # (il programma awk è fra apici singoli: le regex si scrivono con UN backslash, `\.`)
    _hit="$(awk -v lib="lib$_l" -v ext="$LS_DYLIBEXT" 'BEGIN{FS="\t"
                   if (ext=="dylib") pat = "^" lib ".*\\.dylib$"
                   else if (ext=="so") pat = "^" lib ".*\\.so([.][0-9]+)*$"
                   else pat = "^" lib ".*\\.dll$" }
                 /^#/ {next}
                 { b=$1; sub(/^.*\//,"",b)
                   if (b !~ pat) next
                   print b }' "$INDEX" \
           | awk '{print length, $0}' | sort -n | head -1 | cut -d' ' -f2- || true)"
    [ -n "$_hit" ] && LS_FETCH_NAMES+=("$_hit")
  done
fi

# ── `--print <campo>`: un valore scalare, per chi deve leggere la triple prima di decidere. ──
if [ -n "$PRINT" ]; then
  case "$PRINT" in
    triple)   echo "$LS_TRIPLE" ;;
    os)       echo "$LS_OS" ;;
    arch)     echo "$LS_ARCH" ;;
    dylibext) echo "$LS_DYLIBEXT" ;;
    agent)    echo "$LS_AGENT" ;;
    *) die "--print: campo sconosciuto: $PRINT (triple|os|arch|dylibext|agent)" ;;
  esac
  exit 0
fi

# ── Emissione (eval-abile). Array protetti: il chiamante usa `set -u`. ───────────────────────
printf 'LS_OS=%q\n' "$LS_OS"
printf 'LS_ARCH=%q\n' "$LS_ARCH"
printf 'LS_TRIPLE=%q\n' "$LS_TRIPLE"
printf 'LS_DYLIBEXT=%q\n' "$LS_DYLIBEXT"
printf 'LS_TARGETFLAG=%q\n' "$LS_TARGETFLAG"
printf 'LS_STRIP=%q\n' "$LS_STRIP"
printf 'LS_RPATH=%q\n' "$LS_RPATH"
printf 'LS_UNDEF=%q\n' "$LS_UNDEF"
printf 'LS_NM_GU=%q\n' "$LS_NM_GU"
printf 'LS_AGENT=%q\n' "$LS_AGENT"
printf 'LS_SDK=('
for _a in ${LS_SDK[@]+"${LS_SDK[@]}"}; do printf '%q ' "$_a"; done
printf ')\n'
printf 'LS_FRAMEWORKS=('
for _a in ${LS_FRAMEWORKS[@]+"${LS_FRAMEWORKS[@]}"}; do printf '%q ' "$_a"; done
printf ')\n'
printf 'LS_SYS=('
for _a in ${LS_SYS[@]+"${LS_SYS[@]}"}; do printf '%q ' "$_a"; done
printf ')\n'
printf 'LS_THIRD=('
for _a in ${LS_THIRD[@]+"${LS_THIRD[@]}"}; do printf '%q ' "$_a"; done
printf ')\n'
printf 'LS_FETCH_NAMES=('
for _a in ${LS_FETCH_NAMES[@]+"${LS_FETCH_NAMES[@]}"}; do printf '%q ' "$_a"; done
printf ')\n'

# Diagnostica: cosa manca nel RID (informativa, non errore — può essere un RID non ancora
# scaricato, es. il cross in F5). Il chiamante decide se è fatale.
if [ ${#LS_MISSING[@]} -gt 0 ]; then
  echo "gmb_link_set: $RID — terze parti assenti in $CACHE: ${LS_MISSING[*]}" >&2
fi
exit 0
