/* llmwsp_linalg.c — algebra lineare per gli analizzatori WSP. C99. */
#include "llmwsp_linalg.h"
#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void llmwsp_la_matmul(const double *A, const double *B, double *C, size_t m, size_t k, size_t n) {
    memset(C, 0, m * n * sizeof(double));
    for (size_t i = 0; i < m; i++)
        for (size_t p = 0; p < k; p++) {
            double a = A[i * k + p];
            if (a == 0) continue;
            const double *b = B + p * n; double *c = C + i * n;
            for (size_t j = 0; j < n; j++) c[j] += a * b[j];
        }
}

/* ------------------------------------------------------------ SVD (Jacobi a una parte) */
static int cmp_desc(const void *a, const void *b) {
    double x = ((const double *)a)[0], y = ((const double *)b)[0];
    return x < y ? 1 : x > y ? -1 : 0;
}

/* W: n righe (le colonne di A) di lunghezza m, ortogonalizzate sul posto. Restituisce 0 se converge. */
static int jacobi(double *W, size_t m, size_t n) {
    const double eps = 1e-15;
    for (int sweep = 0; sweep < 60; sweep++) {
        int rot = 0;
        for (size_t p = 0; p + 1 < n; p++)
            for (size_t q = p + 1; q < n; q++) {
                double *a = W + p * m, *b = W + q * m, al = 0, be = 0, ga = 0;
                for (size_t i = 0; i < m; i++) { al += a[i] * a[i]; be += b[i] * b[i]; ga += a[i] * b[i]; }
                if (ga == 0 || fabs(ga) <= eps * sqrt(al * be)) continue;
                rot = 1;
                double zeta = (be - al) / (2 * ga);
                double t = (zeta >= 0 ? 1.0 : -1.0) / (fabs(zeta) + sqrt(1 + zeta * zeta));
                double c = 1 / sqrt(1 + t * t), s = c * t;
                for (size_t i = 0; i < m; i++) { double x = a[i], y = b[i]; a[i] = c * x - s * y; b[i] = s * x + c * y; }
            }
        if (!rot) return 0;
    }
    return -1;
}

int llmwsp_la_svd(const double *A, size_t m, size_t n, double *s, double *U) {
    int tr = m < n;                       /* lavora sempre con righe-di-lunghezza >= numero di righe */
    size_t M = tr ? n : m, N = tr ? m : n;   /* W: N vettori di lunghezza M */
    double *W = (double *)malloc(M * N * sizeof(double));
    if (!W) return -1;
    if (!tr) { for (size_t i = 0; i < m; i++) for (size_t j = 0; j < n; j++) W[j * M + i] = A[i * n + j]; }
    else     { memcpy(W, A, m * n * sizeof(double)); }          /* le righe di A sono già i vettori */
    if (jacobi(W, M, N) < 0) { free(W); return -1; }
    double (*ord)[2] = (double (*)[2])malloc(N * sizeof *ord);
    if (!ord) { free(W); return -1; }
    for (size_t j = 0; j < N; j++) {
        double nn = 0; for (size_t i = 0; i < M; i++) nn += W[j * M + i] * W[j * M + i];
        ord[j][0] = sqrt(nn); ord[j][1] = (double)j;
    }
    qsort(ord, N, sizeof *ord, cmp_desc);
    for (size_t j = 0; j < N; j++) s[j] = ord[j][0];
    if (U && !tr) {
        /* colonne normalizzate; per valori nulli completa con Gram-Schmidt su vettori della base canonica */
        double tol = (N ? s[0] : 0) * 1e-13;
        for (size_t j = 0; j < N; j++) {
            size_t src = (size_t)ord[j][1];
            if (s[j] > tol && s[j] > 0) for (size_t i = 0; i < M; i++) U[i * N + j] = W[src * M + i] / s[j];
            else {
                for (size_t e = 0; e < M; e++) {
                    double *v = (double *)calloc(M, sizeof(double));
                    if (!v) { free(ord); free(W); return -1; }
                    v[e] = 1;
                    for (size_t k = 0; k < j; k++) { double d = 0; for (size_t i = 0; i < M; i++) d += v[i] * U[i * N + k]; for (size_t i = 0; i < M; i++) v[i] -= d * U[i * N + k]; }
                    double nn = 0; for (size_t i = 0; i < M; i++) nn += v[i] * v[i];
                    if (nn > 1e-6) { nn = sqrt(nn); for (size_t i = 0; i < M; i++) U[i * N + j] = v[i] / nn; free(v); break; }
                    free(v);
                }
            }
        }
    }
    free(ord); free(W);
    return 0;
}

/* ------------------------------------------------------------ autovalori non simmetrici */
typedef double complex cx;

static void givens(cx a, cx b, cx *c, cx *s) {
    double r = hypot(cabs(a), cabs(b));
    if (r == 0) { *c = 1; *s = 0; return; }
    *c = a / r; *s = b / r;
}

