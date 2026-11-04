' OPTIMIZE (FFI or) — pack/assign/cover/cp: prima slice di OR combinatoria.
' EXPECT: bins= 2 |lb= 2 |knap=22.0|hung=5.0|transp=20.0|setcover=2.0: 2 |pcenter=1.0|kmeans=1.0:11.0|sat= 1 |alldiff= 1||sat2= 1 |alldiff2= 1
IMPORT OPTIMIZE

SUB Main()
  ' ── PACK ────────────────────────────────────────────────────────────────
  DIM items() = {4, 3, 3, 2}
  DIM binOf(4) AS LONG
  PRINT "bins="; OPTIMIZE.BINPACKING(items, 4, 10.0, 4, binOf);
  PRINT "|lb="; OPTIMIZE.BINPACKINGLB(items, 4, 10.0);

  DIM w() = {1, 2, 3}
  DIM v() = {6, 10, 12}
  DIM take(3) AS LONG
  PRINT "|knap="; FORMAT(OPTIMIZE.KNAPSACK01(w, v, 3, 5, take), "#.0");

  ' ── ASSIGN ──────────────────────────────────────────────────────────────
  DIM cost() = {1, 2, 3, 4}
  DIM asg(2) AS LONG
  PRINT "|hung="; FORMAT(OPTIMIZE.ASSIGNHUNGARIAN(cost, 2, 2, asg), "#.0");

  DIM sup() = {10, 10}
  DIM dem() = {10, 10}
  DIM tcost() = {1, 2, 3, 1}
  DIM plan(4) AS DOUBLE
  PRINT "|transp="; FORMAT(OPTIMIZE.TRANSPORTVAM(sup, dem, tcost, 2, 2, plan), "#.0");

  ' ── COVER ───────────────────────────────────────────────────────────────
  ' 3 elementi, 2 insiemi: set0={e0,e1}, set1={e1,e2}
  DIM elem(6) AS LONG
  elem(0)=1 : elem(1)=0
  elem(2)=0 : elem(3)=1
  elem(4)=1 : elem(5)=1
  DIM scost() = {1, 1}
  DIM chosen(2) AS LONG
  DIM nch(1) AS LONG
  PRINT "|setcover="; FORMAT(OPTIMIZE.SETCOVER(elem, scost, 3, 2, chosen, nch), "#.0"); ":"; nch(0);

  ' p-center su 4 punti in linea (distanze |i-j|), p=2 -> raggio 1
  DIM dist(16) AS DOUBLE
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      dist(i*4+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM med(2) AS LONG
  PRINT "|pcenter="; FORMAT(OPTIMIZE.PCENTER(dist, 4, 2, med), "#.0");

  ' k-means 1D: {0,1,10,11}, k=2 -> somma centroidi 11
  DIM pts() = {0, 1, 10, 11}
  DIM cent(2) AS DOUBLE
  DIM lab(4) AS LONG
  PRINT "|kmeans="; FORMAT(OPTIMIZE.KMEANS(pts, 4, 1, 2, cent, lab, 50), "#.0"); ":"; FORMAT(cent(0)+cent(1), "#.0");

  ' ── CP ──────────────────────────────────────────────────────────────────
  ' (x1 OR x2) AND (NOT x1 OR x2) -> SATDPLL
  DIM clauses(3) AS LONG
  clauses(0)=1 : clauses(1)=2 : clauses(2)=-1 : clauses(3)=2
  DIM cstart(1) AS LONG
  cstart(0)=0 : cstart(1)=2
  DIM clen(1) AS LONG
  clen(0)=2 : clen(1)=2
  DIM asat(2) AS LONG
  PRINT "|sat="; OPTIMIZE.SATDPLL(clauses, cstart, clen, 2, 2, asat);

  ' all-different: 3 variabili, dominio {0,1,2}
  DIM dom(8) AS LONG
  FOR i = 0 TO 8
    dom(i) = i MOD 3
  NEXT i
  DIM sol(3) AS LONG
  PRINT "|alldiff="; OPTIMIZE.CPALLDIFF(dom, 3, 3, sol)

  ' regressione (bug FFI IL, 2026-11-04): array NON tipizzati (double) verso parametri AS LONG
  ' -> il marshalling IL deve convertire (prima passava i bit IEEE-754 al nativo).
  DIM cl2() = {1, 2, -1, 2}
  DIM cs2() = {0, 2}
  DIM ln2() = {2, 2}
  DIM asat2(2) AS LONG
  PRINT "|sat2="; OPTIMIZE.SATDPLL(cl2, cs2, ln2, 2, 2, asat2);
  DIM dom2() = {0, 1, 2, 0, 1, 2, 0, 1, 2}
  DIM sol2(3) AS LONG
  PRINT "|alldiff2="; OPTIMIZE.CPALLDIFF(dom2, 3, 3, sol2)
  PRINT ""
END SUB
