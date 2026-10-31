// FILE: Interpreter.StdLib.Trading.Portfolio.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Trading.Portfolio — namespace 'TRADING.PORTFOLIO'
 * 19 funzioni | 0 comandi | 3 tipi | 0 costanti
 *
 * FUNZIONI (19):
 *   BLACKLITTERMAN
 *   CVAR
 *   EQUALLYWEIGHTED
 *   EVOLVE
 *   EXPECTEDRETURN
 *   ISLONGONLY
 *   KELLYCRITERION
 *   MAXRETURN
 *   MAXSHARPE
 *   MEANVARIANCE
 *   MINCDAR
 *   MINCVAR
 *   MINVOLATILITY
 *   PORTFOLIOMAD
 *   PORTFOLIOSEMIMAD
 *   PVAR
 *   RISKPARITY
 *   SHARPERATIO
 *   STDEV
 * TIPI (3):
 *   GaParametersType
 *   PortfolioConstraintsType
 *   PortfolioResultType
    ==== GMB-STDLIB-AUDIT-END ==== */


namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRADING.PORTFOLIO NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'TRADING.PORTFOLIO', ora completo di tutte le strategie di ottimizzazione.
    /// </summary>
    private void CreateTradingPortfolioNamespace()
    {
        const string namespaceName = "TRADING.PORTFOLIO";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayFlag = SymbolFlags.IsArray;

        var doubleArrayType = new DoubleType(); 

        var procedures = new ProcedureDefinition[]
        {
            // --- Funzioni di calcolo base ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioExpectedReturn, "EXPECTEDRETURN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "pesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rendimentiAttesi"), doubleArrayType, arrayFlag, null, false)
            ], new DoubleType(), TokenKind.PortfolioExpectedReturn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioStDev, "STDEV"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "pesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false)
            ], new DoubleType(), TokenKind.PortfolioStDev),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioSharpeRatio, "SHARPERATIO"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimentoPortafoglio"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rischioPortafoglio"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tassoRiskFree"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.PortfolioSharpeRatio),

            // --- Funzioni di ottimizzazione ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MeanVariance, "MEANVARIANCE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimentiAttesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "vincoli"), new PortfolioConstraintsType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.MeanVariance),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MinVolatility, "MINVOLATILITY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false)
            ], new PortfolioResultType(), TokenKind.MinVolatility),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MaxSharpe, "MAXSHARPE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimentiAttesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tassoRiskFree"), new DoubleType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.MaxSharpe),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BlackLitterman, "BLACKLITTERMAN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "marketCaps"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "riskAversion"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tau"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "P"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "Q"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "omega"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tassoRiskFree"), new DoubleType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.BlackLitterman),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MinCvar, "MINCVAR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_storici"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "livello_confidenza"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tassoRiskFree"), new DoubleType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.MinCvar),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MinCdar, "MINCDAR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_storici"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "livello_confidenza"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tassoRiskFree"), new DoubleType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.MinCdar),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.KellyCriterion, "KELLYCRITERION"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_storici"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tassoRiskFree"), new DoubleType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.KellyCriterion),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RiskParity, "RISKPARITY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false)
            ], new PortfolioResultType(), TokenKind.RiskParity),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Evolve, "EVOLVE"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimentiAttesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "matriceCovarianza"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "parametriGA"), new GaParametersType(), noFlags, null, false)
            ], new PortfolioResultType(), TokenKind.Evolve),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EquallyWeighted, "EQUALLYWEIGHTED"),
                [new Parameter(-1, T(TokenKind.Identifier, "numeroAsset"), new LongType(), noFlags, null, false)],
                new PortfolioResultType(), TokenKind.EquallyWeighted),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MaxReturn, "MAXRETURN"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimentiAttesi"), doubleArrayType, arrayFlag, null, false)
            ], new PortfolioResultType(), TokenKind.MaxReturn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioVar, "PVAR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_storici"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "livello_confidenza"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.PortfolioVar),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioCVar, "CVAR"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_storici"), new ArrayType(doubleArrayType), arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "pesi"), doubleArrayType, arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "livello_confidenza"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.PortfolioCVar),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.IsLongOnly, "ISLONGONLY"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "pesi"), doubleArrayType, arrayFlag, null, false)
            ], new BitType(), TokenKind.IsLongOnly),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioMad, "PORTFOLIOMAD"),
            [new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), new ArrayType(new DoubleType()), SymbolFlags.None, null, false)
            ], new DoubleType(), TokenKind.PortfolioMad),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PortfolioSemiMad, "PORTFOLIOSEMIMAD"),
            [new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), new ArrayType(new DoubleType()), SymbolFlags.None, null, false),
             new Parameter(-1, T(TokenKind.Identifier, "target"), new DoubleType(), SymbolFlags.IsOptional, null, false)
            ], new DoubleType(), TokenKind.PortfolioSemiMad),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "PortfolioResult", new PortfolioResultType() },
            { "PortfolioConstraints", new PortfolioConstraintsType() },
            { "GaParameters", new GaParametersType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);

        RegisterTypeSchema(typeof(PortfolioResultType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Weights", new ArrayType(new DoubleType()) },
                { "ExpectedReturn", new DoubleType() },
                { "Volatility", new DoubleType() },
                { "SharpeRatio", new DoubleType() },
                { "Converged", new BitType() }
            });

        RegisterTypeSchema(typeof(PortfolioConstraintsType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "MinWeight", new DoubleType() },
                { "MaxWeight", new DoubleType() },
                { "TargetReturn", new DoubleType() },
                { "TargetVolatility", new DoubleType() },
                { "SumOfWeights", new DoubleType() },
                { "IsLongOnly", new BitType() }
            });

        RegisterTypeSchema(typeof(GaParametersType), new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "PopulationSize", new IntegerType() },
            { "Generations", new IntegerType() },
            { "MutationRate", new DoubleType() },
            { "CrossoverRate", new DoubleType() }
        });
    }
}