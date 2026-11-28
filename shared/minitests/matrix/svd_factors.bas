' Fattori della SVD (U, S, VT) via FFI (LINALG.FACTOR) su A = [[1,2],[3,4]].
' sigma = 5.464986 e 0.365966; U ortonormale; ricostruzione A = U·S·Vᵀ.
' EXPECT: s0= 5.464986|s1= 0.365966|orth=-1|rec=-1
IMPORT LINALG
IMPORT MATH
SUB Main()
  DIM a(3) AS DOUBLE
  a(0)=1 : a(1)=2 : a(2)=3 : a(3)=4
  DIM u(3) AS DOUBLE, s(3) AS DOUBLE, vt(3) AS DOUBLE
  DIM meta(1) AS LONG
  LINALG.FACTOR(a, 2, 2, "SVD.U", u, meta)
  LINALG.FACTOR(a, 2, 2, "SVD.S", s, meta)
  LINALG.FACTOR(a, 2, 2, "SVD.VT", vt, meta)
  PRINT "s0="; MATH.ROUND(s(0), 6)
  PRINT "s1="; MATH.ROUND(s(3), 6)

  ' U ortonormale: |(U^T U) - I| < 1e-9
  DIM i AS LONG
  DIM j AS LONG
  DIM k AS LONG
  DIM maxErr AS DOUBLE
  maxErr = 0
  FOR i = 0 TO 1
    FOR j = 0 TO 1
      DIM sum AS DOUBLE
      sum = 0
      FOR k = 0 TO 1
        sum = sum + u(k * 2 + i) * u(k * 2 + j)
      NEXT
      DIM want AS DOUBLE
      IF i = j THEN want = 1 ELSE want = 0
      IF ABS(sum - want) > maxErr THEN maxErr = ABS(sum - want)
    NEXT
  NEXT
  PRINT "orth="; (maxErr < 0.000000001)

  ' Ricostruzione A = U S V^T (S diagonale n×n)
  maxErr = 0
  FOR i = 0 TO 1
    FOR j = 0 TO 1
      DIM acc AS DOUBLE
      acc = 0
      FOR k = 0 TO 1
        acc = acc + u(i * 2 + k) * s(k * 2 + k) * vt(k * 2 + j)
      NEXT
      DIM orig AS DOUBLE
      IF i = 0 AND j = 0 THEN orig = 1
      IF i = 0 AND j = 1 THEN orig = 2
      IF i = 1 AND j = 0 THEN orig = 3
      IF i = 1 AND j = 1 THEN orig = 4
      IF ABS(acc - orig) > maxErr THEN maxErr = ABS(acc - orig)
    NEXT
  NEXT
  PRINT "rec="; (maxErr < 0.000000001)
END SUB
