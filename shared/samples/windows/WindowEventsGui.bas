' FILE: Windows/WindowEventsGui.bas
' Test Fase W4: ON WINDOW con eventi GUI (SDL). Apre lo schermo grafico, registra gli
' handler ON WINDOW, definisce finestre TUI (che dispatchano eventi Open/Close al
' EventDispatcher) e attende. Con test_graphics_headless termina dopo l'iniezione di ESC.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    PRINT "=== WindowEventsGui ==="

    ' Schermo grafico
    SCREEN 12

    ' Registra handler ON WINDOW
    ON WINDOW OPEN GOSUB FinestraAperta
    ON WINDOW CLOSE GOSUB FinestraChiusa
    ON WINDOW RESIZE GOSUB FinestraResize
    ON WINDOW FOCUS GOSUB FinestraFocus

    ' Definizione finestre → dispatch Open (kind 7) e Focus (kind 4) al dispatcher
    WINDOW DEFINE Main FROM 2, 2 TO 18, 40 TITLE "Principale"
    PRINT "E1_OK"
    WINDOW DEFINE Help FROM 4, 44 TO 16, 76 TITLE "Aiuto" DOUBLE
    PRINT "E2_OK"

    ' Azioni → dispatch eventi
    WINDOW Help CLOSE
    PRINT "E3_OK"

    ' Presenta e attende
    PRINT "=== Test completato ==="
    WAITKEY
END SUB

FinestraAperta:
    PRINT "EVENTO: OPEN"
    RETURN

FinestraChiusa:
    PRINT "EVENTO: CLOSE"
    RETURN

FinestraResize:
    PRINT "EVENTO: RESIZE"
    RETURN

FinestraFocus:
    PRINT "EVENTO: FOCUS"
    RETURN
