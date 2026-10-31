' EXPECT: 1|yes
IMPORT COMPUTER

SUB Main()
    DIM w$ AS STRING
    w$ = COMPUTER.WINDOWS()
    ' assert strutturale (non snapshot): JSON array non vuoto e ben formato.
    IF LEN(w$) > 2 THEN
        IF LEFT$(w$, 1) = "[" AND RIGHT$(w$, 1) = "]" THEN
            PRINT 1
        ELSE
            PRINT 0
        END IF
    ELSE
        PRINT 0
    END IF
    ' contenuto dipendente dall'ambiente: mai stampato (escape \uXXXX diverso fra mondi)
    IF LEN(w$) > 2 THEN PRINT "yes" ELSE PRINT "no"
END SUB
