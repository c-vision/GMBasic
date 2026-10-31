' FILE: Databases/DbVmMarshallerLedger.bas
' =====================================================================
' Test end-to-end del marshaller LEDGER sul backend VM/C: OPEN, ADD
' (postings flat [account, amount]), CLOSE. Output: work/.
' =====================================================================
IMPORT LEDGER
IMPORT IO

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    ON ERROR RESUME NEXT
    KILL g_work + "/vm_ledger.ledger"
    KILL g_work + "/vm_ledger.idx"
    ON ERROR GOTO 0

    h = LEDGER.OPEN(g_work + "/vm_ledger.ledger")
    LEDGER ADD h WITH "2026-01-01", "Apertura" { "Cassa": 100, "Banca": -100 } INTO nId
    LEDGER ADD h WITH "2026-01-02", "Acquisto" { "Cassa": -30, "Merce": 30 } INTO nId2
    LEDGER CLOSE h
    PRINT "VM-LEDGER-MARSHALLER-OK"
END SUB