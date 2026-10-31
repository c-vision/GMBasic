' ==============================================================================
' LedgerEditDemo.bas — LEDGER.EDIT: modifica di una transazione (delete+re-add bilanciato)
' ==============================================================================
' Copre: LEDGER ADD ... INTO id, LEDGER EDIT h ID id WITH ..., LEDGER.COUNT/BALANCE.
' La forma EDIT preserva l'id e la categoria (delete + re-add bilanciato).
' ==============================================================================
IMPORT LEDGER

SUB Main()
    DIM p AS STRING
    p = "/private/tmp/ledger_edit_demo.dat"
    ON ERROR RESUME NEXT
    KILL p
    ON ERROR GOTO 0

    DIM h AS LONG
    h = LEDGER.OPEN(p)
    DIM d AS STRING
    d = "2026-01-15"

    DIM newId AS LONG
    LEDGER ADD h WITH d, "Apertura" {
        "Attivita:Cassa": 1000.00,
        "Patrimonio:Apertura": -1000.00
    } INTO newId
    PRINT "count dopo ADD: "; LEDGER.COUNT(h)

    LEDGER EDIT h ID newId WITH d, "Apertura aggiornata" {
        "Attivita:Cassa": 1001.00,
        "Patrimonio:Apertura": -1001.00
    } INTO newId
    PRINT "count dopo EDIT: "; LEDGER.COUNT(h)
    PRINT "balance: "; LEN(LEDGER.BALANCE(h))
    PRINT
    PRINT "Completato."
END SUB
