#!/bin/bash
# console_tui.sh — apre la console TUI di controllo del ServiceConsole.
# NB: la TUI (WINDOW/finestre) RENDE sul backend IL (test_ilrun), non sul VM host.
#
# CWD = shared/ (la cartella che contiene APPS/): così l'app trova
# APPS/ServiceConsole/serviceconsole.cfg con la sua risalita da IO.CWD().
# Gli strumenti del monorepo si risolvono dalla ROOT (tools/runners/…).
cd "$(dirname "$0")/../.." || exit 1        # → shared/
ROOT="$(cd ../.. && pwd)"                   # → GMBasicMonoRepo/
DLL="$ROOT/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll"
if [ -f "$DLL" ]; then
  exec dotnet "$DLL" APPS/ServiceConsole/src/ServiceConsoleTui.bas
else
  exec dotnet run --project "$ROOT/tools/runners/test_ilrun" -- APPS/ServiceConsole/src/ServiceConsoleTui.bas
fi
