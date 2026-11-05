/* gm_decomp2.c — decomposizioni avanzate (da detectum). */
#include "gm_decomp2.h"
#include "gm_linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GMR(i, j) H[(i) * n + (j)]

/* ============================ Hessenberg ============================ */

gm_status gm_linalg_hessenberg(int n, const gm_real *A, gm_real *H, gm_real *Q)
{
    gm_real *v;
    int i, j, k;
    if (n <= 0 || !A || !H || !Q) return GM_ERR_ARG;
    memcpy(H, A, (size_t)n * n * sizeof(gm_real));
    memset(Q, 0, (size_t)n * n * sizeof(gm_real));
    for (i = 0; i < n; i++) Q[i * n + i] = 1.0;
    v = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!v) return GM_ERR_NO_MEM;
    for (k = 0; k < n - 2; k++) {
        gm_real nrm = 0.0, alpha, vtv = 0.0, beta;
        for (i = k + 1; i < n; i++) nrm += H[i * n + k] * H[i * n + k];
        nrm = sqrt(nrm);
        if (nrm == 0.0) continue;
        alpha = (H[(k + 1) * n + k] >= 0.0) ? -nrm : nrm;
        v[0] = H[(k + 1) * n + k] - alpha;
        for (i = k + 2; i < n; i++) v[i - (k + 1)] = H[i * n + k];
        for (i = 0; i < n - (k + 1); i++) vtv += v[i] * v[i];
        if (vtv == 0.0) continue;
        beta = 2.0 / vtv;
        /* H <- (I - beta v v^T) H  (righe k+1..n-1) */
        for (j = k; j < n; j++) {
            gm_real s = 0.0;
            for (i = 0; i < n - (k + 1); i++) s += v[i] * H[(k + 1 + i) * n + j];
            s *= beta;
            for (i = 0; i < n - (k + 1); i++) H[(k + 1 + i) * n + j] -= s * v[i];
        }
        /* H <- H (I - beta v v^T)  (colonne k+1..n-1) */
        for (i = 0; i < n; i++) {
            gm_real s = 0.0;
            for (j = 0; j < n - (k + 1); j++) s += H[i * n + (k + 1 + j)] * v[j];
            s *= beta;
            for (j = 0; j < n - (k + 1); j++) H[i * n + (k + 1 + j)] -= s * v[j];
        }
        /* Q <- Q (I - beta v v^T) */
        for (i = 0; i < n; i++) {
            gm_real s = 0.0;
            for (j = 0; j < n - (k + 1); j++) s += Q[i * n + (k + 1 + j)] * v[j];
            s *= beta;
            for (j = 0; j < n - (k + 1); j++) Q[i * n + (k + 1 + j)] -= s * v[j];
        }
        H[(k + 1) * n + k] = alpha;
        for (i = k + 2; i < n; i++) H[i * n + k] = 0.0;
    }
    free(v);
    return GM_OK;
}

/* ============================== Schur ============================== */

static void givens(gm_real a, gm_real b, gm_real *c, gm_real *s)
{
    if (b == 0.0) { *c = 1.0; *s = 0.0; }
    else if (fabs(b) > fabs(a)) { gm_real t = a / b; *s = 1.0 / sqrt(1.0 + t * t); *c = (*s) * t; }
    else { gm_real t = b / a; *c = 1.0 / sqrt(1.0 + t * t); *s = (*c) * t; }
}

