#!/bin/bash
# console_tui.sh — apre la console TUI di controllo del ServiceConsole.
# NB: la TUI (WINDOW/finestre) RENDE sul backend IL (test_ilrun), non sul VM host.
cd "$(dirname "$0")/../.." || exit 1
DLL="$PWD/test_ilrun/bin/Debug/net10.0/test_ilrun.dll"
if [ -f "$DLL" ]; then
  exec dotnet "$DLL" APPS/ServiceConsole/src/ServiceConsoleTui.bas
else
  exec dotnet run --project test_ilrun -- APPS/ServiceConsole/src/ServiceConsoleTui.bas
fi
