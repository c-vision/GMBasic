' MATRIX — funzioni matriciali scipy.linalg (EXPM, KRONECKER).
' Oracolo: scipy.linalg.expm / numpy.kron 1.18.
'   expm(I2) = e·I2  → det = e^2 ≈ 7.389056
'   kron(I2,I2) = I4 → det = 1

SUB Main()
  h = MATRIX.CREATE(2, 2)
  e = LINALG_MATRIX.EXPM(h)
  PRINT "det(expm(I)) ="; LINALG_MATRIX.DETERMINANT(e)
  k = MATRIX.KRONECKER(h, h)
  PRINT "det(kron(I,I)) ="; LINALG_MATRIX.DETERMINANT(k)
END SUB
