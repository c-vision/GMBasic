' linalgx.inc — namespace LINALGX: algebra lineare su ARRAY (gmbmath, FFI).
'
' Perché esiste: il namespace builtin `LINALG` lavora su HANDLE di MATRIX, che una
' dylib FFI self-contained non può leggere. LINALGX offre le stesse operazioni su
' **matrici piatte row-major** (`a() AS DOUBLE`) + dimensioni esplicite: è la forma
' che sopravvive al passaggio a FFI e che le 4 gambe calcolano con gli STESSI kernel C
' (`gm_linalg_*`).
'
' Contratto:
'   - matrici e vettori sono array di DOUBLE; una matrice rows×cols è row-major
'     (`a(i*cols + j)`), quindi un `DIM a(rows-1, cols-1)` va passato direttamente;
'   - gli out_() sono dichiarati dal chiamante (n/n*n valori);
'   - le funzioni scalari rendono il valore (-1 su input non valido, come il builtin);
'   - le procedure rendono 0 = ok, -1 = errore (singolare, dimensioni, buffer corto).
'
' Questa è la prima ondata (scalari + solve/inversa) e la seconda (decomposizioni e
' funzioni matriciali con uscite piatte): LU/QR/SVD/Cholesky/Hessenberg/Schur,
' PINVERSE, EXPM/LOGM/SQRTM, POLAR, ORTH, NULLSPACE. Restano in `LINALG` builtin le
' funzioni con ritorno composto non ancora coperte (EIG/EVD, SINM/COSM/TANM/SIGNM,
' FRACTIONALPOWER, SUBSPACEANGLES): quando LINALGX copre l'intera superficie, il
' namespace prende il nome `LINALG` e il builtin viene ritirato (stessa trafila di
' TIMESERIESX→TIMESERIES).

NAMESPACE LINALGX

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