int llmwsp_la_eig(const double *A, size_t n, double *wr, double *wi) {
    if (n == 0) return 0;
    cx *H = (cx *)malloc(n * n * sizeof(cx));
    cx *cs = (cx *)malloc(2 * n * sizeof(cx));
    if (!H || !cs) { free(H); free(cs); return -1; }
    for (size_t i = 0; i < n * n; i++) H[i] = A[i];
    #define h(i, j) H[(i) * n + (j)]
    /* riduzione a Hessenberg con rotazioni di Givens (similitudine unitaria) */
    for (size_t j = 0; j + 2 < n; j++)
        for (size_t i = n - 1; i >= j + 2; i--) {
            cx a = h(i - 1, j), b = h(i, j);
            if (cabs(b) == 0) continue;
            cx c, s; givens(a, b, &c, &s);
            for (size_t k = 0; k < n; k++) { cx t = h(i - 1, k), u = h(i, k); h(i - 1, k) = conj(c) * t + conj(s) * u; h(i, k) = -s * t + c * u; }
            for (size_t k = 0; k < n; k++) { cx t = h(k, i - 1), u = h(k, i); h(k, i - 1) = c * t + s * u; h(k, i) = -conj(s) * t + conj(c) * u; }
        }
    const double eps = 2.2e-16;
    long hi = (long)n - 1; int iter = 0, total = 0;
    while (hi >= 0) {
        long l = hi;
        for (; l > 0; l--) {
            double sub = cabs(h(l, l - 1)), dg = cabs(h(l - 1, l - 1)) + cabs(h(l, l));
            if (sub <= eps * dg || sub < 1e-300) { h(l, l - 1) = 0; break; }
        }
        if (l == hi) { wr[hi] = creal(h(hi, hi)); wi[hi] = cimag(h(hi, hi)); hi--; iter = 0; continue; }
        if (++total > 60 * (int)n + 200) { free(H); free(cs); return -1; }
        ++iter;
        /* shift di Wilkinson: autovalore del blocco 2x2 finale più vicino a h(hi,hi) */
        cx a = h(hi - 1, hi - 1), b = h(hi - 1, hi), c = h(hi, hi - 1), d = h(hi, hi);
        cx tr2 = (a + d) / 2, disc = csqrt(((a - d) / 2) * ((a - d) / 2) + b * c);
        cx e1 = tr2 + disc, e2 = tr2 - disc;
        cx mu = cabs(e1 - d) < cabs(e2 - d) ? e1 : e2;
        if (iter % 10 == 0) mu += 0.75 * cabs(c);          /* shift eccezionale contro i cicli */
        for (long k = l; k <= hi; k++) h(k, k) -= mu;
        for (long k = l; k < hi; k++) {
            cx g, s; givens(h(k, k), h(k + 1, k), &g, &s);
            cs[2 * k] = g; cs[2 * k + 1] = s;
            for (long j = k; j <= hi; j++) { cx t = h(k, j), u = h(k + 1, j); h(k, j) = conj(g) * t + conj(s) * u; h(k + 1, j) = -s * t + g * u; }
        }
        for (long k = l; k < hi; k++) {
            cx g = cs[2 * k], s = cs[2 * k + 1];
            for (long i = l; i <= k + 1; i++) { cx t = h(i, k), u = h(i, k + 1); h(i, k) = g * t + s * u; h(i, k + 1) = -conj(s) * t + conj(g) * u; }
        }
        for (long k = l; k <= hi; k++) h(k, k) += mu;
    }
    #undef h
    free(H); free(cs);
    return 0;
}

/* ------------------------------------------------------------ angoli principali */
int llmwsp_la_principal_cosines(const double *A, size_t p, const double *B, size_t q, size_t m, double *cosv) {
    if (p > m || q > m) return -1;
    double *s1 = (double *)malloc((p + q) * sizeof(double));
    double *Ua = (double *)malloc(m * p * sizeof(double)), *Ub = (double *)malloc(m * q * sizeof(double));
    /* A, B sono passate row-major [m×p] e [m×q] */
    double *G = (double *)malloc(p * q * sizeof(double));
    int rc = -1;
    if (s1 && Ua && Ub && G &&
        llmwsp_la_svd(A, m, p, s1, Ua) == 0 && llmwsp_la_svd(B, m, q, s1 + p, Ub) == 0) {
        for (size_t i = 0; i < p; i++)
            for (size_t j = 0; j < q; j++) {
                double d = 0;
                for (size_t k = 0; k < m; k++) d += Ua[k * p + i] * Ub[k * q + j];
                G[i * q + j] = d;
            }
        size_t r = p < q ? p : q;
        rc = llmwsp_la_svd(G, p, q, cosv, NULL);
        for (size_t i = 0; rc == 0 && i < r; i++) if (cosv[i] > 1) cosv[i] = 1;
    }
    free(s1); free(Ua); free(Ub); free(G);
    return rc;
}
