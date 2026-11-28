' linalg.inc — namespace LINALG: algebra lineare su ARRAY (gmbmath, FFI).
'
' LINALG è la superficie di algebra lineare su **matrici piatte row-major**
' (`a() AS DOUBLE`) + dimensioni esplicite, calcolata dalla dylib `gmbmath`: una
' libreria self-contained non può leggere il registry degli handle `MATRIX`, quindi
' le operazioni viaggiano su buffer DOUBLE e le 4 gambe usano gli STESSI kernel C
' (`gm_linalg_*`).
'
' Contratto:
'   - matrici e vettori sono array di DOUBLE; una matrice rows×cols è row-major
'     (`a(i*cols + j)`), quindi un `DIM a(rows-1, cols-1)` va passato direttamente;
'   - gli out_() sono dichiarati dal chiamante (n/n*n valori);
'   - le funzioni scalari rendono il valore (-1 su input non valido, come il builtin);
'   - le procedure rendono 0 = ok, -1 = errore (singolare, dimensioni, buffer corto).
'
' Superficie completa: scalari/norme, sistemi lineari (anche triangolari),
' decomposizioni (Cholesky/LU/QR/SVD/Hessenberg/Schur/pinv/orth/nullspace),
' autovalori/autovettori (EVD/EIG), funzioni matriciali (exp/log/sqrt/sin/cos/tan/
' sign/potenza frazionaria/polar) e angoli fra sottospazi.

NAMESPACE LINALG

' ── scalari da una matrice ──────────────────────────────────────────────────
' Determinante di una matrice quadrata n×n.
DECLARE FUNCTION DETERMINANT LIB "gmbmath" ALIAS "ofx_linalg_det" (a() AS DOUBLE, n AS LONG) AS DOUBLE
' Rango (via RREF) di una matrice rows×cols.
DECLARE FUNCTION RANK LIB "gmbmath" ALIAS "ofx_linalg_rank" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS LONG
' Norma 2 (massimo valore singolare).
DECLARE FUNCTION NORM LIB "gmbmath" ALIAS "ofx_linalg_norm" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS DOUBLE
' Numero di condizionamento 2-norma (smax/smin; infinito se singolare).
DECLARE FUNCTION COND LIB "gmbmath" ALIAS "ofx_linalg_cond" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS DOUBLE
' Norma nucleare (somma dei valori singolari).
DECLARE FUNCTION NUCLEARNORM LIB "gmbmath" ALIAS "ofx_linalg_nuclear" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS DOUBLE
' Norma di Frobenius.
DECLARE FUNCTION FROBENIUSNORM LIB "gmbmath" ALIAS "ofx_linalg_norm_fro" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS DOUBLE
' Norma 1 (massimo somma di colonna, valori assoluti).
DECLARE FUNCTION ONENORM LIB "gmbmath" ALIAS "ofx_linalg_norm_one" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS DOUBLE
' Norma infinito (massimo somma di riga, valori assoluti).
DECLARE FUNCTION INFINITYNORM LIB "gmbmath" ALIAS "ofx_linalg_norm_inf" (a() AS DOUBLE, rows AS LONG, cols AS LONG) AS DOUBLE

' ── sistemi lineari ─────────────────────────────────────────────────────────
' Risolve A x = b (A n×n): x ha n valori dichiarati dal chiamante.
DECLARE FUNCTION SOLVE LIB "gmbmath" ALIAS "ofx_linalg_solve" (a() AS DOUBLE, n AS LONG, b() AS DOUBLE, x() AS DOUBLE) AS LONG
' Inversa di A (n×n) in out_ (n*n valori).
DECLARE FUNCTION INVERSE LIB "gmbmath" ALIAS "ofx_linalg_inverse" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG

' ═══════════════════════════════════════════════════════════════════════════
' SECONDA ONDATA — decomposizioni e funzioni matriciali.
' Tutte rendono 0 = ok / -1 = errore; ORTH/NULLSPACE rendono il rango r (>=0).
' ═══════════════════════════════════════════════════════════════════════════

