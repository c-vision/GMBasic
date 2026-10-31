// FILE: Interpreter.StdLib.Trading.Metrics.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Trading.Metrics — namespace 'TRADING.METRICS'
 * 32 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (32):
 *   Alpha
 *   AnnualizedDownsideDeviation
 *   AnnualizedReturn
 *   AnnualizedTrackingError
 *   AnnualizedVolatility
 *   Beta
 *   BookValuePerShare
 *   CalmarRatio
 *   ConditionalValueAtRisk
 *   DPS
 *   DilutedEPS
 *   DividendPayoutRatio
 *   DividendYield
 *   DownsideDeviation
 *   EPS
 *   EquityMultiplier
 *   InformationRatio
 *   JarqueBeraTest
 *   MaxDrawdown
 *   NAV
 *   OmegaRatio
 *   PBRatio
 *   PERatio
 *   PSRatio
 *   ReturnsKurtosis
 *   ReturnsSkewness
 *   SHARPERATIO
 *   SortinoRatio
 *   TotalReturn
 *   TreynorRatio
 *   UlcerIndex
 *   ValueAtRisk
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TRADING.METRICS NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'TRADING.METRICS'.
    /// Contiene funzioni specifiche per l'analisi e la valutazione generale di azioni e altri strumenti.
    /// QUESTA VERSIONE È STATA ESTESA PER INCLUDERE TUTTE LE METRICHE DI PERFORMANCE.
    /// </summary>
    private void CreateTradingMetricsNamespace()
    {
        const string namespaceName = "TRADING.METRICS";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayFlag = SymbolFlags.IsArray;

        var doubleArrayType = new ArrayType(new DoubleType());

        var procedures = new ProcedureDefinition[]
        {
            // --- Funzioni di Metriche Finanziarie (esistenti) ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.BookValuePerShare, "BookValuePerShare"), [new Parameter(-1, T(TokenKind.Identifier, "totalEquity"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "commonShares"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.BookValuePerShare, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EPS, "EPS"), [new Parameter(-1, T(TokenKind.Identifier, "netIncome"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "avgShares"), new DoubleType(), noFlags, null, false)], new DoubleType(), TokenKind.EPS, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DilutedEPS, "DilutedEPS"), [new Parameter(-1, T(TokenKind.Identifier, "netIncome"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "avgShares"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "instruments"), new DoubleType(), noFlags, null, false)], new DoubleType(),
                TokenKind.DilutedEPS, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DPS, "DPS"), [new Parameter(-1, T(TokenKind.Identifier, "dividends"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "shares"), new DoubleType(), noFlags, null, false)], new DoubleType(), TokenKind.DPS, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EquityMultiplier, "EquityMultiplier"), [new Parameter(-1, T(TokenKind.Identifier, "assets"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "equity"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.EquityMultiplier, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.NAV, "NAV"), [new Parameter(-1, T(TokenKind.Identifier, "assets"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "liabilities"), new DoubleType(), noFlags, null, false), new Parameter(-1, T(TokenKind.Identifier, "shares"), new DoubleType(), noFlags, null, false)], new DoubleType(), TokenKind.NAV, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PERatio, "PERatio"),
                [new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "eps"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.PERatio),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PSRatio, "PSRatio"),
                [new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "salesPerShare"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.PSRatio),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.PBRatio, "PBRatio"),
                [new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "bookValue"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.PBRatio),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DividendPayoutRatio, "DividendPayoutRatio"),
                [new Parameter(-1, T(TokenKind.Identifier, "dividends"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "netIncome"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.DividendPayoutRatio),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DividendYield, "DividendYield"),
                [new Parameter(-1, T(TokenKind.Identifier, "dividend"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "price"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.DividendYield),

            // --- NUOVE Funzioni di Metriche di Performance ---
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TotalReturn, "TotalReturn"),
                [
                    // CORREZIONE: Usa doubleArrayType invece di DoubleType()
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false)
                ],
                new DoubleType(), TokenKind.TotalReturn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AnnualizedReturn, "AnnualizedReturn"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.AnnualizedReturn),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SortinoRatio, "SortinoRatio"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tasso_riskfree"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.SortinoRatio),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MaxDrawdown, "MaxDrawdown"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false)
                ],
                new DoubleType(), TokenKind.MaxDrawdown),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.CalmarRatio, "CalmarRatio"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.CalmarRatio),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.InformationRatio, "InformationRatio"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_asset"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_benchmark"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.InformationRatio, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Alpha, "Alpha"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_asset"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_benchmark"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tasso_riskfree"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.Alpha),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Beta, "Beta"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_asset"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_benchmark"), doubleArrayType,
                    arrayFlag, null, false)
            ], new DoubleType(), TokenKind.Beta),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TreynorRatio, "TreynorRatio"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_asset"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti_benchmark"), doubleArrayType,
                    arrayFlag, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "tasso_riskfree"), new DoubleType(), noFlags, null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.TreynorRatio, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ValueAtRisk, "ValueAtRisk"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "livello_confidenza"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.ValueAtRisk),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ConditionalValueAtRisk, "ConditionalValueAtRisk"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "livello_confidenza"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.ConditionalValueAtRisk),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.DownsideDeviation, "DownsideDeviation"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "soglia"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.DownsideDeviation),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ReturnsSkewness, "ReturnsSkewness"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false)
                ],
                new DoubleType(), TokenKind.ReturnsSkewness, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.ReturnsKurtosis, "ReturnsKurtosis"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false)
                ],
                new DoubleType(), TokenKind.ReturnsKurtosis, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.JarqueBeraTest, "JarqueBeraTest"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false)
                ],
                new ArrayType(new DoubleType()), TokenKind.JarqueBeraTest, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.AnnualizedVolatility, "AnnualizedVolatility"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.AnnualizedVolatility),
            new NativeProcedureDefinition(-1, publicFlag,
                T(TokenKind.AnnualizedTrackingError, "AnnualizedTrackingError"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "benchmark"), doubleArrayType, arrayFlag,
                        null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
                ], new DoubleType(), TokenKind.AnnualizedTrackingError, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag,
                T(TokenKind.AnnualizedDownsideDeviation, "AnnualizedDownsideDeviation"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "soglia"), new DoubleType(), noFlags, null, false),
                    new Parameter(-1, T(TokenKind.Identifier, "periodi_anno"), new LongType(), noFlags, null, false)
                ], new DoubleType(), TokenKind.AnnualizedDownsideDeviation, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.OmegaRatio, "OmegaRatio"),
            [
                new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "soglia"), new DoubleType(), noFlags, null, false)
            ], new DoubleType(), TokenKind.OmegaRatio, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.UlcerIndex, "UlcerIndex"),
                [
                    new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag,
                        null, false)
                ],
                new DoubleType(), TokenKind.UlcerIndex, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.SharpeRatio, "SHARPERATIO"),
            [new Parameter(-1, T(TokenKind.Identifier, "rendimenti"), doubleArrayType, arrayFlag, null, false)
            ], new DoubleType(), TokenKind.SharpeRatio),
        };

        RegisterBuiltinNamespace(namespaceName, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
