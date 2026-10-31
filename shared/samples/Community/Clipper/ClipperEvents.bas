' FILE: Samples/Clipper/ClipperEvents.bas
' Test ON WINDOW e ON MOUSE

IMPORT WINDOWS
SUB Main()
    PRINT "=== Event Test ==="
    
    ON WINDOW OPEN GOSUB FinestraAperta
    ON WINDOW CLOSE GOSUB FinestraChiusa
    ON WINDOW RESIZE GOSUB FinestraRidimensionata
    ON WINDOW MOVE GOSUB FinestraSpostata
    ON WINDOW FOCUS GOSUB FinestraFocus
    ON WINDOW MINIMIZE GOSUB FinestraMinimizzata
    ON WINDOW RESTORE GOSUB FinestraRipristinata
    ON WINDOW PASTE GOSUB TestoIncollato
    ON MOUSE CLICK GOSUB MouseClicked
    
    PRINT "Handler registrati."
    WAITKEY
    PRINT "Test completato."
    EXIT SUB

FinestraAperta:
    PRINT "Evento: finestra aperta!"
    RETURN

FinestraChiusa:
    PRINT "Evento: finestra chiusa!"
    RETURN

FinestraRidimensionata:
    PRINT "Evento: resize!"
    RETURN

FinestraSpostata:
    PRINT "Evento: finestra spostata!"
    RETURN

FinestraFocus:
    PRINT "Evento: focus!"
    RETURN

FinestraMinimizzata:
    PRINT "Evento: minimizzata!"
    RETURN

FinestraRipristinata:
    PRINT "Evento: ripristinata!"
    RETURN

TestoIncollato:
    PRINT "Evento: paste!"
    RETURN

MouseClicked:
    PRINT "Evento: mouse click!"
    RETURN
END SUB
