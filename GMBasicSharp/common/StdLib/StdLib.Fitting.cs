using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>Namespace 'FITTING' (curve fitting, da gmbstats gs_fit.h).</summary>
    private void CreateFittingNamespace()
    {
        const string namespaceName = "FITTING";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayFlag = SymbolFlags.IsArray;

        var xParam = new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), noFlags, null, false);
        var coefParam = new Parameter(-1, T(TokenKind.Identifier, "coef"), new DoubleType(), arrayFlag, null, false);
        var dataParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new DoubleType(), arrayFlag, null, false);
        var yvParam = new Parameter(-1, T(TokenKind.Identifier, "y"), new DoubleType(), arrayFlag, null, false);
        var aParam = new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), arrayFlag, null, false);
        var bParam = new Parameter(-1, T(TokenKind.Identifier, "b"), new DoubleType(), arrayFlag, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FittingPolyFit, "POLYFIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "x"), new DoubleType(), arrayFlag, null, false),
                 yvParam,
                 new Parameter(-1, T(TokenKind.Identifier, "degree"), new LongType(), noFlags, null, false)],
                new PolyFitResultType(), TokenKind.FittingPolyFit),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FittingPolyEval, "POLYEVAL"),
                [xParam, coefParam], new DoubleType(), TokenKind.FittingPolyEval,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FittingChebEval, "CHEBEVAL"),
                [xParam, coefParam], new DoubleType(), TokenKind.FittingChebEval,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FittingPadeEval, "PADEEVAL"),
                [xParam, aParam, bParam], new DoubleType(), TokenKind.FittingPadeEval,
                ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FittingRSquared, "RSQUARED"),
                [dataParam], new AnyType(), TokenKind.FittingRSquared),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.FittingRSquaredTest, "RSQUAREDTEST"),
                [dataParam], new DoubleType(), TokenKind.FittingRSquaredTest,
                ExecutionKind: NativeExecutionKind.Pure),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "PolyFitResult", new PolyFitResultType() },
        };
        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
