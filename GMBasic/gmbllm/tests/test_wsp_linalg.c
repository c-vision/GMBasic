/* test_wsp_linalg.c — selfcheck dell'algebra lineare WSP (W4): matrici sintetiche a spettro NOTO. */
#include "llmwsp_linalg.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned long long rs = 88172645463325252ULL;
static double rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return ((double)(rs >> 11) / 9007199254740992.0) * 2 - 1; }

/* Q[n×k] ortonormale (Gram-Schmidt su vettori pseudo-casuali) */
static void orth(double *Q, size_t n, size_t k) {
    for (size_t j = 0; j < k; j++) {
        for (size_t i = 0; i < n; i++) Q[i * k + j] = rnd();
        for (int rep = 0; rep < 2; rep++)
            for (size_t c = 0; c < j; c++) {
                double d = 0; for (size_t i = 0; i < n; i++) d += Q[i * k + j] * Q[i * k + c];
                for (size_t i = 0; i < n; i++) Q[i * k + j] -= d * Q[i * k + c];
            }
        double nn = 0; for (size_t i = 0; i < n; i++) nn += Q[i * k + j] * Q[i * k + j];
        nn = sqrt(nn); for (size_t i = 0; i < n; i++) Q[i * k + j] /= nn;
    }
}

static void test_svd(void) {
    size_t m = 60, n = 40, r = 40;
    double *U = malloc(m * r * 8), *V = malloc(n * r * 8), *D = calloc(r * r, 8), *UD = malloc(m * r * 8), *Vt = malloc(r * n * 8), *A = malloc(m * n * 8);
    double sv[40]; orth(U, m, r); orth(V, n, r);
    for (size_t i = 0; i < r; i++) { sv[i] = i < 36 ? 100.0 * pow(1e-4, (double)i / 35) : 0.0; D[i * r + i] = sv[i]; }   /* 36 non nulli + 4 nulli */
    llmwsp_la_matmul(U, D, UD, m, r, r);
    for (size_t i = 0; i < r; i++) for (size_t j = 0; j < n; j++) Vt[i * n + j] = V[j * r + i];
    llmwsp_la_matmul(UD, Vt, A, m, r, n);
    double s[40], *Uo = malloc(m * n * 8);
    CHECK(llmwsp_la_svd(A, m, n, s, Uo) == 0, "svd rc");
    double maxerr = 0; for (size_t i = 0; i < 40; i++) maxerr = fmax(maxerr, fabs(s[i] - sv[i]) / 100.0);
    CHECK(maxerr < 1e-12, "errore SVD %g", maxerr);
    /* ortonormalità di U */
    double oe = 0;
    for (size_t a = 0; a < n; a++) for (size_t b = 0; b < n; b++) { double d = 0; for (size_t i = 0; i < m; i++) d += Uo[i * n + a] * Uo[i * n + b]; oe = fmax(oe, fabs(d - (a == b))); }
    CHECK(oe < 1e-10, "U non ortonormale %g", oe);
    /* matrice larga: stessi valori singolari */
    double *At = malloc(n * m * 8); for (size_t i = 0; i < m; i++) for (size_t j = 0; j < n; j++) At[j * m + i] = A[i * n + j];
    double s2[40]; CHECK(llmwsp_la_svd(At, n, m, s2, NULL) == 0, "svd wide");
    maxerr = 0; for (size_t i = 0; i < 40; i++) maxerr = fmax(maxerr, fabs(s2[i] - sv[i]) / 100.0);
    CHECK(maxerr < 1e-12, "errore SVD wide %g", maxerr);
    printf("SVD: errore max %.2e (relativo a sigma_max)\n", maxerr);
    free(U); free(V); free(D); free(UD); free(Vt); free(A); free(Uo); free(At);
}

