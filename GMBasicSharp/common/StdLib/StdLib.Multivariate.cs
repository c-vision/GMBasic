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
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
