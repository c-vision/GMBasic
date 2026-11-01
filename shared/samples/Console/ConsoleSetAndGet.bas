IMPORT WINDOWS
' ==============================================================================
' FILE: Console/ConsoleSetAndGet.bas
' SET legacy Clipper + funzioni GET della console TUI (namespace CONSOLE)
' ==============================================================================
' SET (stato Clipper, persistente per la sessione):
'   SET WRAP ON/OFF            — tronca o va a capo l'output lungo
'   SET DELIMITERS TO ",;"     — delimitatori di lista per i PRINT multipli
'   SET ESCAPE ON/OFF          — gestione sequenze ESC
'   SET TYPEAHEAD n            — dimensione buffer tastiera
'   SET INTENSITY ON/OFF       — testo intenso
'   SET EPOCH anno             — cutoff anni a 2 cifre per CTOD
'   SET DECIMALS n             — decimali di default
'   SET FIXED ON/OFF           — numero fisso di decimali
'   SET PRINTER ON/OFF         — output verso la stampante
'   SET DEVICE TO PRINTER|SCREEN — destinazione output
'   SET MESSAGE TO riga        — riga messaggi (0 = off)
' Funzioni:
'   WINDOW SET TITLE "titolo"  — titolo finestra
'   CONSOLE.GETCURSORPOS()     — [col, riga] cursore
'   CONSOLE.GETTEXTMETRICS()   — [larghezza, altezza] finestra
'   CONSOLE.GETPALETTECOLOR(i) — [r, g, b] colore palette i
' ==============================================================================

PRINT "=== SET legacy Clipper ==="
SET WRAP ON
PRINT "SET WRAP ON: OK"
SET DELIMITERS TO ",;"
PRINT "SET DELIMITERS TO virgola-punto: OK"
SET ESCAPE ON
PRINT "SET ESCAPE ON: OK"
SET TYPEAHEAD 32
PRINT "SET TYPEAHEAD 32: OK"
SET INTENSITY ON
PRINT "SET INTENSITY ON: OK"
SET EPOCH 1950
PRINT "SET EPOCH 1950: OK"
SET DECIMALS 3
PRINT "SET DECIMALS 3: OK"
PRINT "3.14159 -> "; 3.14159
SET FIXED ON
PRINT "SET FIXED ON: OK"
PRINT "3.14159 -> "; 3.14159
SET FIXED OFF
SET PRINTER OFF
PRINT "SET PRINTER OFF: OK"
SET DEVICE TO SCREEN
PRINT "SET DEVICE TO SCREEN: OK"
SET MESSAGE TO 0
PRINT "SET MESSAGE TO 0: OK"

PRINT
PRINT "=== CONSOLE GET / SETWINDOWTITLE ==="
WINDOW SET TITLE "GMBasic - ConsoleSetAndGet"
PRINT "SETWINDOWTITLE: OK"

DIM pos(1) AS INTEGER
pos = CONSOLE.GETCURSORPOS()
PRINT "GETCURSORPOS: col=" + STR(pos(0)) + " riga=" + STR(pos(1))

DIM met(1) AS INTEGER
met = CONSOLE.GETTEXTMETRICS()
PRINT "GETTEXTMETRICS: larghezza=" + STR(met(0)) + " altezza=" + STR(met(1))

DIM pal(2) AS INTEGER
pal = CONSOLE.GETPALETTECOLOR(1)
PRINT "GETPALETTECOLOR(1): r=" + STR(pal(0)) + " g=" + STR(pal(1)) + " b=" + STR(pal(2))

PRINT
PRINT "Completato."
