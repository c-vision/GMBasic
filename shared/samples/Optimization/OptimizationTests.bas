REM FILE: OptimizationTests.bas

' FILE: Optimize/OptimizationTests.bas
' =================================================================================================
'       TEST UNITARIO: OPTIMIZE - ALGORITMI DI OTTIMIZZAZIONE NUMERICA
' =================================================================================================
' Questo test verifica le funzioni del namespace OPTIMIZE, dedicato alla ricerca di radici,
' minimizzazione di funzioni e solving di sistemi non lineari.
'
' COPERTURA:
' - Ricerca Radici (1D): Newton, Brent, Bisection, Secant.
' - Minimizzazione (1D): GoldenSection.
' - Minimizzazione (Multi-dim): NelderMead, Broyden, Bfgs, LBfgs, Simplex.
' - Least Squares: Linear Least Squares solver.
' - Non-Linear Least Squares: LevenbergMarquardt.
' =================================================================================================

IMPORT OPTIMIZE
IMPORT MATH

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ OPTIMIZE - Algoritmi di Ottimizzazione                                 ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. RICERCA RADICI (Root Finding 1D) ---
    PRINT "--- 1. Ricerca Radici (f(x) = 0) ---"
    
    ' Funzione target: f(x) = x^2 - 4 (Radici: +2, -2)
    DIM Func = FN(x AS DOUBLE) AS DOUBLE => x^2 - 4
    
    ' Newton-Raphson (richiede derivata o stima interna)
    DIM RootNewton = OPTIMIZE.NEWTON(Func, 1.0) ' Guess iniziale 1.0 -> dovrebbe convergere a 2
    PRINT "Newton (x^2 - 4, guess 1): "; MATH.ROUND(RootNewton, 6)
    
    ' Brent (metodo robusto con intervallo)
    DIM RootBrent = OPTIMIZE.BRENT(Func, 0, 5) ' Cerca tra 0 e 5
    PRINT "Brent (0, 5):              "; MATH.ROUND(RootBrent, 6)
    
    ' Bisection
    DIM RootBisect = OPTIMIZE.BISECTION(Func, 0, 5)
    PRINT "Bisection (0, 5):          "; MATH.ROUND(RootBisect, 6)
    
    ' Secant
    DIM RootSecant = OPTIMIZE.SECANT(Func, 1, 3)
    PRINT "Secant (1, 3):             "; MATH.ROUND(RootSecant, 6)
    PRINT

    ' --- 2. MINIMIZZAZIONE MONODIMENSIONALE ---
    PRINT "--- 2. Minimizzazione 1D ---"
    
    ' Funzione target: f(x) = (x-3)^2 + 1 (Minimo a x=3, val=1)
    DIM MinFunc = FN(x AS DOUBLE) AS DOUBLE => (x-3)^2 + 1
    
    ' Golden Section Search
    DIM MinX = OPTIMIZE.GOLDENSECTION(MinFunc, 0, 6)
    PRINT "Golden Section (min (x-3)^2+1): x="; MATH.ROUND(MinX, 6)
    PRINT

    ' --- 3. MINIMIZZAZIONE MULTIDIMENSIONALE ---
    PRINT "--- 3. Minimizzazione N-D ---"
    
    ' Funzione di Rosenbrock (banana function)
    ' f(x,y) = (1-x)^2 + 100(y-x^2)^2. Minimo a (1,1).
    ' NOTA: La firma delle funzioni N-D richiede un array di double come input.
    '       In GM-BASIC, usiamo un FunctionPointerType che accetta Array(Double).
    
    DIM Rosenbrock = FN(v() AS DOUBLE) AS DOUBLE:
        DIM x = v(0)
        DIM y = v(1)
        RETURN (1-x)^2 + 100 * (y - x^2)^2
    END FN
    
    DIM InitialGuess(1) AS DOUBLE = {0.0, 0.0}
    
    ' Nelder-Mead (Simplex Method) - Derivative Free
    DIM MinNM() = OPTIMIZE.NELDERMEAD(Rosenbrock, InitialGuess)
    PRINT "Nelder-Mead (Rosenbrock): ("; MATH.ROUND(MinNM(0), 4); ", "; MATH.ROUND(MinNM(1), 4); ")"
    
    ' BFGS (Quasi-Newton)
    DIM MinBFGS() = OPTIMIZE.BFGS(Rosenbrock, InitialGuess)
    PRINT "BFGS (Rosenbrock):        ("; MATH.ROUND(MinBFGS(0), 4); ", "; MATH.ROUND(MinBFGS(1), 4); ")"

    DIM MinLBFGS() = OPTIMIZE.LBFGS(Rosenbrock, InitialGuess)
    PRINT "L-BFGS (Rosenbrock):      ("; MATH.ROUND(MinLBFGS(0), 4); ", "; MATH.ROUND(MinLBFGS(1), 4); ")"
    
    DIM RootSystem = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE:
        DIM residuals(1) AS DOUBLE
        residuals(0) = v(0)^2 - 2
        residuals(1) = v(1) - 3
        RETURN residuals
    END FN
    DIM RootGuess(1) AS DOUBLE = {1.0, 0.0}
    DIM RootBroyden() = OPTIMIZE.BROYDEN(RootSystem, RootGuess)
    PRINT "Broyden system:           ("; MATH.ROUND(RootBroyden(0), 4); ", "; MATH.ROUND(RootBroyden(1), 4); ")"
    PRINT

    ' --- 4. LEAST SQUARES (Linear & Non-Linear) ---
    PRINT "--- 4. Minimi Quadrati ---"
    
    ' Linear Least Squares: Ax = b
    ' Risolve sistemi sovradeterminati minimizzando ||Ax - b||^2
    DIM A_ls(5) AS DOUBLE ' Matrice 3x2 linearizzata: [[1,1],[1,2],[1,3]]
    A_ls = {1,1, 1,2, 1,3}
    DIM b_ls(2) AS DOUBLE = {2, 4, 6} ' y = 2x. Punti (1,2), (2,4), (3,6)
    
    ' NOTA: La firma richiede Array(Array(Double)) per A.
    ' Usiamo AS ARRAY OF DOUBLE per tipizzare ogni elemento come array.
    DIM MatA(2) AS ARRAY OF DOUBLE
    MatA(0) = {1, 1} : MatA(1) = {1, 2} : MatA(2) = {1, 3}
    
    DIM SolLS() = OPTIMIZE.LEASTSQUARES(MatA, b_ls)
    PRINT "Linear LS (y=2x): Intercept="; MATH.ROUND(SolLS(0), 4); " Slope="; MATH.ROUND(SolLS(1), 4)

    ' Linear programming: massimizza 2x + 3y con x+y<=4 e x<=2.
    DIM Objective(1) AS DOUBLE = {2, 3}
    DIM Constraints(1) AS ARRAY OF DOUBLE
    Constraints(0) = {1, 1} : Constraints(1) = {1, 0}
    DIM Bounds(1) AS DOUBLE = {4, 2}
    DIM SolSimplex() = OPTIMIZE.SIMPLEX(Objective, Constraints, Bounds)
    PRINT "Simplex: x="; MATH.ROUND(SolSimplex(0), 4); " y="; MATH.ROUND(SolSimplex(1), 4); " objective="; MATH.ROUND(SolSimplex(2), 4)
    
    ' Non-Linear Least Squares (Levenberg-Marquardt)
    ' Adatta parametri di un modello ai dati.
    ' Modello: y = a * exp(b * x)
    DIM DataX() = {0, 1, 2, 3, 4}
    DIM DataY() = {1.0, 2.7, 7.4, 20.1, 54.6} ' Exp-like data
    DIM InitialParams(1) AS DOUBLE = {0.5, 0.5} ' Guess per [a, b]
    
    ' Funzione modello: f(params, x) = params(0) * exp(params(1) * x)
    DIM ModelFunc = FN(params() AS DOUBLE, x AS DOUBLE) AS DOUBLE => params(0) * Math.Exp(params(1) * x)
    
    DIM ParamsLM() = OPTIMIZE.LEVENBERGMARQUARDT(ModelFunc, InitialParams, DataX, DataY)
    PRINT "Levenberg-Marquardt: a="; MATH.ROUND(ParamsLM(0), 4); " b="; MATH.ROUND(ParamsLM(1), 4)
    
    ' Conjugate Gradient (minimizzazione multi-dimensionale)
    ' Funzione Rosenbrock: f(x,y) = (1-x)^2 + 100*(y-x^2)^2 (Minimum at x=1, y=1)
    DIM RosenbrockCG = FN(x() AS DOUBLE) AS DOUBLE => (1 - x(0))^2 + 100 * (x(1) - x(0)^2)^2
    DIM StartCG(1) AS DOUBLE = {0, 0}
    
    DIM ResultCG() = OPTIMIZE.CONJUGATEGRADIENT(RosenbrockCG, StartCG)
    PRINT "Conjugate Gradient: x="; MATH.ROUND(ResultCG(0), 4); " y="; MATH.ROUND(ResultCG(1), 4)
    
    PRINT
    PRINT "Test Completato."

END SUB
