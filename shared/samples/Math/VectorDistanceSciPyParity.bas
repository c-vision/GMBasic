' VECTOR — metriche di distanza tra vettori (scipy.spatial.distance).
' Input: a=[1,2,3], b=[4,6,8] (handle da VECTOR.CREATE).
' Oracolo: SciPy 1.18 (scipy.spatial.distance): manhattan=12, chebyshev=5,
' cosine=0.007416666, minkowski(p=3)=6.0.
IMPORT VECTOR

SUB Main()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "manhattan  ="; VECTOR.MANHATTAN(h1, h2)
  PRINT "chebyshev  ="; VECTOR.CHEBYSHEV(h1, h2)
  PRINT "cosine     ="; VECTOR.COSINE(h1, h2)
  PRINT "minkowski  ="; VECTOR.MINKOWSKI(h1, h2, 3)
END SUB
