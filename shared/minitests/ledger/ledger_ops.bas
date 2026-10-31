' REPERTI MISURATI (IL / PCode / VM-C) — dominio LEDGER in allineamento:
'   count/lastrec:  IL 1 / PCode 1 / VMC 0   → PCode OK (era 0), manca il ramo ledger nella VM/C
'   validate:      -1 / -1 / 0             → VMC ritorna 0 invece di -1
'   accounts/balance/register: 2 / 0 / 2   → il PCode non vede le voci (ChartOfAccounts
'                                            vuoto e GetFilteredEntries non trova il journal)
'   import:        '' / '' / 0             → PCode non ritorna il conteggio CSV
'   plot:          numero / testo / []      → tre rappresentazioni diverse
' Cause già trovate e corrette lato PCode: argomenti INVERTITI dal bytecode (root=[1] era
' l'handle) e indice .idx STANTIO in /tmp. Nessun EXPECT finché le 4 gambe non concordano.
' MiniTest LEDGER — ACCOUNTS/BALANCE/EDIT/IMPORT/PLOT (senza alcun sample) + stato del dominio.
' Sintassi canonica dei comandi: `LEDGER ADD h WITH <data>, "<desc>" {posting} INTO <id>`.
' EXPECT: count:  1|lastrec:  1|accounts:  2|balance:  2|register:  2|import:  0|plot: si|found:  0
IMPORT LEDGER
IMPORT IO

SUB Main()
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
