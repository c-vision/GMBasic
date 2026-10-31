' FILE: Databases/RDD_ScopeCommands.bas
' Test RDD DELETE/RECALL/REPLACE con clausole di scope (sintassi legacy).
' Motore in memoria ARRAY — non richiede file su disco.
' ===================================================================

IMPORT RDD
IMPORT CONSOLE

SUB Main()
    PRINT "=== RDD Scope Commands ==="
    PRINT

    ' Apre un'area in memoria ARRAY.
    RDD SET DEFAULT "ARRAY"
    RDD USE ":memory:" ALIAS Area1

    PRINT "1. RDD DELETE ALL"
    RDD DELETE ALL
    PRINT "   DELETE ALL: OK"

    PRINT "2. RDD RECALL"
    RDD RECALL ALL
    PRINT "   RECALL ALL: OK"

    PRINT "3. RDD DELETE NEXT 5"
    RDD DELETE NEXT 5
    PRINT "   DELETE NEXT 5: OK"

    PRINT "4. RDD REPLACE field WITH value"
    RDD REPLACE Nome WITH "nuovo valore" ALL
    PRINT "   REPLACE Nome WITH ... ALL: OK"

    PRINT "5. RDD DELETE (senza scope = record corrente)"
    RDD DELETE
    PRINT "   DELETE corrente: OK"

    PRINT "6. RDD DELETE FOR (condizione su record)"
    RDD DELETE FOR Saldo > 100
    PRINT "   DELETE FOR: OK"

    PRINT "7. RDD RECALL WHILE (condizione fino a primo non-match)"
    RDD RECALL WHILE Saldo > 0
    PRINT "   RECALL WHILE: OK"

    PRINT "8. RDD REPLACE FOR (condizione su record)"
    RDD REPLACE Nome WITH "cliente" FOR Saldo < 50
    PRINT "   REPLACE FOR: OK"

    PRINT "9. RDD DELETE RECORD n (record specifico)"
    RDD DELETE RECORD 3
    PRINT "   DELETE RECORD: OK"

    PRINT "10. RDD DELETE REST (dal corrente alla fine)"
    RDD DELETE REST
    PRINT "   DELETE REST: OK"

    PRINT
    PRINT "--- TEST COMPLETATO ---"
END SUB
