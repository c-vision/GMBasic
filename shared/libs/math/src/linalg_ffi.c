/*
 * linalg_ffi.c — shim ABI per il namespace BASIC LINALGX (algebra lineare su ARRAY).
 *
 * PERCHÉ. Il namespace builtin `LINALG` lavora su HANDLE di MATRIX (`gmb_matrix_create`
 * → un indice nella tabella globale del runtime): una dylib FFI self-contained non può
 * leggere quella tabella, e le gambe managed non la condividono. Qui la stessa algebra
 * viaggia su **buffer piatti row-major** + dimensioni esplicite, il contratto già usato da
 * VEC/MULTIVARIATE/TIMESERIES.
 *
 * ABI FFI (importante): per OGNI parametro array il motore passa il puntatore seguito
 * dalla LUNGHEZZA (int64), come in `ofx_vec_*`. Il DECLARE BASIC non scrive la lunghezza
 * (la aggiunge il motore): la firma C qui sotto la riceve accanto a ogni `double*`.
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
