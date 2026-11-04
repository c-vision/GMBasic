' FILE: Graphics/CoordsAndTextGaps.bas
' Copre quattro funzioni di conversione coordinate (GRAPHICS.C2PX/C2PY/P2CX/P2CY),
' STRINGS.WRAP (2-3 argomenti) e DATETIME.ISDATE.

IMPORT STRINGS
IMPORT DATETIME

SUB Main()
    SCREEN 12, 640, 320, 32

    ' conversione carattere -> pixel e ritorno
    PRINT "C2PX(10)    = "; C2PX(10)
    PRINT "C2PY(5)     = "; C2PY(5)
    PRINT "P2CX(160)   = "; P2CX(160)
    PRINT "P2CY(80)    = "; P2CY(80)
    PRINT "C2PX(10,16) = "; C2PX(10, 16)

    ' wrap per parole: 2 argomenti e 3 argomenti (allineamento)
    PRINT "WRAP(2 arg)  = ["; STRINGS.WRAP("ciao mondo lungo", 5); "]"
    PRINT "WRAP(3 arg)  = ["; WRAP("ciao mondo lungo", 5, 1); "]"

    ' validazione date: solo ISO
    PRINT "ISDATE(2024-01-15) = "; DATETIME.ISDATE("2024-01-15")
    PRINT "ISDATE(15/01/2024) = "; ISDATE("15/01/2024")
    PRINT "ISDATE(2024-13-01) = "; ISDATE("2024-13-01")

    PRINT "CoordsAndTextGaps: completato."
    SCREEN 0
END SUB
