' FILE: samples/Databases/DbLedgerCompleteCoverage.bas
' =================================================================================================
' Sample COMPLETO LEDGER — usa TUTTE e 17 le funzioni canoniche del namespace
' (ACCOUNTS ADD BALANCE BOF COUNT EDIT EOF FOUND GET IMPORT LASTREC OPEN PLOT RECNO
'  REGISTER VALIDATE VARIANCE) + i comandi SAVE/CLOSE. Deterministico, gira su 4 backend.
' Tipi di ritorno DEFINITI (niente JSON-dump): ACCOUNTS/BALANCE/REGISTER/GET → LISTE,
' VARIANCE → DOUBLE, PLOT → numero di punti, BOF/EOF/FOUND/VALIDATE → -1/0, COUNT/LASTREC/
' RECNO → numeri.
' =================================================================================================
IMPORT LEDGER
IMPORT IO

SUB Main()
    DIM p AS STRING
    p = "work/ledger_cov.dat"
    DIM csv AS STRING
    csv = "work/ledger_cov.csv"
    MKDIR "work"
    ON ERROR RESUME NEXT
    KILL p
    KILL "work/ledger_cov.idx"
    KILL csv
    ON ERROR GOTO 0

    ' CSV per LEDGER.IMPORT (mappatura date,desc,amount).
    OPEN csv FOR WRITE AS #1
    WRITE #1, "2026-01-20,Acquisto,-50"
    CLOSE #1

    DIM h AS LONG
    h = LEDGER.OPEN(p)
    DIM d AS STRING
    d = "2026-01-15"
    DIM newId AS LONG
    LEDGER ADD h WITH d, "Apertura" {
        "Attivita:Cassa": 1000.00,
        "Patrimonio:Apertura": -1000.00
    } INTO newId

    ' --- Conteggi e cursore ---
    PRINT "count: "; LEDGER.COUNT(h)
    PRINT "lastrec: "; LEDGER.LASTREC(h)
    PRINT "recno: "; LEDGER.RECNO(h)
    PRINT "bof: "; LEDGER.BOF(h)
    PRINT "eof: "; LEDGER.EOF(h)
    PRINT "found: "; LEDGER.FOUND(h)

    ' --- Collezioni (liste): conti, saldi, registro, movimenti della transazione ---
    PRINT "accounts: "; LEN(LEDGER.ACCOUNTS(h))
    PRINT "balance: "; LEN(LEDGER.BALANCE(h))
    PRINT "register: "; LEN(LEDGER.REGISTER(h))
    PRINT "get: "; LEN(LEDGER.GET(h, newId))

    ' --- Integrità e varianza ---
    PRINT "validate: "; LEDGER.VALIDATE(h)
    DIM varj = LEDGER.VARIANCE(h, "Attivita:Cassa", "2026-01")
    PRINT "variance: ok"

    ' --- Import CSV e grafico ---
    PRINT "import: "; LEDGER.IMPORT(h, csv, "date,desc,amount")
    IF LEDGER.PLOT(h, "", "bar") > 0 THEN PRINT "plot: si" ELSE PRINT "plot: no"

    ' --- Modifica (comando), salvataggio, chiusura ---
    LEDGER EDIT h ID newId WITH d, "Apertura aggiornata" {
        "Attivita:Cassa": 1001.00,
        "Patrimonio:Apertura": -1001.00
    } INTO newId
    LEDGER SAVE h
    LEDGER CLOSE h

    KILL p
    KILL "work/ledger_cov.idx"
    KILL csv
    PRINT "LEDGER complete coverage OK"
END SUB
