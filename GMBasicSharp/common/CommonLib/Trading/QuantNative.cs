// FILE: CommonLib/Trading/QuantNative.cs
// P/Invoke verso libgmbquant (gmbsuite/gmbquant + shared/libs/quant): la versione C#
// delle formule di trading (General.cs / Metrics.cs) e' archiviata in
// gmbasicmonorepo-deprecated/CommonLib/Trading/. Da qui in avanti C# DELEGA a C
// (direzione unica C# -> C), come per la grafica (gmb_gfx) e la compressione.
//
// Il resolver P/Invoke dell'assembly (CommonLib.GmbBoot, module initializer) risolve
// QUALSIASI nome cercando in GMBasic/build/libs/<rid>, ~/.gmbasic/libs/<rid> e cache:
// non serve un resolver per questa classe.
using System.Runtime.InteropServices;

namespace CommonLib.Trading;

internal static class QuantNative
{
    private const string Lib = "gmbquant";
    private const CallingConvention CC = CallingConvention.Cdecl;

    // ── General ─────────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gq_stock_value_zero_growth", CallingConvention = CC)]
    internal static extern double StockValueZeroGrowth(double dividend, double requiredRate);

    [DllImport(Lib, EntryPoint = "gq_stock_value_const_growth", CallingConvention = CC)]
    internal static extern double StockValueConstGrowth(double nextDividend, double rate, double growth);

    [DllImport(Lib, EntryPoint = "gq_preferred_stock_value", CallingConvention = CC)]
    internal static extern double PreferredStockValue(double dividend, double discountRate);

    [DllImport(Lib, EntryPoint = "gq_capm", CallingConvention = CC)]
    internal static extern double Capm(double riskFreeRate, double beta, double marketReturn);

    [DllImport(Lib, EntryPoint = "gq_risk_premium", CallingConvention = CC)]
    internal static extern double RiskPremium(double assetReturn, double riskFreeReturn);

    [DllImport(Lib, EntryPoint = "gq_total_stock_return", CallingConvention = CC)]
    internal static extern double TotalStockReturn(double initialPrice, double endPrice, double dividends);

    [DllImport(Lib, EntryPoint = "gq_capital_gains_yield", CallingConvention = CC)]
    internal static extern double CapitalGainsYield(double initialPrice, double endPrice);

