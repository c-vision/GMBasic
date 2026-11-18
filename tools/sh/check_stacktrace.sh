#!/bin/bash
# check_stacktrace.sh — verifica lo STACK TRACE BASIC sugli errori FATALI (non catturati).
#
# Cosa controlla, su tutte e 4 le gambe: un errore in profondità (Main → B → C) stampa su
# stderr la catena delle procedure, dalla più interna alla più esterna:
#
#     [GMBASIC] ERROR 11: boom (line 2)
#       in C (riga 2)
#       in B (riga 6)
#       in Main (riga 10)
#
#   1. C99Native  — gmb_c99native_run.sh
#   2. VM/C       — chunk NATIVO (gmbchunk) + gmb_vm_host: nomi BASIC, non @sym
#   3. PCodeVM    — tools/runners/test_ilrun --engine=pcode
#   4. IL         — tools/runners/test_ilrun --engine=il
#
# Perché un gate e non un MiniTest: il trace sta su stderr e dipende dai numeri di riga del
# sorgente, mentre l'oracolo dei MiniTest confronta lo stdout. Qui si verificano forma e nomi.
#
# Uso: tools/sh/check_stacktrace.sh [--quiet] [--native-only]
#   --native-only  solo C99Native + VM/C (le due gambe native: veloce, senza dotnet)
# Exit: 0 = ok · 1 = guasto · 2 = setup
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
QUIET=0
NATIVE_ONLY=0
for a in "$@"; do
  case "$a" in
    --quiet) QUIET=1 ;;
    --native-only) NATIVE_ONLY=1 ;;
    *) echo "check_stacktrace: opzione sconosciuta $a" >&2; exit 2 ;;
  esac
done

RUN="$ROOT/tools/sh/gmb_c99native_run.sh"
CHUNK="$ROOT/GMBasic/native/gmb_compiler_c/build/gmbchunk"
VMHOST="$ROOT/GMBasic/native/gmb_vm_c/build/gmb_vm_host"
HARNESS="$ROOT/tools/runners/test_ilrun"
[ -x "$RUN" ] || { echo "check_stacktrace: manca $RUN" >&2; exit 2; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/gmb_stacktrace_check.XXXXXX") || exit 2
trap 'rm -rf "$WORK"' EXIT

# Programma con tre livelli di chiamata: Main → B → C, errore in C.
cat > "$WORK/st.bas" <<'EOF'
SUB C()
    ERROR 11, "boom"
END SUB

SUB B()
    C()
END SUB

SUB Main()
    B()
END SUB
EOF

fail=0
WANT=("in C (riga " "in B (riga " "in Main (riga ")

# Verifica i frame attesi nel file di stderr indicato. $1 = gamba, $2 = file.
check_frames() {
  local leg="$1" f="$2" want
  for want in "${WANT[@]}"; do
    if ! grep -qF "$want" "$f"; then
      echo "FAIL ($leg): manca il frame '$want'" >&2
      sed -n '1,10p' "$f" | sed 's/^/  /' >&2
      fail=1
    fi
  done
}

# ── 1. C99Native ────────────────────────────────────────────────────────────────────────
( cd "$ROOT" && bash "$RUN" "$WORK/st.bas" c99 ) > "$WORK/c99.out" 2> "$WORK/c99.err" || true
check_frames C99Native "$WORK/c99.err"

# ── 2. VM/C (chunk NATIVO, nomi BASIC) ─────────────────────────────────────────────────
if [ -x "$CHUNK" ] && [ -x "$VMHOST" ]; then
  if "$CHUNK" "$WORK/st.bas" "$WORK/st.chunk" > "$WORK/chunk.log" 2>&1; then
    "$VMHOST" "$WORK/st.chunk" > "$WORK/vmc.out" 2> "$WORK/vmc.err" || true
    check_frames "VM/C" "$WORK/vmc.err"
  else
    echo "SKIP (VM/C): gmbchunk non ha compilato il probe" >&2
  fi
else
  echo "SKIP (VM/C): gmbchunk/gmb_vm_host non costruiti" >&2
fi

# ── 3/4. PCodeVM e IL (harness gestito) ────────────────────────────────────────────────
if [ "$NATIVE_ONLY" -eq 0 ]; then
  if [ -d "$HARNESS" ] && command -v dotnet >/dev/null 2>&1; then
    for leg in pcode il; do
      ( cd "$WORK" && dotnet run --project "$HARNESS" -- --engine=$leg st.bas ) \
        > "$WORK/$leg.out" 2> "$WORK/$leg.err" || true
      check_frames "$leg" "$WORK/$leg.err"
    done
  else
    echo "SKIP (PCode/IL): harness o dotnet non disponibili" >&2
  fi
fi

[ "$fail" -eq 0 ] || exit 1
if [ "$QUIET" -eq 0 ]; then
  echo "check_stacktrace: OK (catena Main → B → C sulle gambe verificate)"
  # Solo le righe del trace: il runner può emettere altra diagnostica su stderr.
  grep -E '^\[GMBASIC\] ERROR|^  in ' "$WORK/c99.err" | sed 's/^/  /'
fi
exit 0
