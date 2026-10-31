' FILE: Graphics/GraphicsCellPixel.bas
' =================================================================================================
'       CONVERSIONE CELLE <-> PIXEL: C2PX / C2PY / P2CX / P2CY (namespace GRAPHICS)
' =================================================================================================
'
' - C2PX(colonna, [size]) -> coordinata X in pixel della cella (size = dimensione del font;
'   se omessa si usa la larghezza del font corrente, TEXTWIDTH(" ")).
' - C2PY(riga,    [size]) -> coordinata Y in pixel della cella (size = dimensione del font;
'   se omessa si usa l'altezza del font corrente, TEXTHEIGHT()).
' - P2CX(x,       [size]) -> colonna della cella che contiene il pixel x.
' - P2CY(y,       [size]) -> riga della cella che contiene il pixel y.
'
' =================================================================================================

SUB Main()
    SCREEN 12
    FONT "font.ttf", 12
    COLOR 15, 0

    ' --- 1. Celle -> Pixel con dimensione esplicita (24) ---
    x = C2PX(3, 24)   ' colonna 3 -> x = 3*24 = 72
    y = C2PY(2, 24)   ' riga 2    -> y = 2*24 = 48
    COLOR 14, 0
    DRAW TEXT "C2P", x, y

    ' --- 2. Pixel -> Celle (verifica inversa) ---
    c = P2CX(72, 24)  ' x 72 / 24 = colonna 3
    r = P2CY(48, 24)  ' y 48 / 24 = riga 2
    IF c = 3 AND r = 2 THEN
        PSET (300, 100), 14
    END IF

    ' --- 3. Celle -> Pixel con dimensione del font corrente (omessa) ---
    x2 = C2PX(5)      ' 5 * TEXTWIDTH(" ")
    y2 = C2PY(5)      ' 5 * TEXTHEIGHT()
    COLOR 11, 0
    DRAW TEXT "Font", x2, y2

    SLEEP 1000
END SUB
