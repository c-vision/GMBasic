' EXPECT: cost=8.0|stabile= 1
' TSPGA 10 volte: il costo deve essere SEMPRE lo stesso (determinismo) e il tour valido.
IMPORT OPTIMIZE
FUNCTION IsPerm(t() AS LONG, n AS LONG) AS LONG
  DIM visto(63) AS LONG
  FOR i = 0 TO n - 1
    v = t(i)
    IF v < 0 OR v > n - 1 THEN RETURN 0
    IF visto(v) <> 0 THEN RETURN 0
    visto(v) = 1
  NEXT i
  RETURN 1
END FUNCTION
SUB Main()
  DIM d(24) AS DOUBLE
  DIM t(4) AS LONG
  ok = 1 : c0 = -1
  FOR r = 1 TO 10
    FOR i = 0 TO 4
      FOR j = 0 TO 4
        d(i*5+j) = ABS(i-j)
      NEXT j
    NEXT i
    FOR i = 0 TO 4 : t(i) = i : NEXT i
    c = OPTIMIZE.TSPGA(d, 5, t, 12, 30)
    IF c0 < 0 THEN c0 = c
    IF c <> c0 THEN ok = 0
    IF IsPerm(t, 5) = 0 THEN ok = 0
  NEXT r
  PRINT "cost="; FORMAT(c0, "#.0"); "|stabile="; ok
  PRINT ""
END SUB
