/*
 * linalg_ffi.c — shim ABI per il namespace BASIC LINALG (algebra lineare su ARRAY).
 *
 * PERCHÉ. Il namespace builtin `LINALG` lavora su HANDLE di MATRIX (`gmb_matrix_create`
 * → un indice nella tabella globale del runtime): una dylib FFI self-contained non può
 * leggere quella tabella, e le gambe managed non la condividono. Qui la stessa algebra
 * viaggia su **buffer piatti row-major** + dimensioni esplicite, il contratto già usato da
 * VEC/MULTIVARIATE/TIMESERIES.
 *
 * ABI FFI (importante): per un parametro array 1-D il motore passa il puntatore seguito
 * dalla LUNGHEZZA (int64), come in `ofx_vec_*`; un array a rango >1 passa UNA lunghezza
 * PER DIMENSIONE (contratto FFI esistente, vedi ffi/ffi_dims_*). Le funzioni di questo
 * shim prendono matrici come **buffer 1-D piatto row-major** + righe/colonne esplicite:
 * una matrice DIM a(r,c) non va passata qui (arriverebbe con due lunghezze).
 * Il DECLARE BASIC non scrive la lunghezza (la aggiunge il motore).
 *
 * Una sola implementazione (§0bis): i kernel sono `gm_linalg_*` di gmbmath (la stessa
 * libreria in cui vive questo shim).
 *
 * Convenzioni:
 *   - le matrici sono `double*` row-major, `rows*cols` valori;
 *   - i vettori/out sono dichiarati dal chiamante (n / n*n valori);
 *   - ritorno: le funzioni scalari rendono il valore (-1 su input non valido, come il
 *     builtin), le procedure rendono 0 = ok / -1 = errore.
 */
#include "gm_linalg.h"
#include "gm_decomp2.h"
#include "gm_matfun.h"
#include "gm_eig.h"
#include "gm_sparse_linalg.h"
#include "gm_iter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

/* Valori singolari di una matrice rows×cols: ne scrive `min(rows,cols)` in `s`. Ritorna il
 * numero di valori o -1. */
static int singular_values(i64 rows, i64 cols, const double *a, double *s)
{
    if (!a || !s || rows <= 0 || cols <= 0) return -1;
    i64 m = rows, n = cols;
    const double *src = a;
    double *tmp = NULL;
    if (m < n)
    {
        /* gm_linalg_svd vuole m>=n: si traspone (i valori singolari non cambiano). */
        tmp = (double *)malloc((size_t)(m * n) * sizeof(double));
        if (!tmp) return -1;
        for (i64 i = 0; i < m; i++)
            for (i64 j = 0; j < n; j++)
                tmp[j * m + i] = a[i * n + j];
        i64 t = m; m = n; n = t;
        src = tmp;
    }
    double *U = (double *)malloc((size_t)(m * n) * sizeof(double));
    double *Vt = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *S = (double *)malloc((size_t)n * sizeof(double));
    int rc = -1;
    if (U && Vt && S && gm_linalg_svd((int)m, (int)n, src, U, S, Vt) == GM_OK)
    {
        memcpy(s, S, (size_t)n * sizeof(double));
        rc = (int)n;
    }
    free(U); free(Vt); free(S); free(tmp);
    return rc;
}

/* `a_n` è la lunghezza che l'ABI passa per l'array: su C99 una matrice 2-D arriva con la
 * lunghezza dell'ULTIMA dimensione (non rows*cols), quindi non è una prova di capacità —
 * la dimensione vera la dichiara il chiamante con `n`/`rows`/`cols` (contratto identico al
 * kernel C). Resta il controllo di non-null e di dimensione sensata. */
static int square_ok(const double *a, i64 a_n, i64 n)
{
    (void)a_n;
    return a && n > 0 && n <= 100000;
}

/* DETERMINANT(a, n) — determinante di una matrice quadrata n×n. */
double ofx_linalg_det(const double *a, i64 a_n, i64 n)
{
    if (!square_ok(a, a_n, n)) return -1;
    return (double)gm_linalg_det((int)n, a);
}

/* RANK(a, rows, cols) — rango via RREF. */
i64 ofx_linalg_rank(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    return (i64)gm_linalg_rank((int)rows, (int)cols, a);
}

/* SOLVE(a, n, b, x) — risolve A x = b. x ha n valori; 0 = ok. */
i64 ofx_linalg_solve(const double *a, i64 a_n, i64 n, const double *b, i64 b_n, double *x, i64 x_n)
{
    if (!square_ok(a, a_n, n) || !b || !x || b_n < n || x_n < n) return -1;
    return gm_linalg_solve((int)n, a, b, x) == GM_OK ? 0 : -1;
}

/* INVERSE(a, n, out) — inversa di A (n×n) in out (n*n valori); 0 = ok, -1 = singolare. */
i64 ofx_linalg_inverse(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    if (!square_ok(a, a_n, n) || !out || out_n < n * n) return -1;
    return gm_linalg_inverse((int)n, a, out) == GM_OK ? 0 : -1;
}

/* NORM(a, rows, cols) — norma 2 = massimo valore singolare (scipy.linalg.norm). */
double ofx_linalg_norm(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    i64 k = rows < cols ? rows : cols;
    double *s = (double *)malloc((size_t)k * sizeof(double));
    if (!s) return -1;
    int n = singular_values(rows, cols, a, s);
    double mx = -1;
    if (n > 0)
    {
        mx = s[0];
        for (int i = 1; i < n; i++) if (s[i] > mx) mx = s[i];
    }
    free(s);
    return mx;
}

