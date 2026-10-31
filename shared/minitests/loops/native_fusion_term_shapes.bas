' EXPECT: chain 12|mixed 18|two 12
' MiniTest: forme di termine NON ammesse dalla fusione (OP_NATIVE_LOOP_ACC). La chain a 3
' operandi non e' un pattern supportato: se il matcher la accetta, perde il sotto-terminale.
' Divergenza 2026-09-29: PCodeVM/VM-C davano 6 e 12, IL/C99 12 e 18. Il terzo ciclo e' la
' forma legitima f(I)+g(I), che invece DEVE fondere e valere 12.
DIM i AS INTEGER, acc AS DOUBLE
acc = 0
FOR i = 1 TO 3
    acc = acc + i + i
NEXT
PRINT "chain"; acc
acc = 0
FOR i = 1 TO 3
    acc = acc + ROUND(i) + i * 2
NEXT
PRINT "mixed"; acc
acc = 0
FOR i = 1 TO 3
    acc = acc + ROUND(i) + ABS(i)
NEXT
PRINT "two"; acc
