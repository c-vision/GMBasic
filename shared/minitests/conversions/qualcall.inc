' qualcall.inc — helper per il MiniTest di regressione: funzione qualificata con
' parametro scalare DOPO un array. Il valore deve arrivare col tipo del parametro
' (LONG), non col tipo atteso dalla chiamata che la contiene (MATH.ROUND -> CURRENCY).
NAMESPACE QUALCALL

PUBLIC FUNCTION SCALED(a() AS DOUBLE, k AS LONG) AS DOUBLE
    DIM s AS DOUBLE = 0
    FOR i = 0 TO UBOUND(a)
        s = s + a(i)
    NEXT i
    RETURN s * k
END FUNCTION
