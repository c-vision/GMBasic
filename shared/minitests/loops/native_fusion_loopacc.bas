' EXPECT: pure 10|mixed 10  4
' MiniTest: accumulatore numerico in FOR (OP_NATIVE_LOOP_ACC). Corpo SOLO assegnazione ->
' fusione ammessa; corpo misto -> guard la vieta (altrimenti l'opcode fused consuma il
' ciclo e il resto del corpo gira una volta sola). Bug 2026-09-29 (PCode/VM-C).
DIM i AS INTEGER, acc AS DOUBLE, n AS INTEGER
acc = 0
FOR i = 1 TO 4
    acc = acc + i
NEXT
PRINT "pure"; acc
acc = 0
n = 0
FOR i = 1 TO 4
    n = n + 1
    acc = acc + i
NEXT
PRINT "mixed"; acc; n
