' Dict/set comprehension tipizzate (IL: prima 'DictComprehensionExpr non ancora supportata' → InferType non la conosceva):
' conteggio, lettura per chiave, filtro IF, set con deduplica. (La forma INFERITA `DIM d = {..}` e' coperta solo da IL/PCode/VM-C.)
' EXPECT: n= 3|v3= 9|s= 3|big= 2
SUB Main()
    DIM d AS DICT OF DOUBLE, DOUBLE = { x : x*x FOR x IN {1, 2, 3} }
    PRINT "n="; d.Count
    PRINT "v3="; d(3)
    DIM s AS HASHSET OF DOUBLE = { x MOD 5 FOR x IN {1, 2, 6, 11, 4} }
    PRINT "s="; s.Count
    DIM big AS DICT OF DOUBLE, DOUBLE = { x : x FOR x IN {1, 5, 7, 2} IF x > 4 }
    PRINT "big="; big.Count
END SUB
