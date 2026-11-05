using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    /// <summary>Namespace 'SIMULATION' (RNG deterministico + distribuzioni campionarie).</summary>
    private void CreateSimulationNamespace()
    {
        const string namespaceName = "SIMULATION";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;

        var seedParam = new Parameter(-1, T(TokenKind.Identifier, "seed"), new DoubleType(), noFlags, null, false);
        var rateParam = new Parameter(-1, T(TokenKind.Identifier, "rate"), new DoubleType(), noFlags, null, false);
        var pParam = new Parameter(-1, T(TokenKind.Identifier, "p"), new DoubleType(), noFlags, null, false);
        var nParam = new Parameter(-1, T(TokenKind.Identifier, "n"), new DoubleType(), noFlags, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SimSeed, "SIMSEED"),
                [seedParam], new DoubleType(), TokenKind.SimSeed),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SimUniform, "SIMUNIFORM"),
                [], new DoubleType(), TokenKind.SimUniform),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SimNormal, "SIMNORMAL"),
                [], new DoubleType(), TokenKind.SimNormal),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SimExponential, "SIMEXP"),
                [rateParam], new DoubleType(), TokenKind.SimExponential),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SimBernoulli, "SIMBERNOULLI"),
                [pParam], new DoubleType(), TokenKind.SimBernoulli),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SimBinomial, "SIMBINOMIAL"),
                [nParam, pParam], new DoubleType(), TokenKind.SimBinomial),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase);
        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
