' EXPECT: 3|99|3|99
' MiniTest: FUNCTION che ritorna una tupla nominata — AS (lo AS DOUBLE, hi AS DOUBLE)
' Copre l'accesso sia per nome (result.lo) sia per indice (result(0)). Parità 5/5.
IMPORT CONSOLE

FUNCTION GetBounds() AS (lo AS DOUBLE, hi AS DOUBLE)
    RETURN (lo: 3, hi: 99)
END FUNCTION

SUB Main()
    DIM result = GetBounds()
    PRINT result.lo
    PRINT result.hi
    PRINT result(0)
    PRINT result(1)
END SUB
