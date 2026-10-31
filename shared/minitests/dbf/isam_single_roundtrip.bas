' MiniTest ISAM SINGLE round-trip APPEND -> GET (4 byte float come il C#; sui backend C collassava su LongType).
' (STR$ di un SINGLE in PCode dava 0: corretto, vedi minitests/pcode_single_str.bas.)
IMPORT ISAM

TYPE SRec
    Code AS STRING * 8
    Ratio AS SINGLE
END TYPE

DIM p AS SRec
DIM n AS LONG

SUB Main()
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isamsng.isdb"
    ON ERROR GOTO 0

    ISAM OPEN "/tmp/gmb_mt_isamsng.isdb" OF SRec ALIAS PR INDEXED BY Code
    p.Code = "A" : p.Ratio = 0.5  : ISAM APPEND PR, p
    p.Code = "B" : p.Ratio = 1.25 : ISAM APPEND PR, p
    p.Code = "C" : p.Ratio = -2.5 : ISAM APPEND PR, p

    ISAM GO PR, TOP
    n = 0
    WHILE NOT ISAM.EOF(PR)
        ISAM GET PR, p
        PRINT p.Ratio
        ISAM SKIP PR
        n = n + 1
        IF n >= 10 THEN EXIT
    WEND

    ISAM CLOSE
END SUB
