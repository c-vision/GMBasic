// FILE: Interpreter.StdLib.Trading.Backtests.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Trading.Backtests — namespace 'TRADING.BACKTESTS'
 * 1 funzioni | 0 comandi | 2 tipi | 0 costanti
 *
 * FUNZIONI (1):
 *   RUN
 * TIPI (2):
 *   BacktestResultType
 *   RebalanceStrategyType
    ==== GMB-STDLIB-AUDIT-END ==== */


namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRADING.BACKTESTS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea il modulo virtuale per il namespace 'TRADING.BACKTESTS'.
    /// CORRETTO: Aggiunge la registrazione dei tipi di dato `RebalanceStrategy` e `BacktestResult`.
    /// </summary>
    private void CreateTradingBacktestsNamespace()
    {
        const string namespaceName = "TRADING.BACKTESTS";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayFlag = SymbolFlags.IsArray;
        var optionalArrayFlag = SymbolFlags.IsOptional | SymbolFlags.IsArray;

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Run, "RUN"),
            [
                // Il parametro è un array (arrayFlag) il cui tipo di elemento 
                // è a sua volta un array di double (ArrayType(DoubleType)).
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_storici"), new ArrayType(new ArrayType(new DoubleType())),
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "strategia"), new RebalanceStrategyType(), noFlags, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "portfolio_ottimo"), new DoubleType(), optionalArrayFlag,
                    null, false)
            ], new BacktestResultType(), TokenKind.Run)
        };

        // --- Registrazione dei tipi di dato del namespace ---
        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "RebalanceStrategy", new RebalanceStrategyType() },
            { "BacktestResult", new BacktestResultType() }
        };

        RegisterBuiltinNamespace(namespaceName, procedures, types);

        // --- Registrazione degli schemi per i tipi (per l'accesso ai campi) ---
        RegisterTypeSchema(typeof(RebalanceStrategyType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "Frequency", new LongType() },
                { "TransactionCost", new DoubleType() },
                { "Method", new StringType() }
            });

        RegisterTypeSchema(typeof(BacktestResultType),
            new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
            {
                { "TotalReturn", new DoubleType() },
                { "AnnualizedReturn", new DoubleType() },
                { "MaxDrawdown", new DoubleType() },
                { "SharpeRatio", new DoubleType() },
                { "TradesCount", new LongType() },
                { "Turnover", new DoubleType() },
                { "EquityCurve", new ArrayType(new DoubleType()) }
            });
    }
}