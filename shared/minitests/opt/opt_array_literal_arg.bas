' EXPECT: sum= 3|n= 3   14|s=x|i= 7
' Array literal `{…}` come argomento di una procedura utente con parametro array
' (2026-11-03: su C99 dava "l'argomento array 1 deve essere una variabile array").
FUNCTION Somma(a() AS DOUBLE) AS DOUBLE
    RETURN a(0) + a(1)
END FUNCTION
SUB Stampa(a() AS DOUBLE, n AS DOUBLE)
    PRINT "n="; n; " "; a(0) + a(1) + a(2)
END SUB
FUNCTION Primo(a() AS STRING) AS STRING
    RETURN a(0)
END FUNCTION
SUB Main()
    DIM x AS DOUBLE = 5.0
    PRINT "sum="; Somma({1.0, 2.0})
    CALL Stampa({x, x + 1.0, 3.0}, 3)
    PRINT "s="; Primo({"x", "y"})
    ' elementi interi in un parametro AS DOUBLE (coercizione numerica)
    PRINT "i="; Somma({3, 4})
END SUB
