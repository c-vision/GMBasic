' CLIPPER 5.3 — AEVAL con code block: il block riceve (elemento, indice) [D6].
' Arity 1 = solo elemento; arity 2 = elemento + indice (GMBasic: array 0-based).
' EXPECT: sum= 60|weighted= 140
SUB Main()
    DIM v = {10, 20, 30}
    DIM Tot = 0
    AEVAL v, { |n| Tot := Tot + n }              ' 60
    PRINT "sum="; Tot
    DIM W = 0
    AEVAL v, { |n, i| W := W + n * (i + 1) }     ' 10*1 + 20*2 + 30*3 = 140
    PRINT "weighted="; W
END SUB
