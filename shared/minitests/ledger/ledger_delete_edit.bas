' MiniTest LEDGER — DELETE/EDIT aggiornano indice e saldi (e persistono).
' Regressione 2026-11-03: DELETE append-iva uno storno al journal senza toccare l'indice
' (COUNT/VARIANCE invariati, anche dopo SAVE/CLOSE/OPEN) ed EDIT creava un NUOVO id
' (COUNT 1 → 2). Semantica canonica verificata qui:
'   - DELETE: il blocco diventa storno (`data * desc`, storia) e NON è più una transazione
'     attiva → COUNT -1, VARIANCE/BALANCE lo escludono; PACK scarta gli storni;
'   - EDIT: sostituisce la transazione mantenendo lo STESSO id e lo stesso COUNT.
'
' EXPECT: id1= 1 |count= 1 |var=-300|countD= 0 |varD= 0|countR= 0 |varR= 0|id2= 1 |countE= 1 |varE=-500|idE= 1 |countE2= 1 |varE2=-501|countP= 1 |varP=-501
IMPORT LEDGER

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_ledger_de.dat"

    ON ERROR RESUME NEXT
    KILL p
    KILL "/tmp/gmb_mt_ledger_de.idx"
    ON ERROR GOTO 0

    DIM h = LEDGER.OPEN(p)
    DIM id1 AS LONG
    LEDGER ADD h WITH "2026-02-10", "Farina" {
        "Spese:Farina": 300,
        "Attivita:Cassa": -300
    } INTO id1
    PRINT "id1="; id1; "|count="; LEDGER.COUNT(h); "|var="; LEDGER.VARIANCE(h, "Spese:Farina", "2026-02")

    LEDGER DELETE h ID id1
    PRINT "countD="; LEDGER.COUNT(h); "|varD="; LEDGER.VARIANCE(h, "Spese:Farina", "2026-02")

    LEDGER SAVE h
    LEDGER CLOSE h
    DIM h2 = LEDGER.OPEN(p)
    PRINT "countR="; LEDGER.COUNT(h2); "|varR="; LEDGER.VARIANCE(h2, "Spese:Farina", "2026-02")

    DIM id2 AS LONG
    LEDGER ADD h2 WITH "2026-02-11", "Zucchero" {
        "Spese:Zucchero": 500,
        "Attivita:Cassa": -500
    } INTO id2
    PRINT "id2="; id2; "|countE="; LEDGER.COUNT(h2); "|varE="; LEDGER.VARIANCE(h2, "Spese:Zucchero", "2026-02")

    DIM idE AS LONG
    LEDGER EDIT h2 ID id2 WITH "2026-02-12", "Zucchero corretto" {
        "Spese:Zucchero": 501,
        "Attivita:Cassa": -501
    } INTO idE
    PRINT "idE="; idE; "|countE2="; LEDGER.COUNT(h2); "|varE2="; LEDGER.VARIANCE(h2, "Spese:Zucchero", "2026-02")

    LEDGER PACK h2
    PRINT "countP="; LEDGER.COUNT(h2); "|varP="; LEDGER.VARIANCE(h2, "Spese:Zucchero", "2026-02")
    LEDGER CLOSE h2
END SUB
