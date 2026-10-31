#!/usr/bin/env bash
# check_basic_surface_parity.sh — protocollo 4.1 per la superficie BASIC modificata:
# operatori logici/bitwise, DOEVENTS, FUNCTION Main exit code. Confronta i risultati
# delle 4 gambe (IL, PCode, VM/C, C99Native) e verifica i parametri con i MiniTest.
set -u
ROOT="$(cd "$(dirname "$0")/../../GMBasicSharp" && pwd)"
# I TOOL vivono nella radice del MONOREPO (`tools/`), non più in GMBasic/tools.
MONO="$(cd "$(dirname "$0")/../.." && pwd)"
GB="$(cd "$ROOT/.." && pwd)/GMBasic"
export SDL_VIDEODRIVER=dummy
export DYLD_LIBRARY_PATH="$HOME/.gmbasic/libs/osx-arm64:$GB/build/libs/osx-arm64"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
# `[c99native]` = righe di LOG del runner C (non output del programma): senza il filtro la gamba
# C99 risultava 'diversa' su OGNI test (il gate, prima del fix del path, non la eseguiva affatto).
norm() { sed -E '/^\[IL dump\]/d; /^\[OK\]/d; /^\[gmb_gfx\]/d; /^\[c99native\]/d; /^  exit=/d; /^exit=/d; /^---- /d; /^$/d'; }
run_il()  { dotnet "$MONO/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il --gfx "$1" 2>/dev/null | norm; }
run_pc()  { dotnet "$MONO/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=pcode "$1" 2>/dev/null | norm; }
run_vmc() { "$GB/native/gmb_compiler_c/build/gmb_chunk_driver" "$1" "$TMP/x.chunk" >/dev/null 2>&1 && "$GB/native/gmb_vm_c/build/gmb_vm_host" "$TMP/x.chunk" 2>/dev/null | norm; }
run_c99() { bash "$MONO/tools/sh/gmb_c99native_run.sh" "$1" 2>/dev/null | norm; }
FAIL=0
parity() {
  local n="$1" f="$2"
  run_il "$f" > "$TMP/il"; run_pc "$f" > "$TMP/pc"; run_vmc "$f" > "$TMP/vm"; run_c99 "$f" > "$TMP/c99"
  if [ ! -s "$TMP/il" ]; then echo "  FAIL $n: output vuoto"; FAIL=1; return; fi
  if diff -q "$TMP/il" "$TMP/pc" >/dev/null && diff -q "$TMP/il" "$TMP/vm" >/dev/null && diff -q "$TMP/il" "$TMP/c99" >/dev/null; then
    echo "  PASS $n (4/4 identici su stdout)"; else echo "  FAIL $n: divergenza fra gambe"; diff -u "$TMP/il" "$TMP/vm" | head -20; FAIL=1; fi
}
parity "operators_logical_bitwise" "$GB/../shared/minitests/system/operators_logical_bitwise.bas"
parity "doevents_parity" "$GB/../shared/minitests/graphics/doevents_parity.bas"
parity "exit_proc_semantics" "$GB/../shared/minitests/constructs/exit_proc_semantics.bas"
parity "print_paren_expr_sep" "$GB/../shared/minitests/constructs/print_paren_expr_sep.bas"
parity "named_tuple_field_access" "$GB/../shared/minitests/types/named_tuple_field_access.bas"
parity "OperatorsDoEventsDemo" "$GB/../shared/samples/Types/OperatorsDoEventsDemo.bas"
# Exit code: FUNCTION Main() AS INTEGER -> 7 su tutte le gambe.
F="$MONO/tools/fixtures/exitcode_main.bas"
chk() { local n="$1" got="$2"; [ "$got" = 7 ] && echo "  PASS $n exit=7" || { echo "  FAIL $n exit=$got"; FAIL=1; }; }
dotnet "$MONO/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il "$F" >/dev/null 2>&1; chk IL $?
dotnet "$MONO/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=pcode "$F" >/dev/null 2>&1; chk PCode $?
"$GB/native/gmb_compiler_c/build/gmb_chunk_driver" "$F" "$TMP/e.chunk" >/dev/null 2>&1; "$GB/native/gmb_vm_c/build/gmb_vm_host" "$TMP/e.chunk" >/dev/null 2>&1; chk VM/C $?
bash "$MONO/tools/sh/gmb_c99native_run.sh" "$F" >/dev/null 2>&1; chk C99Native $?
[ $FAIL -eq 0 ] && echo "BASIC SURFACE PARITY 4/4 OK" || { echo "BASIC SURFACE PARITY FAIL"; exit 1; }
