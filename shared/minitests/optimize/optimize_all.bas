' MiniTest NAMESPACE OPTIMIZE — tutte le 34 funzioni della superficie.
'
' Copre: root finding scalare (Newton/Brent/Bisection/Secant/Brentq/Brenth/Ridder/
' Toms748), minimizzazione scalare (GoldenSection/BrentMin/MinimizeScalar), multidim
' (NelderMead/Bfgs/LBfgs/Powell/ConjugateGradient/BasinHopping), vincolata (Tnc/Cobyla/
' Slsqp/Minimize/Brute/DifferentialEvolution/DualAnnealing), curve fitting (LM/LMVec/
' CurveFit), sistemi (Broyden/Fsolve) e algebra (LeastSquares/LeastSquaresFit/
' CholeskySolve/Linprog/Simplex).
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' Scopo: verificare la STESSA versione del namespace sui 4 backend, non un'applicazione.
' EXPECT: newton= 2|brent= 2.00000000162127|bisection= 2.00000000186265|secant= 1.99999999999995|brentq= 1.99999999999998|brenth= 1.99999999999998|ridder= 2|toms748= 2|goldsect= 2.00000000059108|brentmin= 2|minscalar= 2|tnc= 1   1|cobyla=-0.958   0.926|slsqp= 1   1|minimize= 1   1|brute= 1.05  -1.95|de= 1  -2|dual= 1  -2|NM= 1 , 1|BF= 0.999997 , 0.999994|LB= 0.999997 , 0.999994|PW= 0.249913 , 0.040301|CG= 0 , 0|B= 3|LM= 1.0016 , 0.9996|CF= 1.0016 , 0.9996|lstsq= 1.5   0.3333|chol= 1.75   1.5|linprog= 1   1.5|simplex= 0   0|lsfit= 1.5   0.3333|LMV= 1.0016 , 0.9996|min3= 1   1|BR= 1.414214 , 3|FS= 1.414214 , 3
IMPORT OPTIMIZE
IMPORT MATH


