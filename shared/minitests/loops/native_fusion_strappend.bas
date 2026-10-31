' EXPECT: pureabababab 8|it 1|it 2|it 3|it 4|mixedabababab 8
' MiniTest: append di stringa in FOR (OP_NATIVE_STRING_APPEND_LOOP). Corpo SOLO append ->
' fusione ammessa; corpo misto (PRINT + append) -> guard la vieta: senza, il PRINT girava
' una volta sola e il contatore saltava al valore finale. Bug 2026-09-29 (PCode/VM-C).
DIM i AS INTEGER, s AS STRING
s = ""
FOR i = 1 TO 4
    s = s + "ab"
NEXT
PRINT "pure"; s; LEN(s)
s = ""
FOR i = 1 TO 4
    PRINT "it"; i
    s = s + "ab"
NEXT
PRINT "mixed"; s; LEN(s)
