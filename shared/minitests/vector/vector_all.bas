' MiniTest NAMESPACE VECTOR — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: cheb= 5|cos= 0.007417|eucl= 5|manh= 12|mink= 6|dot= 40
IMPORT VECTOR
IMPORT MATH

SUB T_cheb()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "cheb="; VECTOR.CHEBYSHEV(h1, h2)
END SUB

SUB T_cos()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "cos="; MATH.ROUND(VECTOR.COSINE(h1, h2), 6)
END SUB

SUB T_eu()
  DIM v(2) AS DOUBLE = {3, 4, 0}
  PRINT "eucl="; VECTOR.EUCLIDEA(v)
END SUB

SUB T_man()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "manh="; VECTOR.MANHATTAN(h1, h2)
END SUB

SUB T_min()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "mink="; VECTOR.MINKOWSKI(h1, h2, 3)
END SUB

SUB T_prod()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "dot="; VECTOR.PRODUCT(h1, h2)
END SUB

SUB Main()
  T_cheb()
  T_cos()
  T_eu()
  T_man()
  T_min()
  T_prod()
END SUB
