' FILE: Exceptions/ExceptionsRethrow.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE ERRORI - PROPAGAZIONE E RHROW
' =================================================================================================
'
' MODIFICATO: rimosso GOSUB a moduli esterni, usato SUB helper invece di GOSUB.
' =================================================================================================

IMPORT COLLECTIONS
IMPORT JSON

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ExceptionsRethrow - Handler Procedurali e Propagazione                     ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    TestRethrow()

    PRINT
    PRINT "== FINE =="
END SUB

SUB TestRethrow()
    PRINT "--- Test: ON ERROR con handler ---"
    ON ERROR GOTO handler_rethrow
    LET x = 5 / 0
    PRINT "OK (dopo RESUME NEXT)"
    ON ERROR GOTO 0
    END SUB

handler_rethrow:
    PRINT "  Catturato errore:"; ERR.Description
    RESUME NEXT
    RETURN
