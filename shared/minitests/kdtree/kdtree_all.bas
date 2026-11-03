' MiniTest NAMESPACE KDTREE — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: q= 1 , 0|d= 1 , 2.2361|b= 1
IMPORT KDTREE
IMPORT MATH

SUB T_kd()
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

SUB Main()
  T_kd()
END SUB
