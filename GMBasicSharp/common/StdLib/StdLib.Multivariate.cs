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
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvGmm, "GMMEM1D"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "k"), new LongType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "iters"), new LongType(), SymbolFlags.None, null, false)],
                new GmmResultType(), TokenKind.MvGmm),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvBonext, "BONEXT"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "cand"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "l2"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "noise"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "kappa"), new DoubleType(), SymbolFlags.None, null, false)],
                new AnyType(), TokenKind.MvBonext),
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
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvSvmFit, "SVMFIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "X"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "k"), new LongType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "C"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "maxIter"), new LongType(), SymbolFlags.None, null, false)],
                new SvmResultType(), TokenKind.MvSvmFit),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvSvmPredict, "SVMPREDICT"),
                [new Parameter(-1, T(TokenKind.Identifier, "W"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "d"), new LongType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "k"), new LongType(), SymbolFlags.None, null, false)],
                new DoubleType(), TokenKind.MvSvmPredict, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MvGpr, "GPRREGRESS"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "xs"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "l2"), new DoubleType(), SymbolFlags.None, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "noise"), new DoubleType(), SymbolFlags.None, null, false)],
                new GpResultType(), TokenKind.MvGpr),
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
