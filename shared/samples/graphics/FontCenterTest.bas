' FILE: Graphics/FontCenterTest.bas
' Test diagnostico: centraggio verticale dei titoli di finestra.
' Per ogni dimensione di font (12, 16, 20) disegna una barra "titlebar" con:
'   - linea guida SUPERIORE (blu)
'   - linea guida INFERIORE (verde)
'   - il testo "FINESTRA" centrato tra le due guide
' Così si vede subito se il testo è allineato in alto, in basso o al centro.

IMPORT GRAPHICS

SUB Main()
    SCREEN 12
    FONT "font.ttf", 12
    COLOR 1, 1
    CLS

    ' Righe (logiche) per le 3 barre: 12, 16, 20
    DIM tops(2), heights(2), fonts(2)
    tops(0) = 20 : heights(0) = 30 : fonts(0) = 12
    tops(1) = 70 : heights(1) = 30 : fonts(1) = 16
    tops(2) = 120 : heights(2) = 30 : fonts(2) = 20

    DIM i
    FOR i = 0 TO 2
        ' Cambia font alla dimensione corrente
        FONT "font.ttf", fonts(i)

        DIM t = tops(i)
        DIM bh = heights(i)
        DIM b = t + bh

        ' Corpo della barra (grigio scuro)
        COLOR 7, 7
        LINE (30, t)-(450, b), 7, BF

        ' Linea guida SUPERIORE (blu) — y = t
        COLOR 9, 7
        LINE (30, t)-(450, t), 9

        ' Linea guida INFERIORE (verde) — y = b
        COLOR 10, 7
        LINE (30, b)-(450, b), 10

        ' Testo centrato: top = t + (bh - TEXTHEIGHT())/2
        COLOR 15, 7
        DRAW TEXT "FINESTRA " + STR(fonts(i)), 240, t + (bh - TEXTHEIGHT()) / 2, ALIGN_CENTER

        ' Etichetta con le coordinate delle guide
        COLOR 14, 7
        DRAW TEXT "t=" + STR(t) + " b=" + STR(b), 460, t + 4
    NEXT i

    ' Attesa
    WAITKEY
END SUB
