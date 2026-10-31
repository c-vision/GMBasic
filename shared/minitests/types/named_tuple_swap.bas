' EXPECT: 1|2|2|1
' MiniTest: tuple swap — (a, b) = (b, a) con tuple nominate
' Verifica che lo swap atomico funzioni: i valori sono catturati in temporanee
' PRIMA di scrivere i target (altrimenti px verrebbe sovrascritto prima della lettura).
IMPORT CONSOLE
SUB Main()
    DIM p = (x: 1, y: 2)
    DIM (px, py) = p
    PRINT px
    PRINT py
    (px, py) = (py, px)
    PRINT px
    PRINT py
END SUB
