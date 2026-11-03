' EXPECT: w2= 2|w3= 6
' Invocazione di una callback (FUNCTION parametrica) con argomenti ARRAY dentro una
' procedura utente (2026-11-03): su C99 l'array finiva sullo stack come gmb_val_t
' (lista) e il bound non veniva pubblicato; su IL il delegate ref non era adattato.
FUNCTION Ros(p() AS DOUBLE) AS DOUBLE
    RETURN p(0) * p(0) + p(1) * p(1)
END FUNCTION
FUNCTION W2(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS DOUBLE
    RETURN f(x0) + f({1.0, 0.0})
END FUNCTION
FUNCTION W3(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS DOUBLE
    DIM y(1) AS DOUBLE = {2.0, 1.0}
    RETURN f(y) + f(x0)
END FUNCTION
SUB Main()
    DIM x0(1) AS DOUBLE = {1.0, 0.0}
    PRINT "w2="; W2(Ros, x0)
    PRINT "w3="; W3(Ros, x0)
END SUB
