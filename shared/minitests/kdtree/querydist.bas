' MiniTest: KDTREE.QUERYDIST (parita' 4 backend).
' Oracolo SciPy: query([4,4], k=2) → dist [1, sqrt(5)].
' EXPECT: d= 1 , 2.2361
IMPORT KDTREE
SUB Main()
  DIM flat(9) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1}
  h = KDTREE.CREATE(flat, 2)
  DIM x(1) AS DOUBLE = {4, 4}
  DIM d() = KDTREE.QUERYDIST(h, x, 2)
  PRINT "d="; MATH.ROUND(d(0), 4); ","; MATH.ROUND(d(1), 4)
END SUB
