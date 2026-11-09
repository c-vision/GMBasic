' MiniTest: KDTREE.QUERYBALL (parita' 4 backend).
' Oracolo: punti entro raggio 2.5 da [4,4] → {5,4} (d=1) e {2,3} (d=sqrt5).
' EXPECT: 2
IMPORT KDTREE
SUB Main()
  DIM flat(9) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1}
  h = KDTREE.CREATE(flat, 2)
  DIM x(1) AS DOUBLE = {4, 4}
  DIM b() = KDTREE.QUERYBALL(h, x, 2.5)
  PRINT UBOUND(b) + 1
END SUB
