' conic.inc — namespace OPTIMIZE: coni esponenziali/potenza, IPM (LP/QP),
' SOCP, SDP, presolve e forma standard (pacchetto FFI gmbconic).
'
' Tutti i parametri sono espliciti: niente struct C e niente tabelle di puntatori.
' Gli array di uscita sono forniti dal chiamante e dimensionati secondo le
' indicazioni di ogni funzione. Ritorno: 0 = ottimo/ok, 1 = max iter,
' 2 = memoria, 3 = sistema singolare, -1 = argomenti/handle.
'
' Nota sulle tolleranze: `tol` e' la tolleranza assoluta di Newton. Il pavimento
' numerico del doppio rende inutile scendere sotto 1e-8: con tol=1e-10 il solver
' puo' esaurire le iterazioni (ritorno 1) pur avendo il punto corretto. Per i
' problemi piccoli usare 1e-8 e maxIter >= 200.

NAMESPACE OPTIMIZE

' ── coni esponenziali / potenza ────────────────────────────────────────────
' kind: 0=PEXP, 1=PPOW, 2=RPOW. Vettori dei membri: x()/s()/z() = 3.
' f() = valore della barriera (1 elemento).
DECLARE NATIVE FUNCTION EXPCONEBARRIER LIB "gmbconic" ALIAS "ofx_expcone_barrier__nat" (kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, f() AS DOUBLE) AS LONG
' g() = gradiente (3).
DECLARE NATIVE FUNCTION EXPCONEGRAD LIB "gmbconic" ALIAS "ofx_expcone_grad__nat" (kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, g() AS DOUBLE) AS LONG
' H() = Hessiana 3x3 row-major (9).
DECLARE NATIVE FUNCTION EXPCONEHESS LIB "gmbconic" ALIAS "ofx_expcone_hess__nat" (kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, H() AS DOUBLE) AS LONG
' 1 se s e' strettamente interno al cono duale K*.
DECLARE NATIVE FUNCTION EXPCONEDUALIN LIB "gmbconic" ALIAS "ofx_expcone_dual_in__nat" (kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE) AS LONG
' W() = punto di scaling (3).
DECLARE NATIVE FUNCTION EXPCONEDUALPOINT LIB "gmbconic" ALIAS "ofx_expcone_dual_point__nat" (kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE, W() AS DOUBLE) AS LONG
' m() = 7 metriche (GAP, CONDZ, CONDS, REL, MZ, MS, WN).
DECLARE NATIVE FUNCTION EXPCONENTMETRICS LIB "gmbconic" ALIAS "ofx_expcone_nt_metrics__nat" (kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, m() AS DOUBLE) AS LONG
' out() = Theta[9] | Thin[9] | rt[9] | scale (28). Ritorna il ramo (1 NT, 0 Hessiana, -1 errore).
DECLARE NATIVE FUNCTION EXPCONESCALING LIB "gmbconic" ALIAS "ofx_expcone_scaling__nat" (kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, smu AS DOUBLE, out() AS DOUBLE) AS LONG

