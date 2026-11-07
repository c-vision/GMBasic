// FILE: CommonLib/Trading/General.cs
// FACADE P/Invoke verso libgmbquant (gmbsuite/gmbquant + shared/libs/quant).
//
// La versione C# delle formule (General.cs originale, 415 righe) e' archiviata in
// gmbasicmonorepo-deprecated/GMBasicSharp/common/CommonLib/Trading/General.cs.
// L'API pubblica e' identica: i chiamanti (IlRuntime, StdLib.Runtime, Backtest) non
// cambiano. La semantica e' quella C: input non valido -> 0 (nessuna eccezione), come
// il builtin.
namespace CommonLib.Trading;

public static class General
{
    public static double BidAskSpread(double bid, double ask)
        => QuantNative.BidAskSpread(bid, ask);

    public static double BookValuePerShare(double totalEquity, double commonShares)
        => QuantNative.BookValuePerShare(totalEquity, commonShares);

    public static double Capm(double riskFreeRate, double beta, double marketReturn)
        => QuantNative.Capm(riskFreeRate, beta, marketReturn);

    public static double CapitalGainsYield(double initialPrice, double endPrice)
        => QuantNative.CapitalGainsYield(initialPrice, endPrice);

    public static double DilutedEps(double netIncome, double avgShares, double otherInstruments)
        => QuantNative.DilutedEps(netIncome, avgShares, otherInstruments);

    public static double DividendPayoutRatio(double dividends, double netIncome)
        => QuantNative.DividendPayoutRatio(dividends, netIncome);

    public static double DividendYield(double dividends, double price)
        => QuantNative.DividendYield(dividends, price);

    public static double Dps(double dividends, double numShares)
        => QuantNative.Dps(dividends, numShares);

    public static double Eps(double netIncome, double avgShares)
        => QuantNative.Eps(netIncome, avgShares);

    public static double EquityMultiplier(double totalAssets, double equity)
        => QuantNative.EquityMultiplier(totalAssets, equity);

    public static double GeometricMeanReturn(double[] ratesOfReturn)
        => QuantNative.GeometricMeanReturn(ratesOfReturn ?? [], ratesOfReturn?.Length ?? 0);

    public static double HoldingPeriodReturn(double[] percentageReturns)
        => QuantNative.HoldingPeriodReturn(percentageReturns ?? [], percentageReturns?.Length ?? 0);

    public static double Nav(double fundAssets, double fundLiabilities, double outstandingShares)
        => QuantNative.Nav(fundAssets, fundLiabilities, outstandingShares);

    public static double PreferredStockValue(double dividend, double discountRate)
        => QuantNative.PreferredStockValue(dividend, discountRate);

    public static double PeRatio(double price, double eps)
        => QuantNative.PeRatio(price, eps);

    public static double PsRatio(double price, double salesPerShare)
        => QuantNative.PsRatio(price, salesPerShare);

    public static double PbRatio(double price, double bookValue)
        => QuantNative.PbRatio(price, bookValue);

    public static double RiskPremium(double assetReturn, double riskFreeReturn)
        => QuantNative.RiskPremium(assetReturn, riskFreeReturn);

    public static double StockValueConstGrowth(double nextDividend, double rate, double growth)
        => QuantNative.StockValueConstGrowth(nextDividend, rate, growth);

    public static double GrowthRate(double retentionRate, double roe)
        => QuantNative.GrowthRate(retentionRate, roe);

    public static double StockValueZeroGrowth(double dividend, double rate)
        => QuantNative.StockValueZeroGrowth(dividend, rate);

    public static double TaxEquivalentYield(double taxFreeYield, double taxRate)
        => QuantNative.TaxEquivalentYield(taxFreeYield, taxRate);

    public static double TotalStockReturn(double initialPrice, double endPrice, double dividends)
        => QuantNative.TotalStockReturn(initialPrice, endPrice, dividends);
}
