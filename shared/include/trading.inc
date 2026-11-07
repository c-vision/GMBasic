' trading.inc — namespace TRADING_CORE: valutazione azionaria e rendimenti (FFI, libgmbquant).
'
' Le 12 funzioni generali del vecchio namespace TRADING, ora nella libreria C
' `gmbquant` (sorgenti: shared/libs/quant, gmbsuite/gmbquant). Stessi nomi e stessi
' argomenti del builtin:
'
'   StockValueZeroGrowth(dividend, requiredRate)        D / r
'   StockValueConstGrowth(d1, rate, growth)             D1 / (r - g)   (Gordon)
'   PreferredStockValue(dividend, discountRate)         D / r
'   CAPM(riskFree, beta, marketReturn)                  rf + beta*(rm-rf)
'   RiskPremium(assetReturn, riskFreeReturn)            r - rf
'   TotalStockReturn(p0, p1, dividendi)                 ((p1-p0)+div)/p0
'   CapitalGainsYield(p0, p1)                           (p1-p0)/p0
'   HoldingPeriodReturn(returns())                      prod(1+r)-1
'   GeometricMeanReturn(returns())                      prod(1+r)^(1/n)-1
'   GrowthRate(retentionRate, roe)                      retention*roe
'   TaxEquivalentYield(taxFreeYield, taxRate)           tfy/(1-tax)
'   BidAskSpread(bid, ask)                              ask-bid
'
' Input non valido (denominatore zero, serie vuota) -> 0 (nessuna eccezione).
NAMESPACE TRADING_CORE

DECLARE FUNCTION StockValueZeroGrowth LIB "gmbquant" ALIAS "ofx_q_stock_zero_growth" (dividend AS DOUBLE, requiredRate AS DOUBLE) AS DOUBLE
DECLARE FUNCTION StockValueConstGrowth LIB "gmbquant" ALIAS "ofx_q_stock_const_growth" (nextDividend AS DOUBLE, rate AS DOUBLE, growth AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PreferredStockValue LIB "gmbquant" ALIAS "ofx_q_preferred_value" (dividend AS DOUBLE, discountRate AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CAPM LIB "gmbquant" ALIAS "ofx_q_capm" (riskFreeRate AS DOUBLE, beta AS DOUBLE, marketReturn AS DOUBLE) AS DOUBLE
DECLARE FUNCTION RiskPremium LIB "gmbquant" ALIAS "ofx_q_risk_premium" (assetReturn AS DOUBLE, riskFreeReturn AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TotalStockReturn LIB "gmbquant" ALIAS "ofx_q_total_stock_return" (initialPrice AS DOUBLE, endPrice AS DOUBLE, dividends AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CapitalGainsYield LIB "gmbquant" ALIAS "ofx_q_capital_gains_yield" (initialPrice AS DOUBLE, endPrice AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HoldingPeriodReturn LIB "gmbquant" ALIAS "ofx_q_holding_period_return" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GeometricMeanReturn LIB "gmbquant" ALIAS "ofx_q_geometric_mean_return" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION GrowthRate LIB "gmbquant" ALIAS "ofx_q_growth_rate" (retentionRate AS DOUBLE, roe AS DOUBLE) AS DOUBLE
DECLARE FUNCTION TaxEquivalentYield LIB "gmbquant" ALIAS "ofx_q_tax_equivalent_yield" (taxFreeYield AS DOUBLE, taxRate AS DOUBLE) AS DOUBLE
DECLARE FUNCTION BidAskSpread LIB "gmbquant" ALIAS "ofx_q_bid_ask_spread" (bid AS DOUBLE, ask AS DOUBLE) AS DOUBLE
