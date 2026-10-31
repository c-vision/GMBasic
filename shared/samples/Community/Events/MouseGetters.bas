' FILE: Events/MouseGetters.bas
' MOUSE getter functions in headless (deterministic: tutte 0/false).
IMPORT MOUSE
SUB Main()
    PRINT "BUTTON:     "; MOUSE.BUTTON()
    PRINT "DBLCLICK:   "; MOUSE.DBLCLICK()
    PRINT "MIDDLE:     "; MOUSE.MIDDLE()
    PRINT "PRESENT:    "; MOUSE.PRESENT()
    PRINT "WHEEL:      "; MOUSE.WHEEL()
    PRINT "Test Completato."
END SUB