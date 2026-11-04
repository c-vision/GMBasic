' MiniTest ANALYSIS — interpolazioni e stime spettrali con array VERI.
' Sostituisce 9 dummy gen_ANALYSIS_* (CubicSpline/Interpolate*/MaCoefficients/
' YuleWalker/EuclideanDistanceSquared) che passavano SCALARI a funzioni che
' vogliono array — non testavano nulla (recipe triage MiniTest-Oracolo).
' Dati lineari y=2x: i metodi che preservano le rette danno valori esatti.
' MaCoefficients diverge di 1 ULP fra matematica C# e C (stessa classe dei residui
' GAMMAPPF/SHAPIRO): arrotondato a 6 decimali come negli altri minitest (anova.bas).
' EXPECT: interp:  3 , 3 , 2|interp2:  3 , 3 , 5|euclid:  30|yw:  1.33333333333333 , 0.333333333333333|ma:  0.5 , 0.057143
IMPORT ANALYSIS

SUB Main()
    DIM x() = {0, 1, 2, 3, 4}
    DIM y() = {0, 2, 4, 6, 8}
    PRINT "interp: "; ANALYSIS.INTERPOLATELINEAR(x, y, 1.5); ","; ANALYSIS.CUBICSPLINE(x, y, 1.5); ","; ANALYSIS.INTERPOLATESTEP(x, y, 1.5)
    PRINT "interp2: "; ANALYSIS.INTERPOLATEPCHIP(x, y, 1.5); ","; ANALYSIS.INTERPOLATEPOLYNOMIAL(x, y, 1.5); ","; ANALYSIS.INTERPOLATESPLINE(x, y, 2.5)
    PRINT "euclid: "; ANALYSIS.EUCLIDEANDISTANCESQUARED(x, y)
    DIM d() = {1, 2, 3, 4, 5, 6}
    DIM yw() = ANALYSIS.YULEWALKER(d, 2)
    PRINT "yw: "; yw(0); ","; yw(1)
    DIM ma() = ANALYSIS.MACOEFFICIENTS(d, 2)
    PRINT "ma: "; ma(0); ","; MATH.ROUND(ma(1), 6)
END SUB
