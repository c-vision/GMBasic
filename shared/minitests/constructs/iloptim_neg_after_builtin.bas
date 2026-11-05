' EXPECT: r1= 825.76|r2=-825.76|r3= 825.76|r4=-825.76
' Regressione ILOptim: `x = -y` perdeva il segno quando y proveniva da un builtin.
SUB Main()
    DIM r1 AS DOUBLE
    r1 = -FINANCE.PMT(0.027 / 12.0, 300, 180000.0)
    DIM r2 AS DOUBLE
    r2 = -r1
    DIM r3 AS DOUBLE
    r3 = -r2
    DIM r4 AS DOUBLE
    r4 = -MATH.ABS(r1)
    PRINT "r1="; MATH.ROUND(r1, 2)
    PRINT "r2="; MATH.ROUND(r2, 2)
    PRINT "r3="; MATH.ROUND(r3, 2)
    PRINT "r4="; MATH.ROUND(r4, 2)
END SUB