/* COND(a, rows, cols) — numero di condizionamento 2-norma = smax/smin (inf se singolare). */
double ofx_linalg_cond(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    i64 k = rows < cols ? rows : cols;
    double *s = (double *)malloc((size_t)k * sizeof(double));
    if (!s) return -1;
    int n = singular_values(rows, cols, a, s);
    double rc = -1;
    if (n > 0)
    {
        double smax = s[0], smin = s[0];
        for (int i = 1; i < n; i++) { if (s[i] > smax) smax = s[i]; if (s[i] < smin) smin = s[i]; }
        rc = (smin < 1e-300) ? INFINITY : smax / smin;
    }
    free(s);
    return rc;
}

/* NUCLEARNORM(a, rows, cols) — somma dei valori singolari (scipy ord='nuc'). */
double ofx_linalg_nuclear(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    i64 k = rows < cols ? rows : cols;
    double *s = (double *)malloc((size_t)k * sizeof(double));
    if (!s) return -1;
    int n = singular_values(rows, cols, a, s);
    double sum = 0;
    for (int i = 0; i < n; i++) sum += s[i];
    free(s);
    return n > 0 ? sum : -1;
}

/* FROBENIUSNORM(a, rows, cols) — sqrt(sum a_ij^2). */
double ofx_linalg_norm_fro(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    return (double)gm_linalg_norm_frobenius((int)rows, (int)cols, a);
}

/* ONENORM(a, rows, cols) — norma 1 = massimo somma di colonna in valore assoluto. */
double ofx_linalg_norm_one(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    double best = 0;
    for (i64 j = 0; j < cols; j++)
    {
        double sum = 0;
        for (i64 i = 0; i < rows; i++) sum += fabs(a[i * cols + j]);
        if (sum > best) best = sum;
    }
    return best;
}

/* INFINITYNORM(a, rows, cols) — norma infinito = massimo somma di riga in valore assoluto. */
double ofx_linalg_norm_inf(const double *a, i64 a_n, i64 rows, i64 cols)
{
    (void)a_n;
    if (!a || rows <= 0 || cols <= 0) return -1;
    double best = 0;
    for (i64 i = 0; i < rows; i++)
    {
        double sum = 0;
        for (i64 j = 0; j < cols; j++) sum += fabs(a[i * cols + j]);
        if (sum > best) best = sum;
    }
    return best;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SECONDA ONDATA — decomposizioni e funzioni matriciali (out() piatti).
 * Convenzioni (scipy-like, documentate in linalgx.inc):
 *   A = P·L·U (LU), A = Q·R (QR, m>=n), A = U·diag(S)·Vᵀ (SVD, m>=n),
 *   A = L·Lᵀ (Cholesky), A = Q·H·Qᵀ (Hessenberg), A = Z·T·Zᵀ (Schur),
 *   A = Uu·P (polar), A⁺ = pseudoinversa n×m.
 * Tutte rendono 0 = ok / -1 = errore; le funzioni con dimensione di uscita
 * variabile (ORTH/NULLSPACE) rendono il rango r (>=0) o -1.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* CHOLESKY(a, n, L) — A = L·Lᵀ; L triangolare inferiore, zeri sopra. */
i64 ofx_linalg_cholesky(const double *a, i64 a_n, i64 n, double *l, i64 l_n)
{
    (void)a_n;
    if (!a || !l || n <= 0 || n > 100000 || l_n < n * n) return -1;
    memcpy(l, a, (size_t)(n * n) * sizeof(double));
    if (gm_linalg_cholesky((int)n, l) != GM_OK) return -1;
    for (i64 i = 0; i < n; i++)
        for (i64 j = i + 1; j < n; j++) l[i * n + j] = 0.0;
    return 0;
}

/* LU(a, n, L, U, P) — A = P·L·U (P matrice di permutazione, convenzione scipy). */
i64 ofx_linalg_lu(const double *a, i64 a_n, i64 n, double *L, i64 L_n,
                  double *U, i64 U_n, double *P, i64 P_n)
{
    (void)a_n;
    if (!a || !L || !U || !P || n <= 0 || n > 100000
        || L_n < n * n || U_n < n * n || P_n < n * n) return -1;
    double *w = (double *)malloc((size_t)(n * n) * sizeof(double));
    int *piv = (int *)malloc((size_t)n * sizeof(int));
    if (!w || !piv) { free(w); free(piv); return -1; }
    memcpy(w, a, (size_t)(n * n) * sizeof(double));
    if (gm_linalg_lu((int)n, w, piv) != GM_OK) { free(w); free(piv); return -1; }
    for (i64 i = 0; i < n; i++)
        for (i64 j = 0; j < n; j++)
        {
            if (j < i)      { L[i * n + j] = w[i * n + j]; U[i * n + j] = 0.0; }
            else if (j == i){ L[i * n + j] = 1.0;          U[i * n + j] = w[i * n + j]; }
            else            { L[i * n + j] = 0.0;          U[i * n + j] = w[i * n + j]; }
        }
    /* P_lapack·A = L·U: applica gli stessi scambi di riga a I, poi traspone
     * (A = P·L·U con P = P_lapackᵀ, convenzione scipy). */
    for (i64 i = 0; i < n; i++)
        for (i64 j = 0; j < n; j++) P[i * n + j] = (i == j) ? 1.0 : 0.0;
    for (i64 k = 0; k < n; k++)
    {
        int p = piv[k];
        if (p != (int)k)
            for (i64 j = 0; j < n; j++)
            { double t = P[k * n + j]; P[k * n + j] = P[p * n + j]; P[p * n + j] = t; }
    }
    for (i64 i = 0; i < n; i++)
        for (i64 j = i + 1; j < n; j++)
        { double t = P[i * n + j]; P[i * n + j] = P[j * n + i]; P[j * n + i] = t; }
    free(w); free(piv);
    return 0;
}

/* QR(a, rows, cols, Q, R) — A = Q·R, rows>=cols; Q rows×cols, R cols×cols
 * (Gram-Schmidt modificato: diagonale di R positiva). */
i64 ofx_linalg_qr(const double *a, i64 a_n, i64 rows, i64 cols,
                  double *Q, i64 Q_n, double *R, i64 R_n)
{
    (void)a_n;
    if (!a || !Q || !R || rows <= 0 || cols <= 0 || rows < cols) return -1;
    if (Q_n < rows * cols || R_n < cols * cols) return -1;
    return gm_linalg_qr_mgs((int)rows, (int)cols, a, Q, R) == GM_OK ? 0 : -1;
}

/* SVD(a, rows, cols, U, S, VT) — A = U·diag(S)·Vᵀ, rows>=cols;
 * U rows×cols, S cols, VT cols×cols (S decrescenti). */
i64 ofx_linalg_svd(const double *a, i64 a_n, i64 rows, i64 cols,
                   double *U, i64 U_n, double *S, i64 S_n,
                   double *VT, i64 VT_n)
{
    (void)a_n;
    if (!a || !U || !S || !VT || rows <= 0 || cols <= 0 || rows < cols) return -1;
    if (U_n < rows * cols || S_n < cols || VT_n < cols * cols) return -1;
    return gm_linalg_svd((int)rows, (int)cols, a, U, S, VT) == GM_OK ? 0 : -1;
}

/* HESSENBERG(a, n, H) — A = Q·H·Qᵀ, H Hessenberg superiore (Q non esposta). */
i64 ofx_linalg_hessenberg(const double *a, i64 a_n, i64 n, double *H, i64 H_n)
{
    (void)a_n;
    if (!a || !H || n <= 0 || n > 100000 || H_n < n * n) return -1;
    double *Q = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!Q) return -1;
    int rc = gm_linalg_hessenberg((int)n, a, H, Q) == GM_OK ? 0 : -1;
    free(Q);
    return rc;
}

