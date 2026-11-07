' metrics.inc — namespace METRICS: performance e rischio (FFI, libgmbquant).
'
' Erano TRADING.METRICS (18 delle 32 funzioni). Rendimenti in FRAZIONE (0.05 = 5%).
'
'   TotalReturn(returns())                         prod(1+r)-1
'   AnnualizedReturn(returns(), ppy)               (1+TR)^(ppy/n)-1
'   SHARPERATIO(returns())                         mean/std * sqrt(252)   [canone 1-arg]
'   SortinoRatio(returns(), rf, ppy)               sqrt(ppy)*mean(r-rf)/downside(r-rf,0)
'   CalmarRatio(returns(), ppy)                    AR/|maxDD|
'   InformationRatio(returns(), bench(), ppy)      sqrt(ppy)*mean(attivo)/std(attivo)
'   TreynorRatio(returns(), bench(), rf, ppy)      (AR - AR(rf))/beta
'   MaxDrawdown(returns())                         min drawdown composto (negativo)
'   ValueAtRisk(returns(), confidence)             percentile nearest al livello 1-conf
'   ConditionalValueAtRisk(returns(), conf)        media della coda <= VaR
'   Beta(returns(), bench())                       cov/var
'   Alpha(returns(), bench(), rf, ppy)             Jensen annualizzato
'   ReturnsSkewness(returns())                     skew campionaria corretta
'   ReturnsKurtosis(returns())                     curtosi in eccesso non distorta
'   JarqueBeraTest(returns())                      ARRAY [statistica, p-value]
'   AnnualizedTrackingError(returns(), bench(), ppy)
'   OmegaRatio(returns(), threshold)               guadagni/perdite (INF se perdite 0)
'   UlcerIndex(returns())                          sqrt(media dd%^2 composti)
'
' Nota: DownsideDeviation / AnnualizedVolatility / AnnualizedDownsideDeviation sono in
' VOLATILITY (gruppo logico separato).
NAMESPACE METRICS

DECLARE FUNCTION TotalReturn LIB "gmbquant" ALIAS "ofx_q_total_return" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION AnnualizedReturn LIB "gmbquant" ALIAS "ofx_q_annualized_return" (returns() AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION SHARPERATIO LIB "gmbquant" ALIAS "ofx_q_sharpe" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SortinoRatio LIB "gmbquant" ALIAS "ofx_q_sortino" (returns() AS DOUBLE, riskFreeRate AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION CalmarRatio LIB "gmbquant" ALIAS "ofx_q_calmar" (returns() AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION InformationRatio LIB "gmbquant" ALIAS "ofx_q_information" (returns() AS DOUBLE, benchmark() AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION TreynorRatio LIB "gmbquant" ALIAS "ofx_q_treynor" (returns() AS DOUBLE, benchmark() AS DOUBLE, riskFreeRate AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION MaxDrawdown LIB "gmbquant" ALIAS "ofx_q_max_drawdown" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ValueAtRisk LIB "gmbquant" ALIAS "ofx_q_value_at_risk" (returns() AS DOUBLE, confidenceLevel AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ConditionalValueAtRisk LIB "gmbquant" ALIAS "ofx_q_cvar" (returns() AS DOUBLE, confidenceLevel AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Beta LIB "gmbquant" ALIAS "ofx_q_beta" (returns() AS DOUBLE, benchmark() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Alpha LIB "gmbquant" ALIAS "ofx_q_alpha" (returns() AS DOUBLE, benchmark() AS DOUBLE, riskFreeRate AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION ReturnsSkewness LIB "gmbquant" ALIAS "ofx_q_skewness" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ReturnsKurtosis LIB "gmbquant" ALIAS "ofx_q_kurtosis" (returns() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION AnnualizedTrackingError LIB "gmbquant" ALIAS "ofx_q_tracking_error" (returns() AS DOUBLE, benchmark() AS DOUBLE, periodsPerYear AS LONG) AS DOUBLE
DECLARE FUNCTION OmegaRatio LIB "gmbquant" ALIAS "ofx_q_omega" (returns() AS DOUBLE, threshold AS DOUBLE) AS DOUBLE
DECLARE FUNCTION UlcerIndex LIB "gmbquant" ALIAS "ofx_q_ulcer" (returns() AS DOUBLE) AS DOUBLE

' Jarque-Bera restituisce un ARRAY di 2 elementi: la primitiva FFI scrive in un buffer,
' il wrapper BASIC ricostruisce l'array (stessa forma del builtin).
DECLARE FUNCTION JBSTAT LIB "gmbquant" ALIAS "ofx_q_jarque_bera" (returns() AS DOUBLE, out_() AS DOUBLE) AS LONG

PUBLIC FUNCTION JarqueBeraTest(returns() AS DOUBLE) AS ARRAY OF DOUBLE
  DIM o(1) AS DOUBLE
  o(0) = 0 : o(1) = 1
  JBSTAT(returns, o)
  RETURN o
END FUNCTION
