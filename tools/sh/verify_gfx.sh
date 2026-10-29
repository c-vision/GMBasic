#!/usr/bin/env bash
# verify_gfx.sh — verifica END-TO-END in modalita GRAFICA (headless SDL dummy) dei
# controlli CONTROLS e di Clipper @SAY/@GET/@READ, sulle 4 gambe vive del monorepo:
#   IL (test_ilrun --gfx, motore C), PCode, VM/C (test_vmrun), C99Native (gmb_c99native_run).
# Uso: tools/sh/verify_gfx.sh [controlli|clipper|tutti]
set -u
ROOT="$(cd "$(dirname "$0")/../../GMBasicSharp" && pwd)"
MONO="$(cd "$ROOT/.." && pwd)"
GB="$MONO/GMBasic"
FIX="$ROOT/tools/fixtures"
WHAT="${1:-tutti}"
export SDL_VIDEODRIVER=dummy
export GMB_FORM_AUTOKEY="${GMB_FORM_AUTOKEY:-0,ab;30,ENTER;60,25;90,ENTER}"
unset GMB_TUI_SCRIPT 2>/dev/null || true
export DYLD_LIBRARY_PATH="$HOME/.gmbasic/libs/osx-arm64:$GB/build/libs/osx-arm64"

FAIL=0

run_to() { # run_to <secs> <file> <marker> <cmd...>
  local secs=$1 file=$2 marker=$3; shift 3
  ( "$@" ) >"$file" 2>&1 & local p=$!
  for _ in $(seq 1 $((secs*2))); do kill -0 "$p" 2>/dev/null || break; sleep 0.5; done
  kill -9 "$p" 2>/dev/null
  pkill -f "gmb_vm_host" 2>/dev/null; pkill -f "gmb_c99native_driver" 2>/dev/null
  if rg -q -- "$marker" "$file"; then echo "  PASS ($marker)"; else echo "  FAIL (atteso: $marker) — log: $file"; FAIL=1; fi
}

one() { # one <nome> <file> <marker>
  local name=$1 f=$2 marker=$3
  echo "== $name"
  run_to 40 "/tmp/verify_gfx_${name}_il.log"   "$marker" dotnet "$ROOT/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il --gfx "$f"
  run_to 40 "/tmp/verify_gfx_${name}_pc.log"   "$marker" dotnet "$ROOT/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=pcode "$f"
  # compilatore C# → chunk → VM/C
  run_to 60 "/tmp/verify_gfx_${name}_vmc.log"  "$marker" dotnet "$ROOT/test_vmrun/bin/Debug/net10.0/test_vmrun.dll" "$f"
  # compilatore C nativo → chunk → VM/C (stesso binario host)
  run_to 60 "/tmp/verify_gfx_${name}_vmcC.log" "$marker" bash -c "\"$GB/native/gmb_compiler_c/build/gmb_chunk_driver\" \"$f\" /tmp/vg_$name.chunk >/dev/null && \"$GB/native/gmb_vm_c/build/gmb_vm_host\" /tmp/vg_$name.chunk"
  # compilatore C nativo → C → eseguibile (C99Native)
  run_to 60 "/tmp/verify_gfx_${name}_c99.log"  "$marker" bash "$GB/tools/sh/gmb_c99native_run.sh" "$f"
}

[ "$WHAT" = controlli ] || [ "$WHAT" = tutti ] && one controlli "$FIX/gfx_controls.bas" "gfx-ctls-ok"
[ "$WHAT" = clipper ]   || [ "$WHAT" = tutti ] && one clipper   "$FIX/gfx_clipper_read.bas" "n=\[ab\] q= 25"

[ $FAIL -eq 0 ] && echo "TUTTE LE GAMBE PASS" || { echo "FALLIMENTI"; exit 1; }
