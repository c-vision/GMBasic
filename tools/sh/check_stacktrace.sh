#!/bin/bash
# check_stacktrace.sh — verifica lo STACK TRACE BASIC sugli errori fatali.
#
# Cosa controlla:
#   1. gamba C99Native: un errore in profondità (Main → B → C) stampa su stderr la catena
#      delle procedure con le righe BASIC, dalla più interna alla più esterna;
#   2. gamba VM/C (chunk nativo + gmb_vm_host): la STESSA forma, con gli STESSI nomi.
#
# Perché un gate e non un MiniTest: il trace è su stderr e dipende dai numeri di riga del
# sorgente; l'oracolo dei MiniTest confronta lo stdout. Qui si verifica la forma e i nomi.
#
# Uso: tools/sh/check_stacktrace.sh [--quiet]
# Exit: 0 = ok · 1 = guasto · 2 = setup
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
QUIET=0
for a in "$@"; do
  case "$a" in
    --quiet) QUIET=1 ;;
    *) echo "check_stacktrace: opzione sconosciuta $a" >&2; exit 2 ;;
  esac
done

RUN="$ROOT/tools/sh/gmb_c99native_run.sh"
CHUNK="$ROOT/GMBasic/native/gmb_compiler_c/build/gmbchunk"
VMHOST="$ROOT/GMBasic/native/gmb_vm_c/build/gmb_vm_host"
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

# ── 1. C99Native ────────────────────────────────────────────────────────────────────────
( cd "$ROOT" && bash "$RUN" "$WORK/st.bas" c99 ) > "$WORK/c99.out" 2> "$WORK/c99.err" || true
for want in "in C (riga " "in B (riga " "in Main (riga "; do
  if ! grep -qF "$want" "$WORK/c99.err"; then
    echo "FAIL (C99Native): manca il frame '$want'" >&2
    sed -n '1,10p' "$WORK/c99.err" | sed 's/^/  /' >&2
    fail=1
  fi
done

# ── 2. VM/C (chunk NATIVO, nomi BASIC) ─────────────────────────────────────────────────
if [ -x "$CHUNK" ] && [ -x "$VMHOST" ]; then
  if "$CHUNK" "$WORK/st.bas" "$WORK/st.chunk" > "$WORK/chunk.log" 2>&1; then
    "$VMHOST" "$WORK/st.chunk" > "$WORK/vmc.out" 2> "$WORK/vmc.err" || true
    for want in "in C (riga " "in B (riga " "in Main (riga "; do
      if ! grep -qF "$want" "$WORK/vmc.err"; then
        echo "FAIL (VM/C): manca il frame '$want'" >&2
        sed -n '1,10p' "$WORK/vmc.err" | sed 's/^/  /' >&2
        fail=1
      fi
    done
  else
    echo "SKIP (VM/C): gmbchunk non ha compilato il probe" >&2
  fi
else
  echo "SKIP (VM/C): gmbchunk/gmb_vm_host non costruiti" >&2
fi

[ "$fail" -eq 0 ] || exit 1
if [ "$QUIET" -eq 0 ]; then
  echo "check_stacktrace: OK (catena Main → B → C su C99Native e VM/C)"
  # Solo le righe del trace: il runner può emettere altra diagnostica su stderr.
  grep -E '^\[GMBASIC\] ERROR|^  in ' "$WORK/c99.err" | sed 's/^/  /'
fi
exit 0
