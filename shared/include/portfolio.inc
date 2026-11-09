' portfolio.inc — namespace PORTFOLIO: ottimizzazione e rischio multi-asset (FFI).
'
' Erano TRADING.PORTFOLIO (19 funzioni). Il modello e' quello del runtime condiviso
' (gmb_port_* / gmb_tm_portfolio_*): MAXSHARPE e MEANVARIANCE usano la formula chiusa
' w ~ inv(cov)·(mu - rf), MINVOLATILITY w ~ inv(cov)·1, RISKPARITY l'iterazione ERC.
'
' ABI (differenza voluta rispetto al builtin): le funzioni che restituivano il
' "PortfolioResult" ritornano un ARRAY OF DOUBLE con questo layout:
'
'     o(0) = rendimento atteso      o(1) = volatilita'      o(2) = Sharpe
'     o(3 .. 3+n-1) = pesi
'
' KELLYCRITERION / MINCVAR / MINCDAR hanno un solo "peso" (o(3)), come il builtin.
' Il campo Converged del vecchio tipo non esiste: le soluzioni sono deterministiche.
'
' Le matrici di covarianza sono JAGGED (cov(i)(j)); i wrapper le appiattiscono
' row-major. I rendimenti sono in frazione (0.05 = 5%).

NAMESPACE PORTFOLIO

' ── binding nativi (uso interno) ────────────────────────────────────────────

