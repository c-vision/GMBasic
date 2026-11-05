' timeseriesx.inc — namespace TIMESERIESX via FFI (libgmbstats: gmbstats-timeseries/stats).
' Primitive con dimensioni esplicite + wrapper PUBLIC che le deducono dagli array.
' Le uscite composte sono in out() piatto, documentato per funzione.
NAMESPACE TIMESERIESX

DECLARE SUB TXF_KPSS LIB "gmbstats" ALIAS "ofx_ts_kpss" (series() AS DOUBLE, n AS LONG, bandwidth AS LONG, deterministic AS LONG, out() AS DOUBLE)
DECLARE SUB TXF_QLR LIB "gmbstats" ALIAS "ofx_ts_qlr" (l0 AS DOUBLE, lbeta() AS DOUBLE, n AS LONG, kappa AS DOUBLE, out() AS DOUBLE)
DECLARE SUB TXF_DFGLS LIB "gmbstats" ALIAS "ofx_ts_dfgls" (series() AS DOUBLE, n AS LONG, lags AS LONG, deterministic AS LONG, out() AS DOUBLE)
DECLARE FUNCTION TXF_HMMFORWARD LIB "gmbstats" ALIAS "ofx_ts_hmm_forward" (logpi() AS DOUBLE, loga() AS DOUBLE, loge() AS DOUBLE, K AS LONG, n AS LONG) AS DOUBLE
DECLARE FUNCTION TXF_HMMVITERBI LIB "gmbstats" ALIAS "ofx_ts_hmm_viterbi" (logpi() AS DOUBLE, loga() AS DOUBLE, loge() AS DOUBLE, K AS LONG, n AS LONG, path() AS LONG) AS DOUBLE
DECLARE SUB TXF_ENGLEGRANGER LIB "gmbstats" ALIAS "ofx_ts_engle_granger" (Y() AS DOUBLE, n AS LONG, k AS LONG, dependent AS LONG, lags AS LONG, withConstant AS LONG, out() AS DOUBLE)
DECLARE SUB TXF_JOHANSEN LIB "gmbstats" ALIAS "ofx_ts_johansen" (Y() AS DOUBLE, nvars AS LONG, periods AS LONG, lags AS LONG, out() AS DOUBLE)

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

' HMM Viterbi: path(0..n-1) = stato piu' probabile; ritorna il log-lik del path.
PUBLIC FUNCTION HMMVITERBI(logpi() AS DOUBLE, loga() AS DOUBLE, loge() AS DOUBLE, path() AS LONG) AS DOUBLE
    DIM K AS LONG = UBOUND(logpi) + 1
    DIM n AS LONG = (UBOUND(loge) + 1) / K
    RETURN TXF_HMMVITERBI(logpi, loga, loge, K, n, path)
END FUNCTION

' Engle-Granger: Y(righe=osservazioni, colonne=variabili).
' out(5) = statistic, lags, observations, n_regressors, with_constant.
PUBLIC SUB ENGLEGRANGER(Y() AS ARRAY OF DOUBLE, dependent AS LONG, lags AS LONG, withConstant AS LONG, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(Y) + 1
    DIM k AS LONG = UBOUND(Y(0)) + 1
    DIM flat((n * k) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        FOR j = 0 TO k - 1
            flat(i * k + j) = Y(i)(j)
        NEXT j
    NEXT i
    CALL TXF_ENGLEGRANGER(flat, n, k, dependent, lags, withConstant, out)
END SUB

' Johansen: Y(righe=osservazioni, colonne=variabili).
' out(3 + 3*nvars) = nvars, lags, observations, eigenvalues[nvars],
'                    trace[nvars], max[nvars].
PUBLIC SUB JOHANSEN(Y() AS ARRAY OF DOUBLE, lags AS LONG, out() AS DOUBLE)
    DIM periods AS LONG = UBOUND(Y) + 1
    DIM nvars AS LONG = UBOUND(Y(0)) + 1
    DIM flat((periods * nvars) - 1) AS DOUBLE
    FOR i = 0 TO periods - 1
        FOR j = 0 TO nvars - 1
            flat(i * nvars + j) = Y(i)(j)
        NEXT j
    NEXT i
    CALL TXF_JOHANSEN(flat, nvars, periods, lags, out)
END SUB
