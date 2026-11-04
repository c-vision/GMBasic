using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>Namespace 'REGRESSION' (split da ANALYSIS, 2026-11-04).</summary>
    private void CreateRegressionNamespace()
    {
        const string namespaceName = "REGRESSION";
        const SymbolFlags arrayFlag = SymbolFlags.IsArray;

        var xDataParam = new Parameter(-1, T(TokenKind.Identifier, "x_data"), new DoubleType(),
            arrayFlag, null, false);
        var yDataParam = new Parameter(-1, T(TokenKind.Identifier, "y_data"), new DoubleType(),
            arrayFlag, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Regress, "Regress"),
                [xDataParam, yDataParam],
                new RegressionResultType(), TokenKind.Regress),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "RegressionResult", new RegressionResultType() },
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