' A = L·Lᵀ (A simmetrica definita positiva); L n×n triangolare inferiore, zeri sopra.
DECLARE FUNCTION CHOLESKY LIB "gmbmath" ALIAS "ofx_linalg_cholesky" (a() AS DOUBLE, n AS LONG, l() AS DOUBLE) AS LONG
' A = P·L·U (convenzione scipy): P,L,U n×n. L unit lower, U upper.
DECLARE FUNCTION LU LIB "gmbmath" ALIAS "ofx_linalg_lu" (a() AS DOUBLE, n AS LONG, l() AS DOUBLE, u() AS DOUBLE, p() AS DOUBLE) AS LONG
' A = Q·R (rows>=cols): Q rows×cols, R cols×cols, diagonale di R positiva (MGS).
DECLARE FUNCTION QR LIB "gmbmath" ALIAS "ofx_linalg_qr" (a() AS DOUBLE, rows AS LONG, cols AS LONG, q() AS DOUBLE, r() AS DOUBLE) AS LONG
' A = U·diag(S)·Vᵀ (rows>=cols): U rows×cols, S cols (decrescenti), VT cols×cols.
DECLARE FUNCTION SVD LIB "gmbmath" ALIAS "ofx_linalg_svd" (a() AS DOUBLE, rows AS LONG, cols AS LONG, u() AS DOUBLE, s() AS DOUBLE, vt() AS DOUBLE) AS LONG
' A = Q·H·Qᵀ: H n×n in forma di Hessenberg superiore (Q non esposta).
DECLARE FUNCTION HESSENBERG LIB "gmbmath" ALIAS "ofx_linalg_hessenberg" (a() AS DOUBLE, n AS LONG, h() AS DOUBLE) AS LONG
' A = Z·T·Zᵀ (Schur reale): T,Z n×n; T quasi-triangolare superiore.
DECLARE FUNCTION SCHUR LIB "gmbmath" ALIAS "ofx_linalg_schur" (a() AS DOUBLE, n AS LONG, t() AS DOUBLE, z() AS DOUBLE) AS LONG
' Pseudoinversa di Moore-Penrose: out ha cols×rows valori (n×m).
DECLARE FUNCTION PINVERSE LIB "gmbmath" ALIAS "ofx_linalg_pinv" (a() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG
' Funzioni matriciali principali (A n×n): exp(A), log(A), radice quadrata.
DECLARE FUNCTION EXPM LIB "gmbmath" ALIAS "ofx_linalg_expm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION LOGM LIB "gmbmath" ALIAS "ofx_linalg_logm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION SQRTM LIB "gmbmath" ALIAS "ofx_linalg_sqrtm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
' A = U·P (polar): U ortogonale, P hermitiana semidefinita positiva; entrambe n×n.
DECLARE FUNCTION POLAR LIB "gmbmath" ALIAS "ofx_linalg_polar" (a() AS DOUBLE, n AS LONG, u() AS DOUBLE, p() AS DOUBLE) AS LONG
' Base ORTONORMALE dello spazio delle colonne (SVD): out rows×r; rende r.
DECLARE FUNCTION ORTH LIB "gmbmath" ALIAS "ofx_linalg_orth" (a() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG
' Base ortonormale del nucleo destro (SVD, rows>=cols): out cols×(cols-r); rende r.
DECLARE FUNCTION NULLSPACE LIB "gmbmath" ALIAS "ofx_linalg_nullspace" (a() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG

' ═══════════════════════════════════════════════════════════════════════════
' TERZA ONDATA — autovalori/autovettori, funzioni matriciali, sottospazi.
' ═══════════════════════════════════════════════════════════════════════════

' A = V·diag(d)·Vᵀ (A SIMMETRICA): d ha n autovalori ASCENDENTI, vectors n×n
' (colonna k = autovettore k). Rende 0 = ok / -1.
DECLARE FUNCTION EVD LIB "gmbmath" ALIAS "ofx_linalg_evd" (a() AS DOUBLE, n AS LONG, vals_() AS DOUBLE, vecs_() AS DOUBLE) AS LONG
' Autovalori generali + autovettori destri: wr/wi n valori, vr/vi n×n (colonna j =
' autovettore j). Autovettore reale per wi=0; per la coppia complessa il vettore è
' x+i·y. Rende 0 = ok / -1.
DECLARE FUNCTION EIG LIB "gmbmath" ALIAS "ofx_linalg_eig" (a() AS DOUBLE, n AS LONG, wr_() AS DOUBLE, wi_() AS DOUBLE, vr_() AS DOUBLE, vi_() AS DOUBLE) AS LONG
' f(A) = V·diag(f(d))·Vᵀ via EVD SIMMETRICA (stessa semantica del builtin LINALG).
DECLARE FUNCTION SINM LIB "gmbmath" ALIAS "ofx_linalg_sinm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION COSM LIB "gmbmath" ALIAS "ofx_linalg_cosm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION TANM LIB "gmbmath" ALIAS "ofx_linalg_tanm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
DECLARE FUNCTION SIGNM LIB "gmbmath" ALIAS "ofx_linalg_signm" (a() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
' A^p (potenza principale) via EVD simmetrica; out n×n.
DECLARE FUNCTION FRACTIONALPOWER LIB "gmbmath" ALIAS "ofx_linalg_fractionalpower" (a() AS DOUBLE, n AS LONG, p AS DOUBLE, out_() AS DOUBLE) AS LONG
' Angoli principali fra gli spazi delle COLONNE di a e b (stesse righe): out ha
' min(rA,rB) valori (dimensionare almeno min(cols_a, cols_b)); rende il numero o -1.
DECLARE FUNCTION SUBSPACEANGLES LIB "gmbmath" ALIAS "ofx_linalg_subspace_angles" (a() AS DOUBLE, rows_a AS LONG, cols_a AS LONG, b() AS DOUBLE, rows_b AS LONG, cols_b AS LONG, out_() AS DOUBLE) AS LONG
' Risolve A x = b con A triangolare n×n (lower<>0 = inferiore): x ha n valori.
DECLARE FUNCTION SOLVETRIANGULAR LIB "gmbmath" ALIAS "ofx_linalg_solve_triangular" (a() AS DOUBLE, n AS LONG, b() AS DOUBLE, lower AS LONG, x() AS DOUBLE) AS LONG

' ═══════════════════════════════════════════════════════════════════════════
' QUARTA ONDATA — delta gmbmath: traccia, spazi, minimi quadrati, tridiagonali,
' LDLᵀ, Gauss-Jordan, solver iterativi, power iteration.
' ═══════════════════════════════════════════════════════════════════════════

' Somma della diagonale di A (n×n).
DECLARE FUNCTION TRACE LIB "gmbmath" ALIAS "ofx_linalg_trace" (a() AS DOUBLE, n AS LONG) AS DOUBLE
' Forma ridotta a scala (RREF): out rows×cols, rende il rango.
DECLARE FUNCTION RREF LIB "gmbmath" ALIAS "ofx_linalg_rref" (a() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG
' Base dello spazio delle colonne: out rows×cols (usa rows×r), rende il rango r.
DECLARE FUNCTION COLSPACE LIB "gmbmath" ALIAS "ofx_linalg_colspace" (a() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG
' Base dello spazio delle righe: out rows×cols (usa r×cols), rende il rango r.
DECLARE FUNCTION ROWSPACE LIB "gmbmath" ALIAS "ofx_linalg_rowspace" (a() AS DOUBLE, rows AS LONG, cols AS LONG, out_() AS DOUBLE) AS LONG
' Minimi quadrati A x = b (rows>=cols): x ha cols valori.
DECLARE FUNCTION LSTSQ LIB "gmbmath" ALIAS "ofx_linalg_lstsq" (a() AS DOUBLE, rows AS LONG, cols AS LONG, b() AS DOUBLE, x() AS DOUBLE) AS LONG
' Riduzione simmetrica tridiagonale: d,e hanno n valori.
DECLARE FUNCTION SYMTRIDIAG LIB "gmbmath" ALIAS "ofx_linalg_symtridiag" (a() AS DOUBLE, n AS LONG, d() AS DOUBLE, e() AS DOUBLE) AS LONG
' Autovalori di una tridiagonale simmetrica (d,e non modificati): eval n valori.
DECLARE FUNCTION TRIDIAGEIG LIB "gmbmath" ALIAS "ofx_linalg_tridiageig" (d() AS DOUBLE, e() AS DOUBLE, n AS LONG, eval_() AS DOUBLE) AS LONG
' Sistema tridiagonale (lower, diag, upper, rhs di n valori): x n valori.
DECLARE FUNCTION THOMAS LIB "gmbmath" ALIAS "ofx_linalg_thomas" (lower() AS DOUBLE, diag() AS DOUBLE, upper() AS DOUBLE, rhs() AS DOUBLE, n AS LONG, x() AS DOUBLE) AS LONG
' A = L·diag(d)·Lᵀ (simmetrica): l n×n unit-lower, d n valori.
DECLARE FUNCTION LDLT LIB "gmbmath" ALIAS "ofx_linalg_ldlt" (a() AS DOUBLE, n AS LONG, l() AS DOUBLE, d() AS DOUBLE) AS LONG
' Risolve L·diag(d)·Lᵀ x = b.
DECLARE FUNCTION LDLTSOLVE LIB "gmbmath" ALIAS "ofx_linalg_ldltsolve" (l() AS DOUBLE, d() AS DOUBLE, b() AS DOUBLE, n AS LONG, x() AS DOUBLE) AS LONG
' Eliminazione di Gauss-Jordan: risolve A x = b (A non modificato).
DECLARE FUNCTION GAUSSJORDAN LIB "gmbmath" ALIAS "ofx_linalg_gaussjordan" (a() AS DOUBLE, n AS LONG, b() AS DOUBLE, x() AS DOUBLE) AS LONG
' Omega ottimale per SOR (stima del raggio spettrale di Jacobi).
DECLARE FUNCTION OPTIMALOMEGA LIB "gmbmath" ALIAS "ofx_linalg_optimalomega" (a() AS DOUBLE, n AS LONG) AS DOUBLE
' Gradiente coniugato (A simmetrica definita positiva): rende le iterazioni o -1.
DECLARE FUNCTION CG LIB "gmbmath" ALIAS "ofx_linalg_cg" (a() AS DOUBLE, n AS LONG, b() AS DOUBLE, x() AS DOUBLE, maxit AS LONG, tol AS DOUBLE) AS LONG
' GMRES: rende le iterazioni o -1.
DECLARE FUNCTION GMRES LIB "gmbmath" ALIAS "ofx_linalg_gmres" (a() AS DOUBLE, n AS LONG, b() AS DOUBLE, x() AS DOUBLE, restart AS LONG, maxit AS LONG, tol AS DOUBLE) AS LONG
' Autovalore dominante e autovettore (power iteration).
DECLARE FUNCTION POWER LIB "gmbmath" ALIAS "ofx_linalg_power" (a() AS DOUBLE, n AS LONG, iters AS LONG, tol AS DOUBLE, lambda_() AS DOUBLE, v() AS DOUBLE) AS LONG

' Accessore FFI dei fattori: calcola la decomposizione di a (rows×cols) e scrive il
' fattore `kind` in out_ (meta(0..1) = righe/colonne del fattore). Kind: LU.L/U/P,
' QR.Q/R, SVD.U/S/VT, CHOL.L, EVD.D/V, SCHUR.T/Z, POLAR.U/P, EIG.DREAL/DIMAG/VREAL/VIMAG.
DECLARE FUNCTION FACTOR LIB "gmbmath" ALIAS "ofx_linalg_factor" (a() AS DOUBLE, rows AS LONG, cols AS LONG, kind AS STRING, out_() AS DOUBLE, meta() AS LONG) AS LONG
