#!/usr/bin/env bash
# check_exitcode_parity.sh — verifica che FUNCTION Main() AS INTEGER diventi l'exit code del
# processo sulle 4 gambe: IL, PCode, VM/C (chunk C#), C99Native. Atteso: 7.
set -u
ROOT="$(cd "$(dirname "$0")/../../GMBasicSharp" && pwd)"
# I TOOL vivono nella radice del MONOREPO (`tools/`), non più in GMBasic/tools.
MONO="$(cd "$(dirname "$0")/../.." && pwd)"
GB="$(cd "$ROOT/.." && pwd)/GMBasic"
F="$MONO/tools/fixtures/exitcode_main.bas"
export SDL_VIDEODRIVER=dummy
export DYLD_LIBRARY_PATH="$HOME/.gmbasic/libs/osx-arm64:$GB/build/libs/osx-arm64"
FAIL=0
chk() { local n="$1" want="$2" got="$3"; if [ "$got" = "$want" ]; then echo "  PASS $n ($got)"; else echo "  FAIL $n: got $got want $want"; FAIL=1; fi; }
dotnet "$ROOT/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il "$F" >/dev/null 2>&1; chk IL 7 $?
dotnet "$ROOT/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=pcode "$F" >/dev/null 2>&1; chk PCode 7 $?
dotnet "$ROOT/test_vmrun/bin/Debug/net10.0/test_vmrun.dll" "$F" >/dev/null 2>&1; chk VM/C 7 $?
bash "$GB/tools/sh/gmb_c99native_run.sh" "$F" >/dev/null 2>&1; chk C99Native 7 $?
[ $FAIL -eq 0 ] && echo "EXITCODE PARITY OK" || { echo "EXITCODE PARITY FAIL"; exit 1; }
