' timeseriesx.inc — namespace TIMESERIESX via FFI (libgmbstats: gmbstats-timeseries/stats).
' Convenzione: array = puntatore + dimensione UNA volta; uscite in out() piatto documentato.
NAMESPACE TIMESERIESX

' KPSS: out(9) = statistic, bandwidth, observations, deterministic,
'           long_run_variance, critical10, critical5, critical25, critical1.
PUBLIC DECLARE SUB KPSS LIB "gmbstats" ALIAS "ofx_ts_kpss" (series() AS DOUBLE, n AS LONG, bandwidth AS LONG, deterministic AS LONG, out() AS DOUBLE)

' QLR: out(3) = qlr_t, qlr_tilde, best_index.
PUBLIC DECLARE SUB QLR LIB "gmbstats" ALIAS "ofx_ts_qlr" (l0 AS DOUBLE, lbeta() AS DOUBLE, n AS LONG, kappa AS DOUBLE, out() AS DOUBLE)

' DF-GLS: out(5) = statistic, lags, observations, deterministic, alpha_bar.
PUBLIC DECLARE SUB DFGLS LIB "gmbstats" ALIAS "ofx_ts_dfgls" (series() AS DOUBLE, n AS LONG, lags AS LONG, deterministic AS LONG, out() AS DOUBLE)

' HMM forward: log-likelihood di (logpi(K), logA(K*K), logE(n*K)); K e n espliciti.
PUBLIC DECLARE FUNCTION HMMFORWARD LIB "gmbstats" ALIAS "ofx_ts_hmm_forward" (logpi() AS DOUBLE, loga() AS DOUBLE, loge() AS DOUBLE, K AS LONG, n AS LONG) AS DOUBLE
