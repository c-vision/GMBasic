' sparse.inc — namespace SPARSE: matrici sparse e solutori diretti (gmbsuite, FFI).
'
' Due sorgenti già compilate in libgmbmath:
'   gmbcompute/gc_sparse      contenitori COO/CSR/CSC, conversioni, trasposizione
'   gmbmath-linalg            fattorizzazioni sparse dirette (Cholesky, LU)
'
' Lo stato è un HANDLE LONG (> 0) e si libera con FREE. La tabella è statica (64 slot):
' i fattori non sono condivisi fra thread.
'
' Formati:
'   COO  triplette (riga, colonna, valore) in ordine di inserimento (COONEW + COOSET)
'   CSR  rowptr(righe+1), colidx(nnz), val(nnz)
'   CSC  colptr(colonne+1), rowidx(nnz), val(nnz)
'
' Le conversioni sommano i duplicati e ordinano gli indici. TRIPLET legge la k-esima
' entry nello stoccaggio (per CSR riga, per CSC colonna): serve a ispezionare.
'
' Solutori:
'   SPCHOLFACTOR vuole la matrice SIMMETRICA in CSC con il solo TRIANGOLO INFERIORE
'     (rowidx >= colonna); SPCHOLSOLVE risolve in place.
'   SPLUFACTOR vuole la matrice COMPLETA in CSC (anche non simmetrica) e usa il
'     pivoting parziale; SPLUSOLVE risolve in place.
'
' Ritorno: 0 = ok, -1 = handle/argomento non valido. I FACTOR ritornano un handle
' (> 0) oppure -1 (singolare / non definita positiva).

NAMESPACE SPARSE

' ── costruzione ─────────────────────────────────────────────────────────────
' Contenitore di triplette: rows x cols con al più nnz entry (da riempire con COOSET).
DECLARE FUNCTION COONEW LIB "gmbmath" ALIAS "ofx_sp_coo_new" (rows AS LONG, cols AS LONG, nnz AS LONG) AS LONG
' Riempie la tripletta k (0-based): riga, colonna, valore.
DECLARE FUNCTION COOSET LIB "gmbmath" ALIAS "ofx_sp_coo_set" (h AS LONG, k AS LONG, row AS LONG, col AS LONG, val AS DOUBLE) AS LONG
' Matrice densa row-major (a() = righe*colonne) -> CSR, scartando gli zeri.
DECLARE FUNCTION DENSETOCSR LIB "gmbmath" ALIAS "ofx_sp_dense_to_csr" (rows AS LONG, cols AS LONG, a() AS DOUBLE) AS LONG

' ── conversioni (ognuna crea un NUOVO handle: il sorgente resta valido) ─────
DECLARE FUNCTION COOTOCSR LIB "gmbmath" ALIAS "ofx_sp_coo_to_csr" (h AS LONG) AS LONG
DECLARE FUNCTION COOTOCSC LIB "gmbmath" ALIAS "ofx_sp_coo_to_csc" (h AS LONG) AS LONG
DECLARE FUNCTION CSRTOCSC LIB "gmbmath" ALIAS "ofx_sp_csr_to_csc" (h AS LONG) AS LONG
DECLARE FUNCTION CSCTOCSR LIB "gmbmath" ALIAS "ofx_sp_csc_to_csr" (h AS LONG) AS LONG
' Trasposta: da CSR si ottiene la CSC di A^T (e viceversa).
DECLARE FUNCTION CSRTRANSPOSE LIB "gmbmath" ALIAS "ofx_sp_csr_transpose" (h AS LONG) AS LONG
DECLARE FUNCTION CSCTRANSPOSE LIB "gmbmath" ALIAS "ofx_sp_csc_transpose" (h AS LONG) AS LONG

' ── ispezione ───────────────────────────────────────────────────────────────
' out_(0) = righe, out_(1) = colonne, out_(2) = non-zeri.
DECLARE FUNCTION SIZES LIB "gmbmath" ALIAS "ofx_sp_sizes" (h AS LONG, out_() AS LONG) AS LONG
' La k-esima entry nello stoccaggio: out_(0) = riga, out_(1) = colonna, out_(2) = valore.
DECLARE FUNCTION TRIPLET LIB "gmbmath" ALIAS "ofx_sp_triplet" (h AS LONG, k AS LONG, out_() AS DOUBLE) AS LONG

' ── solutori diretti ────────────────────────────────────────────────────────
' Cholesky sparsa: matrice simmetrica definita positiva in CSC, solo triangolo inferiore.
DECLARE FUNCTION SPCHOLFACTOR LIB "gmbmath" ALIAS "ofx_sp_chol_factor" (h_csc AS LONG) AS LONG
' Risolve K x = b in place sul fattore (b() diventa x). 0 = ok, -1 = errore.
DECLARE FUNCTION SPCHOLSOLVE LIB "gmbmath" ALIAS "ofx_sp_chol_solve" (hf AS LONG, b() AS DOUBLE) AS LONG
' LU sparsa con pivoting parziale: matrice completa in CSC.
DECLARE FUNCTION SPLUFACTOR LIB "gmbmath" ALIAS "ofx_sp_lu_factor" (h_csc AS LONG) AS LONG
' Risolve A x = b in place (b() diventa x). 0 = ok, -1 = singolare/errore.
DECLARE FUNCTION SPLUSOLVE LIB "gmbmath" ALIAS "ofx_sp_lu_solve" (hf AS LONG, b() AS DOUBLE) AS LONG

' ── liberazione (qualsiasi contenitore o fattore) ──────────────────────────
DECLARE FUNCTION FREE LIB "gmbmath" ALIAS "ofx_sp_free" (h AS LONG) AS LONG
