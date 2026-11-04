REM FILE: OptimizeLab.bas — pacchetto `opt` (FFI): usa TUTTE le 34 funzioni di OPTIMIZE.
REM Scenario: calibrazione di un modello di crescita, sistema di equilibrio, allocazione
REM ottima con minimizzatori e programmazione lineare. Output deterministico (4 backend).
IMPORT OPTIMIZE
IMPORT MATH

' ── modelli / obiettivi (FUNCTION NOMINATE: testano anche i callback nominati) ──────────
FUNCTION Crescita(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE
    RETURN p(0) * MATH.EXP(p(1) * x)
END FUNCTION

FUNCTION CrescitaVec(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {p(0) * MATH.EXP(p(1) * x(0)), p(0) * MATH.EXP(p(1) * x(1)), p(0) * MATH.EXP(p(1) * x(2)), p(0) * MATH.EXP(p(1) * x(3)), p(0) * MATH.EXP(p(1) * x(4))}
END FUNCTION

FUNCTION Residui(p() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {p(0) - 1.0, p(0) + p(1) - 3.0, p(0) + 2.0 * p(1) - 5.0, p(0) + 3.0 * p(1) - 7.0, p(0) + 4.0 * p(1) - 9.0}
END FUNCTION

FUNCTION Equilibrio(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
END FUNCTION

FUNCTION Perdita(x() AS DOUBLE) AS DOUBLE
    RETURN (x(0) - 1.0) * (x(0) - 1.0) + (x(1) + 2.0) * (x(1) + 2.0)
END FUNCTION

FUNCTION Rosenbrock(x() AS DOUBLE) AS DOUBLE
    RETURN 100.0 * (x(1) - x(0) * x(0)) * (x(1) - x(0) * x(0)) + (1.0 - x(0)) * (1.0 - x(0))
END FUNCTION

FUNCTION Scarto(x() AS DOUBLE) AS DOUBLE
    RETURN (x(0) - 0.5) * (x(0) - 0.5)
END FUNCTION

FUNCTION Radice2(x AS DOUBLE) AS DOUBLE
    RETURN x * x - 2.0
END FUNCTION

FUNCTION Cubo8(x AS DOUBLE) AS DOUBLE
    RETURN x * x * x - 8.0
END FUNCTION

FUNCTION Parabola1(x AS DOUBLE) AS DOUBLE
    RETURN (x - 1.0) * (x - 1.0)
END FUNCTION

FUNCTION Parabola2(x AS DOUBLE) AS DOUBLE
    RETURN (x - 2.0) * (x - 2.0)
END FUNCTION

SUB Main()
    PRINT "=== OptimizeLab: tutte le 34 funzioni OPT ==="

    ' ── 0. dati ───────────────────────────────────────────────────────────────
    DIM xs(4) AS DOUBLE
    xs(0) = 0.0 : xs(1) = 1.0 : xs(2) = 2.0 : xs(3) = 3.0 : xs(4) = 4.0
    DIM ys(4) AS DOUBLE
    ys(0) = 1.0 : ys(1) = MATH.EXP(0.5) : ys(2) = MATH.EXP(1.0) : ys(3) = MATH.EXP(1.5) : ys(4) = MATH.EXP(2.0)

    ' ── 1. curve fitting (LEVENBERGMARQUARDT, LEVENBERGMARQUARDTVEC, CURVEFIT) ─
    DIM p0(1) AS DOUBLE
    p0(0) = 0.5 : p0(1) = 0.5
    DIM fit(1) AS DOUBLE
    CALL OPTIMIZE.LEVENBERGMARQUARDT(Crescita, p0, xs, ys, fit)
    PRINT "LM      a,b ="; MATH.ROUND(fit(0), 3); ","; MATH.ROUND(fit(1), 3)
    CALL OPTIMIZE.LEVENBERGMARQUARDTVEC(CrescitaVec, p0, xs, ys, fit)
    PRINT "LMVEC   a,b ="; MATH.ROUND(fit(0), 3); ","; MATH.ROUND(fit(1), 3)
    CALL OPTIMIZE.CURVEFIT(Crescita, xs, ys, p0, fit)
    PRINT "CURVEFIT a,b="; MATH.ROUND(fit(0), 3); ","; MATH.ROUND(fit(1), 3)

    ' ── 2. minimi quadrati non lineari (LEASTSQUARESFIT) ──────────────────────
    DIM q0(1) AS DOUBLE
    q0(0) = 0.0 : q0(1) = 0.0
    DIM res(4) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARESFIT(Residui, q0, res)
    PRINT "LSFIT    a,b ="; MATH.ROUND(res(0), 3); ","; MATH.ROUND(res(1), 3)

    ' ── 3. sistemi (FSOLVE, BROYDEN) ─────────────────────────────────────────
    DIM e0(1) AS DOUBLE
    e0(0) = 1.0 : e0(1) = 0.0
    DIM root(1) AS DOUBLE
    CALL OPTIMIZE.FSOLVE(Equilibrio, e0, root)
    PRINT "FSOLVE   x,y ="; MATH.ROUND(root(0), 3); ","; MATH.ROUND(root(1), 3)
    CALL OPTIMIZE.BROYDEN(Equilibrio, e0, root)
    PRINT "BROYDEN  x,y ="; MATH.ROUND(root(0), 3); ","; MATH.ROUND(root(1), 3)

    ' ── 4. algebra lineare (LEASTSQUARES, CHOLESKYSOLVE) ───────────────────
    DIM A(5) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0 : A(2) = 1.0 : A(3) = 2.0 : A(4) = 1.0 : A(5) = 3.0
    DIM yb(2) AS DOUBLE
    yb(0) = 3.0 : yb(1) = 5.0 : yb(2) = 7.0
    DIM lin(1) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARES(A, 3, 2, yb, lin)
    PRINT "LSTSQ    a,b ="; MATH.ROUND(lin(0), 3); ","; MATH.ROUND(lin(1), 3)

    DIM SPD(3) AS DOUBLE
    SPD(0) = 4.0 : SPD(1) = 2.0 : SPD(2) = 2.0 : SPD(3) = 3.0
    DIM rhs(1) AS DOUBLE
    rhs(0) = 10.0 : rhs(1) = 8.0
    DIM chol(1) AS DOUBLE
    CALL OPTIMIZE.CHOLESKYSOLVE(SPD, 2, rhs, chol)
    PRINT "CHOLESKY x,y ="; MATH.ROUND(chol(0), 3); ","; MATH.ROUND(chol(1), 3)

    ' ── 5. programmazione lineare (SIMPLEX, LINPROG, LINPROGEQ) ───────────────
    DIM obj(1) AS DOUBLE
    obj(0) = 2.0 : obj(1) = 3.0
    DIM M(3) AS DOUBLE
    M(0) = 1.0 : M(1) = 1.0 : M(2) = 1.0 : M(3) = 0.0
    DIM bm(1) AS DOUBLE
    bm(0) = 4.0 : bm(1) = 2.0
    DIM sx(2) AS DOUBLE
    CALL OPTIMIZE.SIMPLEX(obj, 2, M, 2, bm, sx)
    PRINT "SIMPLEX  x,y,obj ="; MATH.ROUND(sx(0), 3); ","; MATH.ROUND(sx(1), 3); ","; MATH.ROUND(sx(2), 3)

    DIM cmin(1) AS DOUBLE
    cmin(0) = -1.0 : cmin(1) = -1.0
    DIM xl(1) AS DOUBLE
    CALL OPTIMIZE.LINPROG(cmin, 2, M, 2, bm, xl)
    PRINT "LINPROG  x,y ="; MATH.ROUND(xl(0), 3); ","; MATH.ROUND(xl(1), 3)

    DIM Aub(1) AS DOUBLE
    Aub(0) = 1.0 : Aub(1) = 2.0
    DIM bub(0) AS DOUBLE
    bub(0) = 6.0
    DIM Aeq(1) AS DOUBLE
    Aeq(0) = 1.0 : Aeq(1) = -1.0
    DIM beq(0) AS DOUBLE
    beq(0) = 0.0
    CALL OPTIMIZE.LINPROGEQ(cmin, 2, Aub, 1, bub, Aeq, 1, beq, xl)
    PRINT "LINPROGEQ x,y ="; MATH.ROUND(xl(0), 3); ","; MATH.ROUND(xl(1), 3)

    ' ── 6. minimizzatori (9) ─────────────────────────────────────────────────
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM out(1) AS DOUBLE
    CALL OPTIMIZE.NELDERMEAD(Perdita, x0, out)
    PRINT "NELDERMEAD  ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.BFGS(Rosenbrock, x0, out)
    PRINT "BFGS        ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.LBFGS(Rosenbrock, x0, out)
    PRINT "LBFGS       ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.CONJUGATEGRADIENT(Rosenbrock, x0, out)
    PRINT "CG          ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.POWELL(Perdita, x0, out)
    PRINT "POWELL      ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.TNC(Rosenbrock, x0, out)
    PRINT "TNC         ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.COBYLA(Perdita, x0, out)
    PRINT "COBYLA      ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.SLSQP(Rosenbrock, x0, out)
    PRINT "SLSQP       ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.MINIMIZE(Rosenbrock, x0, out)
    PRINT "MINIMIZE    ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)

    ' ── 7. globali (4) ───────────────────────────────────────────────────────
    DIM bnd(3) AS DOUBLE
    bnd(0) = -5.0 : bnd(1) = 5.0 : bnd(2) = -5.0 : bnd(3) = 5.0
    CALL OPTIMIZE.DUALANNEALING(Perdita, bnd, out)
    PRINT "DUALANNEAL  ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPTIMIZE.DIFFERENTIALEVOLUTION(Perdita, bnd, out)
    PRINT "DIFFEVOL    ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    DIM g0(1) AS DOUBLE
    g0(0) = -1.0 : g0(1) = 1.0
    CALL OPTIMIZE.BASINHOPPING(Perdita, g0, out)
    PRINT "BASINHOP    ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    DIM rng(1) AS DOUBLE
    rng(0) = -1.0 : rng(1) = 1.0
    DIM o1(0) AS DOUBLE
    CALL OPTIMIZE.BRUTE(Scarto, rng, o1)
    PRINT "BRUTE       ="; MATH.ROUND(o1(0), 2)

    ' ── 8. radici e minimo 1D (11) ───────────────────────────────────────────
    PRINT "NEWTON      ="; MATH.ROUND(OPTIMIZE.NEWTON(Radice2, 1.0), 4)
    PRINT "BISECTION   ="; MATH.ROUND(OPTIMIZE.BISECTION(Radice2, 1.0, 2.0), 4)
    PRINT "BRENT       ="; MATH.ROUND(OPTIMIZE.BRENT(Radice2, 1.0, 2.0), 4)
    PRINT "SECANT      ="; MATH.ROUND(OPTIMIZE.SECANT(Radice2, 1.0, 2.0), 4)
    PRINT "BRENTQ      ="; MATH.ROUND(OPTIMIZE.BRENTQ(Cubo8, 1.0, 3.0), 4)
    PRINT "BRENTH      ="; MATH.ROUND(OPTIMIZE.BRENTH(Cubo8, 1.0, 3.0), 4)
    PRINT "RIDDER      ="; MATH.ROUND(OPTIMIZE.RIDDER(Cubo8, 1.0, 3.0), 4)
    PRINT "TOMS748     ="; MATH.ROUND(OPTIMIZE.TOMS748(Cubo8, 1.0, 3.0), 4)
    PRINT "GOLDENSECT  ="; MATH.ROUND(OPTIMIZE.GOLDENSECTION(Parabola2, 0.0, 5.0), 4)
    PRINT "BRENTMIN    ="; MATH.ROUND(OPTIMIZE.BRENTMIN(Parabola1, 0.0, 3.0), 4)
    PRINT "MINIMIZESC  ="; MATH.ROUND(OPTIMIZE.MINIMIZESCALAR(Parabola1, 0.0, 3.0), 4)

    PRINT "=== fine (34/34 funzioni usate) ==="
END SUB
