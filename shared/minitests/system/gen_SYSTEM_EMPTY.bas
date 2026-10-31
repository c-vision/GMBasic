' EXPECT: -1|-1|0|-1|0
' EMPTY e' un PREDICATO: in questo linguaggio TRUE = -1 (convenzione BASIC), verificato su tutte le
' gambe (IL/PCode/VM-C danno -1 quando vuoto). Casi: 0, "", "x", "   " (blank), 1.
IMPORT SYSTEM

SUB Main()
    PRINT EMPTY(0)
    PRINT EMPTY("")
    PRINT EMPTY("x")
    PRINT EMPTY("   ")
    PRINT EMPTY(1)
END SUB
