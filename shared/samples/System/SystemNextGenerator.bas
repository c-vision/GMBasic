' FILE: System/SystemNextGenerator.bas
' SYSTEM.Next + GENERATOR PROCEDURE coverage.
IMPORT SYSTEM
GENERATOR RangeGen(n AS LONG) AS LONG
    DIM i AS LONG
    FOR i = 1 TO n
        YIELD i
    NEXT i
END GENERATOR
SUB Main()
    DIM it = RangeGen(5)
    DIM total AS LONG = 0
    DO
        TRY
            total = total + NEXT(it)
        CATCH e AS ERR WHEN e.Number = 144
            EXIT DO
        END TRY
    LOOP
    PRINT "Next sum:   "; total
    PRINT "Test Completato."
END SUB