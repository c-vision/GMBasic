// FILE: Interpreter.StdLib.TimeSeries.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: TimeSeries — namespace 'TIMESERIES'
 * 32 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (32):
 *   TSAUTOCORR
 *   TSBANDPASS
 *   TSBANDSTOP
 *   TSCOVARIANCEMATRIX
 *   TSDECOMPOSE
 *   TSDESEASONALIZE
 *   TSDIFFERENCE
 *   TSEMA
 *   TSHIGHPASS
 *   TSLOWPASS
 *   TSMA
 *   TSMINMAX
 *   TSOUTLIERS
 *   TSRETURNS
 *   TSROLLAUTOCORR
 *   TSROLLCORR
 *   TSROLLCOVARIANCE
 *   TSROLLDELTA
 *   TSROLLKURTOSIS
 *   TSROLLMAD
 *   TSROLLMEAN
 *   TSROLLMEDIAN
 *   TSROLLMINMAX
 *   TSROLLQUANTILE
 *   TSROLLREGRESSION
 *   TSROLLREGRESSIONSINGLE
 *   TSROLLSHARPE
 *   TSROLLSKEWNESS
 *   TSROLLSORTINO
 *   TSROLLSTDDEV
 *   TSROLLZSCORE
 *   TSVOLATILITY
    ==== GMB-STDLIB-AUDIT-END ==== */