/* SCHUR(a, n, T, Z) — A = Z·T·Zᵀ (Schur reale); T quasi-triangolare superiore. */
i64 ofx_linalg_schur(const double *a, i64 a_n, i64 n,
                     double *T, i64 T_n, double *Z, i64 Z_n)
{
    (void)a_n;
    if (!a || !T || !Z || n <= 0 || n > 100000 || T_n < n * n || Z_n < n * n) return -1;
    return gm_linalg_schur((int)n, a, T, Z) == GM_OK ? 0 : -1;
}

/* PINVERSE(a, rows, cols, out) — pseudoinversa di Moore-Penrose, out n×m. */
i64 ofx_linalg_pinv(const double *a, i64 a_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || rows <= 0 || cols <= 0 || out_n < rows * cols) return -1;
    return gm_mat_pinv((int)rows, (int)cols, a, out) == GM_OK ? 0 : -1;
}

/* EXPM/LOGM/SQRTM(a, n, out) — funzioni matriciali principali. */
i64 ofx_linalg_expm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || n <= 0 || n > 100000 || out_n < n * n) return -1;
    return gm_mat_exp((int)n, a, out) == GM_OK ? 0 : -1;
}
i64 ofx_linalg_logm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || n <= 0 || n > 100000 || out_n < n * n) return -1;
    return gm_mat_log((int)n, a, out) == GM_OK ? 0 : -1;
}
i64 ofx_linalg_sqrtm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || n <= 0 || n > 100000 || out_n < n * n) return -1;
    return gm_mat_sqrt((int)n, a, out) == GM_OK ? 0 : -1;
}

/* POLAR(a, n, U, P) — A = U·P, U ortogonale, P hermitiana semidefinita positiva.
 * Calcolata dalla SVD: Uu = U·Vᵀ, P = V·diag(S)·Vᵀ. */
i64 ofx_linalg_polar(const double *a, i64 a_n, i64 n,
                     double *Uout, i64 U_n, double *Pout, i64 P_n)
{
    (void)a_n;
    if (!a || !Uout || !Pout || n <= 0 || n > 100000
        || U_n < n * n || P_n < n * n) return -1;
    i64 nn = n * n;
    double *U = (double *)malloc((size_t)nn * sizeof(double));
    double *S = (double *)malloc((size_t)n * sizeof(double));
    double *VT = (double *)malloc((size_t)nn * sizeof(double));
    double *V = (double *)malloc((size_t)nn * sizeof(double));
    if (!U || !S || !VT || !V) { free(U); free(S); free(VT); free(V); return -1; }
    int rc = -1;
    if (gm_linalg_svd((int)n, (int)n, a, U, S, VT) == GM_OK)
    {
        /* Uu = U·Vᵀ */
        for (i64 i = 0; i < n; i++)
            for (i64 j = 0; j < n; j++)
            {
                double s = 0;
                for (i64 k = 0; k < n; k++) s += U[i * n + k] * VT[k * n + j];
                Uout[i * n + j] = s;   /* VT = Vᵀ, quindi U·Vᵀ è diretto */
            }
        /* V = VTᵀ */
        for (i64 i = 0; i < n; i++)
            for (i64 j = 0; j < n; j++) V[i * n + j] = VT[j * n + i];
        /* P = V·diag(S)·Vᵀ */
        for (i64 i = 0; i < n; i++)
            for (i64 j = 0; j < n; j++)
            {
                double s = 0;
                for (i64 k = 0; k < n; k++) s += V[i * n + k] * S[k] * VT[k * n + j];
                Pout[i * n + j] = s;
            }
        rc = 0;
    }
    free(U); free(S); free(VT); free(V);
    return rc;
}

