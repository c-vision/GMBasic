' FILE: Windows/WindowDemo.bas
' Demo finestre GRAFICHE: sfondo blu, finestra rossa con titlebar (icone minimizza/
' ingrandisci/chiudi) e scritta bianca; parzialmente sopra, una finestra gialla con
' titlebar e scritta nera. Titoli centrati verticalmente nella titlebar usando
' TEXTHEIGHT().

IMPORT GRAPHICS

SUB Main()
    ' ── Schermo grafico 640x480 ──
    SCREEN 12
    FONT "font.ttf", 12

    DIM th = TEXTHEIGHT()          ' altezza logica del font corrente

    ' ── Sfondo intero dello schermo in blu ──
    COLOR 1, 1
    CLS

    ' ══════ Finestra 1: rossa, 170,110 → 470,300 (300x190) ══════
    ' Ombra
    COLOR 0, 1
    LINE (176, 116)-(476, 306), 0, BF
    ' Corpo rosso
    COLOR 4, 4
    LINE (170, 110)-(470, 300), 4, BF
    ' Titlebar (più chiara, separata dal corpo): riga 110..132
    COLOR 12, 4
    LINE (170, 110)-(470, 132), 12, BF
    ' Bordo
    COLOR 15, 4
    LINE (170, 110)-(470, 300), 15, B

    ' Titolo centrato verticalmente nella titlebar: top = 110 + (22 - th)/2
    COLOR 15, 12
    DRAW TEXT "  FINESTRA ROSSA", 180, 110 + (22 - th) / 2

    ' Icone titlebar a destra
    DRAW TEXT " _ ", 402, 110 + (22 - th) / 2
    DRAW TEXT "[ ]", 426, 110 + (22 - th) / 2
    DRAW TEXT " X ", 450, 110 + (22 - th) / 2

    ' Testo bianco nel corpo (sotto la titlebar)
    COLOR 15, 4
    DRAW TEXT "Contenuto in bianco", 320, 165, ALIGN_CENTER
    DRAW TEXT "2 + 2 = " + STR(2 + 2), 320, 200, ALIGN_CENTER
    DRAW TEXT "Linea di riempimento", 320, 235, ALIGN_CENTER
    DRAW TEXT "Ancora una riga", 320, 270, ALIGN_CENTER

    ' ══════ Finestra 2: gialla, 360,200 → 600,350 (240x150), sopra la 1 ══════
    ' Ombra
    COLOR 0, 1
    LINE (366, 206)-(606, 356), 0, BF
    ' Corpo giallo
    COLOR 14, 14
    LINE (360, 200)-(600, 350), 14, BF
    ' Titlebar (più scura): riga 200..222
    COLOR 6, 14
    LINE (360, 200)-(600, 222), 6, BF
    ' Bordo
    COLOR 0, 14
    LINE (360, 200)-(600, 350), 0, B

    ' Titolo centrato verticalmente nella titlebar: top = 200 + (22 - th)/2
    COLOR 15, 6
    DRAW TEXT "  FINESTRA GIALLA", 370, 200 + (22 - th) / 2

    ' Icone titlebar a destra
    DRAW TEXT " _ ", 532, 200 + (22 - th) / 2
    DRAW TEXT "[ ]", 556, 200 + (22 - th) / 2
    DRAW TEXT " X ", 580, 200 + (22 - th) / 2

    ' Testo nero nel corpo
    COLOR 0, 14
    DRAW TEXT "Scritta nera", 480, 245, ALIGN_CENTER
    DRAW TEXT "Sovrapposta", 480, 280, ALIGN_CENTER
    DRAW TEXT "Ultima riga", 480, 315, ALIGN_CENTER

    ' ── Attesa (ESC chiude) ──
    WAITKEY
END SUB
