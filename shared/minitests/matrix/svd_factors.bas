' FATTORI della SVD (U, S, VT) — non solo la ricostruzione.
' Copre tre bug reali del runtime scoperti tardi perche' nessun test stampava i fattori:
'   1. la U era SCALATA per colonna (norma = sigma) invece che ortonormale;
'   2. il fattore "VT" restituiva V, non la trasposta (sbagliato per A non simmetrica);
'   3. nessun oracolo sui valori singolari.
' Matrice asimmetrica A = [[1,2],[3,4]]: sigma = 5.464986 e 0.365966 (analitico:
' autovalori di A^T A = [[10,14],[14,20]] -> (30 +- sqrt(884))/2, radici quadrate).
' EXPECT: s0= 5.464986|s1= 0.365966|orth=-1|rec=-1
IMPORT MATH
SUB Main()
  DIM a(1, 1) AS DOUBLE
  a(0, 0) = 1 : a(0, 1) = 2
  a(1, 0) = 3 : a(1, 1) = 4
  DIM sv = LINALG_MATRIX.SVD(a)
  PRINT "s0="; MATH.ROUND(sv.S(0, 0), 6)
  PRINT "s1="; MATH.ROUND(sv.S(1, 1), 6)

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
        sum = sum + sv.U(k, i) * sv.U(k, j)
      NEXT
      DIM want AS DOUBLE
      IF i = j THEN want = 1 ELSE want = 0
      IF ABS(sum - want) > maxErr THEN maxErr = ABS(sum - want)
    NEXT
  NEXT
  PRINT "orth="; (maxErr < 0.000000001)

  ' Ricostruzione A = U S V^T (con VT vero): |errore| < 1e-9
  maxErr = 0
  FOR i = 0 TO 1
    FOR j = 0 TO 1
      DIM acc AS DOUBLE
      acc = 0
      FOR k = 0 TO 1
        acc = acc + sv.U(i, k) * sv.S(k, k) * sv.VT(k, j)
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
