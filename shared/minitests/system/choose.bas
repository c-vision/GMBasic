' EXPECT: 5|10|b|ctx|a|b|c
' SYSTEM.CHOOSE(indice, v1, v2, ...) — selezione 1-based.
' Le ultime tre righe riproducono il caso che ha rivelato il bug di parità: con istruzioni
' precedenti il compilatore impacchetta i valori in contenitori annidati (catena fino a 10
' livelli) e la gamba VM/C stampava "[a, b, c]" invece dell'elemento (fix: flatten, commit 7fd90098f).
IMPORT SYSTEM

SUB Main()
    PRINT SYSTEM.CHOOSE(1, 5, 6, 7)
    PRINT SYSTEM.CHOOSE(3, 8, 9, 10)
    PRINT SYSTEM.CHOOSE(2, "a", "b", "c")
    PRINT "ctx"
    PRINT CHOOSE(1, "a", "b", "c")
    PRINT CHOOSE(2, "a", "b", "c")
    PRINT CHOOSE(3, "a", "b", "c")
END SUB
