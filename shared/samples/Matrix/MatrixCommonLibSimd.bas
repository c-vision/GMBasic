REM FILE: MatrixCommonLibSimd.bas
REM Copertura delle riduzioni e proprieta CommonLib.Numerics.SimdMatrix.
REM Le norme/determinante sono calcolate via LINALG FFI su un buffer piatto
REM costruito dal valore MATRIX (convivenza MATRIX valori + LINALG array).
IMPORT LINALG

FUNCTION FLATFILL(m AS MATRIX OF (INT, INT, DOUBLE), r AS LONG, c AS LONG, buf() AS DOUBLE) AS LONG
  DIM i AS LONG, j AS LONG
  IF r * c > 64 THEN RETURN 0
  FOR i = 0 TO r - 1
    FOR j = 0 TO c - 1
      buf(i * c + j) = MATRIX.GETVALUE(m, i, j)
    NEXT j
  NEXT i
  RETURN 1
END FUNCTION

FUNCTION FROB(m AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
  DIM r AS LONG, c AS LONG, i AS LONG, j AS LONG
  DIM buf(63) AS DOUBLE
  r = MATRIX.ROWCOUNT(m) : c = MATRIX.COLUMNCOUNT(m)
  IF FLATFILL(m, r, c, buf) = 0 THEN RETURN -1
  RETURN LINALG.FROBENIUSNORM(buf, r, c)
END FUNCTION

FUNCTION ONEN(m AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
  DIM r AS LONG, c AS LONG
  DIM buf(63) AS DOUBLE
  r = MATRIX.ROWCOUNT(m) : c = MATRIX.COLUMNCOUNT(m)
  IF FLATFILL(m, r, c, buf) = 0 THEN RETURN -1
  RETURN LINALG.ONENORM(buf, r, c)
END FUNCTION

FUNCTION INFN(m AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
  DIM r AS LONG, c AS LONG
  DIM buf(63) AS DOUBLE
  r = MATRIX.ROWCOUNT(m) : c = MATRIX.COLUMNCOUNT(m)
  IF FLATFILL(m, r, c, buf) = 0 THEN RETURN -1
  RETURN LINALG.INFINITYNORM(buf, r, c)
END FUNCTION

FUNCTION DETM(m AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
  DIM r AS LONG, c AS LONG
  DIM buf(8) AS DOUBLE
  r = MATRIX.ROWCOUNT(m) : c = MATRIX.COLUMNCOUNT(m)
  IF r <> c OR r > 3 THEN RETURN -1
  IF FLATFILL(m, r, c, buf) = 0 THEN RETURN -1
  RETURN LINALG.DETERMINANT(buf, r)
END FUNCTION

SUB Main()
    DIM m = MATRIX.CREATE(3, 3)
    PRINT "HANDLE="; m
    PRINT "DET="; DETM(m)

    DIM sumsByRow() = MATRIX.ROWSUMS(m)
    DIM sumsByColumn() = MATRIX.COLSUMS(m)
    DIM meansByRow() = MATRIX.ROWMEANS(m)
    DIM meansByColumn() = MATRIX.COLMEANS(m)
    DIM variancesByRow() = MATRIX.ROWVARIANCES(m)
    DIM variancesByColumn() = MATRIX.COLVARIANCES(m)
    DIM maximumPosition() = MATRIX.ARGMAX(m)
    DIM minimumPosition() = MATRIX.ARGMIN(m)

    PRINT "ROWSUMS="; sumsByRow(0); ","; sumsByRow(1); ","; sumsByRow(2)
    PRINT "COLSUMS="; sumsByColumn(0); ","; sumsByColumn(1); ","; sumsByColumn(2)
    PRINT "ROWMEANS="; meansByRow(0); ","; meansByRow(1); ","; meansByRow(2)
    PRINT "COLMEANS="; meansByColumn(0); ","; meansByColumn(1); ","; meansByColumn(2)
    PRINT "ROWVARIANCES="; variancesByRow(0); ","; variancesByRow(1); ","; variancesByRow(2)
    PRINT "COLVARIANCES="; variancesByColumn(0); ","; variancesByColumn(1); ","; variancesByColumn(2)
    PRINT "FROBENIUS="; FROB(m)
    PRINT "ONENORM="; ONEN(m)
    PRINT "INFINITYNORM="; INFN(m)
    PRINT "SYMMETRIC="; MATRIX.ISSYMMETRIC(m)
    PRINT "ORTHOGONAL="; MATRIX.ISORTHOGONAL(m)
    PRINT "UPPER="; MATRIX.ISUPPERTRIANGULAR(m)
    PRINT "LOWER="; MATRIX.ISLOWERTRIANGULAR(m)
    PRINT "ARGMAX="; maximumPosition(0); ","; maximumPosition(1)
    PRINT "ARGMIN="; minimumPosition(0); ","; minimumPosition(1)

    DIM rowVector() = {10.0, 20.0, 30.0}
    DIM columnVector() = {1.0, 2.0, 3.0}
    DIM sliced = MATRIX.SLICE(m, 0, 2, 0, 2)
    DIM reshaped = MATRIX.RESHAPE(m, 1, 9)
    DIM rowsJoined = MATRIX.CONCATROWS(m, m)
    DIM columnsJoined = MATRIX.CONCATCOLS(m, m)
    DIM rowsBroadcast = MATRIX.ADDROWVECTOR(m, rowVector)
    DIM columnsBroadcast = MATRIX.ADDCOLVECTOR(m, columnVector)

    PRINT "SLICE_NORM="; FROB(sliced)
    PRINT "RESHAPE_NORM="; FROB(reshaped)
    PRINT "CONCATROWS_NORM="; FROB(rowsJoined)
    PRINT "CONCATCOLS_NORM="; FROB(columnsJoined)
    PRINT "ADDROWVECTOR_NORM="; FROB(rowsBroadcast)
    PRINT "ADDCOLVECTOR_NORM="; FROB(columnsBroadcast)

    DIM flatMask() = MATRIX.ABOVEMASK(rowsBroadcast, 15.0)
    DIM matrixMask() = MATRIX.ABOVEMATRIX(rowsBroadcast, 15.0)
    PRINT "MAXVALUE="; MATRIX.MAXVALUE(rowsBroadcast)
    PRINT "MINVALUE="; MATRIX.MINVALUE(rowsBroadcast)
    PRINT "ABOVEMASK="; flatMask(0); ","; flatMask(1); ","; flatMask(2)
    PRINT "ABOVEMATRIX="; matrixMask(0); ","; matrixMask(1); ","; matrixMask(2)
    PRINT "ASCSV="; MATRIX.ASCSV(m)
    PRINT "ASJSON="; MATRIX.ASJSON(m)

    DIM cloned = MATRIX.CLONEMATRIX(m)
    DIM rightProduct() = MATRIX.MULTIPLYVECTOR(m, rowVector)
    DIM leftProduct() = MATRIX.MULTIPLYLEFTVECTOR(m, columnVector)
    PRINT "ROWCOUNT="; MATRIX.ROWCOUNT(m)
    PRINT "COLUMNCOUNT="; MATRIX.COLUMNCOUNT(m)
    PRINT "CLONE_NORM="; FROB(cloned)
    PRINT "MULTIPLYVECTOR="; rightProduct(0); ","; rightProduct(1); ","; rightProduct(2)
    PRINT "MULTIPLYLEFTVECTOR="; leftProduct(0); ","; leftProduct(1); ","; leftProduct(2)

    DIM csvMatrix = MATRIX.LOADCSV("samples/Fixtures/MatrixIdentity.csv")
    DIM jsonMatrix = MATRIX.LOADJSON("samples/Fixtures/MatrixIdentity.json")
    PRINT "LOADCSV_NORM="; FROB(csvMatrix)
    PRINT "LOADJSON_NORM="; FROB(jsonMatrix)
END SUB