/* ORTH(a, rows, cols, out) — base ORTONORMALE dello spazio delle colonne (SVD).
 * out ha rows×r valori row-major; rende r (rango numerico) o -1. */
i64 ofx_linalg_orth(const double *a, i64 a_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || rows <= 0 || cols <= 0 || rows < cols) return -1;
    i64 nn = rows * cols, nc = cols * cols;
    double *U = (double *)malloc((size_t)nn * sizeof(double));
    double *S = (double *)malloc((size_t)cols * sizeof(double));
    double *VT = (double *)malloc((size_t)nc * sizeof(double));
    if (!U || !S || !VT) { free(U); free(S); free(VT); return -1; }
    i64 r = -1;
    if (gm_linalg_svd((int)rows, (int)cols, a, U, S, VT) == GM_OK)
    {
        double smax = cols > 0 ? S[0] : 0.0;
        for (i64 i = 1; i < cols; i++) if (S[i] > smax) smax = S[i];
        double tol = ((double)(rows > cols ? rows : cols)) * 1e-12 * (smax > 0 ? smax : 1.0);
        i64 k = 0;
        while (k < cols && S[k] > tol) k++;
        if (out_n >= rows * k)
        {
            for (i64 i = 0; i < rows; i++)
                for (i64 j = 0; j < k; j++) out[i * k + j] = U[i * cols + j];
            r = k;
        }
    }
    free(U); free(S); free(VT);
    return r;
}

/* NULLSPACE(a, rows, cols, out) — base ortonormale del nucleo destro (SVD).
 * rows>=cols; out ha cols×(cols-r) valori; rende r (rango) o -1. */
i64 ofx_linalg_nullspace(const double *a, i64 a_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || rows <= 0 || cols <= 0 || rows < cols) return -1;
    i64 nn = rows * cols, nc = cols * cols;
    double *U = (double *)malloc((size_t)nn * sizeof(double));
    double *S = (double *)malloc((size_t)cols * sizeof(double));
    double *VT = (double *)malloc((size_t)nc * sizeof(double));
    if (!U || !S || !VT) { free(U); free(S); free(VT); return -1; }
    i64 r = -1;
    if (gm_linalg_svd((int)rows, (int)cols, a, U, S, VT) == GM_OK)
    {
        double smax = cols > 0 ? S[0] : 0.0;
        for (i64 i = 1; i < cols; i++) if (S[i] > smax) smax = S[i];
        double tol = ((double)(rows > cols ? rows : cols)) * 1e-12 * (smax > 0 ? smax : 1.0);
        i64 k = 0;
        while (k < cols && S[k] > tol) k++;
        i64 d = cols - k;   /* dimensione del nucleo */
        if (out_n >= cols * d)
        {
            /* Le righe k..cols-1 di VT sono i vettori del nucleo: out(:,j) = VT(k+j,:)ᵀ */
            for (i64 i = 0; i < cols; i++)
                for (i64 j = 0; j < d; j++) out[i * d + j] = VT[(k + j) * cols + i];
            r = k;
        }
    }
    free(U); free(S); free(VT);
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TERZA ONDATA — autovalori/autovettori, funzioni matriciali eigenwise,
 * angoli fra sottospazi. Convenzioni:
 *   EVD  simmetrica (Jacobi): A = V·diag(d)·Vᵀ, d ascendenti, V colonne.
 *   EIG  generale: wr/wi (n) e vr/vi (n×n, colonna j = autovettore j);
 *        il vettore di una coppia complessa è null(vettore destro reale).
 *   COSM/SINM/TANM/SIGNM/FRACTIONALPOWER: f(A) = V·diag(f(d))·Vᵀ via EVD
 *        SIMMETRICA (stessa semantica del builtin LINALG, che usa Jacobi).
 *   SUBSPACEANGLES: angoli principali fra gli spazi delle colonne (SVD delle
 *        basi ortonormali), resi in out_ (min(rA,rB) valori), rende il numero.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Ordina per autovalore crescente le coppie (valore, colonna di V). */
static void eig_pairs_sort_asc(i64 n, double *val, double *vec)
{
    for (i64 i = 1; i < n; i++)
        for (i64 j = i; j > 0 && val[j - 1] > val[j]; j--)
        {
            double t = val[j - 1]; val[j - 1] = val[j]; val[j] = t;
            for (i64 k = 0; k < n; k++)
            {
                double v = vec[k * n + (j - 1)];
                vec[k * n + (j - 1)] = vec[k * n + j];
                vec[k * n + j] = v;
            }
        }
}

/* EVD(a, n, vals, vecs) — A = V·diag(d)·Vᵀ (A simmetrica). d ascendenti. */
i64 ofx_linalg_evd(const double *a, i64 a_n, i64 n,
                   double *vals, i64 vals_n, double *vecs, i64 vecs_n)
{
    (void)a_n;
    if (!a || !vals || !vecs || n <= 0 || n > 100000
        || vals_n < n || vecs_n < n * n) return -1;
    dmat_eig_jacobi((int)n, a, vals, vecs);
    eig_pairs_sort_asc(n, vals, vecs);
    return 0;
}

/* Ultimo vettore singolare destro (colonna di V) di una matrice n×n. */
static int right_null_vector(i64 n, const double *m, double *out)
{
    double *U = (double *)malloc((size_t)(n * n) * sizeof(double));
    double *S = (double *)malloc((size_t)n * sizeof(double));
    double *VT = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!U || !S || !VT) { free(U); free(S); free(VT); return -1; }
    int rc = -1;
    if (gm_linalg_svd((int)n, (int)n, m, U, S, VT) == GM_OK)
    {
        for (i64 i = 0; i < n; i++) out[i] = VT[(n - 1) * n + i];
        rc = 0;
    }
    free(U); free(S); free(VT);
    return rc;
}

