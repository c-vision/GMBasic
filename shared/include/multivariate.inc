' multivariatex.inc — MULTIVARIATE via FFI (libgmbstats), prima slice scalare.
' Stessi nomi/parametri del builtin; n dedotto dagli array dove possibile.
NAMESPACE MULTIVARIATE

DECLARE FUNCTION MXF_BOEI LIB "gmbstats" ALIAS "ofx_bo_ei" (mu AS DOUBLE, sd AS DOUBLE, best AS DOUBLE, xi AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_BOPI LIB "gmbstats" ALIAS "ofx_bo_pi" (mu AS DOUBLE, sd AS DOUBLE, best AS DOUBLE, xi AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_BOUCB LIB "gmbstats" ALIAS "ofx_bo_ucb" (mu AS DOUBLE, sd AS DOUBLE, kappa AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_GAMBP LIB "gmbstats" ALIAS "ofx_gambler_p" (i AS DOUBLE, N AS DOUBLE, p AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_GAMBD LIB "gmbstats" ALIAS "ofx_gambler_d" (i AS DOUBLE, N AS DOUBLE, p AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_ATEIPW LIB "gmbstats" ALIAS "ofx_ate_ipw" (t() AS DOUBLE, y() AS DOUBLE, ps() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_ATEMATCH LIB "gmbstats" ALIAS "ofx_ate_matching" (t() AS DOUBLE, y() AS DOUBLE, ps() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_RMTLP LIB "gmbstats" ALIAS "ofx_rmt_lambda_plus" (n AS LONG, T AS LONG, sigma2 AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_RMTLM LIB "gmbstats" ALIAS "ofx_rmt_lambda_minus" (n AS LONG, T AS LONG, sigma2 AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_RMTCOH LIB "gmbstats" ALIAS "ofx_rmt_coherence" (v() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_RMTNORM LIB "gmbstats" ALIAS "ofx_rmt_spectral_norm" (A() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_RMTDK LIB "gmbstats" ALIAS "ofx_rmt_davis_kahan" (eNorm AS DOUBLE, gap AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_RMTANG LIB "gmbstats" ALIAS "ofx_rmt_principal_angle" (u() AS DOUBLE, v() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_RMTTIGHT LIB "gmbstats" ALIAS "ofx_rmt_tightness" (observed AS DOUBLE, bound AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_MVGAUSS LIB "gmbstats" ALIAS "ofx_mv_gauss_pdf" (x() AS DOUBLE, loc() AS DOUBLE, cov() AS DOUBLE, d AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_MVSTUDENT LIB "gmbstats" ALIAS "ofx_mv_student_pdf" (x() AS DOUBLE, loc() AS DOUBLE, cov() AS DOUBLE, nu AS DOUBLE, d AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_COPCLA LIB "gmbstats" ALIAS "ofx_copula_clayton" (u AS DOUBLE, v AS DOUBLE, theta AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_COPGUM LIB "gmbstats" ALIAS "ofx_copula_gumbel" (u AS DOUBLE, v AS DOUBLE, theta AS DOUBLE) AS DOUBLE
DECLARE FUNCTION MXF_COPFRANK LIB "gmbstats" ALIAS "ofx_copula_frank" (u AS DOUBLE, v AS DOUBLE, theta AS DOUBLE) AS DOUBLE

PUBLIC FUNCTION BOEI(mu AS DOUBLE, sd AS DOUBLE, best AS DOUBLE, xi AS DOUBLE) AS DOUBLE
    RETURN MXF_BOEI(mu, sd, best, xi)
END FUNCTION
PUBLIC FUNCTION BOPI(mu AS DOUBLE, sd AS DOUBLE, best AS DOUBLE, xi AS DOUBLE) AS DOUBLE
    RETURN MXF_BOPI(mu, sd, best, xi)
END FUNCTION
PUBLIC FUNCTION BOUCB(mu AS DOUBLE, sd AS DOUBLE, kappa AS DOUBLE) AS DOUBLE
    RETURN MXF_BOUCB(mu, sd, kappa)
END FUNCTION



PUBLIC FUNCTION GAMBLERRUINP(i AS DOUBLE, N AS DOUBLE, p AS DOUBLE) AS DOUBLE
    RETURN MXF_GAMBP(i, N, p)
END FUNCTION
PUBLIC FUNCTION GAMBLERRUIND(i AS DOUBLE, N AS DOUBLE, p AS DOUBLE) AS DOUBLE
    RETURN MXF_GAMBD(i, N, p)
END FUNCTION

PUBLIC FUNCTION ATEIPW(t() AS DOUBLE, y() AS DOUBLE, ps() AS DOUBLE) AS DOUBLE
    RETURN MXF_ATEIPW(t, y, ps, UBOUND(t) + 1)
END FUNCTION
PUBLIC FUNCTION ATEMATCHING(t() AS DOUBLE, y() AS DOUBLE, ps() AS DOUBLE) AS DOUBLE
    RETURN MXF_ATEMATCH(t, y, ps, UBOUND(t) + 1)
END FUNCTION

PUBLIC FUNCTION RMTLAMBDAPLUS(n AS LONG, T AS LONG, sigma2 AS DOUBLE) AS DOUBLE
    RETURN MXF_RMTLP(n, T, sigma2)
END FUNCTION
PUBLIC FUNCTION RMTLAMBDAMINUS(n AS LONG, T AS LONG, sigma2 AS DOUBLE) AS DOUBLE
    RETURN MXF_RMTLM(n, T, sigma2)
END FUNCTION
PUBLIC FUNCTION RMTCOHERENCE(v() AS DOUBLE) AS DOUBLE
    RETURN MXF_RMTCOH(v, UBOUND(v) + 1)
END FUNCTION
PUBLIC FUNCTION RMTSPECTRALNORM(A() AS DOUBLE, n AS LONG) AS DOUBLE
    RETURN MXF_RMTNORM(A, n)
END FUNCTION
PUBLIC FUNCTION RMTDAVISKAHAN(eNorm AS DOUBLE, gap AS DOUBLE) AS DOUBLE
    RETURN MXF_RMTDK(eNorm, gap)
END FUNCTION
PUBLIC FUNCTION RMTPRINCIPALANGLE(u() AS DOUBLE, v() AS DOUBLE) AS DOUBLE
    RETURN MXF_RMTANG(u, v, UBOUND(u) + 1)
END FUNCTION
PUBLIC FUNCTION RMTTIGHTNESS(observed AS DOUBLE, bound AS DOUBLE) AS DOUBLE
    RETURN MXF_RMTTIGHT(observed, bound)
END FUNCTION

PUBLIC FUNCTION MVGAUSSPDF(x() AS DOUBLE, loc() AS DOUBLE, cov() AS DOUBLE) AS DOUBLE
    RETURN MXF_MVGAUSS(x, loc, cov, UBOUND(x) + 1)
END FUNCTION
PUBLIC FUNCTION MVSTUDENTPDF(x() AS DOUBLE, loc() AS DOUBLE, cov() AS DOUBLE, nu AS DOUBLE) AS DOUBLE
    RETURN MXF_MVSTUDENT(x, loc, cov, nu, UBOUND(x) + 1)
END FUNCTION
PUBLIC FUNCTION COPULACLAYTONCDF(u AS DOUBLE, v AS DOUBLE, theta AS DOUBLE) AS DOUBLE
    RETURN MXF_COPCLA(u, v, theta)
END FUNCTION
PUBLIC FUNCTION COPULAGUMBELCDF(u AS DOUBLE, v AS DOUBLE, theta AS DOUBLE) AS DOUBLE
    RETURN MXF_COPGUM(u, v, theta)
END FUNCTION
PUBLIC FUNCTION COPULAFRANKCDF(u AS DOUBLE, v AS DOUBLE, theta AS DOUBLE) AS DOUBLE
    RETURN MXF_COPFRANK(u, v, theta)
END FUNCTION

' ═══════════════════════════════════════════════════════════════════════════
' Seconda slice: KMeans/PCA/GMM/GP/SVM (risultati composti in out() piatto).
' ═══════════════════════════════════════════════════════════════════════════
DECLARE FUNCTION MXF_KMEANS LIB "gmbstats" ALIAS "ofx_kmeans" (x() AS DOUBLE, n AS LONG, k AS LONG, maxIter AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MXF_PCA LIB "gmbstats" ALIAS "ofx_pca" (data() AS DOUBLE, rows AS LONG, cols AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MXF_GMM LIB "gmbstats" ALIAS "ofx_gmm_1d" (x() AS DOUBLE, n AS LONG, k AS LONG, iters AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MXF_GP LIB "gmbstats" ALIAS "ofx_gp" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, xs() AS DOUBLE, m AS LONG, l2 AS DOUBLE, noise AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MXF_BONEXT LIB "gmbstats" ALIAS "ofx_bo_next" (x() AS DOUBLE, y() AS DOUBLE, cand() AS DOUBLE, n AS LONG, m AS LONG, hyper() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MXF_SVMFIT LIB "gmbstats" ALIAS "ofx_svm_fit" (X() AS DOUBLE, y() AS DOUBLE, n AS LONG, d AS LONG, k AS LONG, C AS DOUBLE, maxIter AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION MXF_SVMPRED LIB "gmbstats" ALIAS "ofx_svm_predict" (W() AS DOUBLE, x() AS DOUBLE, d AS LONG, k AS LONG) AS LONG

' KMeans 1D: out = centri(k) + assegnazioni(n).
PUBLIC SUB KMEANS(data() AS DOUBLE, k AS LONG, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(data) + 1
    CALL MXF_KMEANS(data, n, k, 100, out)
END SUB

' PCA: data jagged (osservazioni x variabili); out = autovalori(m) + autovettori(m*m).
PUBLIC SUB PCA(data() AS ARRAY OF DOUBLE, out() AS DOUBLE)
    DIM rows AS LONG = UBOUND(data) + 1
    DIM cols AS LONG = UBOUND(data(0)) + 1
    DIM flat((rows * cols) - 1) AS DOUBLE
    FOR i = 0 TO rows - 1
        FOR j = 0 TO cols - 1
            flat(i * cols + j) = data(i)(j)
        NEXT j
    NEXT i
    CALL MXF_PCA(flat, rows, cols, out)
END SUB

' GMM 1D: out = mu(k) + var(k) + weight(k).
PUBLIC SUB GMMEM1D(x() AS DOUBLE, k AS LONG, iters AS LONG, out() AS DOUBLE)
    CALL MXF_GMM(x, UBOUND(x) + 1, k, iters, out)
END SUB

' GP: out = mean(m) + var(m).
PUBLIC SUB GPRREGRESS(x() AS DOUBLE, y() AS DOUBLE, xs() AS DOUBLE, l2 AS DOUBLE, noise AS DOUBLE, out() AS DOUBLE)
    CALL MXF_GP(x, y, UBOUND(x) + 1, xs, UBOUND(xs) + 1, l2, noise, out)
END SUB

' BONEXT: out(2) = {best_index, best_value}.
PUBLIC FUNCTION BONEXT(x() AS DOUBLE, y() AS DOUBLE, cand() AS DOUBLE, l2 AS DOUBLE, noise AS DOUBLE, kappa AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(1) AS DOUBLE
    DIM h(2) AS DOUBLE
    h(0) = l2
    h(1) = noise
    h(2) = kappa
    CALL MXF_BONEXT(x, y, cand, UBOUND(x) + 1, UBOUND(cand) + 1, h, o)
    RETURN o
END FUNCTION

' SVM multiclasse: X jagged (n x d); out = W(k*d).
PUBLIC SUB SVMFIT(X() AS ARRAY OF DOUBLE, y() AS DOUBLE, k AS LONG, C AS DOUBLE, maxIter AS LONG, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(X) + 1
    DIM d AS LONG = UBOUND(X(0)) + 1
    DIM flat((n * d) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        FOR j = 0 TO d - 1
            flat(i * d + j) = X(i)(j)
        NEXT j
    NEXT i
    CALL MXF_SVMFIT(flat, y, n, d, k, C, maxIter, out)
END SUB

PUBLIC FUNCTION SVMPREDICT(W() AS DOUBLE, x() AS DOUBLE, d AS LONG, k AS LONG) AS LONG
    DIM __r AS LONG
    __r = MXF_SVMPRED(W, x, d, k)
    RETURN __r
END FUNCTION

' ── Gradient boosting: modello opaco (handle LONG) ────────────────────────
DECLARE FUNCTION MXF_BOOSTFIT LIB "gmbstats" ALIAS "ofx_boost_fit" (X() AS DOUBLE, y() AS DOUBLE, n AS LONG, d AS LONG, engine AS LONG, nEst AS LONG, lr AS DOUBLE, maxDepth AS LONG) AS LONG
DECLARE FUNCTION MXF_BOOSTPRED LIB "gmbstats" ALIAS "ofx_boost_predict" (model AS LONG, x() AS DOUBLE, d AS LONG) AS DOUBLE
DECLARE FUNCTION MXF_BOOSTSCORE LIB "gmbstats" ALIAS "ofx_boost_score" (model AS LONG, X() AS DOUBLE, y() AS DOUBLE, n AS LONG, d AS LONG) AS DOUBLE
DECLARE SUB MXF_BOOSTFREE LIB "gmbstats" ALIAS "ofx_boost_free" (model AS LONG)

PUBLIC FUNCTION BOOSTFIT(X() AS ARRAY OF DOUBLE, y() AS DOUBLE, engine AS LONG, nEst AS LONG, lr AS DOUBLE, maxDepth AS LONG) AS LONG
    DIM n AS LONG = UBOUND(X) + 1
    DIM d AS LONG = UBOUND(X(0)) + 1
    DIM flat((n * d) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        FOR j = 0 TO d - 1
            flat(i * d + j) = X(i)(j)
        NEXT j
    NEXT i
    DIM __h AS LONG
    __h = MXF_BOOSTFIT(flat, y, n, d, engine, nEst, lr, maxDepth)
    RETURN __h
END FUNCTION

PUBLIC FUNCTION BOOSTPREDICT(model AS LONG, x() AS DOUBLE) AS DOUBLE
    RETURN MXF_BOOSTPRED(model, x, UBOUND(x) + 1)
END FUNCTION

PUBLIC FUNCTION BOOSTSCORE(model AS LONG, X() AS ARRAY OF DOUBLE, y() AS DOUBLE) AS DOUBLE
    DIM n AS LONG = UBOUND(X) + 1
    DIM d AS LONG = UBOUND(X(0)) + 1
    DIM flat((n * d) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        FOR j = 0 TO d - 1
            flat(i * d + j) = X(i)(j)
        NEXT j
    NEXT i
    RETURN MXF_BOOSTSCORE(model, flat, y, n, d)
END FUNCTION

PUBLIC SUB BOOSTFREE(model AS LONG)
    CALL MXF_BOOSTFREE(model)
END SUB
