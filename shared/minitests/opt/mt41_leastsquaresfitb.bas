' EXPECT: v= 1 , 2
' OPTIMIZE.LEASTSQUARESFITB — MiniTest §4.1 (parità 4 backend).
' CONTRATTO: out() è dimensionato al NUMERO DI RESIDUI (m) che la callback restituisce; la
' soluzione (i parametri) si legge in out(0..n-1). Con out() più piccolo di m il motore non
' converge (200 iterazioni, nessun risultato): è l'errore che questo test verifica.
IMPORT OPTIMIZE

FUNCTION Res(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) - 1.0, v(0) + v(1) - 3.0, v(0) + 2.0 * v(1) - 5.0, v(0) + 3.0 * v(1) - 7.0, v(0) + 4.0 * v(1) - 9.0}
END FUNCTION

SUB Main()
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM bnd(3) AS DOUBLE
    bnd(0) = 0.0 : bnd(1) = 5.0 : bnd(2) = 0.0 : bnd(3) = 5.0
    DIM o(4) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARESFITB(Res, x0, bnd, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