/* EIG(a, n, wr, wi, vr, vi) — autovalori generali + autovettori destri.
 * Per ogni autovalore il vettore è il null vector destro di (A - λI): reale
 * per λ reale, x+i·y dal sistema reale [[A-aI,-bI],[bI,A-aI]] per λ=a+bi. */
i64 ofx_linalg_eig(const double *a, i64 a_n, i64 n,
                   double *wr, i64 wr_n, double *wi, i64 wi_n,
                   double *vr, i64 vr_n, double *vi, i64 vi_n)
{
    (void)a_n;
    if (!a || !wr || !wi || !vr || !vi || n <= 0 || n > 100000) return -1;
    if (wr_n < n || wi_n < n || vr_n < n * n || vi_n < n * n) return -1;
    if (gm_eig_qr((int)n, a, wr, wi) != GM_OK) return -1;
    double scale = 0.0;
    for (i64 i = 0; i < n; i++) { double v = fabs(wr[i]); if (v > scale) scale = v; }
    if (scale <= 0.0) scale = 1.0;
    double wmax = 0.0;
    for (i64 i = 0; i < n; i++) { double v = fabs(wi[i]); if (v > wmax) wmax = v; }
    double imag_tol = 1e-12 * (scale > wmax ? scale : wmax);
    for (i64 j = 0; j < n; j++)
    {
        if (fabs(wi[j]) <= imag_tol)
        {
            double *M = (double *)malloc((size_t)(n * n) * sizeof(double));
            double *z = (double *)malloc((size_t)n * sizeof(double));
            if (!M || !z) { free(M); free(z); return -1; }
            for (i64 i = 0; i < n; i++)
                for (i64 k = 0; k < n; k++)
                    M[i * n + k] = a[i * n + k] - (i == k ? wr[j] : 0.0);
            int rc = right_null_vector(n, M, z);
            free(M);
            if (rc != 0) { free(z); return -1; }
            /* Normalizza: componente a modulo massimo = 1 (base deterministica). */
            i64 best = 0;
            for (i64 i = 1; i < n; i++)
                if (fabs(z[i]) > fabs(z[best])) best = i;
            double norm = z[best];
            for (i64 i = 0; i < n; i++)
            {
                vr[i * n + j] = norm != 0.0 ? z[i] / norm : z[i];
                vi[i * n + j] = 0.0;
            }
            free(z);
        }
        else
        {
            i64 n2 = 2 * n;
            double *M = (double *)malloc((size_t)(n2 * n2) * sizeof(double));
            double *z = (double *)malloc((size_t)n2 * sizeof(double));
            if (!M || !z) { free(M); free(z); return -1; }
            double ar = wr[j], bi = wi[j];
            for (i64 i = 0; i < n2; i++)
                for (i64 k = 0; k < n2; k++) M[i * n2 + k] = 0.0;
            for (i64 i = 0; i < n; i++)
                for (i64 k = 0; k < n; k++)
                {
                    double v = a[i * n + k] - (i == k ? ar : 0.0);
                    M[i * n2 + k]           = v;          /* A - aI      */
                    M[(n + i) * n2 + (n + k)] = v;        /* A - aI      */
                }
            for (i64 i = 0; i < n; i++)
            {
                M[i * n2 + (n + i)]           = -bi;      /* -bI */
                M[(n + i) * n2 + i]           =  bi;      /* +bI */
            }
            int rc = right_null_vector(n2, M, z);
            free(M);
            if (rc != 0) { free(z); return -1; }
            /* Normalizza: componente a modulo massimo = 1 (base deterministica). */
            i64 best = 0;
            for (i64 i = 1; i < n2; i++)
                if (fabs(z[i]) > fabs(z[best])) best = i;
            double norm = z[best];
            if (norm != 0.0)
                for (i64 i = 0; i < n2; i++) z[i] /= norm;
            for (i64 i = 0; i < n; i++)
            {
                vr[i * n + j] = z[i];
                vi[i * n + j] = z[n + i];
            }
            free(z);
        }
    }
    return 0;
}

/* Funzione matriciale simmetrica: out = V·diag(f(d))·Vᵀ. */
static i64 matfun_sym(const double *a, i64 n, double (*f)(double), double *out)
{
    if (!a || !out || n <= 0 || n > 100000) return -1;
    i64 nn = n * n;
    double *d = (double *)malloc((size_t)n * sizeof(double));
    double *V = (double *)malloc((size_t)nn * sizeof(double));
    if (!d || !V) { free(d); free(V); return -1; }
    dmat_eig_jacobi((int)n, a, d, V);
    for (i64 i = 0; i < n; i++)
        for (i64 j = 0; j < n; j++)
        {
            double s = 0;
            for (i64 k = 0; k < n; k++) s += V[i * n + k] * f(d[k]) * V[j * n + k];
            out[i * n + j] = s;
        }
    free(d); free(V);
    return 0;
}

