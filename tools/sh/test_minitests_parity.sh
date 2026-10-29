#!/bin/zsh
# test_minitests_parity — esecuzione MIRATA della parity MiniTests (4 gambe: IL, PCode, VM-C,
# C99Native) nel monorepo. Le gambe sono SEMPRE 4: il backend C# C99 non esiste piu'.
# Le prime 3 girano nell'harness xUnit, la 4a nel runner nativo (tools/sh/test_minitests_c99native).
# Uso: tools/sh/test_minitests_parity.sh <sottostringa> [<altra> ...] | --all | --interactive [filtri]
set -e
ROOT="$(cd "$(dirname "$0")/../../GMBasicSharp" && pwd)"          # GMBasicSharp
WS="$(cd "$ROOT/.." && pwd)"                       # GMBasicMonoRepo

unset GMB_MINI_ONLY GMB_RUN_ALL
case "$1" in
  --all) export GMB_RUN_ALL=1 ;;
  --interactive) export GMB_RUN_ALL=1; export GMB_RUN_INTERACTIVE=1 ;;
  "") echo "uso: test_minitests_parity.sh <sottostringa> [...] | --all | --interactive" >&2; exit 2 ;;
  *) export GMB_MINI_ONLY="${(j:,:)@}" ;;
esac

flags="--no-build"
[ "${GMB_MINITEST_BUILD:-0}" = "1" ] && flags=""

if [ "$(uname)" = "Darwin" ]; then
  export DYLD_LIBRARY_PATH="$HOME/.gmbasic/libs/ml/osx-arm64:$HOME/.gmbasic/libs/osx-arm64:${DYLD_LIBRARY_PATH:-}"
fi
export GMBASIC_ROOT="$WS/GMBasic"

dotnet test "$ROOT/tests-mixed/GMBASIC.IlBackend.Tests" \
  -p:GmbEdition=Commercial \
  --filter "FullyQualifiedName~MiniTests_IL_PCode_Parity" \
  $flags

# --- 4a gamba: C99Native (runner nativo) — SEMPRE, stesso filtro (o suite intera con --all) --
if [ -n "${GMB_MINI_ONLY:-}" ]; then
  echo "gambe: IL + PCode + VM-C (xUnit) + C99Native (runner nativo)  [4/4]"
  "$WS/tools/sh/test_minitests_c99native"
else
  echo "gambe: IL + PCode + VM-C (xUnit) + C99Native (runner nativo, suite intera)  [4/4]"
  ( unset GMB_MINI_ONLY; "$WS/tools/sh/test_minitests_c99native" )
fi
