' FILE: Loops/LoopsForNextLabelsLineNumbers.bas
' Versione con TUTTE le righe numerate per evitare ambiguita' di compilazione.
' Test EXIT OuterLoop con scope I, J, K dopo l'uscita.

10 CLS
20 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ LoopsForNextLabelsLineNumbers - EXIT OuterLoop con righe numerate        ║"
40 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
50 PRINT

100 PRINT "--- Test EXIT OuterLoop ---"
110 DIM I, J, K, LoopCount

120 OuterLoop: FOR I = 1 TO 3
130   MiddleLoop: FOR J = 1 TO 3
140     InnerLoop: FOR K = 1 TO 3
150       LoopCount = LoopCount + 1
160       IF I = 2 AND J = 2 AND K = 2 THEN
170         PRINT "EXIT a I="; I; " J="; J; " K="; K
180         EXIT OuterLoop
190       END IF
200     NEXT K
210     PRINT "  Dopo inner K=3 (J="; J; ")"
220   NEXT J
230   PRINT "  Dopo middle J=3 (I="; I; ")"
240 NEXT I

250 PRINT
260 PRINT "--- Cicli EXIT Test terminati ---"
270 PRINT "Valore finale di I:"; I
280 PRINT "Valore finale di J:"; J
290 PRINT "Valore finale di K:"; K
300 PRINT "LoopCount:"; LoopCount
310 PRINT "Test completato."
320 END