i64 ofx_linalg_sinm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (out_n < n * n) return -1;
    return matfun_sym(a, n, sin, out);
}
i64 ofx_linalg_cosm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (out_n < n * n) return -1;
    return matfun_sym(a, n, cos, out);
}
i64 ofx_linalg_tanm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (out_n < n * n) return -1;
    return matfun_sym(a, n, tan, out);
}
static double sign_of(double x) { return x == 0.0 ? 0.0 : (x > 0.0 ? 1.0 : -1.0); }
i64 ofx_linalg_signm(const double *a, i64 a_n, i64 n, double *out, i64 out_n)
{
    (void)a_n;
    if (out_n < n * n) return -1;
    return matfun_sym(a, n, sign_of, out);
}
i64 ofx_linalg_fractionalpower(const double *a, i64 a_n, i64 n, double p,
                               double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || n <= 0 || n > 100000 || out_n < n * n) return -1;
    i64 nn = n * n;
    double *d = (double *)malloc((size_t)n * sizeof(double));
    double *V = (double *)malloc((size_t)nn * sizeof(double));
    if (!d || !V) { free(d); free(V); return -1; }
    dmat_eig_jacobi((int)n, a, d, V);
    for (i64 i = 0; i < nn; i++) out[i] = 0.0;
    for (i64 i = 0; i < n; i++)
        for (i64 j = 0; j < n; j++)
        {
            double s = 0;
            for (i64 k = 0; k < n; k++) s += V[i * n + k] * pow(d[k], p) * V[j * n + k];
            out[i * n + j] = s;
        }
    free(d); free(V);
    return 0;
}

/* Base ortonormale dello spazio delle colonne di A rows×cols (SVD).
 * out rows×r, rende r; `rows<cols` passa dalla trasposta. */
static i64 orth_basis(const double *a, i64 rows, i64 cols, double *out, i64 out_n,
                      i64 *out_r)
{
    if (!a || !out || rows <= 0 || cols <= 0) return -1;
    if (rows >= cols)
    {
        i64 nn = rows * cols, nc = cols * cols;
        double *U = (double *)malloc((size_t)nn * sizeof(double));
        double *S = (double *)malloc((size_t)cols * sizeof(double));
        double *VT = (double *)malloc((size_t)nc * sizeof(double));
        if (!U || !S || !VT) { free(U); free(S); free(VT); return -1; }
        i64 r = -1;
        if (gm_linalg_svd((int)rows, (int)cols, a, U, S, VT) == GM_OK)
        {
            double smax = S[0];
            for (i64 i = 1; i < cols; i++) if (S[i] > smax) smax = S[i];
            double tol = (double)(rows > cols ? rows : cols) * 1e-12 * (smax > 0 ? smax : 1.0);
            i64 k = 0;
            while (k < cols && S[k] > tol) k++;
            if (out_n >= rows * k)
            {
                for (i64 i = 0; i < rows; i++)
                    for (i64 j = 0; j < k; j++) out[i * k + j] = U[i * cols + j];
                r = k;
            }
        }
        free(U); free(S); free(VT);
        if (r < 0) return -1;
        *out_r = r;
        return 0;
    }
    /* rows < cols: SVD di Aᵀ (cols×rows); lo spazio delle colonne di A è V
     * (rows×rows) = VTᵀ della SVD di Aᵀ. */
    i64 m = cols, n = rows;
    double *At = (double *)malloc((size_t)(m * n) * sizeof(double));
    double *U = (double *)malloc((size_t)(m * n) * sizeof(double));
    double *S = (double *)malloc((size_t)n * sizeof(double));
    double *VT = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (!At || !U || !S || !VT) { free(At); free(U); free(S); free(VT); return -1; }
    for (i64 i = 0; i < rows; i++)
        for (i64 j = 0; j < cols; j++) At[j * rows + i] = a[i * cols + j];
    i64 r = -1;
    if (gm_linalg_svd((int)m, (int)n, At, U, S, VT) == GM_OK)
    {
        double smax = S[0];
        for (i64 i = 1; i < n; i++) if (S[i] > smax) smax = S[i];
        double tol = (double)(m > n ? m : n) * 1e-12 * (smax > 0 ? smax : 1.0);
        i64 k = 0;
        while (k < n && S[k] > tol) k++;
        if (out_n >= rows * k)
        {
            for (i64 i = 0; i < rows; i++)
                for (i64 j = 0; j < k; j++) out[i * k + j] = VT[j * rows + i];
            r = k;
        }
    }
    free(At); free(U); free(S); free(VT);
    if (r < 0) return -1;
    *out_r = r;
    return 0;
}

/* SUBSPACEANGLES(a, rows, cols, b, rows, cols, out) — angoli principali fra
 * gli spazi delle colonne; out ha min(rA,rB) valori (almeno min(cols_a,cols_b)). */
i64 ofx_linalg_subspace_angles(const double *a, i64 a_n, i64 rows_a, i64 cols_a,
                               const double *b, i64 b_n, i64 rows_b, i64 cols_b,
                               double *out, i64 out_n)
{
    (void)a_n; (void)b_n;
    if (!a || !b || !out || rows_a <= 0 || cols_a <= 0 || rows_b != rows_a || cols_b <= 0)
        return -1;
    i64 maxc = cols_a > cols_b ? cols_a : cols_b;
    i64 cap = rows_a * (rows_a < maxc ? rows_a : maxc);
    double *Ba = (double *)malloc((size_t)cap * sizeof(double));
    double *Bb = (double *)malloc((size_t)cap * sizeof(double));
    if (!Ba || !Bb) { free(Ba); free(Bb); return -1; }
    i64 ra = 0, rb = 0;
    if (orth_basis(a, rows_a, cols_a, Ba, cap, &ra) != 0
        || orth_basis(b, rows_b, cols_b, Bb, cap, &rb) != 0)
    { free(Ba); free(Bb); return -1; }
    i64 len = ra < rb ? ra : rb;
    if (len <= 0 || out_n < len) { free(Ba); free(Bb); return -1; }
    /* C = Baᵀ·Bb (ra×rb), poi valori singolari di C (trasponendo se serve). */
    i64 cn = ra * rb;
    double *C = (double *)malloc((size_t)cn * sizeof(double));
    double *S = (double *)malloc((size_t)(len) * sizeof(double));
    if (!C || !S) { free(Ba); free(Bb); free(C); free(S); return -1; }
    for (i64 i = 0; i < ra; i++)
        for (i64 j = 0; j < rb; j++)
        {
            double s = 0;
            for (i64 k = 0; k < rows_a; k++) s += Ba[k * ra + i] * Bb[k * rb + j];
            C[i * rb + j] = s;
        }
    int rc = -1;
    i64 m = ra, n = rb;
    double *Ct = NULL;
    const double *src = C;
    if (m < n)
    {
        Ct = (double *)malloc((size_t)cn * sizeof(double));
        if (!Ct) { free(Ba); free(Bb); free(C); free(S); return -1; }
        for (i64 i = 0; i < ra; i++)
            for (i64 j = 0; j < rb; j++) Ct[j * ra + i] = C[i * rb + j];
        i64 t = m; m = n; n = t;
        src = Ct;
    }
    double *U = (double *)malloc((size_t)(m * n) * sizeof(double));
    double *VT = (double *)malloc((size_t)(n * n) * sizeof(double));
    if (U && VT && gm_linalg_svd((int)m, (int)n, src, U, S, VT) == GM_OK)
    {
        for (i64 i = 0; i < len; i++)
        {
            double sv = S[i];
            if (sv > 1.0) sv = 1.0;
            if (sv < -1.0) sv = -1.0;
            out[i] = acos(sv);
        }
        rc = len;
    }
    free(U); free(VT); free(Ct); free(C); free(S); free(Ba); free(Bb);
    return rc;
}

