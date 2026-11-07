' volatility.inc — namespace VOLATILITY: volatilità e rischio downside (FFI, libgmbquant).
'
' Erano tre funzioni di TRADING.METRICS, spostate nel gruppo logico della volatilità:
'
'   AnnualizedVolatility(returns(), ppy)             sqrt(ppy) * std campionaria
'   DownsideDeviation(returns(), threshold)         sqrt(media dei (r-thr)^2 per r<thr)
'   AnnualizedDownsideDeviation(returns(), thr, ppy) sqrt(ppy) * DownsideDeviation
'
' Rendimenti in frazione; input non valido -> 0.
NAMESPACE VOLATILITY

DECLARE FUNCTION AnnualizedVolatility LIB "gmbquant" ALIAS "ofx_q_annualized_volatility" (returns() AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION DownsideDeviation LIB "gmbquant" ALIAS "ofx_q_vol_downside" (returns() AS DOUBLE, threshold AS DOUBLE) AS DOUBLE
DECLARE FUNCTION AnnualizedDownsideDeviation LIB "gmbquant" ALIAS "ofx_q_ann_downside" (returns() AS DOUBLE, threshold AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
