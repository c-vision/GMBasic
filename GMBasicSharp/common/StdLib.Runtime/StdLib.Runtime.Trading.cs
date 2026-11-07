// FILE: StdLib.Runtime.Trading.cs
// Dominio Trading della StdLib.Runtime: metriche di performance, azioni, opzioni (Black-Scholes/
// Merton/greche) e obbligazioni. PURO per costruzione (vedi Ast.NativeExecutionKind): nessun
// metodo qui accede a stato che sopravvive oltre la singola chiamata. Rispecchia esattamente le
// firme di CommonLib.Trading.{Metrics,General,Options,Bonds} (nessun riordino argomenti). Arità
// replicata dal vecchio dead file PCodeVm/PCodeProcessor.StdLib.Trading.cs (mai dispatchato).
// CONDITIONALVALUEATRISK non è qui: era già correttamente dispatchata nel main switch prima di
// questa fase (chiama CommonLib.Numerics.ConditionalVaR, non CommonLib.Trading.Metrics), fuori
// scope.

namespace Basic.StdLibRuntime;

public static class TradingFunctions
{
    // --- Metriche di performance ---
    public static double TreynorRatio(double[] returns, double[] benchmarkReturns, double riskFreeRate, int periodsPerYear) => CommonLib.Trading.Metrics.TreynorRatio(returns, benchmarkReturns, riskFreeRate, periodsPerYear);
    public static double OmegaRatio(double[] returns, double threshold) => CommonLib.Trading.Metrics.OmegaRatio(returns, threshold);
    public static double UlcerIndex(double[] returns) => CommonLib.Trading.Metrics.UlcerIndex(returns);
    public static double InformationRatio(double[] returns, double[] benchmarkReturns, int periodsPerYear) => CommonLib.Trading.Metrics.InformationRatio(returns, benchmarkReturns, periodsPerYear);
    public static double[] JarqueBeraTest(double[] returns) => CommonLib.Trading.Metrics.JarqueBeraTest(returns);
    public static double AnnualizedTrackingError(double[] returns, double[] benchmarkReturns, int periodsPerYear) => CommonLib.Trading.Metrics.AnnualizedTrackingError(returns, benchmarkReturns, periodsPerYear);
    public static double AnnualizedDownsideDeviation(double[] returns, double threshold, int periodsPerYear) => CommonLib.Trading.Metrics.AnnualizedDownsideDeviation(returns, threshold, periodsPerYear);
    public static double ReturnsSkewness(double[] returns) => CommonLib.Trading.Metrics.ReturnsSkewness(returns);
    public static double ReturnsKurtosis(double[] returns) => CommonLib.Trading.Metrics.ReturnsKurtosis(returns);

    // --- Azioni (General) ---
    public static double BidAskSpread(double bid, double ask) => CommonLib.Trading.General.BidAskSpread(bid, ask);
    public static double BookValuePerShare(double totalEquity, double commonShares) => CommonLib.Trading.General.BookValuePerShare(totalEquity, commonShares);
    public static double Eps(double netIncome, double avgShares) => CommonLib.Trading.General.Eps(netIncome, avgShares);
    public static double DilutedEps(double netIncome, double avgShares, double otherInstruments) => CommonLib.Trading.General.DilutedEps(netIncome, avgShares, otherInstruments);
    public static double Dps(double dividends, double numShares) => CommonLib.Trading.General.Dps(dividends, numShares);
    public static double EquityMultiplier(double totalAssets, double equity) => CommonLib.Trading.General.EquityMultiplier(totalAssets, equity);
    public static double Nav(double fundAssets, double fundLiabilities, double outstandingShares) => CommonLib.Trading.General.Nav(fundAssets, fundLiabilities, outstandingShares);
    public static double StockValueZeroGrowth(double dividend, double rate) => CommonLib.Trading.General.StockValueZeroGrowth(dividend, rate);
    public static double StockValueConstGrowth(double nextDividend, double rate, double growth) => CommonLib.Trading.General.StockValueConstGrowth(nextDividend, rate, growth);
    public static double PreferredStockValue(double dividend, double discountRate) => CommonLib.Trading.General.PreferredStockValue(dividend, discountRate);
    public static double Capm(double riskFreeRate, double beta, double marketReturn) => CommonLib.Trading.General.Capm(riskFreeRate, beta, marketReturn);
    public static double RiskPremium(double assetReturn, double riskFreeReturn) => CommonLib.Trading.General.RiskPremium(assetReturn, riskFreeReturn);
    public static double TotalStockReturn(double initialPrice, double endPrice, double dividends) => CommonLib.Trading.General.TotalStockReturn(initialPrice, endPrice, dividends);
    public static double CapitalGainsYield(double initialPrice, double endPrice) => CommonLib.Trading.General.CapitalGainsYield(initialPrice, endPrice);
    public static double HoldingPeriodReturn(double[] percentageReturns) => CommonLib.Trading.General.HoldingPeriodReturn(percentageReturns);
    public static double GeometricMeanReturn(double[] ratesOfReturn) => CommonLib.Trading.General.GeometricMeanReturn(ratesOfReturn);
    public static double GrowthRate(double retentionRate, double roe) => CommonLib.Trading.General.GrowthRate(retentionRate, roe);
    public static double TaxEquivalentYield(double taxFreeYield, double taxRate) => CommonLib.Trading.General.TaxEquivalentYield(taxFreeYield, taxRate);