using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    // ========================================================================
    // --- TIMESERIES NAMESPACE ---
    // ========================================================================

    /// <summary>
    /// Crea e registra tutte le definizioni per il namespace 'TIMESERIES'.
    /// Contiene tutte le funzioni avanzate di analisi delle serie temporali.
    /// </summary>
    private void CreateTimeSeriesNamespace()
    {
        const string namespaceName = "TIMESERIES";
        var publicFlag = SymbolFlags.IsPublic;
        var noFlags = SymbolFlags.None;
        var arrayFlag = SymbolFlags.IsArray;

        var doubleArr = new ArrayType(new DoubleType());
        var longArr = new ArrayType(new LongType());
        var doubleMatrix = new ArrayType(new ArrayType(new DoubleType()));

        // Il DataType è il tipo dell'ELEMENTO, il flag IsArray definisce che è un array.
        var seriesParam =
            new Parameter(-1, T(TokenKind.Identifier, "series"), new DoubleType(), arrayFlag, null, false);
        var series2Param =
            new Parameter(-1, T(TokenKind.Identifier, "series2"), new DoubleType(), arrayFlag, null, false);
        var periodParam = new Parameter(-1, T(TokenKind.Identifier, "period"), new LongType(), noFlags, null, false);
        var lagParam = new Parameter(-1, T(TokenKind.Identifier, "lag"), new LongType(), noFlags, null, false);
        var matrixParam = new Parameter(-1, T(TokenKind.Identifier, "data"), doubleArr, arrayFlag, null, false);
        var quantileParam = new Parameter(-1, T(TokenKind.Identifier, "quantile"), new DoubleType(), noFlags, null, false);
        var riskFreeRateParam = new Parameter(-1, T(TokenKind.Identifier, "riskFreeRate"), new DoubleType(), noFlags, null, false);
        var targetReturnParam = new Parameter(-1, T(TokenKind.Identifier, "targetReturn"), new DoubleType(), noFlags, null, false);
        var dataArrayParam = new Parameter(-1, T(TokenKind.Identifier, "data"), new DoubleType(), arrayFlag, null, false);

        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.MovingAverage, "TSMA"),
                [seriesParam, periodParam],
                doubleArr, TokenKind.MovingAverage, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.EMA, "TSEMA"), [seriesParam, periodParam],
                doubleArr,
                TokenKind.EMA, ExecutionKind: NativeExecutionKind.Pure),
            // Wrapper filtri SIGNAL (design+apply): filtro Butterworth su una serie.
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsLowPass, "TSLOWPASS"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "wn"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "order"), new LongType(), noFlags, null, false)],
                doubleArr, TokenKind.TsLowPass),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsHighPass, "TSHIGHPASS"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "wn"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "order"), new LongType(), noFlags, null, false)],
                doubleArr, TokenKind.TsHighPass),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsBandPass, "TSBANDPASS"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "wn1"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "wn2"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "order"), new LongType(), noFlags, null, false)],
                doubleArr, TokenKind.TsBandPass),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsBandStop, "TSBANDSTOP"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "wn1"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "wn2"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "order"), new LongType(), noFlags, null, false)],
                doubleArr, TokenKind.TsBandStop),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingStdDev, "TSROLLSTDDEV"),
                [seriesParam, periodParam],
                doubleArr, TokenKind.RollingStdDev, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingMad, "TSROLLMAD"),
                [seriesParam, periodParam],
                doubleArr, TokenKind.RollingMad, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingZScore, "TSROLLZSCORE"),
                [seriesParam, periodParam],
                doubleArr, TokenKind.RollingZScore, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingAutocorrelation, "TSROLLAUTOCORR"),
                [seriesParam, periodParam, lagParam], doubleArr, TokenKind.RollingAutocorrelation, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingCovariance, "TSROLLCOVARIANCE"),
                [seriesParam, series2Param, periodParam], doubleArr, TokenKind.RollingCovariance, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingCorrelation, "TSROLLCORR"),
                [seriesParam, series2Param, periodParam], doubleArr, TokenKind.RollingCorrelation, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingRegression, "TSROLLREGRESSION"),
                [seriesParam, series2Param, periodParam], doubleMatrix, TokenKind.RollingRegression),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingRegressionSingle, "TSROLLREGRESSIONSINGLE"),
                [seriesParam, periodParam], doubleMatrix, TokenKind.RollingRegressionSingle, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingDelta, "TSROLLDELTA"),
                [seriesParam, periodParam],
                doubleArr, TokenKind.RollingDelta, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Outliers, "TSOUTLIERS"),
            [
                seriesParam,
                new Parameter(-1, T(TokenKind.Identifier, "threshold"), new DoubleType(), noFlags, null, false)
            ], longArr, TokenKind.Outliers),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Difference, "TSDIFFERENCE"),
                [seriesParam, lagParam],
                doubleArr, TokenKind.Difference, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Returns, "TSRETURNS"), [seriesParam], doubleArr,
                TokenKind.Returns, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Volatility, "TSVOLATILITY"), [seriesParam],
                new DoubleType(), TokenKind.Volatility, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsHurst, "TSHURST"), [seriesParam],
                new DoubleType(), TokenKind.TsHurst, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsFracDiff, "TSFRACDIFF"),
                [seriesParam, new Parameter(-1, T(TokenKind.Identifier, "d"), new DoubleType(), noFlags, null, false)],
                doubleArr, TokenKind.TsFracDiff, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsGranger, "TSGRANGER"),
                [seriesParam, series2Param, lagParam], new AnyType(), TokenKind.TsGranger),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsGarch11, "TSGARCH11"),
                [seriesParam], new AnyType(), TokenKind.TsGarch11),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsKalman1D, "TSKALMAN1D"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "a"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "h"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "q"), new DoubleType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "r"), new DoubleType(), noFlags, null, false)],
                doubleArr, TokenKind.TsKalman1D, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsStationaryBootstrap, "TSSTATIONARYBOOTSTRAP"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "window"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "seed"), new LongType(), noFlags, null, false)],
                doubleArr, TokenKind.TsStationaryBootstrap, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsSampleEntropy, "TSSAMPEN"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "m"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "r"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.TsSampleEntropy, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsApproxEntropy, "TSAPPROXENTROPY"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "m"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "r"), new DoubleType(), noFlags, null, false)],
                new DoubleType(), TokenKind.TsApproxEntropy, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsPermEntropy, "TSENTROPYPERM"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "m"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "lag"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.TsPermEntropy, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsOuFit, "TSOUFIT"),
                [seriesParam, new Parameter(-1, T(TokenKind.Identifier, "dt"), new DoubleType(), noFlags, null, false)],
                new AnyType(), TokenKind.TsOuFit),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsLzComplexity, "TSENTROPYLZ"),
                [seriesParam], new DoubleType(), TokenKind.TsLzComplexity, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsBmaWeights, "TSBMAWEIGHTS"),
                [seriesParam], doubleArr, TokenKind.TsBmaWeights, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsHmmFit, "HMMFIT"),
                [new Parameter(-1, T(TokenKind.Identifier, "data"), new DoubleType(), arrayFlag, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nStates"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "nIter"), new LongType(), noFlags, null, false)],
                new HmmResultType(), TokenKind.TsHmmFit),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TsLyapunov, "TSLYAPUNOV"),
                [seriesParam,
                 new Parameter(-1, T(TokenKind.Identifier, "m"), new LongType(), noFlags, null, false),
                 new Parameter(-1, T(TokenKind.Identifier, "tau"), new LongType(), noFlags, null, false)],
                new DoubleType(), TokenKind.TsLyapunov, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Autocorrelation, "TSAUTOCORR"),
                [seriesParam, lagParam],
                new DoubleType(), TokenKind.Autocorrelation, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Deseasonalize, "TSDESEASONALIZE"),
                [seriesParam, periodParam], doubleArr, TokenKind.Deseasonalize, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.Decompose, "TSDECOMPOSE"),
                [seriesParam, periodParam],
                doubleMatrix, TokenKind.Decompose, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TimeSeriesCovarianceMatrix, "TSCOVARIANCEMATRIX"),
                [matrixParam], doubleMatrix, TokenKind.TimeSeriesCovarianceMatrix, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.TimeSeriesMinMax, "TSMINMAX"),
                [seriesParam], doubleArr, TokenKind.TimeSeriesMinMax, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingMean, "TSROLLMEAN"),
                [seriesParam, periodParam], doubleArr, TokenKind.RollingMean, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingSkewness, "TSROLLSKEWNESS"),
                [seriesParam, periodParam], doubleArr, TokenKind.RollingSkewness, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingKurtosis, "TSROLLKURTOSIS"),
                [seriesParam, periodParam], doubleArr, TokenKind.RollingKurtosis, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingMedian, "TSROLLMEDIAN"),
                [seriesParam, periodParam], doubleArr, TokenKind.RollingMedian, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingQuantile, "TSROLLQUANTILE"),
                [seriesParam, periodParam, quantileParam], doubleArr, TokenKind.RollingQuantile, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingSharpe, "TSROLLSHARPE"),
                [seriesParam, periodParam, riskFreeRateParam], doubleArr, TokenKind.RollingSharpe, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingSortino, "TSROLLSORTINO"),
                [seriesParam, periodParam, riskFreeRateParam, targetReturnParam], doubleArr, TokenKind.RollingSortino, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, publicFlag, T(TokenKind.RollingMinMax, "TSROLLMINMAX"),
                [seriesParam, periodParam], doubleMatrix, TokenKind.RollingMinMax, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.Arima, "Arima"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "p"), new IntegerType(), SymbolFlags.None, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "d"), new IntegerType(), SymbolFlags.None, null,
                    false),
                new Parameter(-1, T(TokenKind.Identifier, "q"), new IntegerType(), SymbolFlags.None, null,
                    false)
            ], new ArimaResultType(), TokenKind.Arima),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.HoltWinters, "HoltWinters"),
            [
                dataArrayParam,
                new Parameter(-1, T(TokenKind.Identifier, "alpha"), new DoubleType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "beta"), new DoubleType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "gamma"), new DoubleType(), SymbolFlags.None,
                    null, false),
                new Parameter(-1, T(TokenKind.Identifier, "period"), new IntegerType(), SymbolFlags.None,
                    null, false)
            ], new HoltWintersResultType(), TokenKind.HoltWinters),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.AdfTest, "AdfTest"),
                [dataArrayParam],
                new AdfTestResultType(), TokenKind.AdfTest),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.YuleWalker, "YuleWalker"),
                [dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "order"), new IntegerType(), SymbolFlags.None, null, false)],
                new ArrayType(new DoubleType()), TokenKind.YuleWalker, ExecutionKind: NativeExecutionKind.Pure),

            new NativeProcedureDefinition(-1, SymbolFlags.IsPublic, T(TokenKind.MaCoefficients, "MaCoefficients"),
                [dataArrayParam, new Parameter(-1, T(TokenKind.Identifier, "order"), new IntegerType(), SymbolFlags.None, null, false)],
                new ArrayType(new DoubleType()), TokenKind.MaCoefficients, ExecutionKind: NativeExecutionKind.Pure),
        };

        var types = new Dictionary<string, DataType>(StringComparer.OrdinalIgnoreCase)
        {
            { "ArimaResult", new ArimaResultType() },
            { "HoltWintersResult", new HoltWintersResultType() },
            { "AdfTestResult", new AdfTestResultType() },
            { "HmmResult", new HmmResultType() },
            { "JohansenResult", new JohansenResultType() }
        };
        RegisterBuiltinNamespace(namespaceName, procedures, types);
    }
}
