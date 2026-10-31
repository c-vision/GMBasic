' FILE: samples/Community/Math/VectorMiniTest.bas
' Sample VECTOR — tutte le 6 funzioni di distanza (handle-based).
' Output in work/. I valori esatti sono verificati dai MiniTest.
IMPORT VECTOR
SUB Main()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  DIM mh = VECTOR.MANHATTAN(h1, h2)    : PRINT "manhattan  ok"
  DIM ch = VECTOR.CHEBYSHEV(h1, h2)   : PRINT "chebyshev  ok"
  DIM co = VECTOR.COSINE(h1, h2)      : PRINT "cosine     ok"
  DIM mi = VECTOR.MINKOWSKI(h1, h2, 3): PRINT "minkowski  ok"
  DIM dp = VECTOR.PRODUCT(h1, h2)     : PRINT "dot        ok"
  DIM v(2) AS DOUBLE = {3, 4, 0}
  DIM eu = VECTOR.EUCLIDEA(v)         : PRINT "euclidea   ok"
  PRINT "Sample VECTOR completato."
END SUB
