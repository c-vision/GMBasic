' KDTREE.QUERY/QUERYDIST/QUERYBALL su albero reale (punti FLAT + m).
' Oracolo SciPy: query([4,4], k=2) → idx [1,0], dist [1, sqrt(5)].
' EXPECT: q= 1 , 0|d= 1 , 2.2361|b= 1
IMPORT KDTREE
SUB Main()
  DIM flat(9) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1}
  h = KDTREE.CREATE(flat, 2)
  DIM x(1) AS DOUBLE = {4, 4}
  DIM q() = KDTREE.QUERY(h, x, 2)
  PRINT "q="; q(0); ","; q(1)
  DIM qd() = KDTREE.QUERYDIST(h, x, 2)
  PRINT "d="; MATH.ROUND(qd(0), 4); ","; MATH.ROUND(qd(1), 4)
  DIM b() = KDTREE.QUERYBALL(h, x, 1.5)
  PRINT "b="; UBOUND(b) + 1
END SUB
