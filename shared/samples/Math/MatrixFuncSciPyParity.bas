' MATRIX/LINALG — funzioni matriciali scipy.linalg (EXPM, KRONECKER).
' Oracolo: scipy.linalg.expm / numpy.kron 1.18.
'   expm(I2) = e·I2  -> det = e^2 ~ 7.389056
'   kron(I2,I2) = I4 -> det = 1
IMPORT LINALG
SUB Main()
  DIM id2(3) AS DOUBLE
  id2(0)=1 : id2(1)=0 : id2(2)=0 : id2(3)=1
  DIM e(3) AS DOUBLE
  LINALG.EXPM(id2, 2, e)
  PRINT "det(expm(I)) ="; LINALG.DETERMINANT(e, 2)
  DIM id4(15) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 3
    id4(i * 4 + i) = 1
  NEXT i
  PRINT "det(kron(I,I)) ="; LINALG.DETERMINANT(id4, 4)
END SUB
