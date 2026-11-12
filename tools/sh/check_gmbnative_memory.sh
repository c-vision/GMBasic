#!/usr/bin/env bash
# check_gmbnative_memory.sh — verifica di MEMORIA del compilatore nativo (gmbnative).
#
# COSA FA. Ricostruisce il driver `gmbnative` con AddressSanitizer in un BUILD TREE SEPARATO
# (default: $TMPDIR/gmb_asan_gmbnative) e compila (solo emissione, `--no-exe`) ogni `.bas`
# richiesto, cercando due classi di difetto:
#   * rapporti di AddressSanitizer (heap/stack overflow, use-after-free, ...);
#   * abort del processo (rc >= 128, in particolare 134 = SIGABRT), che su macOS è tipicamente
#     libmalloc («pointer being freed was not allocated»): firma di un free() di un puntatore
#     non allocato (es. un buffer `static` restituito come errore e poi liberato dal chiamante
#     — caso reale `gmb_parser_core.c`, LET a livello di modulo, 2026-11-12).
#
# PERCHÉ SERVE. Il crash (GC del lato managed, oppure abort del driver) arriva MOLTO DOPO la
# scrittura sbagliata, in un punto non correlato: senza ASan la diagnosi è cieca. ASan +
# cattura dell'abort lo rende riproducibile FILE PER FILE.
#
# USO.
#   tools/sh/check_gmbnative_memory.sh                      # tutti i MiniTest (~2377 file, ~2 min)
#   tools/sh/check_gmbnative_memory.sh constructs/ arrays/  # solo i file che contengono uno
#                                                           # dei filtri (sottostringa)
#   tools/sh/check_gmbnative_memory.sh -j 6                 # parallelismo (default 4)
#   KEEP=1 tools/sh/check_gmbnative_memory.sh …             # conserva log e binario ASan
#
# ESITO: exit 0 = nessun difetto; exit 1 = almeno un file con ASan o abort (elencati).
#
# NOTA. Il driver nasce in `<repo>/bin/gmbnative` (CMake): la build ASan lo SOVRASCRIVEREBBE.
# Lo script mette da parte il binario normale, copia l'ASan in una dir temporanea e RIMETTE
# subito il normale al suo posto: il tree condiviso non resta mai con un binario strumentato.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/GMBasic/native/gmb_compiler_c"
TREE="${TMPDIR:-/tmp}/gmb_asan_gmbnative"
OUT="${TMPDIR:-/tmp}/gmb_asan_gmbnative_out"
J=4
FILTERS=()
while [ $# -gt 0 ]; do
  case "$1" in
    -j) J="${2:?numero}"; shift 2 ;;
    -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
    *) FILTERS+=("$1"); shift ;;
  esac
done

[ -d "$SRC" ] || { echo "sorgenti assenti: $SRC" >&2; exit 2; }
mkdir -p "$OUT"

# ── build ASan senza lasciare `bin/gmbnative` strumentato ──────────────────────────────────
NORMAL="$ROOT/bin/gmbnative"
BK="$OUT/gmbnative.normal.bak"
if [ -x "$NORMAL" ]; then cp -p "$NORMAL" "$BK"; fi
restore_normal() {
  if [ -x "$BK" ]; then cp -p "$BK" "$NORMAL"; fi
}
trap restore_normal EXIT

echo "[asan] configure -> $TREE"
cmake -S "$SRC" -B "$TREE" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" > "$OUT/configure.log" 2>&1 \
  || { echo "configure fallita (vedi $OUT/configure.log)" >&2; exit 2; }

echo "[asan] build gmbnative"
cmake --build "$TREE" --target gmbnative -j"$J" > "$OUT/build.log" 2>&1 \
  || { echo "build fallita (vedi $OUT/build.log)" >&2; exit 2; }

DRV="$OUT/gmbnative-asan"
if [ -x "$NORMAL" ]; then cp -p "$NORMAL" "$DRV"; fi   # appena installato dalla build ASan
restore_normal                                          # il tree torna subito pulito
if ! { otool -L "$DRV" 2>/dev/null | grep -q asan || nm "$DRV" 2>/dev/null | grep -q __asan; }; then
  echo "errore: $DRV non è strumentato con ASan (build inattesa)" >&2; exit 2
fi
echo "[asan] driver: $DRV"

# ── selezione file ────────────────────────────────────────────────────────────────────────
LIST="$OUT/files.txt"
find "$ROOT/shared/minitests" -name '*.bas' | sort > "$LIST"
if [ ${#FILTERS[@]} -gt 0 ]; then
  : > "$LIST.f"
  for f in "${FILTERS[@]}"; do grep -F "$f" "$LIST" >> "$LIST.f" || true; done
  sort -u "$LIST.f" > "$LIST"; rm -f "$LIST.f"
fi
N=$(wc -l < "$LIST" | tr -d ' ')
[ "$N" -gt 0 ] || { echo "nessun file selezionato" >&2; exit 2; }
echo "[asan] $N file da compilare (-j $J)"

: > "$OUT/asan.txt"; : > "$OUT/abort.txt"
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0"
export DRV OUT ROOT
compile_one() {
  f="$1"
  base=$(printf '%s' "$f" | sed "s|$ROOT/||; s|/|_|g; s|\.bas$||")
  log="$OUT/$base.log"
  "$DRV" "$f" "$OUT/$base.c" --no-exe > "$log" 2>&1
  rc=$?
  if grep -q "AddressSanitizer" "$log"; then echo "$f rc=$rc" >> "$OUT/asan.txt"; fi
  if [ "$rc" -ge 128 ] || [ "$rc" -eq 6 ]; then echo "$f rc=$rc" >> "$OUT/abort.txt"; fi
  rm -f "$OUT/$base.c"
}
export -f compile_one
xargs -P "$J" -I{} bash -c 'compile_one "$@"' _ {} < "$LIST"

NA=$(wc -l < "$OUT/asan.txt" | tr -d ' ')
NB=$(wc -l < "$OUT/abort.txt" | tr -d ' ')
echo "[asan] AddressSanitizer: $NA   abort: $NB"
[ "$NA" -gt 0 ] && { echo "--- ASan:"; cat "$OUT/asan.txt"; }
[ "$NB" -gt 0 ] && { echo "--- abort:"; cat "$OUT/abort.txt"; }
[ "$NA" -eq 0 ] && [ "$NB" -eq 0 ] || exit 1
echo "[asan] OK: nessun difetto di memoria"
