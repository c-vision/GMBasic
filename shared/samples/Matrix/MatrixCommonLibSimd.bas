REM FILE: MatrixCommonLibSimd.bas
REM Copertura delle riduzioni e proprieta CommonLib.Numerics.SimdMatrix.


SUB Main()
    DIM m = MATRIX.CREATE(3, 3)
    PRINT "HANDLE="; m
    PRINT "DET="; LINALG_MATRIX.DETERMINANT(m)

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
    PRINT "FROBENIUS="; LINALG_MATRIX.FROBENIUSNORM(m)
    PRINT "ONENORM="; LINALG_MATRIX.ONENORM(m)
    PRINT "INFINITYNORM="; LINALG_MATRIX.INFINITYNORM(m)
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

    PRINT "SLICE_NORM="; LINALG_MATRIX.FROBENIUSNORM(sliced)
    PRINT "RESHAPE_NORM="; LINALG_MATRIX.FROBENIUSNORM(reshaped)
    PRINT "CONCATROWS_NORM="; LINALG_MATRIX.FROBENIUSNORM(rowsJoined)
    PRINT "CONCATCOLS_NORM="; LINALG_MATRIX.FROBENIUSNORM(columnsJoined)
    PRINT "ADDROWVECTOR_NORM="; LINALG_MATRIX.FROBENIUSNORM(rowsBroadcast)
    PRINT "ADDCOLVECTOR_NORM="; LINALG_MATRIX.FROBENIUSNORM(columnsBroadcast)

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
    PRINT "CLONE_NORM="; LINALG_MATRIX.FROBENIUSNORM(cloned)
    PRINT "MULTIPLYVECTOR="; rightProduct(0); ","; rightProduct(1); ","; rightProduct(2)
    PRINT "MULTIPLYLEFTVECTOR="; leftProduct(0); ","; leftProduct(1); ","; leftProduct(2)

    DIM csvMatrix = MATRIX.LOADCSV("samples/Fixtures/MatrixIdentity.csv")
    DIM jsonMatrix = MATRIX.LOADJSON("samples/Fixtures/MatrixIdentity.json")
    PRINT "LOADCSV_NORM="; LINALG_MATRIX.FROBENIUSNORM(csvMatrix)
    PRINT "LOADJSON_NORM="; LINALG_MATRIX.FROBENIUSNORM(jsonMatrix)
END SUB
