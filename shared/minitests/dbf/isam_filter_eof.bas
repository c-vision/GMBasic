' MiniTest ISAM SET FILTER + ISAM.EOF — regression guard
' Bug: Bugs/(open) - ISAM-Filter-EoF-Loop-NonTermina-e-Filtro-NonApplicato.md
' Filtro "Price > 10 AND Qty > 0" su 3 record → attese 2 righe (P001, P002).
' Attualmente: pcode HANG (ISAM.EOF non diventa mai vero), il 0 righe, vmc/c99n/c99 3 righe.
' (TYPE rinominato PRec: `TYPE P` + `DIM p AS P` collidono — nomi case-insensitive — e il test non compilava su nessuna gamba.)
' Il loop è LIMITATO (guardia n>=10) per non appendere il runner; l'ESITO (righe) deve essere 2.
' EXPECT: righe:  2|P001P002
IMPORT ISAM

TYPE PRec
    Code AS STRING * 8
    Price AS DOUBLE
    Qty AS LONG
END TYPE

DIM p AS PRec
DIM f AS STRING

SUB Main()
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isamfilter.isdb"
    ON ERROR GOTO 0

    ISAM OPEN "/tmp/gmb_mt_isamfilter.isdb" OF PRec ALIAS PR INDEXED BY Code
    p.Code = "P001" : p.Price = 19.99 : p.Qty = 100 : ISAM APPEND PR, p
    p.Code = "P002" : p.Price = 49.99 : p.Qty = 50  : ISAM APPEND PR, p
    p.Code = "P003" : p.Price = 9.99  : p.Qty = 0   : ISAM APPEND PR, p

    f = "Price > 10 AND Qty > 0"
    ISAM SET FILTER TO &(f) ON -1
    ISAM GO PR, TOP

    n = 0
    codes$ = ""
    WHILE NOT ISAM.EOF(PR)
        ISAM GET PR, p
        n = n + 1
        codes$ = codes$ + p.Code
        ISAM SKIP PR
        IF n >= 10 THEN EXIT
    WEND
    PRINT "righe: " + STR$(n) + "|" + codes$

    ISAM CLOSE
END SUB
