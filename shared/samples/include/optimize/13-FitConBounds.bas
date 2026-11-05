REM FILE: 13-FitConBounds.bas — pacchetto `opt` (FFI): minimi quadrati NON lineari con bounds.
REM Scenario didattico: la stessa retta y = a + b*x stimata (1) senza vincoli e (2) con i
REM parametri confinati in un box; il confronto mostra l'effetto del bound.
REM CONTRATTO: out() va dimensionato al numero di RESIDUI (qui 5), e i parametri stimati si
REM leggono in out(0..n-1) — dimensionarlo piu' corto fa fallire la convergenza.
IMPORT OPTIMIZE

' Residui del modello y = a + b*x sui punti (0,3) (1,5) (2,7) (3,9) (4,11) → a=3, b=2? no: 1+2x
FUNCTION Res(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) - 1.0, v(0) + v(1) - 3.0, v(0) + 2.0 * v(1) - 5.0, v(0) + 3.0 * v(1) - 7.0, v(0) + 4.0 * v(1) - 9.0}
END FUNCTION

SUB Main()
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM o(4) AS DOUBLE

    ' 1) senza bounds: (a,b) = (1,2)
    CALL OPTIMIZE.LEASTSQUARESFIT(Res, x0, o)
    PRINT "fit: a="; FORMAT(o(0), "#.000"); " b="; FORMAT(o(1), "#.000");

    ' 2) con bounds [0,5] su entrambi: stesso ottimo, ma il box impedisce valori fuori scala
    DIM bnd(3) AS DOUBLE
    bnd(0) = 0.0 : bnd(1) = 5.0 : bnd(2) = 0.0 : bnd(3) = 5.0
    CALL OPTIMIZE.LEASTSQUARESFITB(Res, x0, bnd, o)
    PRINT "|bounds: a="; FORMAT(o(0), "#.000"); " b="; FORMAT(o(1), "#.000");

    ' 3) box troppo stretto (b <= 0.5): la stima si ferma sul bordo del dominio ammesso
    DIM bnd2(3) AS DOUBLE
    bnd2(0) = 0.0 : bnd2(1) = 5.0 : bnd2(2) = 0.0 : bnd2(3) = 0.5
    CALL OPTIMIZE.LEASTSQUARESFITB(Res, x0, bnd2, o)
    PRINT "|stretto: a="; FORMAT(o(0), "#.000"); " b="; FORMAT(o(1), "#.000");
    PRINT "|b<=0.5="; (o(1) <= 0.5001)
    PRINT ""
END SUB
