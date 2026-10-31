' EXPECT: 10|20|30
' MiniTest: named tuple field access with dot syntax — t.x
' Verifica che t.name risolva a t(indice) corretto via Linker
IMPORT CONSOLE
SUB Main()
    DIM t = (x: 10, y: 20, z: 30)
    PRINT t.x
    PRINT t.y
    PRINT t.z
END SUB
