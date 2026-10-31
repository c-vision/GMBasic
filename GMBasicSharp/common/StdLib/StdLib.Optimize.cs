// FILE: Interpreter.StdLib.Optimize.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Optimize — namespace 'OPTIMIZE'
 * 34 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (34):
 *   BasinHopping
 *   Bfgs
 *   Bisection
 *   Brent
 *   BrentMin
 *   Brenth
 *   Brentq
 *   Broyden
 *   Brute
 *   CholeskySolve
 *   Cobyla
 *   ConjugateGradient
 *   CurveFit
 *   DifferentialEvolution
 *   DualAnnealing
 *   Fsolve
 *   GoldenSection
 *   LBfgs
 *   LeastSquares
 *   LeastSquaresFit
 *   LevenbergMarquardt
 *   LevenbergMarquardtVec
 *   Linprog
 *   Minimize
 *   MinimizeScalar
 *   NelderMead
 *   Newton
 *   Powell
 *   Ridder
 *   Secant
 *   Simplex
 *   Slsqp
 *   Tnc
 *   Toms748
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- OPTIMIZE NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'OPTIMIZE'.
    /// </summary>
    private void CreateOptimizeNamespace()
    {
        const string namespaceName = "OPTIMIZE";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;
        const SymbolFlags optionalFlag = SymbolFlags.IsOptional;

        var doubleArrayType = new ArrayType(new DoubleType());

        // Funzione scalare f(x) -> y: root-finding / minimizzazione 1D (Newton/Brent/Bisection/
        // Secant/GoldenSection). Nessun mismatch con CommonLib.Numerics: firma genuinamente 1D.
        var scalarFuncParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([new DoubleType()], new DoubleType()), noFlags, null, false);

        // Funzione multivariata f(x()) -> y: minimizzazione N-dimensionale (NelderMead/Bfgs/
        // LBfgs/ConjugateGradient). CORRETTA da FunctionPointerType([Double],Double) — la firma
        // scalare originale non poteva rappresentare una funzione di più variabili, scoperto
        // durante il wiring: nessuna di queste era mai stata dispatchata prima (stub), quindi
        // nessun programma esistente può dipendere dalla firma scalare errata.
        var vectorToScalarFuncParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([doubleArrayType], new DoubleType()), noFlags, null, false);

        // Funzione vettoriale F(x()) -> y(): ricerca di radici di sistemi non lineari (Broyden).
        // Stessa correzione di cui sopra: era scalare, ora vettore-a-vettore.
        var vectorToVectorFuncParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([doubleArrayType], doubleArrayType), noFlags, null, false);

        // Funzione modello f(parametri(), x) -> previsione: curve-fitting Levenberg-Marquardt.
        var lmModelFuncParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([doubleArrayType, new DoubleType()], new DoubleType()), noFlags, null, false);

        // Variante vettoriale: f(parametri(), x()) -> previsione().
        var lmVecModelFuncParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([doubleArrayType, doubleArrayType], doubleArrayType), noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Newton, "Newton"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "guess"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Newton),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Brent, "Brent"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "lower"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "upper"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Brent),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Simplex, "Simplex"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "c"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "A"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.Simplex, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Broyden, "Broyden"),
            [
                vectorToVectorFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.Broyden),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NelderMead, "NelderMead"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.NelderMead),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Bisection, "Bisection"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Bisection),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Secant, "Secant"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "x0"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "x1"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Secant),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LeastSquares, "LeastSquares"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "A"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.LeastSquares, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CholeskySolve, "CholeskySolve"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "A"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.CholeskySolve, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GoldenSection, "GoldenSection"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.GoldenSection),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Bfgs, "Bfgs"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.Bfgs),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LBfgs, "LBfgs"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.LBfgs),

            // ConjugateGradient era dichiarata (A, b) come se fosse un solutore lineare, ma
            // CommonLib.Numerics.ConjugateGradient è un minimizzatore di funzione multivariata
            // (stessa forma di Bfgs/LBfgs/NelderMead) — nessuna funzione con la firma (A,b)
            // esiste. Corretta per matchare l'algoritmo reale, scoperto durante il wiring (era
            // uno stub, nessun programma esistente può dipendere dalla firma errata).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ConjugateGradient, "ConjugateGradient"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.ConjugateGradient),

            // LevenbergMarquardt(Vec) dichiarate con soli 2 parametri (function, initialGuess),
            // ma l'algoritmo di curve-fitting sottostante richiede anche inputs/targets — aggiunti
            // qui, scoperto durante il wiring (era uno stub, nessuna firma precedente da rompere).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LevenbergMarquardt, "LevenbergMarquardt"),
            [
                lmModelFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "inputs"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "targets"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.LevenbergMarquardt),

            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.LevenbergMarquardtVec, "LevenbergMarquardtVec"),
            [
                lmVecModelFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "initialGuess"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "inputs"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "targets"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.LevenbergMarquardtVec),

            // CurveFit: scipy.optimize.curve_fit — modello f(p[], x) → y, dati (x, y), p0.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CurveFit, "CurveFit"),
            [
                lmModelFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "xdata"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "ydata"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "p0"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.CurveFit),

            // DifferentialEvolution: scipy.optimize.differential_evolution — minimizza f(p[]) sui bounds (n×2).
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DifferentialEvolution, "DifferentialEvolution"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag, null, false)
            ], doubleArrayType, TokenKind.DifferentialEvolution),

            // --- scipy.optimize completato: radici scalari (root_scalar) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeBrentq, "Brentq"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.OptimizeBrentq),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeBrenth, "Brenth"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.OptimizeBrenth),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeRidder, "Ridder"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.OptimizeRidder),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeToms748, "Toms748"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.OptimizeToms748),

            // --- minimizzazione scalare (minimize_scalar) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeBrentMin, "BrentMin"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.OptimizeBrentMin),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeMinimizeScalar, "MinimizeScalar"),
            [
                scalarFuncParam, new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "method"), new StringType(), optionalFlag, null, false)
            ], new DoubleType(), TokenKind.OptimizeMinimizeScalar),

            // --- minimizzazione multivariata (minimize) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizePowell, "Powell"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizePowell),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeTnc, "Tnc"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeTnc),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeCobyla, "Cobyla"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeCobyla),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeSlsqp, "Slsqp"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeSlsqp),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeMinimize, "Minimize"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "method"), new StringType(), optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeMinimize),

            // --- root multivariata (root/fsolve) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeFsolve, "Fsolve"),
            [
                vectorToVectorFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeFsolve),

            // --- least_squares nonlineare ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeLeastSquares, "LeastSquaresFit"),
            [
                vectorToVectorFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeLeastSquares),

            // --- programmazione lineare (linprog) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeLinprog, "Linprog"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "c"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "A_ub"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b_ub"), doubleArrayType, arrayFlag | optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "A_eq"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b_eq"), doubleArrayType, arrayFlag | optionalFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag | optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeLinprog),

            // --- globali ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeBrute, "Brute"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "ranges"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "N"), new IntegerType(), optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeBrute),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeBasinHopping, "BasinHopping"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "x0"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "niter"), new IntegerType(), optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeBasinHopping),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OptimizeDualAnnealing, "DualAnnealing"),
            [
                vectorToScalarFuncParam,
                new Parameter(-1, T(TokenKind.Identifier, "bounds"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "maxiter"), new IntegerType(), optionalFlag, null, false)
            ], doubleArrayType, TokenKind.OptimizeDualAnnealing)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            // Non definisco i tipi di risultato espliciti qui per l'ottimizzazione,
            // assumendo che i risultati siano semplicemente array di double.
            // Se necessario, li aggiungeremo in un secondo momento.
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
