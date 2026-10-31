' FILE: Branches/BranchesMatch.bas
' PATTERN MATCHING (MATCH...END MATCH)

IMPORT SYSTEM
IMPORT CONSOLE

SUB Main()
    CLS
    PRINT "=========================================="
    PRINT "  TEST: Pattern Matching (Rust-style)"
    PRINT "=========================================="
    PRINT

    PRINT "--- 1. Primitivi ---"

    ' Test match esatto
    MATCH 3
        CASE 1 =>
    PRINT "uno"
        CASE 2 =>
    PRINT "due"
        CASE 3 =>
    PRINT "OK: 3"
        CASE _ =>
    PRINT "default"
    END MATCH

    ' Test wildcard
    MATCH 99
        CASE 1 =>
    PRINT "uno"
        CASE _ =>
    PRINT "OK: wildcard 99"
    END MATCH

    ' Test range
    MATCH 50
        CASE 1 TO 10 =>
    PRINT "range 1-10"
        CASE 40 TO 60 =>
    PRINT "OK: range 40-60"
        CASE _ =>
    PRINT "default"
    END MATCH

    ' Test valore singolo
    MATCH 7
        CASE 7 =>
    PRINT "OK: 7"
        CASE _ =>
    PRINT "default"
    END MATCH

    ' Test IS >
    MATCH 150
        CASE IS < 50 =>
    PRINT "< 50"
        CASE IS > 100 =>
    PRINT "OK: IS > 100"
        CASE _ =>
    PRINT "default"
    END MATCH

    ' Test stringhe
    MATCH "hello"
        CASE "world" =>
    PRINT "world"
        CASE "hello" =>
    PRINT "OK: stringa hello"
        CASE _ =>
    PRINT "default"
    END MATCH

    PRINT "OK"
    PRINT "Test completato."
END SUB