gm_status gm_linalg_schur(int n, const gm_real *A, gm_real *T, gm_real *Q)
{
    int l, m, k, i, j, iter;
    gm_real mu, x, y, c, s;
    if (n <= 0 || !A || !T || !Q) return GM_ERR_ARG;
    if (gm_linalg_hessenberg(n, A, T, Q) != GM_OK) return GM_ERR_NOCONV;
    for (m = n - 1; m > 0; ) {
        /* deflazione */
        for (l = m; l > 0; l--) {
            gm_real sc = fabs(T[(l - 1) * n + (l - 1)]) + fabs(T[l * n + l]);
            if (sc == 0.0) sc = 1.0;
            if (fabs(T[l * n + (l - 1)]) <= 1e-14 * sc) { T[l * n + (l - 1)] = 0.0; break; }
        }
        if (l == m) { m--; continue; }
        if (l == m - 1) { m -= 2; continue; }
        /* shift di Wilkinson dall'ultimo blocco 2x2 [m-1,m] */
        {
            gm_real a = T[(m - 1) * n + (m - 1)], b = T[(m - 1) * n + m];
            gm_real cc = T[m * n + (m - 1)], dd = T[m * n + m];
            gm_real tr = a + dd, det = a * dd - b * cc, disc = tr * tr - 4.0 * det;
            if (disc >= 0.0) {
                gm_real r1 = 0.5 * (tr + sqrt(disc)), r2 = 0.5 * (tr - sqrt(disc));
                mu = (fabs(r1 - dd) < fabs(r2 - dd)) ? r1 : r2;
            } else mu = dd;
        }
        /* un passo QR implicito su T[l..m][l..m] */
        for (iter = 0; iter < 30; iter++) {
            x = T[l * n + l] - mu;
            y = T[(l + 1) * n + l];
            for (k = l; k <= m - 1; k++) {
                givens(x, y, &c, &s);
                for (j = (k > l ? k - 1 : l); j < n; j++) {
                    gm_real t1 = T[k * n + j], t2 = T[(k + 1) * n + j];
                    T[k * n + j] = c * t1 + s * t2;
                    T[(k + 1) * n + j] = -s * t1 + c * t2;
                }
                for (i = 0; i <= (k + 2 < n ? k + 2 : n - 1); i++) {
                    gm_real t1 = T[i * n + k], t2 = T[i * n + k + 1];
                    T[i * n + k] = c * t1 + s * t2;
                    T[i * n + k + 1] = -s * t1 + c * t2;
                }
                for (i = 0; i < n; i++) {
                    gm_real t1 = Q[i * n + k], t2 = Q[i * n + k + 1];
                    Q[i * n + k] = c * t1 + s * t2;
                    Q[i * n + k + 1] = -s * t1 + c * t2;
                }
                if (k < m - 1) { x = T[(k + 1) * n + k]; y = T[(k + 2) * n + k]; }
            }
            if (fabs(T[m * n + (m - 1)]) <= 1e-14 * (fabs(T[(m - 1) * n + (m - 1)]) + fabs(T[m * n + m]))) {
                T[m * n + (m - 1)] = 0.0;
                break;
            }
        }
        if (iter >= 30) return GM_ERR_NOCONV;
    }
    return GM_OK;
}

/* =============================== COD =============================== */

gm_status gm_linalg_cod(int m, int n, const gm_real *A, gm_real *U,
                        gm_real *T, gm_real *V, int *rank)
{
    gm_real *U2, *S, *V2, *At;
    gm_real smax = 0.0, tol;
    int i, j, mn, r = 0;
    if (m <= 0 || n <= 0 || !A || !U || !T || !V) return GM_ERR_ARG;
    mn = (m < n) ? m : n;
    S = (gm_real *)malloc((size_t)mn * sizeof(gm_real));
    U2 = (gm_real *)malloc((size_t)(m > n ? m * n : n * m) * sizeof(gm_real));
    V2 = (gm_real *)malloc((size_t)(m > n ? n * n : m * m) * sizeof(gm_real));
    At = (gm_real *)malloc((size_t)m * n * sizeof(gm_real));
    if (!S || !U2 || !V2 || !At) { free(S); free(U2); free(V2); free(At); return GM_ERR_NO_MEM; }
    if (m >= n) {
        if (gm_linalg_svd(m, n, A, U2, S, V2) != GM_OK) { free(S); free(U2); free(V2); free(At); return GM_ERR_NOCONV; }
        memcpy(U, U2, (size_t)m * n * sizeof(gm_real));
        for (j = 0; j < n; j++) for (i = 0; i < n; i++) V[i * n + j] = V2[j * n + i];
    } else {
        for (i = 0; i < m; i++) for (j = 0; j < n; j++) At[j * m + i] = A[i * n + j];
        if (gm_linalg_svd(n, m, At, U2, S, V2) != GM_OK) { free(S); free(U2); free(V2); free(At); return GM_ERR_NOCONV; }
        /* U_cod = V2 (m×m), V_cod = U2 (n×m) */
        memcpy(U, V2, (size_t)m * m * sizeof(gm_real));
        memcpy(V, U2, (size_t)n * m * sizeof(gm_real));
    }
    for (i = 0; i < mn; i++) if (S[i] > smax) smax = S[i];
    tol = smax * (gm_real)(m > n ? m : n) * gm_eps();
    memset(T, 0, (size_t)mn * mn * sizeof(gm_real));
    for (i = 0; i < mn; i++) { T[i * mn + i] = S[i]; if (S[i] > tol) r++; }
    if (rank) *rank = r;
    free(S); free(U2); free(V2); free(At);
    return GM_OK;
}

/* ============================== Aasen ============================== */

