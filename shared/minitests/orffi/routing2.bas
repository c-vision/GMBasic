' ORTOOLS (FFI) — routing: MiniTest §4.1 della 2ª fascia.
' TSPEXTRAMILEAGE, TSPGRASP, TSPVNS, TSP3OPT, TSPSA, TSPTABU, TSPHKBOUND, TSPDFJ, TSPERX,
' TSPGA, TSPOROPT, VRPTIMEWINDOWS, VRPMULTIDEPOT, VRPPICKUPDELIVERY.
' Istanze piccole con distanze intere (|i-j|): costo del giro ottimo = 8.
' EXPECT: extra=8.0: 1 |grasp=8.0: 1 |vns=8.0: 1 |3opt=8.0: 1 |sa=8.0: 1 |tabu=8.0: 1 |hk=7.9:-1 |dfj=8.0: 1 |oropt=8.0: 1 |ga=8.0: 1 |erx= 1 : 1 :-1|vrptw= 2 :10.0: 0  1 |mdvrp= 1 :6.0: 0  0 |pdp= 1 :4.0: 1  2  1  1 :-1
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

' Lunghezza del ciclo t() sulla matrice d (n*n).
FUNCTION Lung(t() AS LONG, n AS LONG, d() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO n - 1
    a = t(i)
    IF i = n - 1 THEN
      b = t(0)
    ELSE
      b = t(i + 1)
    END IF
    s = s + d(a*n+b)
  NEXT i
  RETURN s
END FUNCTION

SUB Main()
  DIM d(24) AS DOUBLE
  FOR i = 0 TO 4
    FOR j = 0 TO 4
      d(i*5+j) = ABS(i-j)
    NEXT j
  NEXT i

  ' ── TSP: 9 euristiche/metodi, tutte su 5 città ────────────────────────────────────
  DIM t(4) AS LONG
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "extra="; FORMAT(OPTIMIZE.TSPEXTRAMILEAGE(d, 5, t), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "|grasp="; FORMAT(OPTIMIZE.TSPGRASP(d, 5, t, 60), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|vns="; FORMAT(OPTIMIZE.TSPVNS(d, 5, t, 60), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|3opt="; FORMAT(OPTIMIZE.TSP3OPT(d, 5, t, 60), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|sa="; FORMAT(OPTIMIZE.TSPSA(d, 5, t, 5.0, 0.99, 300), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|tabu="; FORMAT(OPTIMIZE.TSPTABU(d, 5, t, 60, 1), "#.0"); ":"; IsPerm(t, 5);
  DIM duali(4) AS DOUBLE          ' non `pi`: PI è la costante, non una variabile
  PRINT "|hk="; FORMAT(OPTIMIZE.TSPHKBOUND(d, 5, 50, duali), "#.0"); ":"; (duali(0) >= 0);
  FOR i = 0 TO 4 : t(i) = 0 : NEXT i
  PRINT "|dfj="; FORMAT(OPTIMIZE.TSPDFJ(d, 5, t, 10), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "|oropt="; FORMAT(OPTIMIZE.TSPOROPT(d, 5, t, 60), "#.0"); ":"; IsPerm(t, 5);
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "|ga="; FORMAT(OPTIMIZE.TSPGA(d, 5, t, 12, 30), "#.0"); ":"; IsPerm(t, 5);

  ' ERX: crossover di due genitori -> figlio permutazione
  DIM p1(4) AS LONG
  DIM p2(4) AS LONG
  DIM ch(4) AS LONG
  FOR i = 0 TO 4
    p1(i) = i
    p2(i) = 4 - i
  NEXT i
  PRINT "|erx="; OPTIMIZE.TSPERX(p1, p2, 5, ch); ":"; IsPerm(ch, 5); ":"; (Lung(ch, 5, d) >= 8.0)

  ' ── VRP: finestre temporali, multi-deposito, pickup&delivery ──────────────────────
  ' VRPTW: n = NUMERO DI CLIENTI (il deposito è l'indice 0), dist (n+1)x(n+1)
  DIM dem(4) AS DOUBLE
  FOR i = 0 TO 4 : dem(i) = 1 : NEXT i
  DIM tw(11) AS DOUBLE
  FOR i = 0 TO 3
    tw(i) = 0
    tw(4+i) = 100
    tw(8+i) = 0
  NEXT i
  DIM ro(3) AS LONG
  DIM tot(0) AS DOUBLE
  PRINT "vrptw="; OPTIMIZE.VRPTIMEWINDOWS(d, dem, 4, 3.0, 0, tw, ro, tot); ":";
  PRINT FORMAT(tot(0), "#.0"); ":"; ro(0); ro(1);

  DIM ro2(4) AS LONG
  DIM tot2(0) AS DOUBLE
  PRINT "|mdvrp="; OPTIMIZE.VRPMULTIDEPOT(d, dem, 5, 2, 3.0, ro2, tot2); ":";
  PRINT FORMAT(tot2(0), "#.0"); ":"; ro2(0); ro2(1);

  ' PDP: pickup()/delivery() = flag 0/1 per nodo (nodi 1..2*nreq), route() = 2*nreq
  DIM demp(4) AS DOUBLE
  FOR i = 0 TO 4 : demp(i) = 1 : NEXT i
  DIM pk(4) AS LONG
  DIM dl(4) AS LONG
  pk(1)=1 : pk(3)=1
  dl(2)=1 : dl(4)=1
  DIM rt(3) AS LONG
  DIM tot3(0) AS DOUBLE
  ptr = 2
  PRINT "|pdp="; OPTIMIZE.VRPPICKUPDELIVERY(d, demp, ptr, 4.0, pk, dl, rt, tot3); ":";
  PRINT FORMAT(tot3(0), "#.0"); ":"; rt(0); rt(1); rt(2); rt(3); ":"; (tot3(0) >= 0)
  PRINT ""
END SUB
