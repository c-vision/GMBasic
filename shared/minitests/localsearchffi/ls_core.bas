' OPTIMIZE (FFI localsearch) — le 5 funzioni: LSSOLVE, LSSOLVECFG (7 acceptor), LSNEAREST,
' LSCHEAPEST, LSNOWMS. Stato = permutazione di 0..3; punteggio = somma i*state(i) (max 14).
' EXPECT: solve= 1 :-1 14.0:-1||stato= 0  1  2  3|a 0 = 1 :14.0 a 1 = 1 :14.0 a 2 = 1 :14.0 a 3 = 1 :14.0 a 4 = 1 :14.0 a 5 = 1 :14.0 a 6 = 1 :14.0|near= 1 : 0  1  2  3  4 |cheap= 1 : 0  4  3  2  1|now=-1 -1
IMPORT OPTIMIZE

FUNCTION Score(state() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(state)
    s = s + i * state(i)
  NEXT i
  RETURN s
END FUNCTION

SUB Main()
  ' ── LSSOLVE: hill climbing, 1 partenza. out() = hard, soft, steps ───────────────
  DIM init(3) AS LONG
  init(0)=3 : init(1)=2 : init(2)=1 : init(3)=0
  DIM o(2) AS DOUBLE
  r = OPTIMIZE.LSSOLVE(Score, init, 4, 0, 200, o)
  PRINT "solve="; r; ":"; (o(0) = 0); FORMAT(o(1), "#.0"); ":"; (o(2) > 0)
  ' initial() e' IN/OUT: contiene il migliore stato trovato (0,1,2,3 -> punteggio 14)
  PRINT "|stato="; init(0); init(1); init(2); init(3)

  ' ── LSSOLVECFG: i 7 acceptor (0..6) con 3 partenze ──────────────────────────────
  DIM o2(2) AS DOUBLE
  FOR a = 0 TO 6
    r = OPTIMIZE.LSSOLVECFG(Score, init, 4, 0, 300, 3, a, o2)
    PRINT "a"; a; "="; r; ":"; FORMAT(o2(1), "#.0"); " ";
  NEXT a
  PRINT ""

  ' ── LSNEAREST / LSCHEAPEST: 5 città su una retta (distanze |i-j|) ───────────────
  DIM d(24) AS DOUBLE
  FOR i = 0 TO 4
    FOR j = 0 TO 4
      d(i*5+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM perm(4) AS LONG
  PRINT "near="; OPTIMIZE.LSNEAREST(d, 5, 0, perm); ":";
  FOR i = 0 TO 4
    PRINT perm(i);
  NEXT i
  PRINT "|cheap="; OPTIMIZE.LSCHEAPEST(d, 5, perm); ":";
  FOR i = 0 TO 4
    PRINT perm(i);
  NEXT i
  PRINT ""

  ' ── LSNOWMS: tempo CPU in ms, monotono non decrescente ──────────────────────────
  t0 = OPTIMIZE.LSNOWMS()
  FOR i = 1 TO 20000 : NEXT i
  t1 = OPTIMIZE.LSNOWMS()
  PRINT "now="; (t0 >= 0); (t1 >= t0)
  PRINT ""
END SUB