DECLARE FUNCTION PFN_EXPECTED LIB "gmbquant" ALIAS "ofx_q_pf_expected" (w() AS DOUBLE, mean() AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION PFN_STDEV LIB "gmbquant" ALIAS "ofx_q_pf_stdev" (w() AS DOUBLE, nw AS LONG, cov AS MATRIX OF (INT, INT, DOUBLE), ncov AS LONG) AS DOUBLE
DECLARE FUNCTION PFN_SHARPE LIB "gmbquant" ALIAS "ofx_q_pf_sharpe" (ret AS DOUBLE, sd AS DOUBLE, rf AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PFN_LONGONLY LIB "gmbquant" ALIAS "ofx_q_pf_longonly" (w() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PFN_MAD LIB "gmbquant" ALIAS "ofx_q_pf_mad" (r() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PFN_SEMIMAD LIB "gmbquant" ALIAS "ofx_q_pf_semimad" (r() AS DOUBLE, target AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION PFN_VAR LIB "gmbquant" ALIAS "ofx_q_pf_var" (hist AS MATRIX OF (INT, INT, DOUBLE), nh AS LONG, w() AS DOUBLE, nw AS LONG, conf AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION PFN_CVAR LIB "gmbquant" ALIAS "ofx_q_pf_cvar" (hist AS MATRIX OF (INT, INT, DOUBLE), nh AS LONG, w() AS DOUBLE, nw AS LONG, conf AS DOUBLE) AS DOUBLE

DECLARE NATIVE SUB PFN_MINVOL LIB "gmbquant" ALIAS "ofx_q_pf_minvol" (cov AS MATRIX OF (INT, INT, DOUBLE), ncov AS LONG, out() AS DOUBLE, nout AS LONG)
DECLARE NATIVE SUB PFN_MAXSHARPE LIB "gmbquant" ALIAS "ofx_q_pf_maxsharpe" (mean() AS DOUBLE, nm AS LONG, cov AS MATRIX OF (INT, INT, DOUBLE), ncov AS LONG, rf AS DOUBLE, out() AS DOUBLE, nout AS LONG)
DECLARE NATIVE SUB PFN_MEANVAR LIB "gmbquant" ALIAS "ofx_q_pf_meanvar" (mean() AS DOUBLE, nm AS LONG, cov AS MATRIX OF (INT, INT, DOUBLE), ncov AS LONG, out() AS DOUBLE, nout AS LONG)
DECLARE NATIVE SUB PFN_RISKPARITY LIB "gmbquant" ALIAS "ofx_q_pf_riskparity" (cov AS MATRIX OF (INT, INT, DOUBLE), ncov AS LONG, out() AS DOUBLE, nout AS LONG)
DECLARE SUB PFN_EQUALLY LIB "gmbquant" ALIAS "ofx_q_pf_equally" (n AS LONG, out() AS DOUBLE)
DECLARE SUB PFN_MAXRETURN LIB "gmbquant" ALIAS "ofx_q_pf_maxreturn" (mean() AS DOUBLE, out() AS DOUBLE)
DECLARE SUB PFN_KELLY LIB "gmbquant" ALIAS "ofx_q_pf_kelly" (r() AS DOUBLE, rf AS DOUBLE, out() AS DOUBLE)
DECLARE SUB PFN_MINCVAR LIB "gmbquant" ALIAS "ofx_q_pf_mincvar" (r() AS DOUBLE, conf AS DOUBLE, rf AS DOUBLE, out() AS DOUBLE)
DECLARE SUB PFN_MINCDAR LIB "gmbquant" ALIAS "ofx_q_pf_mincdar" (r() AS DOUBLE, conf AS DOUBLE, rf AS DOUBLE, out() AS DOUBLE)
DECLARE SUB PFN_EVOLVE LIB "gmbquant" ALIAS "ofx_q_pf_evolve" (cov() AS DOUBLE, mean() AS DOUBLE, out() AS DOUBLE)
DECLARE SUB PFN_BLACKLITTERMAN LIB "gmbquant" ALIAS "ofx_q_pf_blacklitterman" (caps() AS DOUBLE, cov() AS DOUBLE, risk AS DOUBLE, tau AS DOUBLE, P() AS DOUBLE, Q() AS DOUBLE, omega() AS DOUBLE, rf AS DOUBLE, out() AS DOUBLE)

' ── metriche scalari (drop-in) ──────────────────────────────────────────────

' Rendimento atteso del portafoglio: somma w_i * mean_i.
DECLARE FUNCTION EXPECTEDRETURN LIB "gmbquant" ALIAS "ofx_q_pf_expected" (w() AS DOUBLE, mean() AS DOUBLE) AS DOUBLE

' Volatilita' (deviazione standard): sqrt(w' cov w).
PUBLIC FUNCTION STDEV(w() AS DOUBLE, cov AS MATRIX OF (INT, INT, DOUBLE)) AS DOUBLE
    RETURN PFN_STDEV(w, UBOUND(w) + 1, cov, (UBOUND(cov, 1) + 1) * (UBOUND(cov, 2) + 1))
END FUNCTION

' Sharpe: (ret - rf) / sd; sd = 0 -> 0.
DECLARE FUNCTION SHARPERATIO LIB "gmbquant" ALIAS "ofx_q_pf_sharpe" (ret AS DOUBLE, sd AS DOUBLE, rf AS DOUBLE) AS DOUBLE

' Long-only: -1 (vero) se tutti i pesi sono >= 0, 0 altrimenti.
DECLARE FUNCTION ISLONGONLY LIB "gmbquant" ALIAS "ofx_q_pf_longonly" (w() AS DOUBLE) AS DOUBLE

' Mean Absolute Deviation dalla mediana.
DECLARE FUNCTION PORTFOLIOMAD LIB "gmbquant" ALIAS "ofx_q_pf_mad" (r() AS DOUBLE) AS DOUBLE

' Semi-MAD: solo i rendimenti < target (default 0), scarto medio assoluto dalla mediana.
PUBLIC FUNCTION PORTFOLIOSEMIMAD(r() AS DOUBLE, OPTIONAL target AS DOUBLE = 0) AS DOUBLE
    RETURN PFN_SEMIMAD(r, target)
END FUNCTION

' VaR storico (percentuale) e CVaR (frazione) di un portafoglio multi-asset.
' `pesi` resta nella firma per compatibilita' ma e' IGNORATO: il builtin lavora
' sullo storico appiattito (parita' IL/PCode/VM-C/C99).
PUBLIC FUNCTION PVAR(hist AS MATRIX OF (INT, INT, DOUBLE), pesi() AS DOUBLE, conf AS DOUBLE) AS DOUBLE
    RETURN PFN_VAR(hist, (UBOUND(hist, 1) + 1) * (UBOUND(hist, 2) + 1), pesi, UBOUND(pesi) + 1, conf)
END FUNCTION

PUBLIC FUNCTION CVAR(hist AS MATRIX OF (INT, INT, DOUBLE), pesi() AS DOUBLE, conf AS DOUBLE) AS DOUBLE
    RETURN PFN_CVAR(hist, (UBOUND(hist, 1) + 1) * (UBOUND(hist, 2) + 1), pesi, UBOUND(pesi) + 1, conf)
END FUNCTION

' ── ottimizzatori (ARRAY: [ret, vol, sharpe, pesi...]) ──────────────────────

' Minima varianza: w ~ inv(cov)·1.
PUBLIC FUNCTION MINVOLATILITY(cov AS MATRIX OF (INT, INT, DOUBLE)) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(cov, 1) + 1
    DIM o(n + 2) AS DOUBLE
    CALL PFN_MINVOL(cov, n * n, o, n + 3)
    RETURN o
END FUNCTION

' Massimo Sharpe: w ~ inv(cov)·(mu - rf).
PUBLIC FUNCTION MAXSHARPE(mean() AS DOUBLE, cov AS MATRIX OF (INT, INT, DOUBLE), rf AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(mean) + 1
    DIM o(n + 2) AS DOUBLE
    CALL PFN_MAXSHARPE(mean, n, cov, n * n, rf, o, n + 3)
    RETURN o
END FUNCTION

' Mean-variance: stessa formula chiusa di MAXSHARPE con rf = 0.
PUBLIC FUNCTION MEANVARIANCE(mean() AS DOUBLE, cov AS MATRIX OF (INT, INT, DOUBLE)) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(mean) + 1
    DIM o(n + 2) AS DOUBLE
    CALL PFN_MEANVAR(mean, n, cov, n * n, o, n + 3)
    RETURN o
END FUNCTION

' Risk parity (ERC): contributi di rischio uguali.
PUBLIC FUNCTION RISKPARITY(cov AS MATRIX OF (INT, INT, DOUBLE)) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(cov, 1) + 1
    DIM o(n + 2) AS DOUBLE
    CALL PFN_RISKPARITY(cov, n * n, o, n + 3)
    RETURN o
END FUNCTION

' Pesi uguali 1/n.
PUBLIC FUNCTION EQUALLYWEIGHTED(n AS LONG) AS ARRAY OF DOUBLE
    DIM o(n + 2) AS DOUBLE
    CALL PFN_EQUALLY(n, o)
    RETURN o
END FUNCTION

' 100% sull'asset con rendimento atteso massimo.
PUBLIC FUNCTION MAXRETURN(mean() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(mean) + 1
    DIM o(n + 2) AS DOUBLE
    CALL PFN_MAXRETURN(mean, o)
    RETURN o
END FUNCTION

' Kelly: o(0) = indice di Kelly, o(3) = stesso valore (un solo "asset").
' Accetta una matrice JAGGED di rendimenti (periodi x asset), come il builtin:
' i valori vengono appiattiti (la formula lavora sulla serie, non per asset).
' `rf` resta nella firma per compatibilita' ma e' IGNORATO (parita' VM/C/C99).
PUBLIC FUNCTION KELLYCRITERION(hist() AS ARRAY OF DOUBLE, rf AS DOUBLE) AS ARRAY OF DOUBLE
    DIM rows AS LONG = UBOUND(hist) + 1
    DIM cols AS LONG = UBOUND(hist(0)) + 1
    DIM flat((rows * cols) - 1) AS DOUBLE
    FOR i = 0 TO rows - 1
        FOR j = 0 TO cols - 1
            flat(i * cols + j) = hist(i)(j)
        NEXT j
    NEXT i
    DIM o(3) AS DOUBLE
    CALL PFN_KELLY(flat, rf, o)
    RETURN o
END FUNCTION

' Min-CVaR storico: o(0) = CVaR, o(3) = 1.0 (il builtin non distribuisce pesi).
PUBLIC FUNCTION MINCVAR(hist() AS ARRAY OF DOUBLE, conf AS DOUBLE, rf AS DOUBLE) AS ARRAY OF DOUBLE
    DIM rows AS LONG = UBOUND(hist) + 1
    DIM cols AS LONG = UBOUND(hist(0)) + 1
    DIM flat((rows * cols) - 1) AS DOUBLE
    FOR i = 0 TO rows - 1
        FOR j = 0 TO cols - 1
            flat(i * cols + j) = hist(i)(j)
        NEXT j
    NEXT i
    DIM o(3) AS DOUBLE
    CALL PFN_MINCVAR(flat, conf, rf, o)
    RETURN o
END FUNCTION

' Min-CDaR storico: o(0) = CDaR, o(3) = 1.0.
PUBLIC FUNCTION MINCDAR(hist() AS ARRAY OF DOUBLE, conf AS DOUBLE, rf AS DOUBLE) AS ARRAY OF DOUBLE
    DIM rows AS LONG = UBOUND(hist) + 1
    DIM cols AS LONG = UBOUND(hist(0)) + 1
    DIM flat((rows * cols) - 1) AS DOUBLE
    FOR i = 0 TO rows - 1
        FOR j = 0 TO cols - 1
            flat(i * cols + j) = hist(i)(j)
        NEXT j
    NEXT i
    DIM o(3) AS DOUBLE
    CALL PFN_MINCDAR(flat, conf, rf, o)
    RETURN o
END FUNCTION

' EVOLVE: il parametro GA del builtin non e' riprodotto; la soluzione e' quella
' deterministica di minima varianza (come il runtime condiviso).
PUBLIC FUNCTION EVOLVE(mean() AS DOUBLE, cov() AS ARRAY OF DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(mean) + 1
    DIM flat((n * n) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        FOR j = 0 TO n - 1
            flat(i * n + j) = cov(i)(j)
        NEXT j
    NEXT i
    DIM o(n + 2) AS DOUBLE
    CALL PFN_EVOLVE(flat, mean, o)
    RETURN o
END FUNCTION

' Black-Litterman completo: Pi = lambda*Sigma*w_mkt;
' post = [(tau*Sigma)^-1 + P'Omega^-1 P]^-1 [(tau*Sigma)^-1 Pi + P'Omega^-1 Q];
' poi MAXSHARPE(post, Sigma, rf). P e' k*n, omega k*k (jagged).
PUBLIC FUNCTION BLACKLITTERMAN(caps() AS DOUBLE, cov() AS ARRAY OF DOUBLE, risk AS DOUBLE, tau AS DOUBLE, P() AS ARRAY OF DOUBLE, Q() AS DOUBLE, omega() AS ARRAY OF DOUBLE, rf AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(caps) + 1
    DIM k AS LONG = UBOUND(Q) + 1
    DIM fcov((n * n) - 1) AS DOUBLE
    DIM fP((k * n) - 1) AS DOUBLE
    DIM fom((k * k) - 1) AS DOUBLE
    FOR i = 0 TO n - 1
        FOR j = 0 TO n - 1
            fcov(i * n + j) = cov(i)(j)
        NEXT j
    NEXT i
    FOR i = 0 TO k - 1
        FOR j = 0 TO n - 1
            fP(i * n + j) = P(i)(j)
        NEXT j
    NEXT i
    FOR i = 0 TO k - 1
        FOR j = 0 TO k - 1
            fom(i * k + j) = omega(i)(j)
        NEXT j
    NEXT i
    DIM o(n + 2) AS DOUBLE
    CALL PFN_BLACKLITTERMAN(caps, fcov, risk, tau, fP, Q, fom, rf, o)
    RETURN o
END FUNCTION
