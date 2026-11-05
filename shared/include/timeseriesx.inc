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

' ═══════════════════════════════════════════════════════════════════════════
' Famiglia rolling/trasformazioni. Le funzioni che restituiscono serie hanno
' la STESSA forma del builtin (array di n-w+1 valori per le finestre mobili).
' ═══════════════════════════════════════════════════════════════════════════
DECLARE SUB RXF_MA LIB "gmbstats" ALIAS "ofx_ts_ma" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_EMA LIB "gmbstats" ALIAS "ofx_ts_ema" (x() AS DOUBLE, n AS LONG, period AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_MINMAX LIB "gmbstats" ALIAS "ofx_ts_minmax" (x() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE FUNCTION RXF_AUTOCORR LIB "gmbstats" ALIAS "ofx_ts_autocorr" (x() AS DOUBLE, n AS LONG, lag AS LONG) AS DOUBLE
DECLARE FUNCTION RXF_VOLATILITY LIB "gmbstats" ALIAS "ofx_ts_volatility" (x() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION RXF_DIFFERENCE LIB "gmbstats" ALIAS "ofx_ts_difference" (x() AS DOUBLE, n AS LONG, order AS LONG, out() AS DOUBLE) AS LONG
DECLARE SUB RXF_RETURNS LIB "gmbstats" ALIAS "ofx_ts_returns" (x() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_DESEASONALIZE LIB "gmbstats" ALIAS "ofx_ts_deseasonalize" (x() AS DOUBLE, n AS LONG, period AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_DECOMPOSE LIB "gmbstats" ALIAS "ofx_ts_decompose" (x() AS DOUBLE, n AS LONG, period AS LONG, trend() AS DOUBLE, seasonal() AS DOUBLE, resid() AS DOUBLE)
DECLARE SUB RXF_COVMATRIX LIB "gmbstats" ALIAS "ofx_ts_covmatrix" (data() AS DOUBLE, nobs AS LONG, m AS LONG, out() AS DOUBLE)
DECLARE FUNCTION RXF_OUTLIERS LIB "gmbstats" ALIAS "ofx_ts_outliers" (x() AS DOUBLE, n AS LONG, threshold AS DOUBLE, idx() AS LONG) AS LONG

DECLARE SUB RXF_ROLL_MEAN LIB "gmbstats" ALIAS "ofx_roll_mean" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_STD LIB "gmbstats" ALIAS "ofx_roll_std" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_SKEW LIB "gmbstats" ALIAS "ofx_roll_skew" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_KURT LIB "gmbstats" ALIAS "ofx_roll_kurt" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_MEDIAN LIB "gmbstats" ALIAS "ofx_roll_median" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_QUANTILE LIB "gmbstats" ALIAS "ofx_roll_quantile" (x() AS DOUBLE, n AS LONG, w AS LONG, q AS DOUBLE, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_SHARPE LIB "gmbstats" ALIAS "ofx_roll_sharpe" (x() AS DOUBLE, n AS LONG, w AS LONG, rf AS DOUBLE, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_MAD LIB "gmbstats" ALIAS "ofx_roll_mad" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_ZSCORE LIB "gmbstats" ALIAS "ofx_roll_zscore" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_AUTOCORR LIB "gmbstats" ALIAS "ofx_roll_autocorr" (x() AS DOUBLE, n AS LONG, w AS LONG, lag AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_COV LIB "gmbstats" ALIAS "ofx_roll_cov" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_CORR LIB "gmbstats" ALIAS "ofx_roll_corr" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_REGRESSION LIB "gmbstats" ALIAS "ofx_roll_regression" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, w AS LONG, slope() AS DOUBLE, intercept() AS DOUBLE)
DECLARE SUB RXF_ROLL_REGRESSION_SINGLE LIB "gmbstats" ALIAS "ofx_roll_regression_single" (y() AS DOUBLE, n AS LONG, w AS LONG, slope() AS DOUBLE, intercept() AS DOUBLE)
DECLARE SUB RXF_ROLL_DELTA LIB "gmbstats" ALIAS "ofx_roll_delta" (x() AS DOUBLE, n AS LONG, w AS LONG, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_SORTINO LIB "gmbstats" ALIAS "ofx_roll_sortino" (x() AS DOUBLE, n AS LONG, w AS LONG, rf AS DOUBLE, target AS DOUBLE, out() AS DOUBLE)
DECLARE SUB RXF_ROLL_MINMAX LIB "gmbstats" ALIAS "ofx_roll_minmax" (x() AS DOUBLE, n AS LONG, w AS LONG, mn() AS DOUBLE, mx() AS DOUBLE)

DECLARE FUNCTION RXF_LOWPASS LIB "gmbstats" ALIAS "ofx_ts_lowpass" (x() AS DOUBLE, n AS LONG, wn AS DOUBLE, order AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION RXF_HIGHPASS LIB "gmbstats" ALIAS "ofx_ts_highpass" (x() AS DOUBLE, n AS LONG, wn AS DOUBLE, order AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION RXF_BANDPASS LIB "gmbstats" ALIAS "ofx_ts_bandpass" (x() AS DOUBLE, n AS LONG, w1 AS DOUBLE, w2 AS DOUBLE, order AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION RXF_BANDSTOP LIB "gmbstats" ALIAS "ofx_ts_bandstop" (x() AS DOUBLE, n AS LONG, w1 AS DOUBLE, w2 AS DOUBLE, order AS LONG, out() AS DOUBLE) AS LONG

' ── trasformazioni ─────────────────────────────────────────────────────────
PUBLIC FUNCTION TSEMA(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL RXF_EMA(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSMINMAX(series() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(1) AS DOUBLE
    CALL RXF_MINMAX(series, UBOUND(series) + 1, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSAUTOCORR(series() AS DOUBLE, lag AS LONG) AS DOUBLE
    RETURN RXF_AUTOCORR(series, UBOUND(series) + 1, lag)
END FUNCTION

PUBLIC FUNCTION TSVOLATILITY(series() AS DOUBLE) AS DOUBLE
    RETURN RXF_VOLATILITY(series, UBOUND(series) + 1)
END FUNCTION

PUBLIC FUNCTION TSDIFFERENCE(series() AS DOUBLE, order AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - order - 1) AS DOUBLE
    CALL RXF_DIFFERENCE(series, n, order, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSRETURNS(series() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 2) AS DOUBLE
    CALL RXF_RETURNS(series, n, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSDESEASONALIZE(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL RXF_DESEASONALIZE(series, n, period, o)
    RETURN o
END FUNCTION

' TSDECOMPOSE: array piatto di 3*n valori, righe contigue [trend | stagionalita' | residuo].
PUBLIC FUNCTION TSDECOMPOSE(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM tr(n - 1) AS DOUBLE
    DIM se(n - 1) AS DOUBLE
    DIM re(n - 1) AS DOUBLE
    CALL RXF_DECOMPOSE(series, n, period, tr, se, re)
    DIM o((3 * n) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        o(i) = tr(i)
        o(n + i) = se(i)
        o(2 * n + i) = re(i)
    NEXT i
    RETURN o
END FUNCTION

' Matrice di covarianza: input jagged (osservazioni x variabili), output m*m row-major.
PUBLIC FUNCTION TSCOVARIANCEMATRIX(data() AS ARRAY OF DOUBLE) AS ARRAY OF DOUBLE
    DIM nobs AS LONG = UBOUND(data) + 1
    DIM m AS LONG = UBOUND(data(0)) + 1
    DIM flat((nobs * m) - 1) AS DOUBLE
    FOR i = 0 TO nobs - 1
        FOR j = 0 TO m - 1
            flat(i * m + j) = data(i)(j)
        NEXT j
    NEXT i
    DIM o((m * m) - 1) AS DOUBLE
    CALL RXF_COVMATRIX(flat, nobs, m, o)
    RETURN o
END FUNCTION

' Outlier Z-score: scrive gli indici in out() e ritorna quanti ne ha trovati.
PUBLIC FUNCTION TSOUTLIERS(series() AS DOUBLE, threshold AS DOUBLE, out() AS LONG) AS LONG
    DIM c AS LONG
    c = RXF_OUTLIERS(series, UBOUND(series) + 1, threshold, out)
    RETURN c
END FUNCTION

' ── rolling ────────────────────────────────────────────────────────────────
PUBLIC FUNCTION TSMA(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_MA(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLMEAN(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_MEAN(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLSTDDEV(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_STD(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLSKEWNESS(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_SKEW(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLKURTOSIS(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_KURT(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLMEDIAN(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_MEDIAN(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLQUANTILE(series() AS DOUBLE, period AS LONG, quantile AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_QUANTILE(series, n, period, quantile, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLSHARPE(series() AS DOUBLE, period AS LONG, riskFreeRate AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_SHARPE(series, n, period, riskFreeRate, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLMAD(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_MAD(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLZSCORE(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_ZSCORE(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLAUTOCORR(series() AS DOUBLE, period AS LONG, lag AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_AUTOCORR(series, n, period, lag, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLCOVARIANCE(series() AS DOUBLE, series2() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_COV(series, series2, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLCORR(series() AS DOUBLE, series2() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_CORR(series, series2, n, period, o)
    RETURN o
END FUNCTION

' TSROLLREGRESSION: array piatto 2*cnt interleaved {slope0, intercept0, slope1, ...}.
PUBLIC FUNCTION TSROLLREGRESSION(series() AS DOUBLE, series2() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM cnt AS LONG = n - period + 1
    DIM sl(cnt - 1) AS DOUBLE
    DIM ic(cnt - 1) AS DOUBLE
    CALL RXF_ROLL_REGRESSION(series, series2, n, period, sl, ic)
    DIM o((2 * cnt) - 1) AS DOUBLE
    FOR i = 0 TO cnt - 1
        o(2 * i) = sl(i)
        o(2 * i + 1) = ic(i)
    NEXT i
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLREGRESSIONSINGLE(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM cnt AS LONG = n - period + 1
    DIM sl(cnt - 1) AS DOUBLE
    DIM ic(cnt - 1) AS DOUBLE
    CALL RXF_ROLL_REGRESSION_SINGLE(series, n, period, sl, ic)
    DIM o((2 * cnt) - 1) AS DOUBLE
    FOR i = 0 TO cnt - 1
        o(2 * i) = sl(i)
        o(2 * i + 1) = ic(i)
    NEXT i
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLDELTA(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_DELTA(series, n, period, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSROLLSORTINO(series() AS DOUBLE, period AS LONG, riskFreeRate AS DOUBLE, targetReturn AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - period) AS DOUBLE
    CALL RXF_ROLL_SORTINO(series, n, period, riskFreeRate, targetReturn, o)
    RETURN o
END FUNCTION

' TSROLLMINMAX: array piatto 2*cnt interleaved {min0, max0, min1, ...}.
PUBLIC FUNCTION TSROLLMINMAX(series() AS DOUBLE, period AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM cnt AS LONG = n - period + 1
    DIM mn(cnt - 1) AS DOUBLE
    DIM mx(cnt - 1) AS DOUBLE
    CALL RXF_ROLL_MINMAX(series, n, period, mn, mx)
    DIM o((2 * cnt) - 1) AS DOUBLE
    FOR i = 0 TO cnt - 1
        o(2 * i) = mn(i)
        o(2 * i + 1) = mx(i)
    NEXT i
    RETURN o
END FUNCTION

' ── filtri Butterworth ─────────────────────────────────────────────────────
PUBLIC FUNCTION TSLOWPASS(series() AS DOUBLE, wn AS DOUBLE, order AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL RXF_LOWPASS(series, n, wn, order, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSHIGHPASS(series() AS DOUBLE, wn AS DOUBLE, order AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL RXF_HIGHPASS(series, n, wn, order, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSBANDPASS(series() AS DOUBLE, wn1 AS DOUBLE, wn2 AS DOUBLE, order AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL RXF_BANDPASS(series, n, wn1, wn2, order, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSBANDSTOP(series() AS DOUBLE, wn1 AS DOUBLE, wn2 AS DOUBLE, order AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL RXF_BANDSTOP(series, n, wn1, wn2, order, o)
    RETURN o
END FUNCTION

' ═══════════════════════════════════════════════════════════════════════════
' Famiglia avanzata: Hurst, frazionale, Granger, GARCH, Kalman, bootstrap,
' entropie, Lyapunov, OU, BMA. I risultati composti sono array piatti.
' ═══════════════════════════════════════════════════════════════════════════
DECLARE FUNCTION AXF_HURST LIB "gmbstats" ALIAS "ofx_ts_hurst" (x() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION AXF_FRACDIFF LIB "gmbstats" ALIAS "ofx_ts_fracdiff" (x() AS DOUBLE, n AS LONG, d AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AXF_GRANGER LIB "gmbstats" ALIAS "ofx_ts_granger" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, p AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AXF_GARCH11 LIB "gmbstats" ALIAS "ofx_ts_garch11" (x() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AXF_KALMAN1D LIB "gmbstats" ALIAS "ofx_ts_kalman1d" (z() AS DOUBLE, n AS LONG, a AS DOUBLE, h AS DOUBLE, q AS DOUBLE, r AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AXF_BOOTSTRAP LIB "gmbstats" ALIAS "ofx_ts_bootstrap" (x() AS DOUBLE, n AS LONG, window AS LONG, seed AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AXF_SAMPEN LIB "gmbstats" ALIAS "ofx_ts_sampen" (x() AS DOUBLE, n AS LONG, m AS LONG, r AS DOUBLE) AS DOUBLE
DECLARE FUNCTION AXF_APPROXENT LIB "gmbstats" ALIAS "ofx_ts_approxent" (x() AS DOUBLE, n AS LONG, m AS LONG, r AS DOUBLE) AS DOUBLE
DECLARE FUNCTION AXF_PERM LIB "gmbstats" ALIAS "ofx_ts_perm" (x() AS DOUBLE, n AS LONG, m AS LONG, lag AS LONG) AS DOUBLE
DECLARE FUNCTION AXF_LZ LIB "gmbstats" ALIAS "ofx_ts_lz" (x() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION AXF_LYAP LIB "gmbstats" ALIAS "ofx_ts_lyap" (x() AS DOUBLE, n AS LONG, m AS LONG, tau AS LONG) AS DOUBLE
DECLARE FUNCTION AXF_OUFIT LIB "gmbstats" ALIAS "ofx_ts_oufit" (x() AS DOUBLE, n AS LONG, dt AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AXF_BMA LIB "gmbstats" ALIAS "ofx_ts_bma" (ic() AS DOUBLE, m AS LONG, out() AS DOUBLE) AS LONG

PUBLIC FUNCTION TSHURST(series() AS DOUBLE) AS DOUBLE
    RETURN AXF_HURST(series, UBOUND(series) + 1)
END FUNCTION

PUBLIC FUNCTION TSFRACDIFF(series() AS DOUBLE, d AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL AXF_FRACDIFF(series, n, d, o)
    RETURN o
END FUNCTION

' out(3) = statistic, pvalue, degrees_of_freedom.
PUBLIC FUNCTION TSGRANGER(x() AS DOUBLE, y() AS DOUBLE, p AS LONG) AS ARRAY OF DOUBLE
    DIM o(2) AS DOUBLE
    CALL AXF_GRANGER(x, y, UBOUND(x) + 1, p, o)
    RETURN o
END FUNCTION

' out(3) = omega, alpha, beta.
PUBLIC FUNCTION TSGARCH11(series() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(2) AS DOUBLE
    CALL AXF_GARCH11(series, UBOUND(series) + 1, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSKALMAN1D(z() AS DOUBLE, a AS DOUBLE, h AS DOUBLE, q AS DOUBLE, r AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(z) + 1
    DIM o(n - 1) AS DOUBLE
    CALL AXF_KALMAN1D(z, n, a, h, q, r, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSSTATIONARYBOOTSTRAP(series() AS DOUBLE, window AS LONG, seed AS LONG) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(series) + 1
    DIM o(n - 1) AS DOUBLE
    CALL AXF_BOOTSTRAP(series, n, window, seed, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSSAMPEN(series() AS DOUBLE, m AS LONG, r AS DOUBLE) AS DOUBLE
    RETURN AXF_SAMPEN(series, UBOUND(series) + 1, m, r)
END FUNCTION

PUBLIC FUNCTION TSAPPROXENTROPY(series() AS DOUBLE, m AS LONG, r AS DOUBLE) AS DOUBLE
    RETURN AXF_APPROXENT(series, UBOUND(series) + 1, m, r)
END FUNCTION

PUBLIC FUNCTION TSENTROPYPERM(series() AS DOUBLE, m AS LONG, lag AS LONG) AS DOUBLE
    RETURN AXF_PERM(series, UBOUND(series) + 1, m, lag)
END FUNCTION

PUBLIC FUNCTION TSENTROPYLZ(series() AS DOUBLE) AS DOUBLE
    RETURN AXF_LZ(series, UBOUND(series) + 1)
END FUNCTION

PUBLIC FUNCTION TSLYAPUNOV(series() AS DOUBLE, m AS LONG, tau AS LONG) AS DOUBLE
    RETURN AXF_LYAP(series, UBOUND(series) + 1, m, tau)
END FUNCTION

' out(5) = theta, mu, sigma, phi, half_life.
PUBLIC FUNCTION TSOUFIT(series() AS DOUBLE, dt AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(4) AS DOUBLE
    CALL AXF_OUFIT(series, UBOUND(series) + 1, dt, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TSBMAWEIGHTS(ic() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM m AS LONG = UBOUND(ic) + 1
    DIM o(m - 1) AS DOUBLE
    CALL AXF_BMA(ic, m, o)
    RETURN o
END FUNCTION
