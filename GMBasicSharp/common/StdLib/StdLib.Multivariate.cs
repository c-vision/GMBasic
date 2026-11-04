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
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "KMeansResult", new KMeansResultType() },
            { "PcaResult", new PcaResultType() },
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
