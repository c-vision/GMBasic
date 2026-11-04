' ORTOOLS (FFI) — cover + cp: MiniTest §4.1 della 2ª fascia.
' SETCOVERSOFT, SETCOVERPRICING, SETCOVERSA, SETCOVERLOCAL, SETCOVERGRASP, SETCOVERRGRASP,
' MAXCOVERAGE, PMEDIAN, VERTEXCOVER, HITTINGSET, FACILITYLOCATION, CPBINARY.
' elem[i*n + j] = 1 se l'elemento i è coperto dall'insieme j (m elementi, n insiemi).
' EXPECT: soft=4.0: 0 |price=-3.0: 1  1  0 |sa=4.0: 1 : 1  1 |loc=4.0: 1 : 2 |grasp=4.0: 1 : 2 |rgrasp=4.0: 1 : 2 |maxcov= 2 : 1 |pmed=2.0: 0  3 |vc= 2 : 1 |hit= 2 : 1 |fac=13.0: 2 |cp= 1 : 0  0
IMPORT OPTIMIZE

DIM E(5) AS LONG
DIM C(1) AS DOUBLE

' TRUE se gli insiemi scelti (LISTA di indici, nChosen attivi) coprono tutti gli m elementi.
FUNCTION Copre(chosen() AS LONG, nc AS LONG, m AS LONG) AS LONG
  FOR i = 0 TO m - 1
    ok = 0
    FOR k = 0 TO nc - 1
      IF E(i*2+chosen(k)) <> 0 THEN ok = 1
    NEXT k
    IF ok = 0 THEN RETURN 0
  NEXT i
  RETURN 1
END FUNCTION

' Variante MASCHERA: chosen(j) <> 0 = insieme j scelto.
FUNCTION CopreMask(chosen() AS LONG, m AS LONG) AS LONG
  FOR i = 0 TO m - 1
    ok = 0
    FOR j = 0 TO 1
      IF chosen(j) <> 0 THEN
        IF E(i*2+j) <> 0 THEN ok = 1
      END IF
    NEXT j
    IF ok = 0 THEN RETURN 0
  NEXT i
  RETURN 1
END FUNCTION

SUB Main()
  ' set0 = {e0}, set1 = {e1,e2}  ->  scelti entrambi: costo 1+3 = 4
  E(0)=1 : E(1)=0
  E(2)=0 : E(3)=1
  E(4)=0 : E(5)=1
  C(0)=1 : C(1)=3

  DIM ch(1) AS LONG
  ch(0)=1 : ch(1)=1
  DIM unc(0) AS LONG
  PRINT "soft="; FORMAT(OPTIMIZE.SETCOVERSOFT(E, C, 3, 2, ch, 10.0, unc), "#.0"); ":"; unc(0);

  ' pricing: y = (5,3,-1), costo = base 1 + 2 per elemento -> colonna {e0,e1}, ridotto -3
  DIM y(2) AS DOUBLE
  y(0)=5 : y(1)=3 : y(2)=-1
  DIM cs(2) AS LONG
  PRINT "|price="; FORMAT(OPTIMIZE.SETCOVERPRICING(y, 3, 1.0, 2.0, cs), "#.0"); ":";
  PRINT cs(0); cs(1); cs(2);

  ' SA / local / GRASP / reactive GRASP: tutte devono coprire i 3 elementi
  DIM c1(1) AS LONG
  DIM nc(0) AS LONG
  PRINT "|sa="; FORMAT(OPTIMIZE.SETCOVERSA(E, C, 3, 2, 10.0, 200, 1, c1), "#.0"); ":"; CopreMask(c1, 3); ":"; c1(0); c1(1);
  ' pipeline realistica: greedy -> local search
  DIM gch(1) AS LONG
  DIM ngch(0) AS LONG
  cg = OPTIMIZE.SETCOVER(E, C, 3, 2, gch, ngch)
  DIM c2(1) AS LONG
  c2(0)=gch(0) : c2(1)=gch(1)
  nc(0)=ngch(0)
  PRINT "|loc="; FORMAT(OPTIMIZE.SETCOVERLOCAL(E, C, 3, 2, c2, nc, 50), "#.0"); ":";
  PRINT Copre(c2, nc(0), 3); ":"; nc(0);
  DIM c3(1) AS LONG
  DIM nc3(0) AS LONG
  PRINT "|grasp="; FORMAT(OPTIMIZE.SETCOVERGRASP(E, C, 3, 2, 0.3, 50, c3, nc3), "#.0"); ":";
  PRINT Copre(c3, nc3(0), 3); ":"; nc3(0);
  DIM al(2) AS DOUBLE
  al(0)=2 : al(1)=0.2 : al(2)=0.8
  DIM c4(1) AS LONG
  DIM nc4(0) AS LONG
  PRINT "|rgrasp="; FORMAT(OPTIMIZE.SETCOVERRGRASP(E, C, 3, 2, al, 50, c4, nc4), "#.0"); ":";
  PRINT Copre(c4, nc4(0), 3); ":"; nc4(0);

  ' max coverage con k=1: vince set1 (copre e1,e2) -> 2 elementi
  DIM mc(1) AS LONG
  DIM nmc(0) AS LONG
  PRINT "|maxcov="; OPTIMIZE.MAXCOVERAGE(E, 3, 2, 1, mc, nmc); ":"; nmc(0);

  ' p-median su 4 punti in linea (p=2): costo 2 (mediane in 1 e 2)
  DIM d(15) AS DOUBLE
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      d(i*4+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM med(1) AS LONG
  DIM asg(3) AS LONG
  PRINT "|pmed="; FORMAT(OPTIMIZE.PMEDIAN(d, 4, 2, med, asg, 50), "#.0"); ":";
  PRINT med(0); med(1);

  ' vertex cover del triangolo: 2 vertici
  DIM ed(5) AS LONG
  ed(0)=0 : ed(1)=1
  ed(2)=1 : ed(3)=2
  ed(4)=0 : ed(5)=2
  DIM cov(2) AS LONG
  r = OPTIMIZE.VERTEXCOVER(ed, 3, 3, cov)
  PRINT "|vc="; r; ":"; cov(0)+cov(1)+cov(2);

  DIM hc(1) AS LONG
  DIM nhc(0) AS LONG
  PRINT "|hit="; OPTIMIZE.HITTINGSET(E, 3, 2, hc, nhc); ":"; Copre(hc, nhc(0), 3);

  ' facility location: aprire entrambe conviene (13 < 15)
  DIM oc(1) AS DOUBLE
  oc(0)=5 : oc(1)=6
  DIM cc(3) AS DOUBLE
  cc(0)=1 : cc(1)=9 : cc(2)=9 : cc(3)=1
  DIM op(1) AS LONG
  PRINT "|fac="; FORMAT(OPTIMIZE.FACILITYLOCATION(oc, cc, 2, 2, op), "#.0"); ":";
  PRINT op(0)+op(1);

  ' CP binaria: 2 variabili, D=2, ammessa solo la coppia di valori uguali
  DIM dm(3) AS LONG
  dm(0)=0 : dm(1)=1 : dm(2)=0 : dm(3)=1
  DIM alw(15) AS LONG
  FOR i = 0 TO 15 : alw(i) = 0 : NEXT i   ' inizializzazione esplicita (niente zeri impliciti)
  alw((0*2+1)*4 + 0*2 + 0) = 1
  alw((0*2+1)*4 + 1*2 + 1) = 1
  DIM sol(1) AS LONG
  PRINT "|cp="; OPTIMIZE.CPBINARY(dm, alw, 2, 2, sol); ":"; sol(0); sol(1)
  PRINT ""
END SUB
