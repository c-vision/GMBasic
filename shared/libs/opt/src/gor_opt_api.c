/* gor_opt_api.c — adattatori scipy-like sopra il motore legacy gmi_opt / gmi_optfit. */
#include "gor_optimize.h"
#include "gmi_opt.h"
#include "gmi_optfit.h"
#include "simplex.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Utility                                                                   */
/* ------------------------------------------------------------------------- */

static void norm_method(const char *in, char *out, size_t cap)
{
    size_t i = 0, j = 0;
    if (in == NULL) { out[0] = '\0'; return; }
    while (in[i] != '\0' && j + 1 < cap) {
        unsigned char ch = (unsigned char)in[i++];
        if (ch == '-' || ch == '_' || ch == ' ') continue;
        out[j++] = (char)tolower(ch);
    }
    out[j] = '\0';
}

static int same(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* ------------------------------------------------------------------------- */
/* CurveFit                                                                  */
/* ------------------------------------------------------------------------- */

void gor_opt_curve_fit(gor_opt_model_fn model, void *ctx,
                       const gor_real *x, const gor_real *y, int nx,
                       const gor_real *p0, int np, int maxiter, gor_real tol,
                       gor_real *out)
{
    if (model == NULL || x == NULL || y == NULL || p0 == NULL || out == NULL) return;
    if (nx <= 0 || np <= 0) return;
    if (maxiter <= 0) maxiter = 100;
    if (tol <= 0.0) tol = 1e-8;
    gmi_lm_fit(model, ctx, x, y, nx, p0, np, maxiter, tol, out);
}

/* ------------------------------------------------------------------------- */
/* MinimizeScalar                                                            */
/* ------------------------------------------------------------------------- */

gor_real gor_opt_minimize_scalar(gor_real (*f)(gor_real, void *), void *ctx,
                                 gor_real a, gor_real b, const char *method)
{
    char m[32];

    if (f == NULL) return 0.0;
    norm_method(method, m, sizeof(m));
    if (m[0] == '\0' || same(m, "brent") || same(m, "brentmin"))
        return gmi_opt_brentmin(f, ctx, a, b);
    if (same(m, "golden") || same(m, "goldensection"))
        return gmi_opt_golden(f, a, b, 1e-10, 500, ctx);
    return gmi_opt_brentmin(f, ctx, a, b);
}

/* ------------------------------------------------------------------------- */
/* Minimize (dispatch per nome)                                              */
/* ------------------------------------------------------------------------- */

int gor_opt_minimize(gor_opt_fn f, void *ctx, const gor_real *x0, int n,
                     const char *method, const gor_real *lo, const gor_real *hi,
                     gor_real *out)
{
    char m[32];
    double *start;
    double *blo = NULL, *bhi = NULL;
    int i;

    if (f == NULL || x0 == NULL || out == NULL || n <= 0) return GOR_ERR_ARG;
    norm_method(method, m, sizeof(m));
    if (m[0] == '\0') strcpy(m, "neldermead");

    start = (double *)malloc((size_t)n * sizeof(double));
    if (start == NULL) return GOR_ERR_NOMEM;
    for (i = 0; i < n; i++) start[i] = x0[i];

    if (same(m, "nelder-mead") || same(m, "neldermead")) {
        gmi_opt_nelder_mead(f, start, n, 1e-10, 500, ctx, out);
    } else if (same(m, "bfgs")) {
        gmi_opt_bfgs(f, start, n, 1e-10, 500, ctx, out);
    } else if (same(m, "l-bfgs") || same(m, "lbfgs") || same(m, "lbfgsb")) {
        gmi_opt_lbfgs(f, start, n, 1e-10, 10, 500, ctx, out);
    } else if (same(m, "cg") || same(m, "conjugategradient")) {
        gmi_opt_cg(f, start, n, 1e-10, 500, ctx, out);
    } else if (same(m, "powell")) {
        gmi_opt_powell(f, ctx, x0, n, out);
    } else if (same(m, "tnc") || same(m, "cobyla") || same(m, "slsqp")) {
        if (lo == NULL || hi == NULL) {
            blo = (double *)malloc((size_t)n * sizeof(double));
            bhi = (double *)malloc((size_t)n * sizeof(double));
            if (blo == NULL || bhi == NULL) {
                free(start); free(blo); free(bhi);
                return GOR_ERR_NOMEM;
            }
            for (i = 0; i < n; i++) { blo[i] = -1e30; bhi[i] = 1e30; }
        } else {
            blo = (double *)lo;
            bhi = (double *)hi;
        }
        if (same(m, "tnc")) gmi_opt_tnc(f, ctx, x0, n, blo, bhi, out);
        else if (same(m, "cobyla")) gmi_opt_cobyla(f, ctx, x0, n, blo, bhi, out);
        else gmi_opt_slsqp(f, ctx, x0, n, blo, bhi, out);
        free(blo == (double *)lo ? NULL : blo);
        free(bhi == (double *)hi ? NULL : bhi);
    } else {
        free(start);
        return GOR_ERR_ARG;
    }

    free(start);
    return GOR_OK;
}

/* ------------------------------------------------------------------------- */
/* Simplex (max c'x, A x <= b, x >= 0) — parità con OPTIMIZE.SIMPLEX          */
/* ------------------------------------------------------------------------- */

int gor_opt_simplex(const gor_real *c, const gor_real *A, int m, int n,
                    const gor_real *b, gor_real *x)
{
    double *astd = NULL, *cstd = NULL, *cneg = NULL, *xfull = NULL, *y = NULL;
    int i, j, rc;
    int status;

    if (c == NULL || A == NULL || b == NULL || x == NULL || m <= 0 || n <= 0)
        return GOR_ERR_ARG;
    for (i = 0; i < m; i++) if (b[i] < 0.0) return GOR_ERR_ARG;

    astd = (double *)calloc((size_t)m * (size_t)(n + m), sizeof(double));
    cstd = (double *)calloc((size_t)(n + m), sizeof(double));
    cneg = (double *)calloc((size_t)n, sizeof(double));
    xfull = (double *)calloc((size_t)(n + m), sizeof(double));
    y = (double *)calloc((size_t)m, sizeof(double));
    if (astd == NULL || cstd == NULL || cneg == NULL || xfull == NULL || y == NULL) {
        free(astd); free(cstd); free(cneg); free(xfull); free(y);
        return GOR_ERR_NOMEM;
    }

    for (i = 0; i < m; i++) {
        for (j = 0; j < n; j++) astd[(size_t)i * (size_t)(n + m) + (size_t)j] =
            A[(size_t)i * (size_t)n + (size_t)j];
        astd[(size_t)i * (size_t)(n + m) + (size_t)(n + i)] = 1.0;
    }
    /* simplex_solve_std minimizza: per massimizzare si minimizza -c. */
    for (j = 0; j < n; j++) { cneg[j] = -c[j]; cstd[j] = cneg[j]; }

    status = simplex_solve_std(astd, m, n + m, b, cstd, 10000, xfull, y,
                               NULL, NULL);

    if (status == 0) for (j = 0; j < n; j++) x[j] = xfull[j];
    else for (j = 0; j < n; j++) x[j] = 0.0;

    rc = (status == 0) ? GOR_OK
       : (status == 1) ? GOR_ERR_INFEASIBLE
       : (status == 2) ? GOR_ERR_NOCONV
       : (status == 3) ? GOR_ERR_NOCONV
       : GOR_ERR_NOMEM;

    free(astd); free(cstd); free(cneg); free(xfull); free(y);
    return rc;
}

/* ------------------------------------------------------------------------- */
/* CholeskySolve                                                             */
/* ------------------------------------------------------------------------- */

int gor_opt_cholesky_solve(const gor_real *A, const gor_real *b, int n,
                           gor_real *x)
{
    double *L, *y;
    int i, j, k;
    int ok = 1;

    if (A == NULL || b == NULL || x == NULL || n <= 0) return GOR_ERR_ARG;
    L = (double *)calloc((size_t)n * (size_t)n, sizeof(double));
    y = (double *)calloc((size_t)n, sizeof(double));
    if (L == NULL || y == NULL) { free(L); free(y); return GOR_ERR_NOMEM; }

    for (i = 0; i < n && ok; i++) {
        for (j = 0; j <= i; j++) {
            double sum = A[(size_t)i * (size_t)n + (size_t)j];
            for (k = 0; k < j; k++)
                sum -= L[(size_t)i * (size_t)n + (size_t)k] *
                       L[(size_t)j * (size_t)n + (size_t)k];
            if (i == j) {
                if (sum <= 0.0) { ok = 0; break; }
                L[(size_t)i * (size_t)n + (size_t)j] = sqrt(sum);
            } else {
                L[(size_t)i * (size_t)n + (size_t)j] =
                    sum / L[(size_t)j * (size_t)n + (size_t)j];
            }
        }
    }

    if (ok) {
        for (i = 0; i < n; i++) {
            double sum = b[i];
            for (k = 0; k < i; k++)
                sum -= L[(size_t)i * (size_t)n + (size_t)k] * y[k];
            y[i] = sum / L[(size_t)i * (size_t)n + (size_t)i];
        }
        for (i = n - 1; i >= 0; i--) {
            double sum = y[i];
            for (k = i + 1; k < n; k++)
                sum -= L[(size_t)k * (size_t)n + (size_t)i] * x[k];
            x[i] = sum / L[(size_t)i * (size_t)n + (size_t)i];
        }
    } else {
        for (i = 0; i < n; i++) x[i] = 0.0;
    }

    free(L);
    free(y);
    return ok ? GOR_OK : GOR_ERR_SINGULAR;
}
