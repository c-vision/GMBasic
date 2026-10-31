' FILE: Windows/WindowDesktop.bas
' Demo completa Fase W-Pixel: un desktop con più finestre PIXEL sovrapposte.
' Mostra: z-order (ACTIVATE porta in cima), titlebar con gradiente focus/blur,
' controlli vettoriali (min/max/close), ombra, contenuto multi-riga, scrollbar,
' e grafica disegnata dietro le finestre (il desktop).
' Con test_graphics_headless termina dopo l'iniezione di ESC.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    PRINT "=== WindowDesktop ==="

    ' Desktop grafico 640x480
    SCREEN 12
    FONT "font.ttf", 12

    ' Sfondo: gradiente + alcuni elementi (il "desktop" dietro le finestre).
    CLS
    LINE (0, 0)-(639, 479), 1, BF
    CIRCLE (120, 120), 60, 9
    CIRCLE (520, 380), 80, 10
    LINE (0, 400)-(639, 400), 7
    PRINT "Desktop"

    ' ── Finestra 1: "Main" (prima, in basso nello z) ──
    WINDOW DEFINE Main FROM 30, 30 TO 260, 360 TITLE "Finestra Principale" PIXEL
    WINDOW SELECT Main
    PRINT "Benvenuto nel desktop pixel!"
    PRINT "Questa finestra ha ombra,"
    PRINT "titlebar con gradiente e"
    PRINT "controlli min/max/close."
    DIM i
    FOR i = 1 TO 30
        PRINT "Riga di log " + STR(i)
    NEXT i
    WINDOW SELECT 0
    PRINT "D1_OK"

    ' ── Finestra 2: "Log" (sopra Main, parzialmente sovrapposta) ──
    WINDOW DEFINE Log FROM 180, 60 TO 380, 260 TITLE "Registro" PIXEL
    WINDOW SELECT Log
    PRINT "Evento: finestre pixel attive"
    PRINT "Evento: z-order corretto"
    PRINT "Evento: focus sulla titlebar"
    WINDOW SELECT 0
    PRINT "D2_OK"

    ' ── Finestra 3: "Help" (in cima, piccola) ──
    WINDOW DEFINE Help FROM 120, 120 TO 300, 220 TITLE "Aiuto" PIXEL
    WINDOW SELECT Help
    PRINT "Trascina le finestre."
    PRINT "Attiva una finestra per"
    PRINT "portarla in primo piano."
    WINDOW SELECT 0
    PRINT "D3_OK"

    ' ── Z-order: porta Main in cima (Raise) ──
    WINDOW Main ACTIVATE
    PRINT "D4_OK"

    ' ── Funzioni di lettura sulla finestra attiva ──
    DIM wx
    wx = WINDOWS.X()
    IF wx = 30 THEN
        PRINT "D5_OK"
    ELSE
        PRINT "D5_FAIL"
    ENDIF

    PRINT "=== Test completato ==="
    WAITKEY
END SUB
