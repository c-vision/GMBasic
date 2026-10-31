IMPORT CONSOLE
' ==============================================================================
' FILE: Console/ConsoleMessagePoint.bas
' CONSOLE.MESSAGE() (messaggio corrente) e CONSOLE.POINT() (colore pixel).
' ==============================================================================
'   CONSOLE.MESSAGE("testo")  — imposta il messaggio e ritorna il testo
'   CONSOLE.MESSAGE()         — legge il messaggio corrente
'   CONSOLE.POINT(x, y)       — indice colore del pixel (headless: -1)
' ==============================================================================

PRINT "=== CONSOLE.MESSAGE ==="
PRINT "set -> "; CONSOLE.MESSAGE("Pronto.")
PRINT "get -> "; CONSOLE.MESSAGE()
PRINT "set2 -> "; CONSOLE.MESSAGE("Elaborazione...")
PRINT "get2 -> "; CONSOLE.MESSAGE()

PRINT
PRINT "=== CONSOLE.POINT ==="
PRINT "POINT(0,0) -> "; CONSOLE.POINT(0, 0)
PRINT "POINT(10,20) -> "; CONSOLE.POINT(10, 20)

PRINT
PRINT "Completato."
