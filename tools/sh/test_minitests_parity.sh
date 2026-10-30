#!/bin/zsh
# test_minitests_parity — esecuzione MIRATA della parity MiniTests (4 gambe: IL, PCode, VM-C,
# C99Native) nel monorepo. Le gambe sono SEMPRE 4: il backend C# C99 non esiste piu'.
# Le prime 3 girano nell'harness xUnit, la 4a nel runner nativo (tools/sh/test_minitests_c99native).
# Uso: tools/sh/test_minitests_parity.sh <sottostringa> [<altra> ...] | --all | --interactive [filtri]
# I filtri sono una LISTA (match ANY) su TUTTE e 4 le gambe: gli argomenti vengono uniti con
# virgola in GMB_MINI_ONLY e sia l'xUnit sia il runner nativo la leggono come lista. Un filtro
# che non corrisponde ad alcun file fa FALLIRE la gamba nativa (exit 1): prima la gamba
# stampava `PASS=0 FAIL=0` con exit 0, cioè un falso verde.
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

# RID della macchina (per i path delle librerie native).
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) RID=osx-arm64 ;;
  Darwin-x86_64) RID=osx-x64 ;;
  Linux-x86_64) RID=linux-x64 ;;
  Linux-aarch64|Linux-arm64) RID=linux-arm64 ;;
  *) RID=osx-arm64 ;;
esac
# ORDINE = il codice che si sta testando PRIMA della cache: `build/libs/<rid>` sono le librerie
# appena costruite (AI/engine comprese), `~/.gmbasic/libs` è la cache dei bundle PUBBLICATI.
# Con la cache davanti, una lib pubblicata più vecchia dei sorgenti veniva caricata in SILENZIO
# dalle gambe IL/PCode mentre VM/C e C99 (link statici) usavano il build locale → divergenze
# fantasma su llm/* (caso reale 2026-10-29: `LLM TENSOR INFO` con `arch=` vuoto su IL/PCode e
# `arch=qwen3_moe` su VM/C; con build/libs davanti: PASS su tutte le gambe).
if [ "$(uname)" = "Darwin" ]; then
  export DYLD_LIBRARY_PATH="$WS/GMBasic/build/libs/$RID:$HOME/.gmbasic/libs/ml/$RID:$HOME/.gmbasic/libs/$RID:${DYLD_LIBRARY_PATH:-}"
fi
export GMBASIC_ROOT="$WS/GMBasic"

dotnet test "$ROOT/tests/GMBASIC.IlBackend.Tests" \
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
