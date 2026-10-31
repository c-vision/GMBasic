' EXPECT: [1, 2]|[3, 4]
' MiniTest: tuple annidate — ((1, 2), (3, 4))
' Verifica che un elemento di tupla che è a sua volta una tupla venga reso in forma
' compatta "[e0, e1]" (come VM/C), non "System.Object[]" (ToString del CLR).
IMPORT CONSOLE
SUB Main()
    DIM nested = ((1, 2), (3, 4))
    PRINT nested(0)
    PRINT nested(1)
END SUB
