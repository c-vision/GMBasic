' EXPECT: 10|20|30
' MiniTest: named tuple destructuring — DIM (a, b, c) = tuple
' Verifica che la destrutturazione funzioni con tuple nominate e anonime
IMPORT CONSOLE
SUB Main()
    ' Destrutturazione da tupla nominata
    DIM t = (x: 10, y: 20, z: 30)
    DIM (a, b, c) = t
    PRINT a
    PRINT b
    PRINT c
END SUB