gm_status gm_linalg_aasen(int n, const gm_real *Ain, gm_real *L, gm_real *T, int *piv)
{
    gm_real *W, *h;
    gm_real a00, a01 = 0.0, tmp;
    int i, j, k, jp;
    if (n <= 0 || !Ain || !L || !T || !piv) return GM_ERR_ARG;
    W = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    if (!W) return GM_ERR_NO_MEM;
#define AT(i, j) W[(i) + (j) * n]
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) AT(i, j) = Ain[i * n + j];
    h = W;
    a00 = W[0];
    for (j = 1; j < n; j++) AT(0, j) = (gm_real)j;
    for (j = 0; j < n; j++) {
        if (j > 0) {
            h[0] = AT(0, 1);
            if (j > 1) {
                h[0] *= AT(j, 1);
                if (j == 2) {
                    h[1] = AT(1, 1) * AT(2, 1) + AT(1, 2);
                } else {
                    h[1] = AT(1, 1) * AT(j, 1) + AT(1, 2) * AT(j, 2);
                    for (k = 2; k < j - 1; k++)
                        h[k] = AT(k, k) * AT(j, k) + AT(k - 1, k) * AT(j, k - 1) + AT(k, k + 1) * AT(j, k + 1);
                    h[j - 1] = AT(j - 1, j - 1) * AT(j, j - 1) + AT(j - 2, j - 1) * AT(j, j - 2) + AT(j - 1, j);
                }
            }
            tmp = 0.0;
            for (k = 1; k < j; k++) tmp += AT(j, k) * h[k];
            h[j] = AT(j, j) - tmp;
            AT(j, j) = h[j];
            if (j > 1) AT(j, j) -= AT(j - 1, j) * AT(j, j - 1);
            for (i = j + 1; i < n; i++) {
                tmp = 0.0;
                for (k = 1; k <= j; k++) tmp += AT(i, k) * h[k];
                h[i] = AT(j, i) - tmp;
            }
        }
        if (j < n - 1) {
            gm_real *Ajp, *Aj1;
            for (jp = j + 1, k = j + 1; k < n; k++) if (fabs(h[k]) > fabs(h[jp])) jp = k;
            for (k = 0; k < n; k++) { tmp = AT(jp, k); AT(jp, k) = AT(j + 1, k); AT(j + 1, k) = tmp; }
            Ajp = &AT(0, jp); Aj1 = &AT(0, j + 1);
            tmp = Ajp[0]; Ajp[0] = Aj1[0]; Aj1[0] = tmp;
            for (k = j + 1; k < n; k++) { tmp = Ajp[k]; Ajp[k] = Aj1[k]; Aj1[k] = tmp; }
            if (j == 0) a01 = AT(0, 1);
            AT(j, j + 1) = h[j + 1];
            if (j < n - 2) {
                if (h[jp] != 0.0) for (i = j + 2; i < n; i++) AT(i, j + 1) = h[i] / h[j + 1];
                else for (i = j + 2; i < n; i++) AT(i, j + 1) = 0.0;
            }
        }
    }
    W[0] = a00;
    if (n > 1) W[1] = a01;
    for (j = 2; j < n; j++) { AT(j, 0) = AT(0, j); for (i = 0; i < j - 1; i++) AT(i, j) = 0.0; }
    /* estrazione: T = diag + superdiag; L = triangolo stretto (col>=1) unit */
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) { L[i * n + j] = 0.0; T[i * n + j] = 0.0; }
    for (i = 0; i < n; i++) L[i * n + i] = 1.0;
    for (j = 0; j < n; j++) {
        T[j * n + j] = W[j + j * n];
        if (j + 1 < n) { T[j * n + j + 1] = W[j + (j + 1) * n]; T[(j + 1) * n + j] = T[j * n + j + 1]; }
    }
    for (i = 0; i < n; i++) for (j = 1; j < i; j++) L[i * n + j] = W[i + j * n];
    piv[0] = 0;
    for (j = 1; j < n; j++) piv[j] = (int)W[j];
#undef AT
    free(W);
    return GM_OK;
}