/* SOLVETRIANGULAR(a, n, b, n, lower, x) — risolve A x = b con A triangolare
 * n×n (lower != 0: triangolare inferiore, altrimenti superiore). Rende 0/-1. */
i64 ofx_linalg_solve_triangular(const double *a, i64 a_n, i64 n,
                                const double *b, i64 b_n, i64 lower,
                                double *x, i64 x_n)
{
    (void)a_n;
    if (!a || !b || !x || n <= 0 || n > 100000 || b_n < n || x_n < n) return -1;
    if (lower)
    {
        for (i64 i = 0; i < n; i++)
        {
            double s = 0;
            for (i64 j = 0; j < i; j++) s += a[i * n + j] * x[j];
            double dii = a[i * n + i];
            if (dii == 0.0) return -1;
            x[i] = (b[i] - s) / dii;
        }
    }
    else
    {
        for (i64 i = n - 1; i >= 0; i--)
        {
            double s = 0;
            for (i64 j = i + 1; j < n; j++) s += a[i * n + j] * x[j];
            double dii = a[i * n + i];
            if (dii == 0.0) return -1;
            x[i] = (b[i] - s) / dii;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * QUARTA ONDATA — delta di gmbmath: traccia/RREF/spazi, minimi quadrati,
 * tridiagonali, LDLᵀ, Gauss-Jordan, solver iterativi e power iteration.
 * Convenzioni: 0/-1 per le procedure; le funzioni con dimensione variabile
 * (RREF/COLSPACE/ROWSPACE) rendono il rango; i solver iterativi rendono il
 * numero di iterazioni (>=0) o -1.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* TRACE(a, n) — somma della diagonale. */
double ofx_linalg_trace(const double *a, i64 a_n, i64 n)
{
    (void)a_n;
    if (!a || n <= 0 || n > 100000) return -1.0;
    return gm_linalg_trace((int)n, a);
}

/* RREF(a, rows, cols, out) — forma ridotta a scala; out rows×cols, rende il rango. */
i64 ofx_linalg_rref(const double *a, i64 a_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || rows <= 0 || cols <= 0 || out_n < rows * cols) return -1;
    memcpy(out, a, (size_t)(rows * cols) * sizeof(double));
    int r = gm_linalg_rref((int)rows, (int)cols, out, NULL);
    return r < 0 ? -1 : (i64)r;
}

/* COLSPACE(a, rows, cols, out) — base dello spazio delle colonne (out rows×r,
 * dimensionare rows×cols); rende il rango r. */
i64 ofx_linalg_colspace(const double *a, i64 a_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || rows <= 0 || cols <= 0 || out_n < rows * cols) return -1;
    int r = gm_linalg_col_space((int)rows, (int)cols, a, out);
    return r < 0 ? -1 : (i64)r;
}

/* ROWSPACE(a, rows, cols, out) — base dello spazio delle righe (out r×cols,
 * dimensionare rows×cols); rende il rango r. */
i64 ofx_linalg_rowspace(const double *a, i64 a_n, i64 rows, i64 cols, double *out, i64 out_n)
{
    (void)a_n;
    if (!a || !out || rows <= 0 || cols <= 0 || out_n < rows * cols) return -1;
    int r = gm_linalg_row_space((int)rows, (int)cols, a, out);
    return r < 0 ? -1 : (i64)r;
}

/* LSTSQ(a, rows, cols, b, x) — minimi quadrati (rows>=cols), x ha cols valori. */
i64 ofx_linalg_lstsq(const double *a, i64 a_n, i64 rows, i64 cols,
                     const double *b, i64 b_n, double *x, i64 x_n)
{
    (void)a_n;
    if (!a || !b || !x || rows <= 0 || cols <= 0 || rows < cols
        || b_n < rows || x_n < cols) return -1;
    return gm_linalg_lstsq((int)rows, (int)cols, a, b, x) == GM_OK ? 0 : -1;
}

/* SYMTRIDIAG(a, n, d, e) — riduzione simmetrica tridiagonale: d,e n valori. */
i64 ofx_linalg_symtridiag(const double *a, i64 a_n, i64 n, double *d, i64 d_n,
                          double *e, i64 e_n)
{
    (void)a_n;
    if (!a || !d || !e || n <= 0 || n > 100000 || d_n < n || e_n < n) return -1;
    return gm_linalg_sym_tridiag((int)n, a, d, e) == GM_OK ? 0 : -1;
}

/* TRIDIAGEIG(d, e, n, eval) — autovalori di una tridiagonale simmetrica
 * (d,e non modificati), eval n valori. */
i64 ofx_linalg_tridiageig(const double *d, i64 d_n, const double *e, i64 e_n,
                          i64 n, double *eval, i64 eval_n)
{
    if (!d || !e || !eval || n <= 0 || n > 100000
        || d_n < n || e_n < n || eval_n < n) return -1;
    double *dc = (double *)malloc((size_t)n * sizeof(double));
    double *ec = (double *)malloc((size_t)n * sizeof(double));
    if (!dc || !ec) { free(dc); free(ec); return -1; }
    memcpy(dc, d, (size_t)n * sizeof(double));
    memcpy(ec, e, (size_t)n * sizeof(double));
    int rc = gm_linalg_tridiag_eig((int)n, dc, ec, eval) == GM_OK ? 0 : -1;
    free(dc); free(ec);
    return rc;
}

/* THOMAS(lower, diag, upper, rhs, n, x) — sistema tridiagonale. */
i64 ofx_linalg_thomas(const double *lower, i64 lo_n, const double *diag, i64 di_n,
                      const double *upper, i64 up_n, const double *rhs, i64 rh_n,
                      i64 n, double *x, i64 x_n)
{
    if (!lower || !diag || !upper || !rhs || !x || n <= 0 || n > 100000
        || lo_n < n || di_n < n || up_n < n || rh_n < n || x_n < n) return -1;
    return gm_linalg_thomas((int)n, lower, diag, upper, rhs, x) == GM_OK ? 0 : -1;
}

/* LDLT(a, n, l, d) — A = L·diag(d)·Lᵀ (simmetrica); L n×n unit-lower, d n. */
i64 ofx_linalg_ldlt(const double *a, i64 a_n, i64 n, double *l, i64 l_n,
                    double *d, i64 d_n)
{
    (void)a_n;
    if (!a || !l || !d || n <= 0 || n > 100000 || l_n < n * n || d_n < n) return -1;
    memcpy(l, a, (size_t)(n * n) * sizeof(double));
    if (gm_linalg_ldlt((int)n, l, d) != GM_OK) return -1;
    for (i64 i = 0; i < n; i++)
        for (i64 j = i + 1; j < n; j++) l[i * n + j] = 0.0;
    return 0;
}

/* LDLTSOLVE(l, d, b, n, x) — risolve L·diag(d)·Lᵀ x = b. */
i64 ofx_linalg_ldltsolve(const double *l, i64 l_n, const double *d, i64 d_n,
                         const double *b, i64 b_n, i64 n, double *x, i64 x_n)
{
    if (!l || !d || !b || !x || n <= 0 || n > 100000
        || l_n < n * n || d_n < n || b_n < n || x_n < n) return -1;
    return gm_linalg_ldlt_solve((int)n, l, d, b, x) == GM_OK ? 0 : -1;
}

/* GAUSSJORDAN(a, n, b, x) — eliminazione completa (A non modificato). */
i64 ofx_linalg_gaussjordan(const double *a, i64 a_n, i64 n,
                           const double *b, i64 b_n, double *x, i64 x_n)
{
    (void)a_n;
    if (!a || !b || !x || n <= 0 || n > 100000 || b_n < n || x_n < n) return -1;
    return gm_linalg_gauss_jordan((int)n, a, b, x) == GM_OK ? 0 : -1;
}

/* OPTIMALOMEGA(a, n) — omega ottimale per SOR (raggio spettrale di Jacobi). */
double ofx_linalg_optimalomega(const double *a, i64 a_n, i64 n)
{
    (void)a_n;
    if (!a || n <= 0 || n > 100000) return -1.0;
    return gm_iter_optimal_omega((int)n, a);
}

/* CG(a, n, b, x, maxit, tol) — gradiente coniugato; rende le iterazioni o -1. */
i64 ofx_linalg_cg(const double *a, i64 a_n, i64 n, const double *b, i64 b_n,
                  double *x, i64 x_n, i64 maxit, double tol)
{
    (void)a_n;
    if (!a || !b || !x || n <= 0 || n > 100000 || b_n < n || x_n < n) return -1;
    int it = 0;
    return gm_iter_cg((int)n, a, b, x, (int)maxit, tol, &it) == GM_OK ? (i64)it : -1;
}

/* GMRES(a, n, b, x, restart, maxit, tol) — rende le iterazioni o -1. */
i64 ofx_linalg_gmres(const double *a, i64 a_n, i64 n, const double *b, i64 b_n,
                     double *x, i64 x_n, i64 restart, i64 maxit, double tol)
{
    (void)a_n;
    if (!a || !b || !x || n <= 0 || n > 100000 || b_n < n || x_n < n) return -1;
    int it = 0;
    return gm_iter_gmres((int)n, a, b, x, (int)restart, (int)maxit, tol, &it) == GM_OK
           ? (i64)it : -1;
}

/* POWER(a, n, iters, tol, lambda, v) — autovalore dominante e autovettore.
 * Nota: il kernel parte dal vettore di tutti uni e scrive v in uscita. */
i64 ofx_linalg_power(const double *a, i64 a_n, i64 n, i64 iters, double tol,
                     double *lambda, i64 lambda_n, double *v, i64 v_n)
{
    (void)a_n;
    if (!a || !lambda || !v || n <= 0 || n > 100000 || lambda_n < 1 || v_n < n) return -1;
    return gm_eig_power((int)n, a, lambda, v, (int)iters, tol) == GM_OK ? 0 : -1;
}
