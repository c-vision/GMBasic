#!/usr/bin/env bash
# tools/sh/release.sh — compone l'ARTEFATTO di release (P2 del piano Release-1.0).
#
# Cosa fa (niente install(): lo staging è l'unica fonte, come da piano):
#   1. verifica che gli artefatti di build esistano (e con --build li produce: runtime C,
#      compilatore, gmbadmin, VM/C);
#   2. compone  <out>/gmbasic-<ver>-<rid>/  con la struttura che `gmb doctor` sa leggere:
#        bin/                      gmb, gmbnative, gmbchunk, gmbadmin, gmb_vm_host, gmblsp,
#                                  gmblibs/gmbfetch/gmbbrains/gmbfixtures/gmbimgdiff/gmbtheme
#        bin/libs/<rid>/           dylib del runtime (libgmb_eval, _bsp, _runtime_ai, …)
#        GMBasic/native/*/include  header per gli `-I` della compilazione
#        GMBasic/native/*/build/*.a  statiche del runtime/compilatore/DB/doc/agent
#        GMBasic/gmbllm/include    header del model compiler (PUL)
#        shared/                   settings (manifest), include (.inc), Controls
#        LLMEnv/                   SOLO se presente accanto al monorepo (header + ABI + lib)
#        release-manifest.json     versione, RID, sha256+size di OGNI file
#   3. crea  gmbasic-<ver>-<rid>.zip  +  .sha256  (verifica con `gmb doctor --verify`)
#
# Uso:
#   bash tools/sh/release.sh                       # RID host, versione da GMB_VERSION
#   bash tools/sh/release.sh --build               # produce prima i 4 progetti (lento)
#   bash tools/sh/release.sh --version 1.0.0 --out /tmp/rel
#   bash tools/sh/release.sh --rid linux-x64       # solo se la cross toolchain è pronta
#   bash tools/sh/release.sh --no-zip              # solo l'albero (per ispezione)
#
# Exit: 0 = artefatto composto · 1 = pezzo obbligatorio mancante · 2 = uso/ambiente.
# Nessuna variabile d'ambiente: i parametri passano solo dai flag.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION=""
RID=""
OUT=""
DO_BUILD=0
DO_ZIP=1
QUIET=0

while [ $# -gt 0 ]; do
    case "$1" in
        --build)   DO_BUILD=1; shift ;;
        --no-zip)  DO_ZIP=0; shift ;;
        --version) VERSION="${2:?}"; shift 2 ;;
        --version=*) VERSION="${1#*=}"; shift ;;
        --rid)     RID="${2:?}"; shift 2 ;;
        --rid=*)   RID="${1#*=}"; shift ;;
        --out)     OUT="${2:?}"; shift 2 ;;
        --out=*)   OUT="${1#*=}"; shift ;;
        --quiet)   QUIET=1; shift ;;
        -h|--help) awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"; exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --help)" >&2; exit 2 ;;
    esac
done

say() { [ "$QUIET" = 1 ] || echo "$*"; }
die() { echo "release: $*" >&2; exit 1; }

# ── RID e versione ────────────────────────────────────────────────────────────────────────────
if [ -z "$RID" ]; then
    case "$(uname -s)/$(uname -m)" in
        Darwin/arm64)  RID="osx-arm64" ;;
        Darwin/x86_64) RID="osx-x64" ;;
        Linux/x86_64)  RID="linux-x64" ;;
        Linux/aarch64) RID="linux-arm64" ;;
        *) die "RID non riconosciuto ($(uname -s)/$(uname -m)): usa --rid" ;;
    esac
fi
if [ -z "$VERSION" ]; then
    VERSION="$(sed -n 's/.*gmb \([0-9][0-9.]*\).*/\1/p' "$ROOT/GMBasic/native/gmb_compiler_c/src/cli/gmb_cli.c" | head -1)"
    [ -n "$VERSION" ] || die "versione non deducibile: usa --version X.Y.Z"
fi
OUT="${OUT:-$ROOT/dist}"
STAGE="$OUT/gmbasic-$VERSION-$RID"

say "release: GMBasic $VERSION — RID $RID"
say "release: staging in $STAGE"

