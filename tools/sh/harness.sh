#!/bin/zsh
# harness.sh — esegue UN file .bas sulle gambe gestite (IL/PCode) o VM/C.
# Uso: tools/sh/harness.sh <il|pcode|vmc|ast> <file.bas>
# Env: GMB_TUI_SCRIPT=$'a\rb\r' per input scriptato dei test interattivi.
set -e
if [ $# -lt 2 ]; then echo "uso: harness.sh <il|pcode|vmc|ast> <file.bas>" >&2; exit 2; fi
MODE=$1; FILE=$2
ROOT="$(cd "$(dirname "$0")/../../GMBasicSharp" && pwd)"
WS="$(cd "$ROOT/.." && pwd)"
export GMBASIC_ROOT="$WS/GMBasic"
if [ "$(uname)" = "Darwin" ]; then
  export DYLD_LIBRARY_PATH="$HOME/.gmbasic/libs/ml/osx-arm64:$HOME/.gmbasic/libs/osx-arm64:${DYLD_LIBRARY_PATH:-}"
fi
case $MODE in
  il)    exec dotnet "$WS/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il "$FILE" ;;
  pcode) exec dotnet "$WS/tools/runners/test_pcode/bin/Debug/net10.0/test_pcode.dll" "$FILE" ;;
  vmc)   exec dotnet "$WS/tools/runners/test_vmrun/bin/Debug/net10.0/test_vmrun.dll" "$FILE" ;;
  ast)   exec dotnet "$WS/tools/runners/test_astdump/bin/Debug/net10.0/test_astdump.dll" "$FILE" ;;
  *) echo "modo sconosciuto: $MODE" >&2; exit 2 ;;
esac
