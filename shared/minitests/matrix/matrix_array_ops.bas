' MiniTest MATRIX — funzioni array/vector con matrici VERE (non handle fittizi).
' A = [[1,2],[0,3]].
' NOTA: ARGMAX/ARGMIN hanno una collisione preesistente lato IL con AMAX/AMIN
' (vedi Bugs/ se tracciato) — non testati qui.
' EXPECT: rowsums:  3 , 3|colsums:  1 , 5|rowmeans:  1.5 , 1.5|colmeans:  0.5 , 2.5|rowvar:  0.25 , 2.25|colvar:  0.25 , 0.25

SUB Main()
    a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 1) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 0) : a = MATRIX.SETVALUE(a, 1, 1, 3)

    DIM rs() = MATRIX.ROWSUMS(a)
    PRINT "rowsums: "; rs(0); ","; rs(1)
    DIM cs() = MATRIX.COLSUMS(a)
    PRINT "colsums: "; cs(0); ","; cs(1)
    DIM rm() = MATRIX.ROWMEANS(a)
    PRINT "rowmeans: "; rm(0); ","; rm(1)
    DIM cm() = MATRIX.COLMEANS(a)
    PRINT "colmeans: "; cm(0); ","; cm(1)
    DIM rv() = MATRIX.ROWVARIANCES(a)
    PRINT "rowvar: "; rv(0); ","; rv(1)
    DIM cv() = MATRIX.COLVARIANCES(a)
    PRINT "colvar: "; cv(0); ","; cv(1)
END SUB
