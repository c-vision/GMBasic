' MiniTest ISAM DOUBLE round-trip APPEND -> GET (+ filtro numerico sul DOUBLE)
' Bug: Bugs/(open) - ISAM-Double-Troncato-Backend-C.md — sui backend C il DOUBLE tornava troncato a intero
' (19.99 -> 19): lo schema C collassava N/F/I/B su LongType.
' EXPECT: P001|19.99|100;P002|-3.25|50;P003|0.125|0;|filtrati:2
IMPORT ISAM

TYPE DRec
    Code AS STRING * 8
    Price AS DOUBLE
    Qty AS LONG
END TYPE

DIM p AS DRec
DIM n AS LONG
DIM s AS STRING

SUB Main()
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isamdbl.isdb"
    ON ERROR GOTO 0

    ISAM OPEN "/tmp/gmb_mt_isamdbl.isdb" OF DRec ALIAS PR INDEXED BY Code
    p.Code = "P001" : p.Price = 19.99 : p.Qty = 100 : ISAM APPEND PR, p
    p.Code = "P002" : p.Price = -3.25 : p.Qty = 50  : ISAM APPEND PR, p
    p.Code = "P003" : p.Price = 0.125 : p.Qty = 0   : ISAM APPEND PR, p

    ISAM GO PR, TOP
    s = ""
    n = 0
    WHILE NOT ISAM.EOF(PR)
        ISAM GET PR, p
        s = s + RTRIM$(p.Code) + "|" + LTRIM$(STR$(p.Price)) + "|" + LTRIM$(STR$(p.Qty)) + ";"
        ISAM SKIP PR
        n = n + 1
        IF n >= 10 THEN EXIT
    WEND

    ISAM SET FILTER TO Price > 0 ON -1
    ISAM GO PR, TOP
    n = 0
    WHILE NOT ISAM.EOF(PR)
        n = n + 1
        ISAM SKIP PR
        IF n >= 10 THEN EXIT
    WEND
    PRINT s + "|filtrati:" + LTRIM$(STR$(n))

    ISAM CLOSE
END SUB
