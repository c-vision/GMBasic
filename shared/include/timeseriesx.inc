' timeseriesx.inc — namespace TIMESERIESX via FFI (libgmbstats: gmbstats-timeseries/stats).
' Primitive con dimensioni esplicite + wrapper PUBLIC che le deducono dagli array.
NAMESPACE TIMESERIESX

DECLARE SUB TXF_KPSS LIB "gmbstats" ALIAS "ofx_ts_kpss" (series() AS DOUBLE, n AS LONG, bandwidth AS LONG, deterministic AS LONG, out() AS DOUBLE)
DECLARE SUB TXF_QLR LIB "gmbstats" ALIAS "ofx_ts_qlr" (l0 AS DOUBLE, lbeta() AS DOUBLE, n AS LONG, kappa AS DOUBLE, out() AS DOUBLE)
DECLARE SUB TXF_DFGLS LIB "gmbstats" ALIAS "ofx_ts_dfgls" (series() AS DOUBLE, n AS LONG, lags AS LONG, deterministic AS LONG, out() AS DOUBLE)
DECLARE FUNCTION TXF_HMMFORWARD LIB "gmbstats" ALIAS "ofx_ts_hmm_forward" (logpi() AS DOUBLE, loga() AS DOUBLE, loge() AS DOUBLE, K AS LONG, n AS LONG) AS DOUBLE

' KPSS: out(9) = statistic, bandwidth, observations, deterministic,
'           long_run_variance, critical10, critical5, critical25, critical1.
PUBLIC SUB KPSS(series() AS DOUBLE, bandwidth AS LONG, deterministic AS LONG, out() AS DOUBLE)
    CALL TXF_KPSS(series, UBOUND(series) + 1, bandwidth, deterministic, out)
END SUB

' QLR: out(3) = qlr_t, qlr_tilde, best_index.
PUBLIC SUB QLR(l0 AS DOUBLE, lbeta() AS DOUBLE, kappa AS DOUBLE, out() AS DOUBLE)
    CALL TXF_QLR(l0, lbeta, UBOUND(lbeta) + 1, kappa, out)
END SUB

' DF-GLS: out(5) = statistic, lags, observations, deterministic, alpha_bar.
PUBLIC SUB DFGLS(series() AS DOUBLE, lags AS LONG, deterministic AS LONG, out() AS DOUBLE)
    CALL TXF_DFGLS(series, UBOUND(series) + 1, lags, deterministic, out)
END SUB

' HMM forward: log-likelihood di (logpi(K), logA(K*K), logE(n*K)).
' K = dimensione di logpi; n = dimensione di loge / K.
PUBLIC FUNCTION HMMFORWARD(logpi() AS DOUBLE, loga() AS DOUBLE, loge() AS DOUBLE) AS DOUBLE
    DIM K AS LONG = UBOUND(logpi) + 1
    DIM n AS LONG = (UBOUND(loge) + 1) / K
    RETURN TXF_HMMFORWARD(logpi, loga, loge, K, n)
END FUNCTION
