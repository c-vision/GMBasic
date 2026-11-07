// FILE: CommonLib/Trading/Metrics.cs
// FACADE P/Invoke verso libgmbquant (gmbsuite/gmbquant + shared/libs/quant).
//
// La versione C# delle formule (Metrics.cs originale, 430 righe) e' archiviata in
// gmbasicmonorepo-deprecated/GMBasicSharp/common/CommonLib/Trading/Metrics.cs.
// L'API pubblica e' identica: i chiamanti (IlRuntime, StdLib.Runtime, Backtest) non
// cambiano. Input non valido -> 0 (semantica C/builtin), niente eccezioni.
namespace CommonLib.Trading;

public static class Metrics
{
    public static double TotalReturn(double[] returns)
        => QuantNative.TotalReturn(returns ?? [], returns?.Length ?? 0);

    public static double AnnualizedReturn(double[] returns, int periodsPerYear)
        => QuantNative.AnnualizedReturn(returns ?? [], returns?.Length ?? 0, periodsPerYear);

    /// <summary>Sharpe con risk-free e periodsPerYear: sqrt(ppy)*mean(r-rf)/std(r-rf).</summary>
    public static double SharpeRatio(double[] returns, double riskFreeRate, int periodsPerYear)
        => QuantNative.SharpeRatioRf(returns ?? [], returns?.Length ?? 0, riskFreeRate, periodsPerYear);

    public static double SortinoRatio(double[] returns, double riskFreeRate, int periodsPerYear)
        => QuantNative.SortinoRatio(returns ?? [], returns?.Length ?? 0, riskFreeRate, periodsPerYear);

    public static double CalmarRatio(double[] returns, int periodsPerYear)
        => QuantNative.CalmarRatio(returns ?? [], returns?.Length ?? 0, periodsPerYear);

    public static double InformationRatio(double[] returns, double[] benchmarkReturns, int periodsPerYear)
    {
        var n = Math.Min(returns?.Length ?? 0, benchmarkReturns?.Length ?? 0);
        return QuantNative.InformationRatio(returns ?? [], benchmarkReturns ?? [], n, periodsPerYear);
    }

    public static double TreynorRatio(double[] returns, double[] benchmarkReturns,
        double riskFreeRate, int periodsPerYear)
    {
        var n = Math.Min(returns?.Length ?? 0, benchmarkReturns?.Length ?? 0);
        return QuantNative.TreynorRatio(returns ?? [], benchmarkReturns ?? [], n, riskFreeRate, periodsPerYear);
    }

    public static double MaxDrawdown(double[] returns)
        => QuantNative.MaxDrawdown(returns ?? [], returns?.Length ?? 0);

    public static double DownsideDeviation(double[] returns, double threshold)
        => QuantNative.DownsideDeviation(returns ?? [], returns?.Length ?? 0, threshold);

    public static double ValueAtRisk(double[] returns, double confidenceLevel)
        => QuantNative.ValueAtRisk(returns ?? [], returns?.Length ?? 0, confidenceLevel);

    public static double ConditionalValueAtRisk(double[] returns, double confidenceLevel)
        => QuantNative.ConditionalValueAtRisk(returns ?? [], returns?.Length ?? 0, confidenceLevel);

    public static double Beta(double[] returns, double[] benchmarkReturns)
    {
        var n = Math.Min(returns?.Length ?? 0, benchmarkReturns?.Length ?? 0);
        return QuantNative.Beta(returns ?? [], benchmarkReturns ?? [], n);
    }

    public static double Alpha(double[] returns, double[] benchmarkReturns,
        double riskFreeRate, int periodsPerYear)
    {
        var n = Math.Min(returns?.Length ?? 0, benchmarkReturns?.Length ?? 0);
        return QuantNative.Alpha(returns ?? [], benchmarkReturns ?? [], n, riskFreeRate, periodsPerYear);
    }

    public static double ReturnsSkewness(double[] returns)
        => QuantNative.ReturnsSkewness(returns ?? [], returns?.Length ?? 0);

    public static double ReturnsKurtosis(double[] returns)
        => QuantNative.ReturnsKurtosis(returns ?? [], returns?.Length ?? 0);

    /// <summary>[statistica Jarque-Bera, p-value] — n &lt; 4 restituisce [0,1].</summary>
    public static double[] JarqueBeraTest(double[] returns)
    {
        var result = new double[2] { 0.0, 1.0 };
        QuantNative.JarqueBera(returns ?? [], returns?.Length ?? 0, result);
        return result;
    }

    public static double AnnualizedVolatility(double[] returns, int periodsPerYear)
        => QuantNative.AnnualizedVolatility(returns ?? [], returns?.Length ?? 0, periodsPerYear);

    public static double AnnualizedTrackingError(double[] returns, double[] benchmarkReturns,
        int periodsPerYear)
    {
        var n = Math.Min(returns?.Length ?? 0, benchmarkReturns?.Length ?? 0);
        return QuantNative.AnnualizedTrackingError(returns ?? [], benchmarkReturns ?? [], n, periodsPerYear);
    }

    public static double AnnualizedDownsideDeviation(double[] returns, double threshold,
        int periodsPerYear)
        => QuantNative.AnnualizedDownsideDeviation(returns ?? [], returns?.Length ?? 0, threshold, periodsPerYear);

    public static double OmegaRatio(double[] returns, double threshold)
        => QuantNative.OmegaRatio(returns ?? [], returns?.Length ?? 0, threshold);

    public static double UlcerIndex(double[] returns)
        => QuantNative.UlcerIndex(returns ?? [], returns?.Length ?? 0);
}
