using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>Namespace 'MULTIVARIATE' (split da ANALYSIS, 2026-11-04).</summary>
    private void CreateMultivariateNamespace()
    {
        const string namespaceName = "MULTIVARIATE";
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;

        var dataArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new DoubleType(),
            arrayFlag, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.KMeans, "KMeans"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "k"), new IntegerType(), SymbolFlags.None, null,
                    false)
            ], new KMeansResultType(), TokenKind.KMeans),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Pca, "Pca"),
                [dataArrayParam], new PcaResultType(),
                TokenKind.Pca),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvGpr, "GPRREGRESS"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "xs"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "l2"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "noise"), new DoubleType(), SymbolFlags.None, null, false)],
                new GpResultType(), TokenKind.MvGpr),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtLambdaPlus, "RMTLAMBDAPLUS"),
                [new Parameter(-1, T(TokenKind.Identifier, "n"), new LongType(), SymbolFlags.None, null, false), new Parameter(-1, T(TokenKind.Identifier, "T"), new LongType(), SymbolFlags.None, null, false), new Parameter(-1, T(TokenKind.Identifier, "sigma2"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.RmtLambdaPlus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtLambdaMinus, "RMTLAMBDAMINUS"),
                [new Parameter(-1, T(TokenKind.Identifier, "n"), new LongType(), SymbolFlags.None, null, false), new Parameter(-1, T(TokenKind.Identifier, "T"), new LongType(), SymbolFlags.None, null, false), new Parameter(-1, T(TokenKind.Identifier, "sigma2"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.RmtLambdaMinus, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtCoherence, "RMTCOHERENCE"),
                [new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), arrayFlag, null, false)],
                new DoubleType(), TokenKind.RmtCoherence, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtSpectralNorm, "RMTSPECTRALNORM"),
                [new Parameter(-1, T(TokenKind.Identifier, "A"), new DoubleType(), arrayFlag, null, false), new Parameter(-1, T(TokenKind.Identifier, "n"), new LongType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.RmtSpectralNorm, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtDavisKahan, "RMTDAVISKAHAN"),
                [new Parameter(-1, T(TokenKind.Identifier, "eNorm"), new DoubleType(), SymbolFlags.None, null, false), new Parameter(-1, T(TokenKind.Identifier, "gap"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.RmtDavisKahan, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtPrincipalAngle, "RMTPRINCIPALANGLE"),
                [new Parameter(-1, T(TokenKind.Identifier, "u"), new DoubleType(), arrayFlag, null, false), new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), arrayFlag, null, false)],
                new DoubleType(), TokenKind.RmtPrincipalAngle, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.RmtTightness, "RMTTIGHTNESS"),
                [new Parameter(-1, T(TokenKind.Identifier, "observed"), new DoubleType(), SymbolFlags.None, null, false), new Parameter(-1, T(TokenKind.Identifier, "bound"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.RmtTightness, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvGaussPdf, "MvGaussPdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false), new Parameter(-1, T(TokenKind.Identifier, "loc"), new DoubleType(), arrayFlag, null, false), new Parameter(-1, T(TokenKind.Identifier, "cov"), new DoubleType(), arrayFlag, null, false)], new DoubleType(), TokenKind.MvGaussPdf,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvStudentPdf, "MvStudentPdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false), new Parameter(-1, T(TokenKind.Identifier, "loc"), new DoubleType(), arrayFlag, null, false), new Parameter(-1, T(TokenKind.Identifier, "cov"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nu"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.MvStudentPdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CopulaClaytonCdf, "CopulaClaytonCdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "u"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "theta"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.CopulaClaytonCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CopulaGumbelCdf, "CopulaGumbelCdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "u"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "theta"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.CopulaGumbelCdf, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.CopulaFrankCdf, "CopulaFrankCdf"),
                [new Parameter(-1, T(TokenKind.Identifier, "u"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "v"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "theta"), new DoubleType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.CopulaFrankCdf, ExecutionKind: NativeExecutionKind.Pure),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "KMeansResult", new KMeansResultType() },
            { "PcaResult", new PcaResultType() },
            { "GpResult", new GpResultType() },
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
