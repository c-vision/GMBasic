' EXPECT: bas 0 =  0  :4.0,0.0|bas 1 =  0  :0.0,4.0|bas 2 =  0  :4.0,0.0|bas 3 =  4  :111.0,222.0
' Base iniziale con colonne di SLACK (2026-11-04): `basis()` accetta indici in [0, n+m):
' 0..n-1 = colonne di A, n..n+m-1 = slack (unitario, costo 0). Un indice fuori range e'
' RIFIUTATO (ritorno 4) invece di leggere fuori da A (UB: risultati diversi fra backend).
IMPORT OPTIMIZE
SUB Main()
  DIM A(1) AS DOUBLE
  DIM b(0) AS DOUBLE
  DIM c(1) AS DOUBLE
  DIM bas(0) AS LONG
  DIM x(1) AS DOUBLE
  A(0)=1 : A(1)=1 : b(0)=4 : c(0)=-1 : c(1)=-1
  FOR bs = 0 TO 3
    bas(0)=bs : x(0)=111 : x(1)=222
    r = OPTIMIZE.LPREVISED(A, b, c, bas, x, 1, 2)
    PRINT "bas"; bs; "= "; r; " :"; FORMAT(x(0), "#.0"); ","; FORMAT(x(1), "#.0"); " ";
  NEXT bs
  PRINT ""
END SUB