static void test_eig(void) {
    size_t n = 24;
    double *B = calloc(n * n, 8), *Q = malloc(n * n * 8), *T = malloc(n * n * 8), *A = malloc(n * n * 8), *Qt = malloc(n * n * 8);
    double want[24][2]; size_t w = 0;
    /* 6 coppie complesse r*e^{±iθ} + 12 reali (di segno misto) */
    for (size_t b = 0; b < 6; b++) {
        double r = 0.5 + b, th = 0.3 + 0.4 * b, x = r * cos(th), y = r * sin(th);
        size_t i = 2 * b;
        B[i * n + i] = x; B[i * n + i + 1] = -y; B[(i + 1) * n + i] = y; B[(i + 1) * n + i + 1] = x;
        want[w][0] = x; want[w][1] = y; w++; want[w][0] = x; want[w][1] = -y; w++;
    }
    for (size_t k = 12; k < n; k++) { double v = (k % 3 == 0 ? -1 : 1) * (0.2 + 0.37 * (double)(k - 12)); B[k * n + k] = v; want[w][0] = v; want[w][1] = 0; w++; }
    orth(Q, n, n);
    for (size_t i = 0; i < n; i++) for (size_t j = 0; j < n; j++) Qt[i * n + j] = Q[j * n + i];
    llmwsp_la_matmul(Q, B, T, n, n, n); llmwsp_la_matmul(T, Qt, A, n, n, n);    /* A = Q B Q^T */
    double wr[24], wi[24];
    CHECK(llmwsp_la_eig(A, n, wr, wi) == 0, "eig rc");
    double got[24][2]; for (size_t i = 0; i < n; i++) { got[i][0] = wr[i]; got[i][1] = wi[i]; }
    /* abbinamento al più vicino (le coppie coniugate hanno parte reale uguale a meno del rumore) */
    double e = 0; int used[24] = {0};
    for (size_t i = 0; i < n; i++) {
        double best = 1e300; size_t bj = 0;
        for (size_t j = 0; j < n; j++) if (!used[j]) { double d = hypot(got[j][0] - want[i][0], got[j][1] - want[i][1]); if (d < best) { best = d; bj = j; } }
        used[bj] = 1; e = fmax(e, best);
    }
    CHECK(e < 1e-10, "errore autovalori %g", e);
    printf("EIG: errore max %.2e\n", e);
    free(B); free(Q); free(T); free(A); free(Qt);
}

static void test_cos(void) {
    size_t m = 12; double A[12 * 5], B[12 * 5];
    memset(A, 0, sizeof A); memset(B, 0, sizeof B);
    for (size_t j = 0; j < 5; j++) A[j * 5 + j] = 1;                     /* e0..e4 */
    size_t bi[5] = {0, 1, 2, 7, 8};
    for (size_t j = 0; j < 5; j++) B[bi[j] * 5 + j] = 1;                 /* e0,e1,e2,e7,e8 */
    double c[5];
    CHECK(llmwsp_la_principal_cosines(A, 5, B, 5, m, c) == 0, "cos rc");
    CHECK(fabs(c[0] - 1) < 1e-12 && fabs(c[1] - 1) < 1e-12 && fabs(c[2] - 1) < 1e-12 && c[3] < 1e-12 && c[4] < 1e-12,
          "coseni %g %g %g %g %g", c[0], c[1], c[2], c[3], c[4]);
    /* sottospazi con angolo noto: e0 vs cos(t)e0+sin(t)e1 */
    double a1[12] = {0}, b1[12] = {0}; a1[0] = 1; b1[0] = cos(0.7); b1[1] = sin(0.7);
    double c1[1]; CHECK(llmwsp_la_principal_cosines(a1, 1, b1, 1, m, c1) == 0 && fabs(c1[0] - cos(0.7)) < 1e-14, "angolo noto");
}

int main(void) {
    test_svd(); test_eig(); test_cos();
    printf(fails ? "WSP LINALG: %d FALLITI\n" : "WSP LINALG: OK\n", fails);
    return fails != 0;
}
