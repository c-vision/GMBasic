' EXPECT: r= 120
' Regressione: una chiamata QUALIFICATA dentro un'altra chiamata (MATH.ROUND ha il
' parametro value AS CURRENCY) riscriveva gli argomenti col contesto esterno:
' il letterale 3 diventava CCUR(3) e su IL arrivava 0 al parametro k AS LONG.
IMPORT QUALCALL
SUB Main()
    DIM a(2) AS DOUBLE
    a(0) = 10: a(1) = 20: a(2) = 30
    PRINT "r="; MATH.ROUND(QUALCALL.SCALED(a, 3), 6)
END SUB
