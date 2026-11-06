' MiniTest ANALYSIS — interpolazioni e stime spettrali con array VERI.
' Sostituisce 9 dummy gen_ANALYSIS_* (CubicSpline/Interpolate*/MaCoefficients/
' YuleWalker/EuclideanDistanceSquared) che passavano SCALARI a funzioni che
' vogliono array — non testavano nulla (recipe triage MiniTest-Oracolo).
' Dati lineari y=2x: i metodi che preservano le rette danno valori esatti.
' MaCoefficients diverge di 1 ULP fra matematica C# e C (stessa classe dei residui
' GAMMAPPF/SHAPIRO): arrotondato a 6 decimali come negli altri minitest (anova.bas).
' EXPECT: interp:  3 , 3 , 2|interp2:  3 , 3 , 5|euclid:  30|yw:  1.33333333333333 , 0.333333333333333|ma:  0.5 , 0.057143
IMPORT TIMESERIES

SUB Main()
    DIM x() = {0, 1, 2, 3, 4}
    DIM y() = {0, 2, 4, 6, 8}
    PRINT "interp: "; NUMERICAL.InterpolateLinear(x, y, 1.5); ","; NUMERICAL.CubicSpline(x, y, 1.5); ","; NUMERICAL.InterpolateStep(x, y, 1.5)
    PRINT "interp2: "; NUMERICAL.InterpolatePchip(x, y, 1.5); ","; NUMERICAL.InterpolatePolynomial(x, y, 1.5); ","; NUMERICAL.InterpolateSpline(x, y, 2.5)
    PRINT "euclid: "; VECTOR.EuclideanDistanceSquared(x, y)
    DIM d() = {1, 2, 3, 4, 5, 6}
    DIM yw() = TIMESERIES.YuleWalker(d, 2)
    PRINT "yw: "; yw(0); ","; yw(1)
    DIM ma() = TIMESERIES.MaCoefficients(d, 2)
    PRINT "ma: "; ma(0); ","; MATH.ROUND(ma(1), 6)
END SUB
