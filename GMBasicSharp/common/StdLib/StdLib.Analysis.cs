// FILE: Interpreter.StdLib.Analysis.cs
// ReSharper disable InconsistentNaming

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Analysis — namespace 'ANALYSIS'
 * 44 funzioni | 0 comandi | 7 tipi | 0 costanti
 *
 * FUNZIONI (44):
 *   AdfTest
 *   Anova
 *   Arima
 *   Correlation
 *   CubicSpline
 *   CumTrapezoid
 *   Differentiate
 *   EuclideanDistanceSquared
 *   Fft
 *   FftComplex
 *   FftFreq
 *   FftShift
 *   HoltWinters
 *   Ifft
 *   IfftComplex
 *   IfftShift
 *   Integrate
 *   InterpolateClamp
 *   InterpolateGeneric
 *   InterpolateLinear
 *   InterpolatePchip
 *   InterpolatePolynomial
 *   InterpolateSpline
 *   InterpolateStep
 *   Irfft
 *   KMeans
 *   Kurtosis
 *   MaCoefficients
 *   Mad
 *   Mean
 *   Median
 *   Pca
 *   Regress
 *   Rfft
 *   Simpson
 *   Skewness
 *   SolveIvp
 *   SolveOde
 *   Splev
 *   Splrep
 *   StdDev
 *   Trapezoid
 *   Variance
 *   YuleWalker
 * TIPI (7):
 *   AdfTestResultType
 *   AnovaResultType
 *   ArimaResultType
 *   HoltWintersResultType
 *   KMeansResultType
 *   PcaResultType
 *   RegressionResultType
    ==== GMB-STDLIB-AUDIT-END ==== */


namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- ANALYSIS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'ANALYSIS' (Statistiche Avanzate e Calcolo Numerico).
    /// </summary>
    private void CreateAnalysisNamespace()
    {
        const string namespaceName = "ANALYSIS";
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;

        // Il DataType è il tipo dell'ELEMENTO, il flag IsArray definisce che è un array.
        var dataArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new DoubleType(),
            arrayFlag, null, false);
        var xDataParam = new Parameter(-1, T(TokenKind.Identifier, "x_data"), new DoubleType(),
            arrayFlag, null, false);
        var yDataParam = new Parameter(-1, T(TokenKind.Identifier, "y_data"), new DoubleType(),
            arrayFlag, null, false);

        // Definizione esplicita della firma della callback f(x) -> y
        // Accetta un Double e restituisce un Double.
        var funcParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([new DoubleType()], new DoubleType()), SymbolFlags.None,
            null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Anova, "Anova"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "groups"), new ArrayType(new DoubleType()),
                    arrayFlag, null, false)
            ], new AnovaResultType(), TokenKind.Anova),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Arima, "Arima"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "p"), new IntegerType(), SymbolFlags.None, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "d"), new IntegerType(), SymbolFlags.None, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "q"), new IntegerType(), SymbolFlags.None, null,
                    false)
            ], new ArimaResultType(), TokenKind.Arima),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Regress, "Regress"),
                [xDataParam, yDataParam],
                new RegressionResultType(), TokenKind.Regress),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.KMeans, "KMeans"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "k"), new IntegerType(), SymbolFlags.None, null,
                    false)
            ], new KMeansResultType(), TokenKind.KMeans),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.HoltWinters, "HoltWinters"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "gamma"), new DoubleType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "period"), new IntegerType(), SymbolFlags.None,
                    null, false)
            ], new HoltWintersResultType(), TokenKind.HoltWinters),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AdfTest, "AdfTest"),
                [dataArrayParam],
                new AdfTestResultType(), TokenKind.AdfTest),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Pca, "Pca"),
                [dataArrayParam], new PcaResultType(),
                TokenKind.Pca),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Mean, "Mean"),
                [dataArrayParam], new DoubleType(),
                TokenKind.Mean),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Median, "Median"),
                [dataArrayParam], new DoubleType(),
                TokenKind.Median),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Variance, "Variance"),
                [dataArrayParam], new DoubleType(),
                TokenKind.Variance),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.StdDev, "StdDev"),
                [dataArrayParam], new DoubleType(),
                TokenKind.StdDev),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Skewness, "Skewness"),
                [dataArrayParam], new DoubleType(),
                TokenKind.Skewness),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Kurtosis, "Kurtosis"),
                [dataArrayParam], new DoubleType(),
                TokenKind.Kurtosis),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Mad, "Mad"),
                [dataArrayParam], new DoubleType(),
                TokenKind.Mad),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Correlation, "Correlation"),
                [xDataParam, yDataParam],
                new DoubleType(), TokenKind.Correlation),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Fft, "Fft"),
                [dataArrayParam],
                new ArrayType(new ArrayType(new DoubleType())), TokenKind.Fft),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Ifft, "Ifft"),
                [dataArrayParam],
                new ArrayType(new ArrayType(new DoubleType())), TokenKind.Ifft),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.FftComplex, "FftComplex"),
                [xDataParam, yDataParam],
                new ArrayType(new ArrayType(new DoubleType())), TokenKind.FftComplex),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.IfftComplex, "IfftComplex"),
                [xDataParam, yDataParam],
                new ArrayType(new ArrayType(new DoubleType())), TokenKind.IfftComplex),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolateLinear, "InterpolateLinear"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolateLinear, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolateSpline, "InterpolateSpline"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolateSpline, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolatePolynomial, "InterpolatePolynomial"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolatePolynomial, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolatePchip, "InterpolatePchip"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolatePchip, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolateStep, "InterpolateStep"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolateStep, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolateClamp, "InterpolateClamp"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolateClamp),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.InterpolateGeneric, "InterpolateGeneric"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "method"), new StringType(), SymbolFlags.None,
                        null, false),
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.InterpolateGeneric),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Integrate, "Integrate"),
            [
                funcParam,
                new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.None, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.None, null,
                    false)
            ], new DoubleType(), TokenKind.Integrate),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.Differentiate, "Differentiate"),
                [
                    funcParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ],
                new DoubleType(), TokenKind.Differentiate),

            // --- Nuclei scipy.integrate / scipy.interpolate / scipy.fft (2026-08-25) ---
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Simpson, "Simpson"),
            [
                funcParam,
                new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "n"), new IntegerType(), SymbolFlags.None, null, false)
            ], new DoubleType(), TokenKind.Simpson),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Trapezoid, "Trapezoid"),
                [yDataParam, xDataParam],
                new DoubleType(), TokenKind.Trapezoid),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CumTrapezoid, "CumTrapezoid"),
                [yDataParam, xDataParam],
                new ArrayType(new DoubleType()), TokenKind.CumTrapezoid),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.SolveOde, "SolveOde"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "ode_function"),
                    new FunctionPointerType([new DoubleType(), new DoubleType()], new DoubleType()),
                    SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y0"), new DoubleType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "t0"), new DoubleType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "t1"), new DoubleType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "steps"), new IntegerType(), SymbolFlags.None, null, false)
            ], new ArrayType(new DoubleType()), TokenKind.SolveOde),

            // scipy.integrate.solve_ivp (RK45 adattivo): f(t, y[]) → y'[], traiettoria matrice.
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.SolveIvp, "SolveIvp"),
            [
                /* Bug trovato 2026-09-16 (Lambda-Param-Array-Indicizzato-Cross-Backend, AiWiki):
                 * il primo parametro del callback (t, l'istante corrente) era dichiarato
                 * ArrayType(Double) come il secondo (y, il vettore di stato) — un lambda
                 * codeblock `{|t, y| ...}` ereditava quel tipo via RewriteSingleArgument/
                 * LambdaScope (Linker.Rewrites.Expressions.cs), rendendo "t" un array invece
                 * di uno scalare. La firma reale del callback ODE è f(t: double, y: double[])
                 * → double[] (dy/dt). */
                new Parameter(-1, T(TokenKind.Identifier, "ode_function"),
                    new FunctionPointerType([new DoubleType(), new ArrayType(new DoubleType())], new ArrayType(new DoubleType())),
                    SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "y0"), new ArrayType(new DoubleType()), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "t0"), new DoubleType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "t1"), new DoubleType(), SymbolFlags.None, null, false)
            ], new LongType(), TokenKind.SolveIvp),

            // scipy.interpolate P1: splrep/splev; scipy.fft P1: rfft/irfft.
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.InterpSplrep, "Splrep"),
                [xDataParam, yDataParam], new LongType(), TokenKind.InterpSplrep),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.InterpSplev, "Splev"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "spline"), new LongType(), SymbolFlags.None, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "xq"), new ArrayType(new DoubleType()), SymbolFlags.None, null, false)
                ], new ArrayType(new DoubleType()), TokenKind.InterpSplev),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.FftRfft, "Rfft"),
                [xDataParam], new LongType(), TokenKind.FftRfft),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.FftIrfft, "Irfft"),
                [xDataParam, yDataParam], new ArrayType(new DoubleType()), TokenKind.FftIrfft),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic,
                T(TokenKind.CubicSpline, "CubicSpline"),
                [
                    xDataParam, yDataParam,
                    new Parameter(-1, T(TokenKind.Identifier, "x_new"), new DoubleType(), SymbolFlags.None,
                        null, false)
                ], new DoubleType(), TokenKind.CubicSpline, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.FftFreq, "FftFreq"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "n"), new IntegerType(), SymbolFlags.None, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "d"), new DoubleType(), SymbolFlags.None, null, false)
            ], new ArrayType(new DoubleType()), TokenKind.FftFreq),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.FftShift, "FftShift"),
                [dataArrayParam],
                new ArrayType(new DoubleType()), TokenKind.FftShift),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.IfftShift, "IfftShift"),
                [dataArrayParam],
                new ArrayType(new DoubleType()), TokenKind.IfftShift),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.YuleWalker, "YuleWalker"),
                [dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "order"), new IntegerType(), SymbolFlags.None, null, false)],
                new ArrayType(new DoubleType()), TokenKind.YuleWalker, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MaCoefficients, "MaCoefficients"),
                [dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "order"), new IntegerType(), SymbolFlags.None, null, false)],
                new ArrayType(new DoubleType()), TokenKind.MaCoefficients, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.EuclideanDistanceSquared, "EuclideanDistanceSquared"),
                [xDataParam, yDataParam], new DoubleType(), TokenKind.EuclideanDistanceSquared,
                ExecutionKind: NativeExecutionKind.Pure)
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "AnovaResult", new AnovaResultType() },
            { "ArimaResult", new ArimaResultType() },
            { "RegressionResult", new RegressionResultType() },
            { "KMeansResult", new KMeansResultType() },
            { "HoltWintersResult", new HoltWintersResultType() },
            { "AdfTestResult", new AdfTestResultType() },
            { "PcaResult", new PcaResultType() }
        };

        RegisterTypeSchema(typeof(AnovaResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "FValue", new DoubleType() },
            { "PValue", new DoubleType() },
            { "DFBetween", new IntegerType() },
            { "DFWithin", new IntegerType() },
            { "SSBetween", new DoubleType() },
            { "SSWithin", new DoubleType() },
            { "MSBetween", new DoubleType() },
            { "MSWithin", new DoubleType() }
        });

        RegisterTypeSchema(typeof(ArimaResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ArCoeffs", new ArrayType(new DoubleType()) },
            { "MaCoeffs", new ArrayType(new DoubleType()) },
            { "AIC", new DoubleType() },
            { "Fitted", new ArrayType(new DoubleType()) }
        });

        RegisterTypeSchema(typeof(RegressionResultType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Slope", new DoubleType() },
                { "Intercept", new DoubleType() },
                { "R2", new DoubleType() },
                { "StandardError", new DoubleType() }
            });

        RegisterTypeSchema(typeof(KMeansResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Centroids", new ArrayType(new DoubleType()) },
            { "Assignments", new ArrayType(new IntegerType()) },
            { "Iterations", new IntegerType() }
        });

        RegisterTypeSchema(typeof(HoltWintersResultType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Forecast", new ArrayType(new DoubleType()) },
                { "Mse", new DoubleType() }
            });

        RegisterTypeSchema(typeof(AdfTestResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "Statistic", new DoubleType() },
            { "PValue", new DoubleType() }
        });

        RegisterTypeSchema(typeof(PcaResultType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "EigenValues", new ArrayType(new DoubleType()) },
            { "EigenVectors", new ArrayType(new ArrayType(new DoubleType())) }
        });

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
