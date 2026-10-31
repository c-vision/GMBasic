' FILE: samples/database/CsvFinancialReport.bas
' ===================================================================
' REPORT FINANZIARIO COMPLESSO su CSV (driver testo completo):
'   1. genera un file transazioni CSV in work
'   2. lo apre con RDD USE (driver CSV)
'   3. DELETE FOR (filtra righe invalide/stub)
'   4. SET ORDER (sort per data)
'   5. naviga e aggrega saldi per conto (VAL/stringhe)
'   6. scrive un REPORT CSV via OPEN FOR OUTPUT + PRINT #
'   7. COPY TO (copia ordinata)
' Percorsi: fixtures in samples/fixtures, output in work.
' ===================================================================
IMPORT RDD
IMPORT DBF
IMPORT IO

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "=== CSV Financial Report ==="
    PRINT

    ' --- 1. Genera il CSV delle transazioni ---
    PRINT "1. generazione transactions.csv"
    OPEN g_work + "/transactions.csv" FOR OUTPUT AS #1
    PRINT #1, "id,data,conto,importo,desc"
    PRINT #1, "1,2026-01-05,1000,150.00,stipendio"
    PRINT #1, "2,2026-01-03,5000,-50.00,bolletta"
    PRINT #1, "3,2026-01-06,1000,75.50,rimborso"
    PRINT #1, "4,2026-01-01,5000,-120.00,affitto"
    PRINT #1, "5,2026-01-07,3000,25.00,stub"
    PRINT #1, "6,2026-01-04,1000,-30.00,spesa"
    CLOSE #1
    PRINT "   scritto 6 righe"
    PRINT

    ' --- 2. Apri con RDD (driver CSV auto) ---
    PRINT "2. RDD USE transactions.csv"
    RDD USE g_work + "/transactions.csv" ALIAS T
    PRINT "   record: "; DBF.RECNO()
    DBF GO BOTTOM
    PRINT "   ultimo recno: "; DBF.RECNO()
    PRINT

    ' --- 3. DELETE FOR: rimuovi le righe stub ---
    PRINT "3. DELETE FOR desc='stub' (soft)"
    RDD DELETE FOR desc = "stub"
    PRINT "   marcata cancellata (soft, visibile nel dettaglio sotto)"
    PRINT

    ' --- 4. SET ORDER (sort per data) ---
    PRINT "4. SET ORDER by data"
    RDD SET ORDER 1, "data"
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        PRINT "   "; RDD.GETFIELD(1, 1); "  "; RDD.GETFIELD(1, 2); "  "; RDD.GETFIELD(1, 3)
        DBF SKIP 1
    LOOP
    PRINT

    ' --- 5. Aggrega saldi per conto (VAL) ---
    PRINT "5. aggregazione saldi per conto"
    DIM s1000 AS DOUBLE = 0
    DIM s5000 AS DOUBLE = 0
    DIM s3000 AS DOUBLE = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        c$ = RDD.GETFIELD(1, 2)
        imp = VAL(RDD.GETFIELD(1, 3))
        IF c$ = "1000" THEN
            s1000 = s1000 + imp
        ELSEIF c$ = "5000" THEN
            s5000 = s5000 + imp
        ELSEIF c$ = "3000" THEN
            s3000 = s3000 + imp
        END IF
        DBF SKIP 1
    LOOP
    PRINT "   conto 1000: "; s1000
    PRINT "   conto 5000: "; s5000
    PRINT "   conto 3000: "; s3000
    PRINT

    ' --- 6. Scrivi il REPORT CSV ---
    PRINT "6. report -> work/report.csv"
    OPEN g_work + "/report.csv" FOR OUTPUT AS #2
    PRINT #2, "conto,importo_totale"
    PRINT #2, "1000,"; s1000
    PRINT #2, "5000,"; s5000
    PRINT #2, "3000,"; s3000
    CLOSE #2
    PRINT

    ' --- 7. COPY TO (copia ordinata) ---
    PRINT "7. COPY TO work/transactions_sorted.csv"
    RDD COPY TO g_work + "/transactions_sorted.csv" VIA "CSV"
    DBF CLOSE
    PRINT

    PRINT "--- CSV FINANCIAL REPORT OK ---"
END SUB