gm_status gm_linalg_aasen_solve(int n, const gm_real *L, const gm_real *T,
                                const int *piv, const gm_real *b, gm_real *x)
{
    gm_real *y, *z, *bb, *a, *c, *d;
    int i;
    if (n <= 0 || !L || !T || !piv || !b || !x) return GM_ERR_ARG;
    y = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    z = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    bb = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    a = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    c = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    d = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!y || !z || !bb || !a || !c || !d) { free(y); free(z); free(bb); free(a); free(c); free(d); return GM_ERR_NO_MEM; }
    for (i = 0; i < n; i++) bb[i] = b[piv[i]];
    /* L z = bb (forward) */
    for (i = 0; i < n; i++) {
        gm_real s = bb[i];
        int j;
        for (j = 1; j < i; j++) s -= L[i * n + j] * z[j];
        z[i] = s;
    }
    /* T y = z (tridiagonale, Thomas) */
    for (i = 0; i < n; i++) { d[i] = T[i * n + i]; a[i] = (i > 0) ? T[i * n + (i - 1)] : 0.0; c[i] = (i < n - 1) ? T[i * n + (i + 1)] : 0.0; }
    for (i = 1; i < n; i++) {
        gm_real mfact = a[i] / d[i - 1];
        d[i] -= mfact * c[i - 1];
        z[i] -= mfact * z[i - 1];
    }
    y[n - 1] = z[n - 1] / d[n - 1];
    for (i = n - 2; i >= 0; i--) y[i] = (z[i] - c[i] * y[i + 1]) / d[i];
    /* L^T y = y (backward): w[i] = y[i] - sum_{j>i} L[j][i] w[j] */
    for (i = n - 1; i >= 0; i--) {
        gm_real s = y[i];
        int j;
        for (j = i + 1; j < n; j++) s -= L[j * n + i] * y[j];
        y[i] = s;
    }
    for (i = 0; i < n; i++) x[piv[i]] = y[i];
    free(y); free(z); free(bb); free(a); free(c); free(d);
    return GM_OK;
}

/* ============================== BVLS ============================== */

gm_status gm_linalg_bvls(int m, int n, const gm_real *C, const gm_real *d,
                         const gm_real *lower, const gm_real *upper, gm_real *x)
{
    int *freev;
    gm_real *g, *r, *Cfr, *A, *rhs, *xf, *tmp;
    int i, j, k, iter, nfree;
    if (m <= 0 || n <= 0 || !C || !d || !x) return GM_ERR_ARG;
    freev = (int *)malloc((size_t)n * sizeof(int));
    g = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    r = (gm_real *)malloc((size_t)m * sizeof(gm_real));
    Cfr = (gm_real *)malloc((size_t)m * n * sizeof(gm_real));
    A = (gm_real *)malloc((size_t)n * n * sizeof(gm_real));
    rhs = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    xf = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    tmp = (gm_real *)malloc((size_t)n * sizeof(gm_real));
    if (!freev || !g || !r || !Cfr || !A || !rhs || !xf || !tmp) {
        free(freev); free(g); free(r); free(Cfr); free(A); free(rhs); free(xf); free(tmp); return GM_ERR_NO_MEM;
    }
    for (i = 0; i < n; i++) {
        x[i] = 0.0;
        if (lower && x[i] < lower[i]) x[i] = lower[i];
        if (upper && x[i] > upper[i]) x[i] = upper[i];
        freev[i] = 1;
    }
    for (iter = 0; iter < 3 * n + 10; iter++) {
        /* r = d - C x */
        for (i = 0; i < m; i++) { gm_real s = 0.0; for (j = 0; j < n; j++) s += C[i * n + j] * x[j]; r[i] = d[i] - s; }
        /* LS sui liberi: minimizza ||C_F x_F - (d - C_B x_B)|| */
        nfree = 0;
        for (j = 0; j < n; j++) if (freev[j]) nfree++;
        if (nfree > 0) {
            int a = 0;
            for (j = 0; j < n; j++) if (freev[j]) { for (i = 0; i < m; i++) Cfr[i * nfree + a] = C[i * n + j]; a++; }
            for (i = 0; i < nfree; i++) for (j = 0; j < nfree; j++) {
                gm_real s = 0.0; for (k = 0; k < m; k++) s += Cfr[k * nfree + i] * Cfr[k * nfree + j]; A[i * nfree + j] = s;
            }
            for (i = 0; i < nfree; i++) { gm_real s = 0.0; for (k = 0; k < m; k++) s += Cfr[k * nfree + i] * r[k]; rhs[i] = s; }
            if (gm_linalg_solve(nfree, A, rhs, xf) == GM_OK) {
                /* riporta in x, verifica i bound */
                int worst = -1; gm_real wv = 0.0; a = 0;
                for (j = 0; j < n; j++) if (freev[j]) {
                    gm_real v = xf[a]; gm_real viol = 0.0;
                    if (lower && v < lower[j]) viol = lower[j] - v;
                    if (upper && v > upper[j]) viol = v - upper[j];
                    if (viol > wv) { wv = viol; worst = j; }
                    tmp[j] = v; a++;
                }
                if (worst >= 0) {
                    /* muovi la variabile piu' violata al suo bound */
                    x[worst] = (lower && tmp[worst] < lower[worst]) ? lower[worst] : upper[worst];
                    freev[worst] = 0;
                    continue;
                }
                for (j = 0; j < n; j++) if (freev[j]) x[j] = tmp[j];
            }
        }
        /* KKT: rilascia la variabile al bound con il moltiplicatore piu' violato */
        for (i = 0; i < m; i++) { gm_real s = 0.0; for (j = 0; j < n; j++) s += C[i * n + j] * x[j]; r[i] = d[i] - s; }
        for (j = 0; j < n; j++) { gm_real s = 0.0; for (i = 0; i < m; i++) { gm_real cx = 0.0; int q; for (q = 0; q < n; q++) cx += C[i * n + q] * x[q]; s += C[i * n + j] * (cx - d[i]); } g[j] = s; }
        {
            int best = -1; gm_real bv = 0.0;
            for (j = 0; j < n; j++) {
                if (freev[j]) continue;
                if (lower && x[j] <= lower[j] + 1e-12 && g[j] < -bv) { bv = -g[j]; best = j; }
                else if (upper && x[j] >= upper[j] - 1e-12 && g[j] > bv) { bv = g[j]; best = j; }
            }
            if (best < 0) break;
            freev[best] = 1;
        }
    }
    free(freev); free(g); free(r); free(Cfr); free(A); free(rhs); free(xf); free(tmp);
    return GM_OK;
}

