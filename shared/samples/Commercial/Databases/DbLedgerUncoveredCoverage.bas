' FILE: Databases/DbLedgerUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: LEDGER - COMANDO BUDGET NON ANCORA COPERTO DAGLI ESEMPI
' =================================================================================================
' Questo test documenta il comando LEDGER.BUDGET che non compariva in alcun esempio .bas.
'
' COPERTURA:
' - LEDGER.BUDGET hLedger "account" amount "period" : gestisce il budget di un conto.
'
' NOTA BUG RUNTIME (rilevato dall'audit):
'   LEDGER.BUDGET genera in fase di LINK il tipo di istruzione 'LedgerBudgetStmt',
'   ma il Linker.ResolveStatement NON gestisce un case per BUDGET. Il comando è
'   sintatticamente valido ma non è risolto dal linker (manca il case). Per questo
'   motivo la chiamata eseguibile è commentata rimanendo però documentata quaggiù.
'
'   Sintassi corretta (non eseguibile finché il bug non viene corretto):
'     ' LEDGER.BUDGET hLedger "SPESE" 1000.00 "2025-01"
'
' =================================================================================================
IMPORT LEDGER WITH ALIAS
IMPORT IO

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "LEDGER - BUDGET scoperto dagli esempi"
    PRINT "============================================================"
    PRINT

    MKDIR g_work

    ' Setup: apre un libro mastro. Il file .dat/.idx viene RIMOSSO prima: senza, il sample
    ' dipendeva dallo stato lasciato dalle esecuzioni precedenti (e i backend divergevano:
    ' IL contava 0 righe di balance e la VM/C 2 su un .dat stantio).
    ON ERROR RESUME NEXT
    KILL g_work + "/budget.dat"
    KILL g_work + "/budget.idx"
    ON ERROR GOTO 0

    DIM hLedger AS LONG
    hLedger = LEDGER.OPEN(g_work + "/budget.dat")
    IF hLedger <= 0 THEN
        PRINT "Errore apertura ledger."
        END
    END IF

    ' 1. LEDGER.BUDGET
    PRINT "-- LEDGER.BUDGET --"
    PRINT "BUDGET: non eseguito - bug linker LedgerBudgetStmt (vedi NOTA in testa al file)."
    ' ' LEDGER.BUDGET hLedger "SPESE" 1000.00 "2025-01" (retest commentato)

    ' 2. LEDGER.BALANCE / LEDGER.PLOT: forme FUNZIONE (il census conta le procedure come
    '    chiamata `NOME(` — queste due non comparivano in alcun sample; la forma COMANDO
    '    `LEDGER BALANCE … INTO` è coperta da DbLedgerModernTests.bas).
    PRINT "-- LEDGER.BALANCE / LEDGER.PLOT --"
    ' BALANCE: forme a 1 e a 2 argomenti (la divergenza IL 0 / VM/C 2 era effetto della
    ' corruzione delle liste del ledger, chiusa il 2026-09-11).
    PRINT "balance 1 arg: "; LEN(LEDGER.BALANCE(hLedger))
    PRINT "balance 2 arg: "; LEN(LEDGER.BALANCE(hLedger, ""))
    ' PLOT ritorna un numero instabile (handle/id): si asserisce il FATTO (>0), come nel
    ' MiniTest ledger_ops.bas — così il sample resta deterministico su tutte le gambe.
    ' Serve almeno UNA transazione: su un libro vuoto IL conta comunque un punto (>0) mentre
    ' i backend C ritornano 0 → con un punto reale le 4 gambe concordano.
    DIM d AS STRING
    d = "2026-01-31"
    DIM newId AS LONG
    LEDGER ADD hLedger WITH d, "Apertura" {
        "Attivita:Cassa": 100.00,
        "Patrimonio:Apertura": -100.00
    } INTO newId
    IF LEDGER.PLOT(hLedger, "", "bar") > 0 THEN PRINT "plot: si" ELSE PRINT "plot: no"

    LEDGER CLOSE hLedger
    PRINT "Test Completato."
END SUB
