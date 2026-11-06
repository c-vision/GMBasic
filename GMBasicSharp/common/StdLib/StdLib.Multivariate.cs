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
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvBoostFit, "BOOSTFIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "X"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "engine"), new LongType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nEst"), new LongType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "lr"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "maxDepth"), new LongType(), SymbolFlags.None, null, false)],
                new ArrayType(new DoubleType()), TokenKind.MvBoostFit),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvBoostPredict, "BOOSTPREDICT"),
                [new Parameter(-1, T(TokenKind.Identifier, "model"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false)],
                new DoubleType(), TokenKind.MvBoostPredict, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvBoostScore, "BOOSTSCORE"),
                [new Parameter(-1, T(TokenKind.Identifier, "model"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "X"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false)],
                new DoubleType(), TokenKind.MvBoostScore, ExecutionKind: NativeExecutionKind.Pure),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "KMeansResult", new KMeansResultType() },
            { "PcaResult", new PcaResultType() },
            { "GpResult", new GpResultType() },
            { "SvmResult", new SvmResultType() },
            { "GmmResult", new GmmResultType() },
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
