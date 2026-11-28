/*
 * linalg_ffi.c — shim ABI per il namespace BASIC LINALGX (algebra lineare su ARRAY).
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
