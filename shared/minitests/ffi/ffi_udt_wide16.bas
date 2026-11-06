' UDT con 16 campi e larghezze miste (matrice F5: struct >8 campi).
' EXPECT: sz= 120|s= 136
TYPE Wide16
    A AS INTEGER
    B AS LONG
    C AS DOUBLE
    D AS SINGLE
    E AS QUAD
    F AS BYTE
    G AS DOUBLE
    H AS LONG
    I AS DOUBLE
    J AS INTEGER
    K AS DOUBLE
    L AS LONG
    M AS DOUBLE
    N AS QUAD
    O AS DOUBLE
    P AS SINGLE
END TYPE

DECLARE FUNCTION Wide16Size LIB "liblayout_ext" ALIAS "wide16_size" () AS LONG
DECLARE FUNCTION Wide16Sum  LIB "liblayout_ext" ALIAS "wide16_sum" (w AS Wide16) AS DOUBLE

SUB Main()
    PRINT "sz="; Wide16Size()
    DIM w AS Wide16
    w.A = 1 : w.B = 2 : w.C = 3 : w.D = 4 : w.E = 5 : w.F = 6 : w.G = 7 : w.H = 8
    w.I = 9 : w.J = 10 : w.K = 11 : w.L = 12 : w.M = 13 : w.N = 14 : w.O = 15 : w.P = 16
    PRINT "s="; Wide16Sum(w)
END SUB
