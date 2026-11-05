/* gm_matfun.c — funzioni matriciali (da detectum). */
#include "gm_matfun.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void matmul(int n, const gm_real *A, const gm_real *B, gm_real *C)
{
    int i, j, k;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            gm_real s = 0.0;
            for (k = 0; k < n; k++) s += A[i * n + k] * B[k * n + j];
            C[i * n + j] = s;
        }
}

static gm_real norm1(int n, const gm_real *A)
{
    int i, j;
    gm_real mx = 0.0;
    for (j = 0; j < n; j++) {
        gm_real s = 0.0;
        for (i = 0; i < n; i++) s += fabs(A[i * n + j]);
        if (s > mx) mx = s;
    }
    return mx;
}

gm_status gm_mat_exp(int n, const gm_real *A, gm_real *E)
{
    gm_real *X, *T, *P;
    gm_real nrm;
    int i, s = 0, k;
    if (n <= 0 || !A || !E) return GM_ERR_ARG;
    nrm = norm1(n, A);
    while (nrm > 0.5) { nrm *= 0.5; s++; }
    X = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    T = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    P = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    if (!X || !T || !P) { free(X); free(T); free(P); return GM_ERR_NO_MEM; }
    { gm_real sc = (gm_real)ldexp(1.0, -s); for (i = 0; i < n * n; i++) X[i] = A[i] * sc; }
    memset(E, 0, (size_t)n * n * sizeof(gm_real));
    memset(T, 0, (size_t)n * n * sizeof(gm_real));
    for (i = 0; i < n; i++) { E[i * n + i] = 1.0; T[i * n + i] = 1.0; }
    for (k = 1; k <= 16; k++) {
        matmul(n, T, X, P);
        for (i = 0; i < n * n; i++) T[i] = P[i] / (gm_real)k;
        for (i = 0; i < n * n; i++) E[i] += T[i];
    }
    for (k = 0; k < s; k++) { matmul(n, E, E, P); memcpy(E, P, (size_t)n * n * sizeof(gm_real)); }
    free(X); free(T); free(P);
    return GM_OK;
}

gm_status gm_mat_sqrt(int n, const gm_real *A, gm_real *S)
{
    gm_real *Y, *Z, *Yi, *Zi, *T1, *T2;
    int i, k, done = 0;
    if (n <= 0 || !A || !S) return GM_ERR_ARG;
    Y = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    Z = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    Yi = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    Zi = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    T1 = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    T2 = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    if (!Y || !Z || !Yi || !Zi || !T1 || !T2) {
        free(Y); free(Z); free(Yi); free(Zi); free(T1); free(T2); return GM_ERR_NO_MEM;
    }
    memcpy(Y, A, (size_t)n * n * sizeof(gm_real));
    memset(Z, 0, (size_t)n * n * sizeof(gm_real));
    for (i = 0; i < n; i++) Z[i * n + i] = 1.0;
    for (k = 0; k < 60 && !done; k++) {
        gm_real diff = 0.0;
        if (gm_linalg_inverse(n, Y, Yi) != GM_OK || gm_linalg_inverse(n, Z, Zi) != GM_OK) {
            free(Y); free(Z); free(Yi); free(Zi); free(T1); free(T2); return GM_ERR_SINGULAR;
        }
        for (i = 0; i < n * n; i++) T1[i] = 0.5 * (Y[i] + Zi[i]);
        for (i = 0; i < n * n; i++) T2[i] = 0.5 * (Z[i] + Yi[i]);
        for (i = 0; i < n * n; i++) diff += fabs(T1[i] - Y[i]);
        memcpy(Y, T1, (size_t)n * n * sizeof(gm_real));
        memcpy(Z, T2, (size_t)n * n * sizeof(gm_real));
        if (diff < 1e-14) done = 1;
    }
    memcpy(S, Y, (size_t)n * n * sizeof(gm_real));
    free(Y); free(Z); free(Yi); free(Zi); free(T1); free(T2);
    return done ? GM_OK : GM_ERR_NOCONV;
}

