' FILE: Scopes/ScopesStatic.bas
' Test: STATIC keyword — variabili che mantengono il valore tra chiamate
' Per specifica VARIABLES #3: STATIC crea una variabile a livello di procedura
' che MANTIENE il suo valore tra le chiamate successive.

IMPORT SYSTEM
IMPORT JSON

SUB TestStatic()
    STATIC Counter AS INTEGER = 0
    DIM LocalCounter AS INTEGER = 0
    Counter = Counter + 1
    LocalCounter = LocalCounter + 1
    PRINT "Counter (STATIC):"; Counter; " | LocalCounter (DIM):"; LocalCounter
END SUB

SUB TestStaticWithString()
    STATIC Message AS STRING = ""
    Message = Message + "*"
    PRINT "Message (STATIC): '"; Message; "' (lunghezza:"; LEN(Message); ")"
END SUB

SUB TestStaticArray()
    STATIC Accum() AS DOUBLE = {1, 2, 3}
    DIM i AS INTEGER
    FOR i = 0 TO UBOUND(Accum)
        Accum(i) = Accum(i) * 2
    NEXT i
    PRINT "Accum (STATIC array):"; JSON.Stringify(Accum)
END SUB

SUB Main()
    CLS
    PRINT "=== Test STATIC Variables ==="
    PRINT "Le variabili STATIC mantengono il valore tra chiamate successive."
    PRINT "Le variabili DIM vengono reinizializzate ad ogni chiamata."
    PRINT

    PRINT "--- Chiamata 1 ---"
    TestStatic()
    PRINT

    PRINT "--- Chiamata 2 ---"
    TestStatic()
    PRINT

    PRINT "--- Chiamata 3 ---"
    TestStatic()
    PRINT

    PRINT "--- Test STATIC string ---"
    TestStaticWithString()
    TestStaticWithString()
    TestStaticWithString()
    PRINT

    PRINT "--- Test STATIC array ---"
    TestStaticArray()
    TestStaticArray()
    TestStaticArray()
    PRINT

    PRINT "=== Fine test ==="
END SUB