SUB O_scalar()
  PRINT "newton="; OPTIMIZE.NEWTON({ |x| RETURN x ^ 2 - 4 }, 1.0)
  PRINT "brent="; OPTIMIZE.BRENT({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "bisection="; OPTIMIZE.BISECTION({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "secant="; OPTIMIZE.SECANT({ |x| RETURN x ^ 2 - 4 }, 1.0, 5.0)
  PRINT "brentq="; OPTIMIZE.BRENTQ({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "brenth="; OPTIMIZE.BRENTH({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "ridder="; OPTIMIZE.RIDDER({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "toms748="; OPTIMIZE.TOMS748({ |x| RETURN x ^ 2 - 4 }, 0.0, 5.0)
  PRINT "goldsect="; OPTIMIZE.GOLDENSECTION({ |x| RETURN (x - 2) ^ 2 }, 0.0, 5.0)
  PRINT "brentmin="; OPTIMIZE.BRENTMIN({ |x| RETURN (x - 2) ^ 2 }, 0.0, 5.0)
  PRINT "minscalar="; OPTIMIZE.MINIMIZESCALAR({ |x| RETURN (x - 2) ^ 2 }, 0.0, 5.0)
END SUB


SUB O_bounded()
  DIM x0(1) AS DOUBLE = {-1.2, 1.0}
  DIM bd(1) AS ARRAY OF DOUBLE
  bd(0) = {-5.0, 5.0} : bd(1) = {-5.0, 5.0}
  DIM f = FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
  DIM tn() = OPTIMIZE.TNC(f, x0, bd)
  PRINT "tnc="; MATH.ROUND(tn(0), 3); " "; MATH.ROUND(tn(1), 3)
  DIM co() = OPTIMIZE.COBYLA(f, x0, bd)
  PRINT "cobyla="; MATH.ROUND(co(0), 3); " "; MATH.ROUND(co(1), 3)
  DIM sq() = OPTIMIZE.SLSQP(f, x0, bd)
  PRINT "slsqp="; MATH.ROUND(sq(0), 3); " "; MATH.ROUND(sq(1), 3)
  DIM mn() = OPTIMIZE.MINIMIZE(f, x0, "powell", bd)
  PRINT "minimize="; MATH.ROUND(mn(0), 3); " "; MATH.ROUND(mn(1), 3)
  DIM rg(1) AS ARRAY OF DOUBLE
  rg(0) = {-3.0, 3.0} : rg(1) = {-3.0, 3.0}
  DIM g = FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) ^ 2 + (p(1) + 2) ^ 2
  DIM br() = OPTIMIZE.BRUTE(g, rg, 41)
  PRINT "brute="; MATH.ROUND(br(0), 3); " "; MATH.ROUND(br(1), 3)
  DIM de() = OPTIMIZE.DIFFERENTIALEVOLUTION(g, rg)
  PRINT "de="; MATH.ROUND(de(0), 2); " "; MATH.ROUND(de(1), 2)
  DIM da() = OPTIMIZE.DUALANNEALING(g, rg, 200)
  PRINT "dual="; MATH.ROUND(da(0), 2); " "; MATH.ROUND(da(1), 2)
END SUB


SUB O_multidim()
  DIM Rosen = FN(v() AS DOUBLE) AS DOUBLE:
    DIM x = v(0) : DIM y = v(1)
    RETURN (1 - x) ^ 2 + 100 * (y - x ^ 2) ^ 2
  END FN
  DIM G(1) AS DOUBLE = {0.0, 0.0}
  DIM NM() = OPTIMIZE.NELDERMEAD(Rosen, G)
  PRINT "NM="; MATH.ROUND(NM(0), 6); ","; MATH.ROUND(NM(1), 6)
  DIM BF() = OPTIMIZE.BFGS(Rosen, G)
  PRINT "BF="; MATH.ROUND(BF(0), 6); ","; MATH.ROUND(BF(1), 6)
  DIM LB() = OPTIMIZE.LBFGS(Rosen, G)
  PRINT "LB="; MATH.ROUND(LB(0), 6); ","; MATH.ROUND(LB(1), 6)
  DIM PW() = OPTIMIZE.POWELL(Rosen, G)
  PRINT "PW="; MATH.ROUND(PW(0), 6); ","; MATH.ROUND(PW(1), 6)
  DIM Quad = FN(v() AS DOUBLE) AS DOUBLE:
    RETURN v(0) ^ 2 + v(1) ^ 2
  END FN
  DIM QG(1) AS DOUBLE = {1.0, 1.0}
  DIM CG() = OPTIMIZE.CONJUGATEGRADIENT(Quad, QG)
  PRINT "CG="; MATH.ROUND(CG(0), 6); ","; MATH.ROUND(CG(1), 6)
END SUB


SUB O_global()
  DIM f = FN(x() AS DOUBLE) AS DOUBLE => (x(0) - 3) ^ 2 + 1
  DIM X0(0) AS DOUBLE = {10.0}
  DIM B() = OPTIMIZE.BASINHOPPING(f, X0, 5)
  PRINT "B="; MATH.ROUND(B(0), 4)
END SUB


SUB O_curvefit()
  DIM DataX(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM DataY(4) AS DOUBLE = {1.0, 2.7, 7.4, 20.1, 54.6}
  DIM P0(1) AS DOUBLE = {0.5, 0.5}
  DIM Model = FN(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE => p(0) * EXP(p(1) * x)
  DIM LM() = OPTIMIZE.LEVENBERGMARQUARDT(Model, P0, DataX, DataY)
  PRINT "LM="; MATH.ROUND(LM(0), 4); ","; MATH.ROUND(LM(1), 4)
  DIM CF() = OPTIMIZE.CURVEFIT(Model, DataX, DataY, P0)
  PRINT "CF="; MATH.ROUND(CF(0), 4); ","; MATH.ROUND(CF(1), 4)
END SUB


SUB O_linalg()
  DIM A(2) AS ARRAY OF DOUBLE
  A(0) = {1.0, 1.0} : A(1) = {2.0, 1.0} : A(2) = {3.0, 1.0}
  DIM b(2) AS DOUBLE = {2.0, 3.0, 5.0}
  DIM x() = OPTIMIZE.LEASTSQUARES(A, b)
  PRINT "lstsq="; MATH.ROUND(x(0), 4); " "; MATH.ROUND(x(1), 4)
  DIM C(1) AS ARRAY OF DOUBLE
  C(0) = {4.0, 2.0} : C(1) = {2.0, 3.0}
  DIM d(1) AS DOUBLE = {10.0, 8.0}
  DIM ch() = OPTIMIZE.CHOLESKYSOLVE(C, d)
  PRINT "chol="; MATH.ROUND(ch(0), 4); " "; MATH.ROUND(ch(1), 4)
  DIM Aub(1) AS ARRAY OF DOUBLE
  Aub(0) = {1.0, 2.0} : Aub(1) = {3.0, 2.0}
  DIM bub(1) AS DOUBLE = {4.0, 6.0}
  DIM lp() = OPTIMIZE.LINPROG({-1.0, -1.0}, Aub, bub)
  PRINT "linprog="; MATH.ROUND(lp(0), 4); " "; MATH.ROUND(lp(1), 4)
  DIM sp() = OPTIMIZE.SIMPLEX({-1.0, -1.0}, Aub, bub)
  PRINT "simplex="; MATH.ROUND(sp(0), 4); " "; MATH.ROUND(sp(1), 4)
  DIM x0(1) AS DOUBLE = {0.0, 0.0}
  DIM lf() = OPTIMIZE.LEASTSQUARESFIT(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * 1 + p(1) - 2, p(0) * 2 + p(1) - 3, p(0) * 3 + p(1) - 5}, x0)
  PRINT "lsfit="; MATH.ROUND(lf(0), 4); " "; MATH.ROUND(lf(1), 4)
END SUB


SUB O_lmvec()
  DIM DataX(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM DataY(4) AS DOUBLE = {1.0, 2.7, 7.4, 20.1, 54.6}
  DIM P0(1) AS DOUBLE = {0.5, 0.5}
  DIM Model = FN(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE:
    RETURN {p(0) * EXP(p(1) * x(0)), p(0) * EXP(p(1) * x(1)), p(0) * EXP(p(1) * x(2)), p(0) * EXP(p(1) * x(3)), p(0) * EXP(p(1) * x(4))}
  END FN
  DIM LM() = OPTIMIZE.LEVENBERGMARQUARDTVEC(Model, P0, DataX, DataY)
  PRINT "LMV="; MATH.ROUND(LM(0), 4); ","; MATH.ROUND(LM(1), 4)
END SUB


SUB O_minimize_nobounds()
  DIM x0(1) AS DOUBLE = {-1.2, 1.0}
  DIM f = FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
  DIM mn() = OPTIMIZE.MINIMIZE(f, x0, "powell")
  PRINT "min3="; MATH.ROUND(mn(0), 3); " "; MATH.ROUND(mn(1), 3)
END SUB


SUB O_systems()
  DIM Sys = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE:
    DIM r(1) AS DOUBLE
    r(0) = v(0) ^ 2 - 2
    r(1) = v(1) - 3
    RETURN r
  END FN
  DIM Guess(1) AS DOUBLE = {1.0, 0.0}
  DIM BR() = OPTIMIZE.BROYDEN(Sys, Guess)
  PRINT "BR="; MATH.ROUND(BR(0), 6); ","; MATH.ROUND(BR(1), 6)
  DIM FS() = OPTIMIZE.FSOLVE(Sys, Guess)
  PRINT "FS="; MATH.ROUND(FS(0), 6); ","; MATH.ROUND(FS(1), 6)
END SUB


SUB Main()
  O_scalar()
  O_bounded()
  O_multidim()
  O_global()
  O_curvefit()
  O_linalg()
  O_lmvec()
  O_minimize_nobounds()
  O_systems()
END SUB