/* ====================== sqrt quasi-triangolare ====================== */

gm_status gm_mat_sqrt_quasitriu(int n, const gm_real *T, gm_real *S)
{
    int i, j, k;
    if (n <= 0 || !T || !S) return GM_ERR_ARG;
    memset(S, 0, (size_t)n * n * sizeof(gm_real));
    for (j = 0; j < n; j++) {
        int blk = (j + 1 < n && T[(j + 1) * n + j] != 0.0) ? 2 : 1;
        if (blk == 1) {
            if (T[j * n + j] < 0.0) return GM_ERR_ARG;
            S[j * n + j] = sqrt(T[j * n + j]);
        } else {
            /* blocco 2x2 [j,j+1]: S_blk^2 = T_blk (autovalori reali) */
            gm_real a = T[j * n + j], b = T[j * n + (j + 1)], c = T[(j + 1) * n + j], d = T[(j + 1) * n + (j + 1)];
            gm_real det = a * d - b * c, tr = a + d, s2 = sqrt(det), t2 = sqrt(tr + 2.0 * s2);
            if (t2 == 0.0) return GM_ERR_ARG;
            S[j * n + j] = (a + s2) / t2;
            S[j * n + (j + 1)] = b / t2;
            S[(j + 1) * n + j] = c / t2;
            S[(j + 1) * n + (j + 1)] = (d + s2) / t2;
        }
        /* ricorrenza di Parlett per la colonna j (e j+1) */
        for (i = j - 1; i >= 0; i--) {
            if (blk == 1) {
                gm_real sum = T[i * n + j];
                for (k = i + 1; k < j; k++) sum -= S[i * n + k] * S[k * n + j];
                S[i * n + j] = sum / (S[i * n + i] + S[j * n + j]);
            } else {
                /* risolve [S_ii S_ij + S_ij S_jj] = T_ij per S_ij (2x2) */
                gm_real R0 = T[i * n + j], R1 = T[i * n + (j + 1)];
                gm_real sjj = S[j * n + j], sj1 = S[j * n + (j + 1)], sj0 = S[(j + 1) * n + j], s11 = S[(j + 1) * n + (j + 1)];
                gm_real denom = S[i * n + i];
                for (k = i + 1; k < j; k++) { R0 -= S[i * n + k] * S[k * n + j]; R1 -= S[i * n + k] * S[k * n + (j + 1)]; }
                /* (S_ii + sjj) x0 + sj0 x1 = R0 ; sj1 x0 + (S_ii + s11) x1 = R1 */
                gm_real a11 = denom + sjj, a12 = sj0, a21 = sj1, a22 = denom + s11;
                gm_real dd = a11 * a22 - a12 * a21;
                if (dd == 0.0) return GM_ERR_SINGULAR;
                S[i * n + j] = (R0 * a22 - a12 * R1) / dd;
                S[i * n + (j + 1)] = (a11 * R1 - R0 * a21) / dd;
            }
        }
        if (blk == 2) j++;
    }
    return GM_OK;
}
