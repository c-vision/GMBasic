' FILE: Clipper/WindowViewport.bas
' Test finestra attiva (viewport): WINDOW SELECT + comandi polimorfici che si adattano
' alla finestra attiva (PRINT/LOCATE/CLS/@SAY/BOX/SCROLL). Poi ritorno allo schermo base.

IMPORT WINDOWS

SUB Main()
    PRINT "=== WindowViewport ==="

    ' ── Finestra con buffer ──
    WINDOW DEFINE Log FROM 2, 2 TO 20, 60 TITLE "Log"
    WINDOW Log CLEAR

    ' ── Seleziona Log come finestra attiva ──
    WINDOW SELECT Log
    PRINT "V1_OK (PRINT va nella finestra Log)"

    ' ── PRINT dentro la finestra attiva ──
    PRINT "Prima riga nel buffer"
    PRINT "Seconda riga"
    PRINT "=== blocco ==="
    PRINT "V2_OK"

    ' ── @ SAY a coordinate relative ──
    @ 2, 2 SAY "SAY@(2,2)"
    PRINT "V3_OK"

    ' ── BOX nella finestra attiva ──
    BOX 4, 2, 10, 40
    PRINT "V4_OK"

    ' ── SCROLL della finestra ──
    SCROLL 0, 0, 20, 60, 2
    PRINT "V5_OK"

    ' ── CLS svuota il buffer della finestra ──
    CLS
    PRINT "V6_OK (buffer svuotato)"

    ' ── Ritorno allo schermo base ──
    WINDOW SELECT 0
    PRINT "V7_OK (schermo base di nuovo)"

    ' ── Ora PRINT va sullo schermo base ──
    PRINT "=== Test completato ==="
END SUB
