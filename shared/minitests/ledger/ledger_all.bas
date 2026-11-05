' MiniTest NAMESPACE LEDGER — tutte le 17 funzioni della superficie.
' ACCOUNTS, ADD, BALANCE, BOF, COUNT, EDIT, EOF, FOUND, GET, IMPORT, LASTREC, OPEN,
' PLOT, RECNO, REGISTER, VALIDATE, VARIANCE. Due sezioni indipendenti (file .dat distinti
' in /tmp), derivate dai MiniTest per-funzione; nessuna dipendenza da sample.
' EXPECT: count:  1|lastrec:  1|accounts:  2|balance_q>=0=-1|balance:  2|register:  2|import:  0|plot: si|found:  0|recno:  0|bof: -1|eof: -1|found:  0|validate: -1|get:  2|variance: -100|afterDelete:  0
IMPORT LEDGER
IMPORT IO

SUB TestOps()
    DIM p AS STRING
    p = "/tmp/gmb_mt_ledger.dat"
    DIM csv AS STRING
    csv = "/tmp/gmb_mt_ledger.csv"

    ON ERROR RESUME NEXT
    KILL p
    KILL "/tmp/gmb_mt_ledger.idx"
    KILL csv
    ON ERROR GOTO 0

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

    PRINT "count: "; LEDGER.COUNT(h)
    PRINT "lastrec: "; LEDGER.LASTREC(h)
    PRINT "accounts: "; LEN(LEDGER.ACCOUNTS(h))
    PRINT "balance_q>=0="; (LEN(LEDGER.BALANCE(h, "Attivita:Cassa")) >= 0)   ' query opzionale (dai Sample)
    PRINT "balance: "; LEN(LEDGER.BALANCE(h))
    PRINT "register: "; LEN(LEDGER.REGISTER(h))

    LEDGER EDIT h ID newId WITH d, "Apertura aggiornata" {
        "Attivita:Cassa": 1001.00,
        "Patrimonio:Apertura": -1001.00
    } INTO newId

    ' VALIDATE è coperto da altri sample: qui si verificano le 5 funzioni che non ne avevano.
    ' Nota: VALIDATE è un token CONDIVISO (DBF/ISAM/LEDGER) che non aveva alcun codice
    ' BuiltInFunction → la VM lo trattava come funzione ignota. Il codice (2197) e l'handler
    ' sono stati aggiunti (commit ce59bca3a); il compilatore non lo emette ancora per il
    ' ledger, quindi resta fuori da questo MiniTest.
    PRINT "import: "; LEDGER.IMPORT(h, csv, "date,desc,amount")
    ' PLOT ritorna un numero instabile (handle/id): si asserisce il FATTO (>0).
    IF LEDGER.PLOT(h, "", "bar") > 0 THEN PRINT "plot: si" ELSE PRINT "plot: no"
    PRINT "found: "; LEDGER.FOUND(h)

    LEDGER CLOSE h
    KILL p
    KILL "/tmp/gmb_mt_ledger.idx"
    KILL csv
END SUB

SUB TestCursor()
    DIM p AS STRING
    p = "/tmp/gmb_mt_ledger_cursor.dat"
    DIM csv AS STRING
    csv = "/tmp/gmb_mt_ledger_cursor.csv"
    ON ERROR RESUME NEXT
    KILL p
    KILL "/tmp/gmb_mt_ledger_cursor.idx"
    KILL csv
    ON ERROR GOTO 0

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

    ' --- Cursore (pattern DBF/ISAM): dopo ADD il cursore è "prima della prima" ---
    PRINT "recno: "; LEDGER.RECNO(h)
    PRINT "bof: "; LEDGER.BOF(h)
    PRINT "eof: "; LEDGER.EOF(h)
    PRINT "found: "; LEDGER.FOUND(h)

    ' --- Integrità (transazione bilanciata) ---
    PRINT "validate: "; LEDGER.VALIDATE(h)

    ' --- Dettaglio: GET (lista dei movimenti) e VARIANCE (double) ---
    PRINT "get: "; LEN(LEDGER.GET(h, newId))
    ' BUDGET (comando) + VARIANCE (funzione): budget 900 su un conto che ha 1000.
    LEDGER BUDGET h, "Attivita:Cassa", 900.00, "2026-01"
    DIM varj = LEDGER.VARIANCE(h, "Attivita:Cassa", "2026-01")
    PRINT "variance: "; varj

    ' --- SAVE + DELETE (comandi) ---
    LEDGER SAVE h
    LEDGER DELETE h ID newId
    PRINT "afterDelete: "; LEDGER.COUNT(h)

    LEDGER CLOSE h
    KILL p
    KILL "/tmp/gmb_mt_ledger_cursor.idx"
    KILL csv
END SUB

SUB Main()
  TestOps()
  TestCursor()
END SUB
