#!/bin/bash
# console_gui.sh — console TUI di ServiceConsole su C99Native, in questo Terminale.
# Tasti: a/f avvia|ferma tutti · s/t/r stop|start|restart selezionato · 1-8 selezione
#        q o ESC = esci e chiudi la finestra.
#
# CWD = shared/ (la cartella che contiene Apps/): l'app trova
# Apps/ServiceConsole/serviceconsole.cfg; lo script di build sta nella ROOT (tools/sh/).
cd "$(dirname "$0")/../.." || exit 1        # → shared/
ROOT="$(cd ../.. && pwd)"                   # → GMBasicMonoRepo/
echo "Avvio console (compilazione C99Native, ~1-2 min)..."
bash "$ROOT/tools/sh/gmb_c99native_run.sh" ../shared/Apps/ServiceConsole/src/ServiceConsole.bas gui
# Chiusura finestra Terminal all'uscita (ESC/q).
osascript -e 'tell application "Terminal" to close (every window whose name contains "console_gui")' >/dev/null 2>&1 &
exit 0
