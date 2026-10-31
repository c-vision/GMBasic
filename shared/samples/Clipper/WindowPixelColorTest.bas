' FILE: Clipper/WindowPixelColorTest.bas
' Test manuale: schermo BLU, finestra ROSSA centrata, titolo GIALLO.

IMPORT WINDOWS

IMPORT CONTROLS
IMPORT GRAPHICS

SUB Main()
    SCREEN 12
    FONT "font.ttf", 12
    CLS RGB(0, 0, 255)

    WINDOW DEFINE Win FROM 140, 170 TO 340, 470 TITLE "TEST" PIXEL
    WINDOW SELECT Win
    COLOR RGB(255, 255, 0), RGB(255, 0, 0)
    WINDOW SELECT 0

    STATUSBAR "Pronto - premi un tasto"
    WAITKEY
END SUB