    // --- Opzioni (Black-Scholes / Merton / greche) ---
    public static double BsCall(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.BsCall(s, k, t, r, v);
    public static double BsPut(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.BsPut(s, k, t, r, v);
    public static double MertonCall(double s, double k, double t, double r, double q, double v) => CommonLib.Trading.Options.MertonCall(s, k, t, r, q, v);
    public static double MertonPut(double s, double k, double t, double r, double q, double v) => CommonLib.Trading.Options.MertonPut(s, k, t, r, q, v);
    public static double BinomialOption(double s, double k, double t, double r, double v, int steps, bool isPut) => CommonLib.Trading.Options.BinomialOption(s, k, t, r, v, steps, isPut, false);
    public static double Delta(double s, double k, double t, double r, double v, bool isPut) => CommonLib.Trading.Options.Delta(s, k, t, r, v, isPut);
    public static double Gamma(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.Gamma(s, k, t, r, v);
    public static double Theta(double s, double k, double t, double r, double v, bool isPut) => CommonLib.Trading.Options.Theta(s, k, t, r, v, isPut);
    public static double Vega(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.Vega(s, k, t, r, v);
    public static double Rho(double s, double k, double t, double r, double v, bool isPut) => CommonLib.Trading.Options.Rho(s, k, t, r, v, isPut);
    public static double Vanna(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.Vanna(s, k, t, r, v);
    public static double Volga(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.Volga(s, k, t, r, v);
    public static double Charm(double s, double k, double t, double r, double v, bool isPut) => CommonLib.Trading.Options.Charm(s, k, t, r, v, isPut);
    public static double Speed(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.Speed(s, k, t, r, v);
    public static double Color(double s, double k, double t, double r, double v) => CommonLib.Trading.Options.Color(s, k, t, r, v);

    // --- Obbligazioni (Bonds) ---
    public static double BondPrice(DateTime settlement, DateTime maturity, double rate, double yld, double redemption, int frequency, int model = 0) => CommonLib.Trading.Bonds.BondPriceModel(settlement, maturity, rate, yld, redemption, frequency, model);
    public static double BondYield(DateTime settlement, DateTime maturity, double rate, double pr, double redemption, int frequency, int model = 0) => CommonLib.Trading.Bonds.BondYieldModel(settlement, maturity, rate, pr, redemption, frequency, model);
    public static double BondEquivalentYield(double faceValue, double bondPrice, double daysToMaturity) => CommonLib.Trading.Bonds.BondEquivalentYield(faceValue, bondPrice, daysToMaturity);
    public static double CurrentYield(double annualCoupons, double bondPrice) => CommonLib.Trading.Bonds.CurrentYield(annualCoupons, bondPrice);
    public static double YieldToMaturity(double couponPayment, double faceValue, double price, double years) => CommonLib.Trading.Bonds.YieldToMaturity(couponPayment, faceValue, price, years);
    public static double ZeroCouponBondValue(double faceValue, double rate, double time) => CommonLib.Trading.Bonds.ZeroCouponBondValue(faceValue, rate, time);
    public static double ZeroCouponBondYield(double faceValue, double price, double time) => CommonLib.Trading.Bonds.ZeroCouponBondYield(faceValue, price, time);
}
