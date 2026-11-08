// FILE: Interpreter.StdLib.Trading.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Trading — namespace 'TRADING'
 * 12 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (12):
 *   BidAskSpread
 *   CAPM
 *   CapitalGainsYield
 *   GeometricMeanReturn
 *   GrowthRate
 *   HoldingPeriodReturn
 *   PreferredStockValue
 *   RiskPremium
 *   StockValueConstGrowth
 *   StockValueZeroGrowth
 *   TaxEquivalentYield
 *   TotalStockReturn
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRADING NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'TRADING'.
    /// Contiene funzioni generali di valutazione e analisi che non rientrano
    /// specificamente negli altri sotto-namespace di trading.
    /// </summary>
    private void CreateTradingNamespace()
    {
        const string namespaceName = "TRADING";
        const SymbolFlags publicFlag = SymbolFlags.IsPublic;
        const SymbolFlags noFlags = SymbolFlags.None;
        
        var procedures = new ProcedureDefinition[]
        {
            // Funzioni di valutazione e modelli
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StockValueZeroGrowth, "StockValueZeroGrowth"), 
                [
                    new Parameter(-1, T(TokenKind.Identifier, "dividend"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "requiredRate"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(),
                TokenKind.StockValueZeroGrowth, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.StockValueConstGrowth, "StockValueConstGrowth"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "d1"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "k"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "g"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(),
                TokenKind.StockValueConstGrowth, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PreferredStockValue, "PreferredStockValue"), 
                [
                    new Parameter(-1, T(TokenKind.Identifier, "dividend"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "requiredRate"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(),
                TokenKind.PreferredStockValue, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CAPM, "CAPM"), 
                [
                    new Parameter(-1, T(TokenKind.Identifier, "riskFreeRate"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "marketReturn"), new DoubleType(), noFlags, null, false)
                ], new DoubleType(),
                TokenKind.CAPM, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TaxEquivalentYield, "TaxEquivalentYield"), 
                [
                    new Parameter(-1, T(TokenKind.Identifier, "taxFreeYield"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "taxRate"), new DoubleType(), noFlags, null, false)
                ],
                new DoubleType(),
                TokenKind.TaxEquivalentYield, ExecutionKind: NativeExecutionKind.Pure),

            // Calcoli di rendimento e rischio più generici
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RiskPremium, "RiskPremium"), 
                [
                    new Parameter(-1, T(TokenKind.Identifier, "expectedReturn"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "riskFreeRate"), new DoubleType(), noFlags, null, false)
                ], new DoubleType(),
                TokenKind.RiskPremium, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TotalStockReturn, "TotalStockReturn"),
                [new Parameter(-1, T(TokenKind.Identifier, "initialPrice"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "endPrice"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "dividends"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.TotalStockReturn, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CapitalGainsYield, "CapitalGainsYield"),
                [new Parameter(-1, T(TokenKind.Identifier, "initialPrice"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "endPrice"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.CapitalGainsYield, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.HoldingPeriodReturn, "HoldingPeriodReturn"), [new Parameter(-1, T(TokenKind.Identifier, "returns"), new DoubleType(), SymbolFlags.IsArray, null, false)],
                new DoubleType(),
                TokenKind.HoldingPeriodReturn, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GeometricMeanReturn, "GeometricMeanReturn"), [new Parameter(-1, T(TokenKind.Identifier, "returns"), new DoubleType(), SymbolFlags.IsArray, null, false)],
                new DoubleType(),
                TokenKind.GeometricMeanReturn, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.GrowthRate, "GrowthRate"), [new Parameter(-1, T(TokenKind.Identifier, "retentionRate"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "roe"), new DoubleType(), noFlags, null, false)], new DoubleType(),
                TokenKind.GrowthRate, ExecutionKind: NativeExecutionKind.Pure),

            // Funzioni relative alla microstruttura del mercato
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BidAskSpread, "BidAskSpread"), [new Parameter(-1, T(TokenKind.Identifier, "bid"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "ask"), new DoubleType(), noFlags, null, false)],
                new DoubleType(),
                TokenKind.BidAskSpread, ExecutionKind: NativeExecutionKind.Pure)
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
