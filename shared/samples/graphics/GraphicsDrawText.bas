SUB Main()
    SCREEN 12 ' Serve la finestra grafica (DRAW TEXT senza SCREEN non disegna nulla)
    FONT "font.ttf", 12
    COLOR 15, 0
    CLS
    COLOR 14, 0 ' giallo
    DRAW TEXT "Hello", 100, 100
    COLOR 11, 0 ' azzurro chiaro
    DRAW TEXT "Center", 320, 240, ALIGN_CENTER
    WAITKEY
END SUB
