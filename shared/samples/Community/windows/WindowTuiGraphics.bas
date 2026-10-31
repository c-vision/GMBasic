' FILE: Windows/WindowTuiGraphics.bas
' Test Fase W3: finestre TUI (WindowManager) renderizzate sulla pagina GRAFICA (SDL),
' non solo ANSI console. Apre lo schermo grafico, definisce finestre e attende un tasto.
' Con test_graphics_headless termina dopo l'iniezione di ESC.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    PRINT "=== WindowTuiGraphics ==="

    ' Schermo grafico (640x480)
    SCREEN 12

    ' Definizione finestre TUI (renderizzate sulla pagina grafica via RenderTui)
    WINDOW DEFINE Main FROM 2, 2 TO 18, 40 TITLE "Finestra Principale"
    PRINT "W1_OK"
    WINDOW DEFINE Help FROM 4, 44 TO 16, 76 TITLE "Aiuto" DOUBLE
    PRINT "W2_OK"

    ' Titolo della finestra focalizzata
    WINDOW SET TITLE "Principale Modificata"
    PRINT "W3_OK"

    ' AZIONI (ACTIVATE attiva anche la viewport: si torna allo schermo base per i marker)
    WINDOW Help ACTIVATE
    WINDOW SELECT 0
    PRINT "W4_OK"
    WINDOW Main ACTIVATE
    WINDOW SELECT 0
    PRINT "W5_OK"

    ' Chiude e ridisegna
    WINDOW Help CLOSE
    PRINT "W6_OK"

    ' Presenta e attende (il runner inietta ESC per chiudere)
    PRINT "=== Test completato ==="
    WAITKEY
END SUB