gm_status gm_mat_log(int n, const gm_real *A, gm_real *L)
{
    gm_real *B, *Bs, *X, *T, *P;
    int i, s = 0, k;
    gm_real d;
    if (n <= 0 || !A || !L) return GM_ERR_ARG;
    B = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    Bs = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    X = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    T = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    P = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    if (!B || !Bs || !X || !T || !P) { free(B); free(Bs); free(X); free(T); free(P); return GM_ERR_NO_MEM; }
    memcpy(B, A, (size_t)n * n * sizeof(gm_real));
    for (;;) {
        d = 0.0;
        for (i = 0; i < n * n; i++) d += fabs(B[i] - (i / n == i % n ? 1.0 : 0.0));
        if (d < 0.25 || s >= 20) break;
        if (gm_mat_sqrt(n, B, Bs) != GM_OK) { free(B); free(Bs); free(X); free(T); free(P); return GM_ERR_NOCONV; }
        memcpy(B, Bs, (size_t)n * n * sizeof(gm_real));
        s++;
    }
    for (i = 0; i < n * n; i++) X[i] = B[i];
    for (i = 0; i < n; i++) X[i * n + i] -= 1.0;
    memset(L, 0, (size_t)n * n * sizeof(gm_real));
    memset(T, 0, (size_t)n * n * sizeof(gm_real));
    for (i = 0; i < n; i++) T[i * n + i] = 1.0;
    for (k = 1; k <= 40; k++) {
        gm_real sign = (k % 2 == 1) ? 1.0 : -1.0;
        matmul(n, T, X, P);
        for (i = 0; i < n * n; i++) T[i] = P[i];
        for (i = 0; i < n * n; i++) L[i] += sign * T[i] / (gm_real)k;
    }
    for (i = 0; i < n * n; i++) L[i] *= (gm_real)ldexp(1.0, s);
    free(B); free(Bs); free(X); free(T); free(P);
    return GM_OK;
}

/* pinv per m >= n: P (n×m) = V S+ U^T, con U (m×n), S (n), Vt (n×n). */
static gm_status pinv_tall(int m, int n, const gm_real *A, gm_real *P)
{
    gm_real *U, *S, *Vt, *tmp;
    gm_real tol, smax = 0.0;
    int i, j, k;
    U = (gm_real *)malloc((size_t)m * n * sizeof(gm_real));
    S = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    Vt = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    tmp = (gm_real *)malloc((size_t)n * m * sizeof(gm_real));
    if (!U || !S || !Vt || !tmp) { free(U); free(S); free(Vt); free(tmp); return GM_ERR_NO_MEM; }
    if (gm_linalg_svd(m, n, A, U, S, Vt) != GM_OK) { free(U); free(S); free(Vt); free(tmp); return GM_ERR_NOCONV; }
    for (i = 0; i < n; i++) if (S[i] > smax) smax = S[i];
    tol = smax * (gm_real)(m > n ? m : n) * gm_eps();
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++) {
            gm_real s = 0.0;
            for (k = 0; k < n; k++) {
                gm_real sinv = (S[k] > tol) ? 1.0 / S[k] : 0.0;
                s += Vt[(size_t)k * n + i] * sinv * U[(size_t)j * n + k];
            }
            tmp[(size_t)i * m + j] = s;
        }
    memcpy(P, tmp, (size_t)n * m * sizeof(gm_real));
    free(U); free(S); free(Vt); free(tmp);
    return GM_OK;
}

gm_status gm_mat_pinv(int m, int n, const gm_real *A, gm_real *P)
{
    gm_real *At, *Pt;
    int i, j;
    if (m <= 0 || n <= 0 || !A || !P) return GM_ERR_ARG;
    if (m >= n) return pinv_tall(m, n, A, P);
    /* pinv(A) = (pinv(A^T))^T ; A^T è (n×m) con n>m */
    At = (gm_real *)malloc((size_t)n * m * sizeof(gm_real));
    Pt = (gm_real *)malloc((size_t)m * n * sizeof(gm_real));
    if (!At || !Pt) { free(At); free(Pt); return GM_ERR_NO_MEM; }
    for (i = 0; i < m; i++) for (j = 0; j < n; j++) At[(size_t)j * m + i] = A[(size_t)i * n + j];
    if (pinv_tall(n, m, At, Pt) != GM_OK) { free(At); free(Pt); return GM_ERR_NOCONV; }
    for (i = 0; i < m; i++) for (j = 0; j < n; j++) P[(size_t)j * m + i] = Pt[(size_t)i * n + j];
    free(At); free(Pt);
    return GM_OK;
}
