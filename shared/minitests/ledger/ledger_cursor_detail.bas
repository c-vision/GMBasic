' MiniTest LEDGER — cursore (BOF/EOF/RECNO/FOUND), integrità (VALIDATE), dettaglio
' (GET/VARIANCE) e comandi SAVE/DELETE. Complementa minitests/ledger/ledger_ops.bas
' (che copre COUNT/LASTREC/ACCOUNTS/BALANCE/REGISTER/IMPORT/PLOT/ADD/EDIT/CLOSE).
' Path assoluti in /tmp: la CWD e' la cartella del file su tutte le gambe; stato
' azzerato prima del run (file/.idx/csv).
' EXPECT: recno:  0|bof: -1|eof: -1|found:  0|validate: -1|get:  2|variance: ok|afterDelete:  0
IMPORT LEDGER
IMPORT IO

SUB Main()
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
    DIM varj = LEDGER.VARIANCE(h, "Attivita:Cassa", "2026-01")
    PRINT "variance: ok"

    ' --- SAVE + DELETE (comandi) ---
    LEDGER SAVE h
    LEDGER DELETE h ID newId
    PRINT "afterDelete: "; LEDGER.COUNT(h)

    LEDGER CLOSE h
    KILL p
    KILL "/tmp/gmb_mt_ledger_cursor.idx"
    KILL csv
END SUB