# ── 1. artefatti di build ─────────────────────────────────────────────────────────────────────
needed_libs() {  # NOMI delle statiche minime (le build dir variano: gmb_doc/agent sono sottoprogetti)
    printf '%s\n' libgmb_runtime.a libgmb_runtime_min.a libgmb_chart.a libgmb_db.a \
                  libgmb_compiler_c.a libgmb_doc.a libgmb_agent_runtime.a
}
has_lib() {  # <nome> → 0 se esiste in una build dir sotto native/
    [ -n "$(find "$ROOT/GMBasic/native" -name "$1" -path "*/build/*" -print -quit 2>/dev/null)" ]
}
needed_dylibs() {
    printf '%s\n' libgmb_eval libgmb_bsp
}
needed_bins() {
    printf '%s\n' gmb gmbnative gmbchunk gmbadmin gmb_vm_host gmblsp gmblibs gmbfetch gmbbrains gmbfixtures gmbimgdiff gmbtheme
}

build_all() {
    local p
    for p in gmb_runtime_c gmb_compiler_c gmbadmin gmb_vm_c; do
        say "release: build $p…"
        cmake -S "$ROOT/GMBasic/native/$p" -B "$ROOT/GMBasic/native/$p/build" >/dev/null 2>&1 || die "configure fallito: $p"
        cmake --build "$ROOT/GMBasic/native/$p/build" -j "${JOBS:-6}" >/dev/null 2>&1 || die "build fallita: $p"
    done
}

missing=0
while IFS= read -r lib; do has_lib "$lib" || { say "release: MANCA $lib"; missing=$((missing + 1)); }; done < <(needed_libs)
while IFS= read -r d; do
    if [ ! -f "$ROOT/GMBasic/native/gmb_vm_c/build/$d.dylib" ] && [ ! -f "$ROOT/GMBasic/native/gmb_vm_c/build/$d.so" ]; then
        say "release: MANCA $d (build di gmb_vm_c)"; missing=$((missing + 1))
    fi
done < <(needed_dylibs)
while IFS= read -r b; do [ -x "$ROOT/bin/$b" ] || { say "release: MANCA bin/$b"; missing=$((missing + 1)); }; done < <(needed_bins)
if [ "$missing" -gt 0 ]; then
    if [ "$DO_BUILD" = 1 ]; then
        build_all
    else
        die "$missing pezzi mancanti: rilancia con --build (o compila i 4 progetti)"
    fi
fi

# ── 2. composizione dell'albero ───────────────────────────────────────────────────────────────
rm -rf "$STAGE"
mkdir -p "$STAGE/bin/libs/$RID" "$STAGE/GMBasic/native" "$STAGE/shared" "$STAGE/LLMEnv"

copied=0
cp_one() {  # <src-rel> <dst>
    local src="$ROOT/$1" dst="$2"
    [ -e "$src" ] || return 1
    mkdir -p "$(dirname "$dst")"
    if [ -d "$src" ]; then cp -R "$src" "$dst"; else cp -p "$src" "$dst"; fi
    copied=$((copied + 1))
    return 0
}

while IFS= read -r b; do cp_one "bin/$b" "$STAGE/bin/$b" || true; done < <(needed_bins)

# dylib del runtime accanto ai binari (rpath @loader_path/libs/<rid>)
if [ -d "$ROOT/GMBasic/build/libs/$RID" ]; then
    for f in "$ROOT/GMBasic/build/libs/$RID"/*.dylib "$ROOT/GMBasic/build/libs/$RID"/*.so; do
        [ -e "$f" ] && cp_one "GMBasic/build/libs/$RID/$(basename "$f")" "$STAGE/bin/libs/$RID/$(basename "$f")"
    done
fi
# libgmb_eval/_bsp stanno nella build dir della VM, non in build/libs
for f in libgmb_eval.dylib libgmb_bsp.dylib libgmb_eval.so libgmb_bsp.so; do
    [ -f "$ROOT/GMBasic/native/gmb_vm_c/build/$f" ] || continue
    cp_one "GMBasic/native/gmb_vm_c/build/$f" "$STAGE/bin/libs/$RID/$f"
    # stessa posizione del checkout: è dove `gmb doctor` le cerca
    cp_one "GMBasic/native/gmb_vm_c/build/$f" "$STAGE/GMBasic/native/gmb_vm_c/build/$f"
done

# ── dipendenze a CARICO (dylib @rpath) dei binari: devono viaggiare nello zip ─────────────────
# `gmbadmin`/`gmb_vm_host` linkano libcrypto/libssl/SDL2/… a load time: un `gmb libs ensure`
# non può salvarli (il processo non parte). Si copia la CHIUSURA transitiva: le dipendenze
# delle dylib copiate possono averne altre (es. `libSDL2_ttf` → `libSDL3`, che in dev c'è
# nella cache e senza cui SDL2 apre un DIALOG MODALE e il processo resta appeso).
deps_of() {  # <file> → nomi @rpath
    if [ "$(uname -s)" = "Darwin" ]; then
        otool -L "$1" 2>/dev/null | awk '/@rpath\//{sub(/^@rpath\//,"",$1); print $1}'
    else
        readelf -d "$1" 2>/dev/null | awk '/NEEDED/ && /\[/{gsub(/[\[\]]/,"",$NF); print $NF}'
    fi
}
copy_dylib_deps() {
    local cache="$HOME/.gmbasic/libs/$RID" round=0 new=1
    while [ "$new" = 1 ] && [ "$round" -lt 6 ]; do
        new=0; round=$((round + 1))
        for src in "$STAGE"/bin/* "$STAGE"/bin/libs/"$RID"/*; do
            [ -f "$src" ] || continue
            case "$(basename "$src")" in libs) continue ;; esac
            deps_of "$src" | sort -u | while IFS= read -r n; do
                [ -n "$n" ] || continue
                [ -f "$STAGE/bin/libs/$RID/$n" ] && continue
                for cand in "$ROOT/GMBasic/build/libs/$RID/$n" "$cache/$n" "$cache/lib/$n" \
                            "$ROOT/GMBasic/native/gmb_vm_c/build/$n" "$ROOT/bin/libs/$RID/$n"; do
                    if [ -f "$cand" ]; then
                        cp -p "$cand" "$STAGE/bin/libs/$RID/$n"
                        echo "$n" >> "$OUT/.deps-added"
                        break
                    fi
                done
            done
        done
        # qualcosa è stato aggiunto in questo giro? (allora il giro dopo guarda anche le nuove)
        [ -s "$OUT/.deps-added" ] && { new=1; : > "$OUT/.deps-added"; }
    done
    # le dylib copiate devono avere un path di caricamento relativo (mai quello del builder)
    for d in "$STAGE"/bin/libs/"$RID"/*.dylib; do
        [ -f "$d" ] || continue
        install_name_tool -id "@rpath/$(basename "$d")" "$d" 2>/dev/null || true
    done
}
copy_dylib_deps

# ── companion caricati con dlopen (invisibili a otool/readelf) ────────────────────────────────
# `libSDL2.dylib` (build pubblicata) cerca `libSDL3.dylib` A RUNTIME: se manca non fallisce con
# un errore, apre un **dialogo modale** (`NSAlert runModal`) e il processo resta appeso — trovato
# il 2026-11-16 sull'artefatto (`gmbadmin` non stampava nemmeno `--help`). Si copia dal build o
# dalla cache; poi si ripete la chiusura perché anche SDL3 ha le sue dipendenze.
for n in libSDL3.dylib libSDL3_ttf.dylib; do
    [ -f "$STAGE/bin/libs/$RID/$n" ] && continue
    for cand in "$ROOT/GMBasic/build/libs/$RID/$n" "$HOME/.gmbasic/libs/$RID/$n" \
                "$HOME/.gmbasic/libs/$RID/lib/$n" "$ROOT/bin/libs/$RID/$n"; do
        if [ -f "$cand" ]; then cp -p "$cand" "$STAGE/bin/libs/$RID/$n"; say "release: companion $n"; break; fi
    done
done
copy_dylib_deps

# header + statiche — le STESSE radici che `gmb_cc_inc_dirs` passa al `cc` del programma
# emesso (gmb_compiler_c/frontend/src/gmb_cc_defaults.c). Senza, un programma qualunque non
# compila nell'artefatto («'gmb_cua_runtime.h' file not found»: quel header sta nella RADICE di
# gmb_agent_runtime, non in `include/`).
INC_ROOTS="native/gmb_runtime_c/include native/gmb_runtime_c/src
native/gmb_chart/include native/gmb_compiler_c/include
native/gmb_db_c/include native/gmb_torch/include
native/gmb_doc_c/include native/gmb_agent_runtime
gmbllm/include gmbllm/include/quant gmbllm/include/train gmbllm/include/lora"
for rel in $INC_ROOTS; do
    [ -d "$ROOT/GMBasic/$rel" ] || continue
    mkdir -p "$STAGE/GMBasic/$(dirname "$rel")"
    cp -R "$ROOT/GMBasic/$rel" "$STAGE/GMBasic/$rel"
done
while IFS= read -r f; do
    rel="${f#$ROOT/}"
    cp_one "$rel" "$STAGE/$rel"
done < <(find "$ROOT/GMBasic/native" -name "libgmb_*.a" -path "*/build/*" 2>/dev/null | sort)

# shared: impostazioni e contratti BASIC (le librerie FFI arrivano da `gmb libs ensure`)
for d in settings include Controls; do cp_one "shared/$d" "$STAGE/shared/$d"; done

# LLMEnv (Apache-2.0): header + ABI + licenza. Se non c'è, `gmb doctor --fix` la installa.
for cand in "$ROOT/../LLMEnv" "$HOME/Documents/Projects/LLMEnv"; do
    [ -d "$cand" ] || continue
    cp_one "${cand#$ROOT/}/ABI" "$STAGE/LLMEnv/ABI" 2>/dev/null || true
    for d in LLMCodec/include LLMJinja LLMEngine/include LLMDesc LLMToken/include; do
        [ -d "$cand/$d" ] && cp_one "$(realpath --relative-to="$ROOT" "$cand/$d" 2>/dev/null || echo "$cand/$d")" "$STAGE/LLMEnv/$d" 2>/dev/null
    done
    for f in LICENSE NOTICE; do [ -f "$cand/$f" ] && cp_one "$(realpath --relative-to="$ROOT" "$cand/$f" 2>/dev/null || echo "$cand/$f")" "$STAGE/LLMEnv/$f"; done
    break
done

# ── 3. manifest con sha256 di ogni file ──────────────────────────────────────────────────────
MAN="$STAGE/release-manifest.json"
{
    printf '{\n  "product": "GMBasic",\n  "version": "%s",\n  "rid": "%s",\n  "built_at": "%s",\n  "files": {\n' \
        "$VERSION" "$RID" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    first=1
    ( cd "$STAGE" && find . -type f ! -name release-manifest.json | sed 's|^\./||' | sort ) | while IFS= read -r rel; do
        sha="$(shasum -a 256 "$STAGE/$rel" | awk '{print $1}')"
        size="$(wc -c < "$STAGE/$rel" | tr -d ' ')"
        [ "$first" = 1 ] || printf ',\n'
        printf '    "%s": {"sha256": "%s", "size": %s}' "$rel" "$sha" "$size"
        first=0
    done
    printf '\n  }\n}\n'
} > "$MAN"

nfiles=$(find "$STAGE" -type f | wc -l | tr -d ' ')
size=$(du -sh "$STAGE" | awk '{print $1}')
say "release: $nfiles file, $size, manifest $MAN"

# ── 4. zip + sha256 ─────────────────────────────────────────────────────────────────────────
if [ "$DO_ZIP" = 1 ]; then
    ( cd "$OUT" && rm -f "gmbasic-$VERSION-$RID.zip" "gmbasic-$VERSION-$RID.zip.sha256" \
        && zip -qry "gmbasic-$VERSION-$RID.zip" "gmbasic-$VERSION-$RID" ) || die "zip fallito"
    ( cd "$OUT" && shasum -a 256 "gmbasic-$VERSION-$RID.zip" > "gmbasic-$VERSION-$RID.zip.sha256" )
    say "release: $OUT/gmbasic-$VERSION-$RID.zip ($(du -h "$OUT/gmbasic-$VERSION-$RID.zip" | awk '{print $1}'))"
fi

say "release: OK"
say "  verifica:  cd <cartella scompattata> && export PATH=\"\$PWD/bin:\$PATH\""
say "             gmb doctor            # deve uscire 0"
say "             gmb doctor --verify   # confronta i file col manifest (sha256)"
say "             gmb eval \"PRINT 40+2\""
exit 0
