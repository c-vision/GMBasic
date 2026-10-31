' FILE: Windows/WindowContentGui.bas
' Test contenuto finestre su GRAFICA (SDL): buffer, scroll, scrollbar.
' Stesso comportamento del sample TUI Clipper/WindowContent.bas — la parità TUI/GUI
' è garantita perché lo stesso WindowManager rende su SDL.

IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
    SCREEN 12
    FONT "font.ttf", 12

    ' Finestra con buffer scrollabile
    WINDOW DEFINE Log FROM 2, 2 TO 22, 40 TITLE "Log"
    WINDOW Log CLEAR
    WINDOW Log PRINT "=== Log di sistema ==="
    DIM i
    FOR i = 1 TO 40
        WINDOW Log PRINT "Riga di log " + STR(i)
    NEXT i
    WINDOW Log PRINT "Fine log"
    PRINT "G1_OK"

    ' Scroll di 10 righe
    WINDOW Log SCROLL 10
    PRINT "G2_OK"

    ' Attesa (ESC chiude)
    PRINT "=== Test completato ==="
    WAITKEY
END SUB
