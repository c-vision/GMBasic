using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>Namespace 'NUMERICAL' (split da ANALYSIS, 2026-11-04).</summary>
    private void CreateNumericalNamespace()
    {
        const string namespaceName = "NUMERICAL";
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;

        var dataArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new DoubleType(),
            arrayFlag, null, false);
        var xDataParam = new Parameter(-1, T(TokenKind.Identifier, "x_data"), new DoubleType(),
            arrayFlag, null, false);
        var yDataParam = new Parameter(-1, T(TokenKind.Identifier, "y_data"), new DoubleType(),
            arrayFlag, null, false);
        var funcParam = new Parameter(-1, T(TokenKind.Identifier, "function"),
            new FunctionPointerType([new DoubleType()], new DoubleType()), SymbolFlags.None,
            null, false);

        var procedures = new ProcedureDefinition[]
        {
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
                /* Bug trovato 2026-09-16 (Lambda-Param-Array-Indicizzato-Cross-Backend,):
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
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
