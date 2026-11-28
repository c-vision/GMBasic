#!/usr/bin/env bash
# ui_golden_check.sh — GATE dei colori della UI: filtro daltonia + golden image.
#
# Esegue shared/minitests/controls/ui_golden.bas (percorso PIXEL/GRAPHICS) in un
# SDL headless (driver dummy) e verifica DUE cose:
#   A) ogni coppia "normale / filtrata" DEVE differire: se sono uguali il filtro
#      non è arrivato ai pixel e il test non sta provando nulla;
#   B) ognuna delle 4 schermate DEVE corrispondere al golden del RID (pct <= tol).
#
#   ui_golden.<rid>.png        UI (griglia flow=, ruoli del tema)
#   ui_golden_deut.<rid>.png   la stessa con ui.color_filter = deuteranopia
#   ui_palette.<rid>.png       dipinto a colori noti (palette VGA16)
#   ui_palette_deut.<rid>.png  lo stesso con il filtro
#
# Uso: tools/sh/ui_golden_check.sh [--bless] [--tol PCT] [--quiet]
#   --bless     riscrive i golden del RID (solo dopo aver GUARDATO le immagini)
#   --tol PCT   tolleranza gmbimgdiff contro il golden (default 0.0: deterministico)
#
# Build e artefatti in una cartella FUORI dal repo (working tree condiviso).
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SAMPLE="$REPO/shared/minitests/controls/ui_golden.bas"
GOLDEN="$REPO/shared/minitests/controls/golden"
DIFF="$REPO/GMBasic/native/gmb_runtime_c/build/gmbimgdiff"

BLESS=0
TOL="0.0"
QUIET=0
while [ $# -gt 0 ]; do
  case "$1" in
    --bless) BLESS=1; shift ;;
    --tol)   TOL="${2:-0.0}"; shift 2 ;;
    --quiet) QUIET=1; shift ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "ui_golden_check: argomento non riconosciuto: $1" >&2; exit 2 ;;
  esac
done
say() { [ "$QUIET" = "1" ] || echo "$@"; }

# RID del host (i golden sono per RID: la resa del testo/pixel può cambiare).
os="$(uname -s)"; arch="$(uname -m)"
case "$os:$arch" in
  Darwin:arm64)  RID="osx-arm64" ;;
  Darwin:x86_64) RID="osx-x64" ;;
  Linux:x86_64)  RID="linux-x64" ;;
  Linux:aarch64) RID="linux-arm64" ;;
  *) echo "ui_golden_check: RID non riconosciuto ($os/$arch)" >&2; exit 2 ;;
esac

[ -f "$SAMPLE" ] || { echo "ui_golden_check: manca $SAMPLE" >&2; exit 1; }
[ -x "$DIFF" ] || cmake --build "$REPO/GMBasic/native/gmb_runtime_c/build" --target gmbimgdiff -j4 >/dev/null 2>&1
[ -x "$DIFF" ] || { echo "ui_golden_check: gmbimgdiff non disponibile" >&2; exit 1; }

WORK="${TMPDIR:-/tmp}/gmb-ui-golden-$RID"
rm -rf "$WORK"
mkdir -p "$WORK/work" "$GOLDEN" || exit 1
cp "$SAMPLE" "$WORK/ui_golden.bas" || exit 1

# Esecuzione headless: SAVE SCREEN scrive in work/ relativo alla CWD del programma.
( cd "$WORK" && SDL_VIDEODRIVER=dummy bash "$REPO/tools/sh/gmb_c99native_run.sh" \
    "$WORK/ui_golden.bas" uigold "" ) >"$WORK/run.log" 2>&1
rc=$?
for f in ui_golden ui_golden_deut ui_palette ui_palette_deut; do
  if [ ! -s "$WORK/work/$f.png" ]; then
    echo "ROSSO  $f: screenshot non prodotta (rc=$rc, vedi $WORK/run.log)" >&2
    tail -8 "$WORK/run.log" >&2
    exit 1
  fi
done
say "esecuzione ok (RID $RID): 4 schermate in $WORK/work"

fail=0
# A) le coppie devono DIFFERIRE (se sono identiche il filtro non agisce).
for f in ui_golden ui_palette; do
  if "$DIFF" "$WORK/work/$f.png" "$WORK/work/${f}_deut.png" >/dev/null 2>&1; then
    echo "ROSSO  $f: filtrata IDENTICA alla normale (ui.color_filter non arriva ai pixel)"
    fail=1
  else
    say "OK     $f: la schermata filtrata differisce da quella normale"
  fi
done

# B) confronto con il golden del RID.
for f in ui_golden ui_golden_deut ui_palette ui_palette_deut; do
  g="$GOLDEN/$f.$RID.png"
  if [ "$BLESS" = "1" ]; then
    cp "$WORK/work/$f.png" "$g" || { echo "ROSSO  $f: bless fallito" >&2; fail=1; continue; }
    say "BLESS  $f -> $g"
    continue
  fi
  if [ ! -f "$g" ]; then
    echo "ROSSO  $f: manca il golden $g (generalo con --bless e GUARDALO)"
    fail=1
    continue
  fi
  if out="$("$DIFF" "$WORK/work/$f.png" "$g" "$TOL" 2>&1)"; then
    say "OK     $f: $out"
  else
    echo "ROSSO  $f: $out"
    fail=1
  fi
done

[ "$fail" = "0" ] && say "ui_golden_check: VERDE (RID $RID)"
exit $fail