' ── IPM: min 1/2 x'Qx + c'x  s.t.  A x = b, x >= 0 ────────────────────────
' A = m x n row-major. Uscite: x (n), y (m), z (n) — le prime due sono i primali,
' z i ridotti (dual slack). IPMLP e' il caso Q assente; IPMQP prende Q (n x n,
' simmetrica row-major). Le varianti WARM partono da x0 (n) e y0 (m).
DECLARE NATIVE FUNCTION IPMLP LIB "gmbconic" ALIAS "ofx_ipm_lp__nat" (A() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION IPMLPWARM LIB "gmbconic" ALIAS "ofx_ipm_lp_warm__nat" (A() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE, x0() AS DOUBLE, y0() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION IPMQP LIB "gmbconic" ALIAS "ofx_ipm_qp__nat" (A() AS DOUBLE, Q() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION IPMQPWARM LIB "gmbconic" ALIAS "ofx_ipm_qp_warm__nat" (A() AS DOUBLE, Q() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE, x0() AS DOUBLE, y0() AS DOUBLE) AS LONG

' Varianti sparse: A in CSC (Aptr n+1, Arow/ Aval per gli nnz, 0-based);
' IPMQPCSC prende anche Q nel triangolo inferiore in CSC (Qptr, Qrow, Qval).
DECLARE NATIVE FUNCTION IPMLPCSC LIB "gmbconic" ALIAS "ofx_ipm_lp_csc__nat" (Aptr() AS LONG, Arow() AS LONG, Aval() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION IPMQPCSC LIB "gmbconic" ALIAS "ofx_ipm_qp_csc__nat" (Aptr() AS LONG, Arow() AS LONG, Aval() AS DOUBLE, Qptr() AS LONG, Qrow() AS LONG, Qval() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE) AS LONG

' ── SOCP: min c'x  s.t.  E x = d,  G x + h in K ────────────────────────────
' E = p x n row-major, d (p), c (n). G = K x n row-major con K = somma delle
' dimensioni dei coni, h (K). I coni sono descritti da tre array paralleli:
'   ctype(k) = 0 R+ (cnm = 1, una riga di G), 1 QUAD (SOC), 2 RQUAD
'   cnm(k)   = dimensione del cono k
'   cmem()   = concatenazione degli indici dei membri dei soli coni SOC, in
'              ordine di cono (cnm(k) valori per ogni cono con ctype <> 0)
' Uscite: x (n), y (p), lam (K). SOCPSOLVESP usa la stessa matrice KKT ma con
' fattorizzazione sparsa: stessi risultati, costo O(fill). E/G/h restano densi.
DECLARE NATIVE FUNCTION SOCPSOLVE LIB "gmbconic" ALIAS "ofx_socp_solve__nat" (n AS LONG, p AS LONG, E() AS DOUBLE, d() AS DOUBLE, c() AS DOUBLE, ncones AS LONG, ctype() AS LONG, cnm() AS LONG, cmem() AS LONG, G() AS DOUBLE, h() AS DOUBLE, tolGap AS DOUBLE, tolFeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, lam() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION SOCPSOLVESP LIB "gmbconic" ALIAS "ofx_socp_solve_sparse__nat" (n AS LONG, p AS LONG, E() AS DOUBLE, d() AS DOUBLE, c() AS DOUBLE, ncones AS LONG, ctype() AS LONG, cnm() AS LONG, cmem() AS LONG, G() AS DOUBLE, h() AS DOUBLE, tolGap AS DOUBLE, tolFeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, lam() AS DOUBLE) AS LONG

' ── SDP / conico unificato ─────────────────────────────────────────────────
' min c'x + <C,X> + <c_i,z_i>  s.t.  A x + <A_kj,X_j> + <a_ki,z_i> = b_k,
' x >= 0, X_j PSD, z_i in cono (SOC o esponenziale/potenza).
' Blocchi in array piatti, offset deducibili dalle dimensioni:
'   dims(j) = ordine d_j;  Cbar = matrici d_j x d_j consecutive;
'   Abar = per k, per j: matrice d_j x d_j (m * somma_j d_j^2)
'   socdims(i) = k_i;  Csoc = vettori k_i consecutivi;  Asoc = per k, per i
'   ekind(i) = 0 PEXP, 1 PPOW, 2 RPOW;  ealpha(i);  Cexp = 3 per blocco;
'   Aexp = per k, per i: 3 valori.
' Uscite con lo stesso layout: x (n), Xbar, y (m), Sbar, Zsoc, Ssoc, Zexp, Sexp;
' fb(0) = 1 se il solver ha salvato un punto near-optimal di fallback.
' Con problemi piccoli: tol=1e-8, rtol*=1e-7, nearRel=1000, maxIter>=300.
DECLARE NATIVE FUNCTION SDPSOLVE LIB "gmbconic" ALIAS "ofx_sdp_solve__nat" (m AS LONG, n AS LONG, A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, nb AS LONG, dims() AS LONG, Cbar() AS DOUBLE, Abar() AS DOUBLE, nsoc AS LONG, socdims() AS LONG, Csoc() AS DOUBLE, Asoc() AS DOUBLE, nep AS LONG, ekind() AS LONG, ealpha() AS DOUBLE, Cexp() AS DOUBLE, Aexp() AS DOUBLE, maxIter AS LONG, tol AS DOUBLE, rtolPri AS DOUBLE, rtolDual AS DOUBLE, rtolGap AS DOUBLE, nearRel AS DOUBLE, x() AS DOUBLE, Xbar() AS DOUBLE, y() AS DOUBLE, Sbar() AS DOUBLE, Zsoc() AS DOUBLE, Ssoc() AS DOUBLE, Zexp() AS DOUBLE, Sexp() AS DOUBLE, fb() AS LONG) AS LONG

' ── presolve LP (handle = stato) ───────────────────────────────────────────
' PRESOLVENEW riduce min c'x, A x = b, x >= 0 (A in CSC, 0-based) e ritorna un
' handle > 0 (o -1). Il ridotto si legge con PRESOLVEDIMS + PRESOLVEREDUCED.
DECLARE NATIVE FUNCTION PRESOLVENEW LIB "gmbconic" ALIAS "ofx_presolve_new__nat" (Aptr() AS LONG, Arow() AS LONG, Aval() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, m AS LONG, n AS LONG, tol AS DOUBLE) AS LONG
DECLARE FUNCTION PRESOLVECHANGED LIB "gmbconic" ALIAS "ofx_presolve_changed" (h AS LONG) AS LONG
' out(0)=mr, out(1)=nr, out(2)=nnz(Ar).
DECLARE NATIVE FUNCTION PRESOLVEDIMS LIB "gmbconic" ALIAS "ofx_presolve_dims__nat" (h AS LONG, out_() AS LONG) AS LONG
' Copia il ridotto in CSC: Arptr (nr+1), Arrow/Arval (nnz), br (mr), cr (nr).
' Ritorna nr (o -1). Dimensionare prima con PRESOLVEDIMS.
DECLARE NATIVE FUNCTION PRESOLVEREDUCED LIB "gmbconic" ALIAS "ofx_presolve_reduced__nat" (h AS LONG, Arptr() AS LONG, Arrow() AS LONG, Arval() AS DOUBLE, br() AS DOUBLE, cr() AS DOUBLE) AS LONG
' Riporta la soluzione ridotta (xred nr, yred mr) nello spazio originale.
DECLARE NATIVE FUNCTION PRESOLVEPOSTSOLVE LIB "gmbconic" ALIAS "ofx_presolve_postsolve__nat" (h AS LONG, xred() AS DOUBLE, yred() AS DOUBLE, xfull() AS DOUBLE, yfull() AS DOUBLE) AS LONG
' Come sopra, per una direzione (raggio di Farkas) omogenea.
DECLARE NATIVE FUNCTION PRESOLVEPOSTSOLVEDIR LIB "gmbconic" ALIAS "ofx_presolve_postsolve_dir__nat" (h AS LONG, rred() AS DOUBLE, yred() AS DOUBLE, rfull() AS DOUBLE, yfull() AS DOUBLE) AS LONG
DECLARE FUNCTION PRESOLVEFREE LIB "gmbconic" ALIAS "ofx_presolve_free" (h AS LONG) AS LONG

' ── forma standard (handle = stato) ────────────────────────────────────────
' STDFORMBUILD prende l'LP/QP utente (vincoli EQ/LO/UP/free, bounds, Q come
' triplette) e costruisce  min 1/2 xt'Qs xt + cs'xt  s.t. As xt = bs, xt >= 0.
' A e' in CSC per colonne (colptr nvar+1, sub/val per gli nnz); Q e' data come
' nq triplette simmetriche (qi,qj,qv) — nq=0 per un LP (gli array sono ignorati,
' passarne uno di 1 elemento). lx/ux/lc/uc: i lati liberi si indicano con
' -1E308 / +1E308 (il binding li converte in -INFINITY / +INFINITY).
' STDFORMGETA/GETB/GETC/GETQ materializzano il problema standard da dare
' a IPMLP/IPMQP; MAPX/MAPDIR/MAPY riportano la soluzione nello spazio originale.
DECLARE NATIVE FUNCTION STDFORMBUILD LIB "gmbconic" ALIAS "ofx_stdform_build__nat" (nvar AS LONG, ncon AS LONG, c_int() AS DOUBLE, qi() AS LONG, qj() AS LONG, qv() AS DOUBLE, nq AS LONG, lx() AS DOUBLE, ux() AS DOUBLE, lc() AS DOUBLE, uc() AS DOUBLE, colptr() AS LONG, srow() AS LONG, val() AS DOUBLE) AS LONG
' out(0)=m, out(1)=n, out(2)=nvar, out(3)=ncon, out(4)=cfix.
DECLARE NATIVE FUNCTION STDFORMINFO LIB "gmbconic" ALIAS "ofx_stdform_info__nat" (h AS LONG, out_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION STDFORMGETA LIB "gmbconic" ALIAS "ofx_stdform_get_a__nat" (h AS LONG, A_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION STDFORMGETB LIB "gmbconic" ALIAS "ofx_stdform_get_b__nat" (h AS LONG, b_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION STDFORMGETC LIB "gmbconic" ALIAS "ofx_stdform_get_c__nat" (h AS LONG, c_() AS DOUBLE) AS LONG
' Q densa n x n; ritorna -1 se il problema e' un LP (nessuna Q).
DECLARE NATIVE FUNCTION STDFORMGETQ LIB "gmbconic" ALIAS "ofx_stdform_get_q__nat" (h AS LONG, Q_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION STDFORMMAPX LIB "gmbconic" ALIAS "ofx_stdform_map_x__nat" (h AS LONG, xt() AS DOUBLE, x_() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION STDFORMMAPDIR LIB "gmbconic" ALIAS "ofx_stdform_map_dir__nat" (h AS LONG, dxt() AS DOUBLE, dxt_out() AS DOUBLE) AS LONG
DECLARE NATIVE FUNCTION STDFORMMAPY LIB "gmbconic" ALIAS "ofx_stdform_map_y__nat" (h AS LONG, ystd() AS DOUBLE, y_() AS DOUBLE) AS LONG
DECLARE FUNCTION STDFORMFREE LIB "gmbconic" ALIAS "ofx_stdform_free" (h AS LONG) AS LONG
