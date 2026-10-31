' FILE: Windows/WindowPixel.bas
' Test Fase W-Pixel: finestre PIXEL-BASED su schermo grafico (niente celle TUI).
' WINDOW DEFINE ... PIXEL crea una finestra con rect in PIXEL LOGICI, resa con
' primitive di alto livello (rounded rect, gradiente titlebar, ombra, controlli
' vettoriali min/max/close) sul buffer della pagina grafica.
' Sintassi: DEFINE name FROM top, left TO bottom, right → larghezza = right-left,
' altezza = bottom-top. (20,20)-(200,300) = 280x180 pixel.
' Con test_graphics_headless termina dopo l'iniezione di ESC.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    PRINT "=== WindowPixel ==="

    ' Schermo grafico (640x480)
    SCREEN 12
    FONT "font.ttf", 12

    ' Finestra PIXEL: (top,left)=(20,20), (bottom,right)=(200,300) in pixel logici.
    WINDOW DEFINE Main FROM 20, 20 TO 200, 300 TITLE "Finestra Pixel" PIXEL
    PRINT "P1_OK"

    ' Le coordinate della finestra attiva si leggono con le funzioni WINDOWS.*
    DIM wx
    wx = WINDOWS.X()
    IF wx = 20 THEN
        PRINT "P2_OK"
    ELSE
        PRINT "P2_FAIL"
    ENDIF

    ' PRINT va nel buffer della finestra attiva (viewport corrente).
    WINDOW SELECT Main
    PRINT "Contenuto dentro la finestra pixel"
    WINDOW SELECT 0
    PRINT "P3_OK"

    ' Box nel buffer della finestra (coordinate relative al client area).
    WINDOW SELECT Main
    BOX 1, 1, 3, 20 SINGLE
    WINDOW SELECT 0
    PRINT "P4_OK"

    ' Azioni: minimizza/ripristina (solo stato, nessun crash).
    WINDOW Main MINIMIZE
    WINDOW Main RESTORE
    PRINT "P5_OK"

    PRINT "=== Test completato ==="
    WAITKEY
END SUB
