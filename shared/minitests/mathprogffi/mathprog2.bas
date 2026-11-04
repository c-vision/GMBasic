' ORTOOLS/OPTIMIZE (FFI mathprog) — MiniTest §4.1 della 2ª fascia:
' LPREVISED, LPBOUNDS, LPDUAL, LPREDUCEDCOSTS, LPALTERNATE, MATHHARDFIXING,
' MATHLOCALBRANCHING, GOMORYCUT.
' LP (uguaglianze, come LPSOLVE): min x0 + x1  s.t.  x0 + x1 = 4,  x >= 0  -> costo 4.
' EXPECT: rev= 0 :4.000,0.000|bnd= 1 :0.000,0.000|dual= 0 :0.000|red= 1 :-1.000,-1.000|alt= 0 :0.000|fix=4.0,0.0|lb=4.0,0.0|cut= 1 :-1
IMPORT OPTIMIZE

SUB Main()
  DIM A(3) AS DOUBLE
  DIM b(1) AS DOUBLE
  DIM c(1) AS DOUBLE
  A(0)=1 : A(1)=2
  A(2)=2 : A(3)=1
  b(0)=4 : b(1)=4
  c(0)=-1 : c(1)=-1      ' minimizza -x0-x1 = massimizza x0+x1

  ' ── revised simplex (base iniziale = colonne di slack 2,3) ───────────────────────
  DIM bas(0) AS LONG
  DIM x(1) AS DOUBLE
  bas(0)=2                          ' base iniziale: colonna di slack (n = 2)
  r = OPTIMIZE.LPREVISED(A, b, c, bas, x, 1, 2)
  PRINT "rev="; r; ":"; FORMAT(x(0), "#.000"); ","; FORMAT(x(1), "#.000");

  ' ── con bounds espliciti [0,10] su entrambe le variabili ─────────────────────────
  DIM bnd(3) AS DOUBLE
  DIM xb(1) AS DOUBLE
  bnd(0)=0 : bnd(1)=10 : bnd(2)=0 : bnd(3)=10
  r = OPTIMIZE.LPBOUNDS(A, bnd, c, xb, 1, 2)
  PRINT "|bnd="; r; ":"; FORMAT(xb(0), "#.000"); ","; FORMAT(xb(1), "#.000");

  ' ── duale e costi ridotti ────────────────────────────────────────────────────────
  DIM bas2(0) AS LONG
  DIM xd(1) AS DOUBLE
  bas2(0)=2
  r = OPTIMIZE.LPDUAL(A, b, c, bas2, xd, 1, 2)
  PRINT "|dual="; r; ":"; FORMAT(xd(0), "#.000");
  DIM y(0) AS DOUBLE
  DIM red(1) AS DOUBLE
  y(0)=xd(0)
  r = OPTIMIZE.LPREDUCEDCOSTS(A, c, y, red, 1, 2)
  PRINT "|red="; r; ":"; FORMAT(red(0), "#.000"); ","; FORMAT(red(1), "#.000");

  ' ── ottimo alternativo (stesso valore, soluzione diversa) ────────────────────────
  DIM x2(1) AS DOUBLE
  DIM z2(0) AS DOUBLE
  r = OPTIMIZE.LPALTERNATE(A, b, c, x2, 1, 2, z2)
  PRINT "|alt="; r; ":"; FORMAT(z2(0), "#.000");

  ' ── matheuristica su un problema intero: min x0+x1, x intere -> (1,1), costo 2 ──
  DIM idx(1) AS LONG
  idx(0)=1 : idx(1)=1
  DIM xi(1) AS DOUBLE
  DIM info(3) AS DOUBLE
  CALL OPTIMIZE.MATHHARDFIXING(A, b, c, idx, xi, info, 1, 2)
  PRINT "|fix="; FORMAT(xi(0), "#.0"); ","; FORMAT(xi(1), "#.0");
  CALL OPTIMIZE.MATHLOCALBRANCHING(A, b, c, idx, xi, info, 1, 2)
  PRINT "|lb="; FORMAT(xi(0), "#.0"); ","; FORMAT(xi(1), "#.0");

  ' ── taglio di Gomory da una riga frazionaria del tableau ─────────────────────────
  DIM row(2) AS DOUBLE
  DIM cut(2) AS DOUBLE
  row(0)=1 : row(1)=0.5 : row(2)=2.3
  r = OPTIMIZE.GOMORYCUT(row, 2, 2, cut)
  PRINT "|cut="; r; ":"; ((cut(0) <> 0) OR (cut(1) <> 0))
  PRINT ""
END SUB
