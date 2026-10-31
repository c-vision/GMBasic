' EXPECT: cos= 0.007417
' (distanza coseno = 1 - similarità; arrotondata a 6 decimali per evitare
'  differenze di formattazione floating-point fra le gambe)
IMPORT VECTOR
IMPORT MATH
SUB Main()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "cos="; MATH.ROUND(VECTOR.COSINE(h1, h2), 6)
END SUB
