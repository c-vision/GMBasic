' MiniTest MATRIX — ABOVEMASK/ABOVEMATRIX/ADDCOLVECTOR/ADDROWVECTOR/ARGMIN/
' MULTIPLYVECTOR/MULTIPLYLEFTVECTOR/ASJSON con matrici VERE.
' A = [[1,2],[0,3]]; v = [10,20].
' EXPECT: abovemask:  0 ,-1 , 0 ,-1|abovematrix:  0 ,-1 , 0 ,-1|argmin:  1 , 0|addcol:  11|addrow:  11|mulvec:  50 , 60|mulleft:  10 , 80|asjson: [[1,2],[0,3]]
IMPORT MATRIX

SUB Main()
    a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 1) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 0) : a = MATRIX.SETVALUE(a, 1, 1, 3)
    DIM v(1)
    v(0) = 10 : v(1) = 20

    DIM m() = MATRIX.ABOVEMASK(a, 1)
    PRINT "abovemask: "; m(0); ","; m(1); ","; m(2); ","; m(3)
    DIM am() = MATRIX.ABOVEMATRIX(a, 1)
    PRINT "abovematrix: "; am(0); ","; am(1); ","; am(2); ","; am(3)

    DIM argminv() = MATRIX.ARGMIN(a)
    PRINT "argmin: "; argminv(0); ","; argminv(1)

    DIM acv = MATRIX.ADDCOLVECTOR(a, v)
    PRINT "addcol: "; MATRIX.GETVALUE(acv, 0, 0)
    DIM arv = MATRIX.ADDROWVECTOR(a, v)
    PRINT "addrow: "; MATRIX.GETVALUE(arv, 0, 0)

    DIM mv() = MATRIX.MULTIPLYVECTOR(a, v)
    PRINT "mulvec: "; mv(0); ","; mv(1)
    DIM mlv() = MATRIX.MULTIPLYLEFTVECTOR(a, v)
    PRINT "mulleft: "; mlv(0); ","; mlv(1)

    PRINT "asjson: "; MATRIX.ASJSON(a)
END SUB
