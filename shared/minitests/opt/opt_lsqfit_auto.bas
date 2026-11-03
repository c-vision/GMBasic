' EXPECT: auto= 1.75  , -0.05|sub= 1.75  , -0.05|m4= 4
' LEASTSQUARESFIT drop-in: m dedotto dal valore di ritorno della callback (m=4 > n=2,
' quindi il vecchio troncamento a n su VM-C darebbe un risultato diverso).
IMPORT OPT
IMPORT MATH
FUNCTION Res(p() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {p(0) + p(1) - 2.0, 2.0 * p(0) + p(1) - 3.1, 3.0 * p(0) + p(1) - 5.0, 4.0 * p(0) + p(1) - 7.2}
END FUNCTION
SUB Main()
    DIM x0(1) AS DOUBLE = {0.0, 0.0}
    DIM a() = OPT.LEASTSQUARESFIT(Res, x0)
    PRINT "auto="; MATH.ROUND(a(0), 4); " , "; MATH.ROUND(a(1), 4)
    DIM o(3) AS DOUBLE
    CALL OPT.LEASTSQUARESFIT(Res, x0, o)
    PRINT "sub="; MATH.ROUND(o(0), 4); " , "; MATH.ROUND(o(1), 4)
    DIM r() = Res(x0)
    PRINT "m4="; UBOUND(r) + 1
END SUB
