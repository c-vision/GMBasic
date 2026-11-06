' Sample ANALYSIS — interpolazioni e stime spettrali con dati veri.
' Dati lineari y=2x (i metodi che preservano le rette danno valori esatti) + serie
' per YuleWalker/MaCoefficients. Compagno del MiniTest analysis_interp_real.bas.
IMPORT TIMESERIES

SUB Main()
    DIM x() = {0, 1, 2, 3, 4}
    DIM y() = {0, 2, 4, 6, 8}
    PRINT "x_new  linear  cubic  step  pchip  poly"
    DIM pts() = {0.5, 1.5, 2.5, 3.5}
    FOR k = 0 TO 3
        xn = pts(k)
        PRINT xn; "  "; NUMERICAL.InterpolateLinear(x, y, xn); "  "; NUMERICAL.CubicSpline(x, y, xn); "  "; NUMERICAL.InterpolateStep(x, y, xn); "  "; NUMERICAL.InterpolatePchip(x, y, xn); "  "; NUMERICAL.InterpolatePolynomial(x, y, xn)
    NEXT k
    PRINT "spline@2.5: "; NUMERICAL.InterpolateSpline(x, y, 2.5)
    PRINT "euclid: "; VECTOR.EuclideanDistanceSquared(x, y)
    DIM d() = {1, 2, 3, 4, 5, 6}
    DIM yw() = TIMESERIES.YuleWalker(d, 2)
    PRINT "yulewalker: "; yw(0); ","; MATH.ROUND(yw(1), 6)
    DIM ma() = TIMESERIES.MaCoefficients(d, 2)
    PRINT "macoef: "; ma(0); ","; MATH.ROUND(ma(1), 6)
END SUB

