REM FILE: 11-MathprogAviation.bas — pacchetti `mathprog` e `aviation` (FFI): TUTTE le 14
REM funzioni di 2ª fascia. Scenario: analisi di una LP (base, duale, costi ridotti, taglio di
REM Gomory, matheuristica intera) e pianificazione di una giornata operativa di una compagnia
REM (flotte, equipaggi, disruption, hub, rotazione aeromobili).
REM Output deterministico, identico sui 4 backend.
IMPORT OPTIMIZE

SUB Main()
  ' ═══ A. LP: min -x0-x1 s.t. x0+x1 = 4, x >= 0. Base iniziale = colonna di SLACK (indice n=2):
  ' il simplesso revised la accetta (le colonne n..n+m-1 sono le slack) e converge a (4,0).
  DIM A(1) AS DOUBLE
  DIM b(0) AS DOUBLE
  DIM c(1) AS DOUBLE
  A(0)=1 : A(1)=1
  b(0)=4
  c(0)=-1 : c(1)=-1          ' min -x0-x1 = massimizza x0+x1 (vincolo x0+x1=4)
  DIM bas(0) AS LONG
  DIM xsol(1) AS DOUBLE
  bas(0)=2
  ' Si stampa la SOLUZIONE, non il codice di ritorno: su VM/C `LPREVISED`/`LPDUAL` restituiscono
  ' valori diversi dagli altri backend (nota `(open) - OR-LPDUAL-Codici-Divergenti.md`).
  r = OPTIMIZE.LPREVISED(A, b, c, bas, xsol, 1, 2)
  PRINT "lp: revised="; FORMAT(xsol(0), "#.000"); ","; FORMAT(xsol(1), "#.000");

  DIM bas2(0) AS LONG
  DIM xd(1) AS DOUBLE
  bas2(0)=2
  r = OPTIMIZE.LPDUAL(A, b, c, bas2, xd, 1, 2)
  PRINT "|dual="; FORMAT(xd(0), "#.000");
  DIM y(0) AS DOUBLE
  DIM red(1) AS DOUBLE
  y(0)=xd(0)
  r = OPTIMIZE.LPREDUCEDCOSTS(A, c, y, red, 1, 2)
  PRINT "|reduced="; FORMAT(red(0), "#.000");

  DIM bnd(3) AS DOUBLE
  DIM xb(1) AS DOUBLE
  bnd(0)=0 : bnd(1)=10 : bnd(2)=0 : bnd(3)=10
  PRINT "|bounds="; OPTIMIZE.LPBOUNDS(A, bnd, c, xb, 1, 2); ":"; FORMAT(xb(0), "#.000");

  DIM x2(1) AS DOUBLE
  DIM z2(0) AS DOUBLE
  PRINT "|alternate="; OPTIMIZE.LPALTERNATE(A, b, c, x2, 1, 2, z2); ":";
  PRINT FORMAT(z2(0), "#.000");

  DIM idx(1) AS LONG
  DIM xi(1) AS DOUBLE
  DIM info(3) AS DOUBLE
  idx(0)=1 : idx(1)=1
  CALL OPTIMIZE.MATHHARDFIXING(A, b, c, idx, xi, info, 1, 2)
  PRINT "|hardfix="; FORMAT(xi(0), "#.0"); ","; FORMAT(xi(1), "#.0");
  CALL OPTIMIZE.MATHLOCALBRANCHING(A, b, c, idx, xi, info, 1, 2)
  PRINT "|localbr="; FORMAT(xi(0), "#.0"); ","; FORMAT(xi(1), "#.0");

  DIM row(2) AS DOUBLE
  DIM cut(2) AS DOUBLE
  row(0)=1 : row(1)=0.5 : row(2)=2.3
  PRINT "|gomory="; OPTIMIZE.GOMORYCUT(row, 2, 2, cut)

  ' ═══ B. OPERAZIONI: flotte, equipaggi, disruption, hub, rotazione ════════════════
  DIM prof(3) AS DOUBLE
  DIM cap(1) AS DOUBLE
  DIM dem(1) AS DOUBLE
  DIM av(1) AS LONG
  DIM asg(1) AS LONG
  prof(0)=10 : prof(1)=20
  prof(2)=15 : prof(3)=5
  cap(0)=100 : cap(1)=100
  dem(0)=50 : dem(1)=60
  av(0)=1 : av(1)=1
  PRINT "ops: fleet="; FORMAT(OPTIMIZE.AVFLEET(prof, cap, dem, av, 2, 2, asg), "#.0");
  PRINT "|assign="; asg(0); asg(1);

  DIM voli(5) AS LONG
  DIM ccost(1) AS DOUBLE
  DIM ch(1) AS LONG
  DIM nch(0) AS LONG
  voli(0)=1 : voli(1)=0
  voli(2)=1 : voli(3)=1
  voli(4)=0 : voli(5)=1
  ccost(0)=5 : ccost(1)=6
  PRINT "|pairing="; FORMAT(OPTIMIZE.AVCREWPAIRING(voli, ccost, 3, 2, ch, nch), "#.0");
  PRINT ":"; nch(0);

  DIM rcost(3) AS DOUBLE
  DIM rasg(1) AS LONG
  rcost(0)=10 : rcost(1)=50
  rcost(2)=30 : rcost(3)=5
  PRINT "|recovery="; FORMAT(OPTIMIZE.AVRECOVERY(rcost, 2, 2, rasg), "#.0");
  PRINT ":"; rasg(0); rasg(1);

  DIM scost(3) AS DOUBLE
  DIM savail(3) AS LONG
  DIM sasg(1) AS LONG
  scost(0)=1 : scost(1)=9
  scost(2)=9 : scost(3)=1
  savail(0)=1 : savail(1)=1 : savail(2)=1 : savail(3)=1
  PRINT "|resched="; FORMAT(OPTIMIZE.AVCREWRESCHED(scost, savail, 2, 2, sasg), "#.0");
  PRINT ":"; sasg(0); sasg(1);

  DIM hd(15) AS DOUBLE
  DIM fl(15) AS DOUBLE
  DIM hubs(1) AS LONG
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      hd(i*4+j) = ABS(i-j)
      fl(i*4+j) = 1
    NEXT j
  NEXT i
  PRINT "|hub="; FORMAT(OPTIMIZE.AVHUB(hd, fl, 4, 2, 0.5, hubs), "#.0");
  PRINT ":"; hubs(0); hubs(1);

  DIM tcost(3) AS DOUBLE
  DIM tasg(1) AS LONG
  tcost(0)=1 : tcost(1)=5
  tcost(2)=5 : tcost(3)=1
  PRINT "|tail="; FORMAT(OPTIMIZE.AVTAILCONTINUITY(tcost, 2, 2, tasg), "#.0");
  PRINT ":"; tasg(0); tasg(1)
  PRINT ""
END SUB