    [DllImport(Lib, EntryPoint = "gq_holding_period_return", CallingConvention = CC)]
    internal static extern double HoldingPeriodReturn([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_geometric_mean_return", CallingConvention = CC)]
    internal static extern double GeometricMeanReturn([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_growth_rate", CallingConvention = CC)]
    internal static extern double GrowthRate(double retentionRate, double roe);

    [DllImport(Lib, EntryPoint = "gq_tax_equivalent_yield", CallingConvention = CC)]
    internal static extern double TaxEquivalentYield(double taxFreeYield, double taxRate);

    [DllImport(Lib, EntryPoint = "gq_bid_ask_spread", CallingConvention = CC)]
    internal static extern double BidAskSpread(double bid, double ask);

    // ── Metrics ──────────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gq_total_return", CallingConvention = CC)]
    internal static extern double TotalReturn([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_annualized_return", CallingConvention = CC)]
    internal static extern double AnnualizedReturn([In] double[] returns, int n, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_sharpe_ratio", CallingConvention = CC)]
    internal static extern double SharpeRatio([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_sharpe_ratio_rf", CallingConvention = CC)]
    internal static extern double SharpeRatioRf([In] double[] returns, int n, double riskFree, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_sortino_ratio", CallingConvention = CC)]
    internal static extern double SortinoRatio([In] double[] returns, int n, double riskFree, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_calmar_ratio", CallingConvention = CC)]
    internal static extern double CalmarRatio([In] double[] returns, int n, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_information_ratio", CallingConvention = CC)]
    internal static extern double InformationRatio([In] double[] returns, [In] double[] benchmark, int n, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_treynor_ratio", CallingConvention = CC)]
    internal static extern double TreynorRatio([In] double[] returns, [In] double[] benchmark, int n, double riskFree, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_max_drawdown", CallingConvention = CC)]
    internal static extern double MaxDrawdown([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_downside_deviation", CallingConvention = CC)]
    internal static extern double DownsideDeviation([In] double[] returns, int n, double threshold);

    [DllImport(Lib, EntryPoint = "gq_value_at_risk", CallingConvention = CC)]
    internal static extern double ValueAtRisk([In] double[] returns, int n, double confidence);

    [DllImport(Lib, EntryPoint = "gq_conditional_value_at_risk", CallingConvention = CC)]
    internal static extern double ConditionalValueAtRisk([In] double[] returns, int n, double confidence);

    [DllImport(Lib, EntryPoint = "gq_beta", CallingConvention = CC)]
    internal static extern double Beta([In] double[] returns, [In] double[] benchmark, int n);

    [DllImport(Lib, EntryPoint = "gq_alpha", CallingConvention = CC)]
    internal static extern double Alpha([In] double[] returns, [In] double[] benchmark, int n, double riskFree, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_returns_skewness", CallingConvention = CC)]
    internal static extern double ReturnsSkewness([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_returns_kurtosis", CallingConvention = CC)]
    internal static extern double ReturnsKurtosis([In] double[] returns, int n);

    [DllImport(Lib, EntryPoint = "gq_jarque_bera", CallingConvention = CC)]
    internal static extern int JarqueBera([In] double[] returns, int n, [Out] double[] output);

    [DllImport(Lib, EntryPoint = "gq_annualized_tracking_error", CallingConvention = CC)]
    internal static extern double AnnualizedTrackingError([In] double[] returns, [In] double[] benchmark, int n, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_omega_ratio", CallingConvention = CC)]
    internal static extern double OmegaRatio([In] double[] returns, int n, double threshold);

    [DllImport(Lib, EntryPoint = "gq_ulcer_index", CallingConvention = CC)]
    internal static extern double UlcerIndex([In] double[] returns, int n);

    // ── Volatility ───────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gq_annualized_volatility", CallingConvention = CC)]
    internal static extern double AnnualizedVolatility([In] double[] returns, int n, int periodsPerYear);

    [DllImport(Lib, EntryPoint = "gq_vol_downside_deviation", CallingConvention = CC)]
    internal static extern double DownsideDeviationVol([In] double[] returns, int n, double threshold);

    [DllImport(Lib, EntryPoint = "gq_annualized_downside_deviation", CallingConvention = CC)]
    internal static extern double AnnualizedDownsideDeviation([In] double[] returns, int n, double threshold, int periodsPerYear);

    // ── Fundamentals ─────────────────────────────────────────────────────────
    [DllImport(Lib, EntryPoint = "gq_book_value_per_share", CallingConvention = CC)]
    internal static extern double BookValuePerShare(double totalEquity, double commonShares);

    [DllImport(Lib, EntryPoint = "gq_diluted_eps", CallingConvention = CC)]
    internal static extern double DilutedEps(double netIncome, double avgShares, double otherInstruments);

    [DllImport(Lib, EntryPoint = "gq_dividend_payout_ratio", CallingConvention = CC)]
    internal static extern double DividendPayoutRatio(double dividends, double netIncome);

    [DllImport(Lib, EntryPoint = "gq_dividend_yield", CallingConvention = CC)]
    internal static extern double DividendYield(double dividends, double price);

    [DllImport(Lib, EntryPoint = "gq_dps", CallingConvention = CC)]
    internal static extern double Dps(double dividends, double numShares);

    [DllImport(Lib, EntryPoint = "gq_eps", CallingConvention = CC)]
    internal static extern double Eps(double netIncome, double avgShares);

    [DllImport(Lib, EntryPoint = "gq_equity_multiplier", CallingConvention = CC)]
    internal static extern double EquityMultiplier(double totalAssets, double equity);

    [DllImport(Lib, EntryPoint = "gq_nav", CallingConvention = CC)]
    internal static extern double Nav(double fundAssets, double fundLiabilities, double outstandingShares);

    [DllImport(Lib, EntryPoint = "gq_pb_ratio", CallingConvention = CC)]
    internal static extern double PbRatio(double price, double bookValue);

    [DllImport(Lib, EntryPoint = "gq_pe_ratio", CallingConvention = CC)]
    internal static extern double PeRatio(double price, double eps);

    [DllImport(Lib, EntryPoint = "gq_ps_ratio", CallingConvention = CC)]
    internal static extern double PsRatio(double price, double salesPerShare);
}
