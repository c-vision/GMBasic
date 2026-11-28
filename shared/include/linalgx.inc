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
' Questa è la PRIMA ondata (scalari + solve/inversa). Decomposizioni (LU/QR/SVD/EIG…)
' restano in `LINALG` finché non si decide il modello di ritorno (oggi sono valori con
' campi MATRIX); quando LINALGX copre l'intera superficie, il namespace prende il nome
' `LINALG` e il builtin viene ritirato (stessa trafila di TIMESERIESX→TIMESERIES).

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
