' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Graphics/GraphicsUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: GRAPHICS - COMANDI BSAVE/BLOAD NON ANCORA COPERTI DAGLI ESEMPI
' =================================================================================================
' Questo test copre i comandi GRAPHICS che non comparivano in alcun esempio .bas:
'   BSAVE, BLOAD.
'
' COPERTURA:
' - BSAVE(x1, y1, x2, y2, buffer) : salva un'area grafica binaria.
' - BLOAD(x1, y1, x2, y2, buffer) : carica un'area grafica binaria.
' =================================================================================================
IMPORT GRAPHICS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "GRAPHICS - BSAVE/BLOAD scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' Setup: modalità grafica VGA 640x480, 16 colori
    SCREEN 12
    LINE (5, 5)-(100, 100), 7

    ' 1. BSAVE (buffer per l'area)
    PRINT "-- BSAVE --"
    DIM buff(4095) AS BYTE
    BSAVE (5,5)-(100,100), buff
    PRINT "BSAVE area in buffer (4096 byte)."
    PRINT

    ' 2. BLOAD (x, y, buffer: ripristina l'area alle coordinate date)
    PRINT "-- BLOAD --"
    BLOAD (200,200), buff
    PRINT "BLOAD area dal buffer."
    PRINT

    SCREEN 0
    PRINT "Test Completato."
END SUB