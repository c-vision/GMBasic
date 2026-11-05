/* gs_kalman.c — filtri di Kalman multivariati. */
#include "gs_kalman.h"
#include "gm_linalg.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void mat_mul(int a, int b, int c, const gc_real *A, const gc_real *B, gc_real *out)
{
    int i, j, k;
    for (i = 0; i < a; i++)
        for (j = 0; j < c; j++) {
            gc_real s = 0.0;
            for (k = 0; k < b; k++) s += A[i * b + k] * B[k * c + j];
            out[i * c + j] = s;
        }
}

static void mat_transpose(int m, int n, const gc_real *A, gc_real *out)
{
    int i, j;
    for (i = 0; i < m; i++) for (j = 0; j < n; j++) out[j * m + i] = A[i * n + j];
}

gc_status gs_kalman_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                          const gc_real *F, const gc_real *H, const gc_real *Q,
                          const gc_real *R, const gc_real *x0, const gc_real *P0,
                          gc_real *xs, gc_real *ps)
{
    gc_real *x, *P, *y, *S, *K, *PHT, *I_KH, *tmp1, *tmp2, *tmp3, *tmp4;
    int i, t;
    if (!zs || !F || !H || !Q || !R || !x0 || !P0 || !xs || !ps || n < 1 || dim_x < 1 || dim_z < 1) return GC_ERR_ARG;
    x = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    P = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    y = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
    S = (gc_real *)malloc((size_t)dim_z * dim_z * sizeof(gc_real));
    K = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
    PHT = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
    I_KH = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp1 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp2 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp3 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp4 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    if (!x || !P || !y || !S || !K || !PHT || !I_KH || !tmp1 || !tmp2 || !tmp3 || !tmp4) {
        free(x); free(P); free(y); free(S); free(K); free(PHT); free(I_KH);
        free(tmp1); free(tmp2); free(tmp3); free(tmp4);
        return GC_ERR_NO_MEM;
    }
    memcpy(x, x0, (size_t)dim_x * sizeof(gc_real));
    memcpy(P, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
    for (t = 0; t < n; t++) {
        /* predict: x = Fx, P = FPF' + Q */
        mat_mul(dim_x, dim_x, 1, F, x, tmp1);
        memcpy(x, tmp1, (size_t)dim_x * sizeof(gc_real));
        mat_mul(dim_x, dim_x, dim_x, F, P, tmp1);
        mat_transpose(dim_x, dim_x, F, tmp3);   /* tmp3 = F' (dim_x x dim_x) */
        mat_mul(dim_x, dim_x, dim_x, tmp1, tmp3, tmp2);
        for (i = 0; i < dim_x * dim_x; i++) P[i] = tmp2[i] + Q[i];
        /* update: y = z - Hx, S = H P H' + R, K = P H' S^-1, x += Ky,
         * P = (I-KH)P(I-KH)' + K R K' (Joseph) */
        {
            gc_real *Ht = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            gc_real *Kt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            gc_real *one = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
            int col;
            if (!Ht || !Kt || !one) {
                free(Ht); free(Kt); free(one);
                free(x); free(P); free(y); free(S); free(K); free(PHT); free(I_KH);
                free(tmp1); free(tmp2); free(tmp3); free(tmp4);
                return GC_ERR_NO_MEM;
            }
            mat_transpose(dim_z, dim_x, H, Ht);
            mat_mul(dim_x, dim_x, dim_z, P, Ht, PHT);          /* PHT = P H' (dim_x x dim_z) */
            mat_mul(dim_z, dim_x, dim_z, H, PHT, S);           /* S = H PHT + R */
            for (i = 0; i < dim_z * dim_z; i++) S[i] += R[i];
            /* K = PHT * S^-1: risolvi S' X = PHT' per colonna di K' */
            {
                gc_real *PHTt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
                if (!PHTt) { free(Ht); free(Kt); free(one); free(PHTt);
                    free(x); free(P); free(y); free(S); free(K); free(PHT); free(I_KH);
                    free(tmp1); free(tmp2); free(tmp3); free(tmp4);
                    return GC_ERR_NO_MEM; }
                mat_transpose(dim_x, dim_z, PHT, PHTt);   /* PHTt = PHT' (dim_z x dim_x) */
                for (col = 0; col < dim_x; col++) {
                    int r;
                    for (r = 0; r < dim_z; r++) one[r] = PHTt[r * dim_x + col];
                    if (gm_linalg_solve(dim_z, S, one, Kt + col * dim_z) != 0) {
                        free(PHTt); free(Ht); free(Kt); free(one);
                        free(x); free(P); free(y); free(S); free(K); free(PHT); free(I_KH);
                        free(tmp1); free(tmp2); free(tmp3); free(tmp4);
                        return GC_ERR_SINGULAR;
                    }
                }
                for (i = 0; i < dim_x; i++) {
                    int j2;
                    for (j2 = 0; j2 < dim_z; j2++) K[i * dim_z + j2] = Kt[j2 * dim_x + i];
                }
                free(PHTt);
            }
            mat_mul(dim_z, dim_x, 1, H, x, tmp3);
            for (i = 0; i < dim_z; i++) y[i] = zs[t * dim_z + i] - tmp3[i];
            mat_mul(dim_x, dim_z, 1, K, y, tmp1);
            for (i = 0; i < dim_x; i++) x[i] += tmp1[i];
            mat_mul(dim_x, dim_z, dim_x, K, H, I_KH);
            for (i = 0; i < dim_x * dim_x; i++)
                I_KH[i] = (i % (dim_x + 1) == 0 ? 1.0 : 0.0) - I_KH[i];
            mat_mul(dim_x, dim_x, dim_x, I_KH, P, tmp1);
            mat_transpose(dim_x, dim_x, I_KH, tmp3);
            mat_mul(dim_x, dim_x, dim_x, tmp1, tmp3, tmp2);
            mat_mul(dim_x, dim_z, dim_z, K, R, tmp1);
            mat_transpose(dim_x, dim_z, K, tmp3);
            mat_mul(dim_x, dim_z, dim_x, tmp1, tmp3, tmp4);
            for (i = 0; i < dim_x * dim_x; i++) P[i] = tmp2[i] + tmp4[i];
            free(Ht); free(Kt); free(one);
        }
        memcpy(&xs[t * dim_x], x, (size_t)dim_x * sizeof(gc_real));
        memcpy(&ps[t * dim_x * dim_x], P, (size_t)dim_x * dim_x * sizeof(gc_real));
    }
    free(x); free(P); free(y); free(S); free(K); free(PHT); free(I_KH);
    free(tmp1); free(tmp2); free(tmp3); free(tmp4);
    return GC_OK;
}
gc_status gs_kalman_rts(const gc_real *xs, const gc_real *ps, int n, int dim_x,
                        const gc_real *F, const gc_real *Q, gc_real *xs_s,
                        gc_real *ps_s)
{
    int t, i;
    if (!xs || !ps || !F || !Q || !xs_s || !ps_s || n < 1 || dim_x < 1) return GC_ERR_ARG;
    memcpy(&xs_s[(n - 1) * dim_x], &xs[(n - 1) * dim_x], (size_t)dim_x * sizeof(gc_real));
    memcpy(&ps_s[(n - 1) * dim_x * dim_x], &ps[(n - 1) * dim_x * dim_x], (size_t)dim_x * dim_x * sizeof(gc_real));
    for (t = n - 2; t >= 0; t--) {
        gc_real *Fp = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *Ppred = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *xpred = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
        gc_real *Ft = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *tmp1 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *tmp2 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *K = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *diff = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
        gc_real *tmp3 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
        gc_real *I = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *tmp4 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        if (!Fp || !Ppred || !xpred || !Ft || !tmp1 || !tmp2 || !K || !diff || !tmp3 || !I || !tmp4) {
            free(Fp); free(Ppred); free(xpred); free(Ft); free(tmp1); free(tmp2);
            free(K); free(diff); free(tmp3); free(I); free(tmp4);
            return GC_ERR_NO_MEM;
        }
        /* xpred = F xs[t+1], Ppred = F P[t+1] F' + Q */
        memcpy(Fp, F, (size_t)dim_x * dim_x * sizeof(gc_real));
        mat_mul(dim_x, dim_x, 1, F, &xs[(t + 1) * dim_x], xpred);
        mat_mul(dim_x, dim_x, dim_x, F, &ps[(t + 1) * dim_x * dim_x], tmp1);
        mat_transpose(dim_x, dim_x, F, Ft);
        mat_mul(dim_x, dim_x, dim_x, tmp1, Ft, tmp2);
        for (i = 0; i < dim_x * dim_x; i++) Ppred[i] = tmp2[i] + Q[i];
        /* K = P[t+1] F' Ppred^-1: risolvi Ppred' X = (P[t+1] F')' */
        mat_mul(dim_x, dim_x, dim_x, &ps[(t + 1) * dim_x * dim_x], Ft, tmp1);  /* P F' */
        {
            gc_real *PFt_t = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
            gc_real *one = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            gc_real *Kt = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
            int col;
            if (!PFt_t || !one || !Kt) {
                free(PFt_t); free(one); free(Kt); free(Fp); free(Ppred); free(xpred);
                free(Ft); free(tmp1); free(tmp2); free(K); free(diff); free(tmp3); free(I); free(tmp4);
                return GC_ERR_NO_MEM;
            }
            mat_transpose(dim_x, dim_x, tmp1, PFt_t);
            for (col = 0; col < dim_x; col++) {
                int r;
                for (r = 0; r < dim_x; r++) one[r] = PFt_t[r * dim_x + col];
                if (gm_linalg_solve(dim_x, Ppred, one, Kt + col * dim_x) != 0) {
                    free(PFt_t); free(one); free(Kt); free(Fp); free(Ppred); free(xpred);
                    free(Ft); free(tmp1); free(tmp2); free(K); free(diff); free(tmp3); free(I); free(tmp4);
                    return GC_ERR_SINGULAR;
                }
            }
            for (i = 0; i < dim_x * dim_x; i++) K[i] = Kt[i];
            free(PFt_t); free(one); free(Kt);
        }
        /* x_s[t] = xs[t] + K (x_s[t+1] - xpred) */
        for (i = 0; i < dim_x; i++) diff[i] = xs_s[(t + 1) * dim_x + i] - xpred[i];
        mat_mul(dim_x, dim_x, 1, K, diff, tmp3);
        for (i = 0; i < dim_x; i++) xs_s[t * dim_x + i] = xs[t * dim_x + i] + tmp3[i];
        /* P_s[t] = P[t] + K (P_s[t+1] - Ppred) K' */
        for (i = 0; i < dim_x * dim_x; i++)
            tmp1[i] = ps_s[(t + 1) * dim_x * dim_x + i] - Ppred[i];
        mat_mul(dim_x, dim_x, dim_x, K, tmp1, tmp2);
        mat_transpose(dim_x, dim_x, K, Ft);
        mat_mul(dim_x, dim_x, dim_x, tmp2, Ft, tmp4);
        for (i = 0; i < dim_x * dim_x; i++)
            ps_s[t * dim_x * dim_x + i] = ps[t * dim_x * dim_x + i] + tmp4[i];
        free(Fp); free(Ppred); free(xpred); free(Ft); free(tmp1); free(tmp2);
        free(K); free(diff); free(tmp3); free(I); free(tmp4);
    }
    return GC_OK;
}

gc_status gs_kalman_ekf_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                              gs_kf_fx fx, gs_kf_fx_jac fxj,
                              gs_kf_hx hx, gs_kf_hx_jac hxj,
                              const gc_real *Q, const gc_real *R,
                              const gc_real *x0, const gc_real *P0, void *ctx,
                              gc_real *xs, gc_real *ps)
{
    gc_real *x, *P, *F, *H, *tmp1, *tmp2, *tmp3, *zbar, *y;
    int i, t;
    if (!zs || !fx || !fxj || !hx || !hxj || !Q || !R || !x0 || !P0 || !xs || !ps) return GC_ERR_ARG;
    x = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    P = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    F = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    H = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
    tmp1 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp2 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp3 = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
    zbar = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
    y = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
    if (!x || !P || !F || !H || !tmp1 || !tmp2 || !tmp3 || !zbar || !y) {
        free(x); free(P); free(F); free(H); free(tmp1); free(tmp2); free(tmp3); free(zbar); free(y);
        return GC_ERR_NO_MEM;
    }
    memcpy(x, x0, (size_t)dim_x * sizeof(gc_real));
    memcpy(P, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
    for (t = 0; t < n; t++) {
        gc_real *Ht = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
        gc_real *S = (gc_real *)malloc((size_t)dim_z * dim_z * sizeof(gc_real));
        gc_real *PHT = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
        gc_real *K = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
        gc_real *IKH = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *tmp4 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        if (!Ht || !S || !PHT || !K || !IKH || !tmp4) {
            free(Ht); free(S); free(PHT); free(K); free(IKH); free(tmp4);
            free(x); free(P); free(F); free(H); free(tmp1); free(tmp2); free(tmp3); free(zbar); free(y);
            return GC_ERR_NO_MEM;
        }
        /* predict */
        fx(x, NULL, x, ctx);
        fxj(x, NULL, F, ctx);
        mat_mul(dim_x, dim_x, dim_x, F, P, tmp1);
        mat_transpose(dim_x, dim_x, F, tmp3);
        mat_mul(dim_x, dim_x, dim_x, tmp1, tmp3, tmp2);
        for (i = 0; i < dim_x * dim_x; i++) P[i] = tmp2[i] + Q[i];
        /* update */
        hxj(x, H, ctx);
        mat_transpose(dim_z, dim_x, H, Ht);
        mat_mul(dim_x, dim_x, dim_z, P, Ht, PHT);
        mat_mul(dim_z, dim_x, dim_z, H, PHT, S);
        for (i = 0; i < dim_z * dim_z; i++) S[i] += R[i];
        {
            gc_real *PHTt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            gc_real *one = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
            gc_real *Kt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            int col;
            if (!PHTt || !one || !Kt) {
                free(PHTt); free(one); free(Kt); free(Ht); free(S); free(PHT); free(K); free(IKH); free(tmp4);
                free(x); free(P); free(F); free(H); free(tmp1); free(tmp2); free(tmp3); free(zbar); free(y);
                return GC_ERR_NO_MEM;
            }
            mat_transpose(dim_x, dim_z, PHT, PHTt);
            for (col = 0; col < dim_x; col++) {
                int r;
                for (r = 0; r < dim_z; r++) one[r] = PHTt[r * dim_x + col];
                if (gm_linalg_solve(dim_z, S, one, Kt + col * dim_z) != 0) {
                    free(PHTt); free(one); free(Kt); free(Ht); free(S); free(PHT); free(K); free(IKH); free(tmp4);
                    free(x); free(P); free(F); free(H); free(tmp1); free(tmp2); free(tmp3); free(zbar); free(y);
                    return GC_ERR_SINGULAR;
                }
            }
            for (i = 0; i < dim_x; i++) {
                int j2;
                for (j2 = 0; j2 < dim_z; j2++) K[i * dim_z + j2] = Kt[j2 * dim_x + i];
            }
            free(PHTt); free(one); free(Kt);
        }
        hx(x, zbar, ctx);
        for (i = 0; i < dim_z; i++) y[i] = zs[t * dim_z + i] - zbar[i];
        mat_mul(dim_x, dim_z, 1, K, y, tmp1);
        for (i = 0; i < dim_x; i++) x[i] += tmp1[i];
        mat_mul(dim_x, dim_z, dim_x, K, H, IKH);
        for (i = 0; i < dim_x * dim_x; i++)
            IKH[i] = (i % (dim_x + 1) == 0 ? 1.0 : 0.0) - IKH[i];
        mat_mul(dim_x, dim_x, dim_x, IKH, P, tmp1);
        mat_transpose(dim_x, dim_x, IKH, tmp3);
        mat_mul(dim_x, dim_x, dim_x, tmp1, tmp3, tmp2);
        mat_mul(dim_x, dim_z, dim_x, K, R, tmp1);
        mat_transpose(dim_x, dim_z, K, tmp3);
        mat_mul(dim_x, dim_z, dim_x, tmp1, tmp3, tmp4);
        for (i = 0; i < dim_x * dim_x; i++) P[i] = tmp2[i] + tmp4[i];
        memcpy(&xs[t * dim_x], x, (size_t)dim_x * sizeof(gc_real));
        memcpy(&ps[t * dim_x * dim_x], P, (size_t)dim_x * dim_x * sizeof(gc_real));
        free(Ht); free(S); free(PHT); free(K); free(IKH); free(tmp4);
    }
    free(x); free(P); free(F); free(H); free(tmp1); free(tmp2); free(tmp3); free(zbar); free(y);
    return GC_OK;
}

/* --- UKF: sigma points (merwe scaled) --- */

static void ukf_sigmas(const gc_real *x, const gc_real *P, int dim_x, gc_real alpha,
                       gc_real beta, gc_real kappa, gc_real *sigmas, gc_real *Wm, gc_real *Wc,
                       gc_real *lambda)
{
    int i, j;
    gc_real n = (gc_real)dim_x;
    gc_real lam = alpha * alpha * (n + kappa) - n;
    gc_real *A, *L;
    A = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    L = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    for (i = 0; i < dim_x * dim_x; i++) A[i] = (n + lam) * P[i];
    if (gm_linalg_cholesky(dim_x, A) != 0) { free(A); free(L); return; }
    /* A ora e' L (lower). */
    *lambda = lam;
    for (i = 0; i < dim_x; i++) {
        for (j = 0; j < dim_x; j++) sigmas[(i + 1) * dim_x + j] = x[j] + A[j * dim_x + i];
        for (j = 0; j < dim_x; j++) sigmas[(dim_x + 1 + i) * dim_x + j] = x[j] - A[j * dim_x + i];
    }
    for (j = 0; j < dim_x; j++) sigmas[j] = x[j];
    Wm[0] = lam / (n + lam);
    Wc[0] = lam / (n + lam) + (1.0 - alpha * alpha + beta);
    for (i = 1; i <= 2 * dim_x; i++) { Wm[i] = 1.0 / (2.0 * (n + lam)); Wc[i] = Wm[i]; }
    (void)L;
    free(A); free(L);
}

gc_status gs_kalman_ukf_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                              gs_kf_fx fx, gs_kf_hx hx,
                              const gc_real *Q, const gc_real *R,
                              const gc_real *x0, const gc_real *P0,
                              gc_real alpha, gc_real beta, gc_real kappa,
                              void *ctx, gc_real *xs, gc_real *ps)
{
    int ns = 2 * dim_x + 1;
    gc_real *x, *P, *sig, *Wm, *Wc, *x2, *P2;
    int i, t, j;
    gc_real lam = 0.0;
    if (!zs || !fx || !hx || !Q || !R || !x0 || !P0 || !xs || !ps) return GC_ERR_ARG;
    x = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    P = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    sig = (gc_real *)malloc((size_t)ns * dim_x * sizeof(gc_real));
    Wm = (gc_real *)malloc((size_t)ns * sizeof(gc_real));
    Wc = (gc_real *)malloc((size_t)ns * sizeof(gc_real));
    x2 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    P2 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    if (!x || !P || !sig || !Wm || !Wc || !x2 || !P2) {
        free(x); free(P); free(sig); free(Wm); free(Wc); free(x2); free(P2);
        return GC_ERR_NO_MEM;
    }
    memcpy(x, x0, (size_t)dim_x * sizeof(gc_real));
    memcpy(P, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
    for (t = 0; t < n; t++) {
        gc_real *zsig = (gc_real *)malloc((size_t)ns * dim_z * sizeof(gc_real));
        gc_real *zmean = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
        gc_real *Pz = (gc_real *)malloc((size_t)dim_z * dim_z * sizeof(gc_real));
        gc_real *Pxz = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
        gc_real *K = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
        gc_real *tmp1 = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
        gc_real *tmp2 = (gc_real *)malloc((size_t)dim_z * dim_z * sizeof(gc_real));
        gc_real *tmp3 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *y = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
        if (!zsig || !zmean || !Pz || !Pxz || !K || !tmp1 || !tmp2 || !tmp3 || !y) {
            free(zsig); free(zmean); free(Pz); free(Pxz); free(K); free(tmp1); free(tmp2); free(tmp3); free(y);
            free(x); free(P); free(sig); free(Wm); free(Wc); free(x2); free(P2);
            return GC_ERR_NO_MEM;
        }
        ukf_sigmas(x, P, dim_x, alpha, beta, kappa, sig, Wm, Wc, &lam);
        /* predict: propaga i sigma points */
        for (i = 0; i < ns; i++) fx(&sig[i * dim_x], NULL, &sig[i * dim_x], ctx);
        for (j = 0; j < dim_x; j++) { x2[j] = 0.0; for (i = 0; i < ns; i++) x2[j] += Wm[i] * sig[i * dim_x + j]; }
        for (i = 0; i < dim_x * dim_x; i++) P2[i] = 0.0;
        for (i = 0; i < ns; i++) {
            gc_real *d = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            for (j = 0; j < dim_x; j++) d[j] = sig[i * dim_x + j] - x2[j];
            for (j = 0; j < dim_x; j++) {
                int k2;
                for (k2 = 0; k2 < dim_x; k2++) P2[j * dim_x + k2] += Wc[i] * d[j] * d[k2];
            }
            free(d);
        }
        for (i = 0; i < dim_x * dim_x; i++) P2[i] += Q[i];
        /* update: propaga in misura */
        for (i = 0; i < ns; i++) hx(&sig[i * dim_x], &zsig[i * dim_z], ctx);
        for (j = 0; j < dim_z; j++) { zmean[j] = 0.0; for (i = 0; i < ns; i++) zmean[j] += Wm[i] * zsig[i * dim_z + j]; }
        for (i = 0; i < dim_z * dim_z; i++) Pz[i] = 0.0;
        for (i = 0; i < dim_x * dim_z; i++) Pxz[i] = 0.0;
        for (i = 0; i < ns; i++) {
            gc_real *dz = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
            gc_real *dx = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            for (j = 0; j < dim_z; j++) dz[j] = zsig[i * dim_z + j] - zmean[j];
            for (j = 0; j < dim_x; j++) dx[j] = sig[i * dim_x + j] - x2[j];
            for (j = 0; j < dim_z; j++) {
                int k2;
                for (k2 = 0; k2 < dim_z; k2++) Pz[j * dim_z + k2] += Wc[i] * dz[j] * dz[k2];
            }
            for (j = 0; j < dim_x; j++) {
                int k2;
                for (k2 = 0; k2 < dim_z; k2++) Pxz[j * dim_z + k2] += Wc[i] * dx[j] * dz[k2];
            }
            free(dz); free(dx);
        }
        for (i = 0; i < dim_z * dim_z; i++) Pz[i] += R[i];
        /* K = Pxz Pz^-1 */
        {
            gc_real *Pzt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            gc_real *one = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
            gc_real *Kt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            int col;
            if (!Pzt || !one || !Kt) {
                free(Pzt); free(one); free(Kt); free(zsig); free(zmean); free(Pz); free(Pxz); free(K);
                free(tmp1); free(tmp2); free(tmp3); free(y);
                free(x); free(P); free(sig); free(Wm); free(Wc); free(x2); free(P2);
                return GC_ERR_NO_MEM;
            }
            mat_transpose(dim_x, dim_z, Pxz, Pzt);
            for (col = 0; col < dim_x; col++) {
                int r;
                for (r = 0; r < dim_z; r++) one[r] = Pzt[r * dim_x + col];
                if (gm_linalg_solve(dim_z, Pz, one, Kt + col * dim_z) != 0) {
                    free(Pzt); free(one); free(Kt); free(zsig); free(zmean); free(Pz); free(Pxz); free(K);
                    free(tmp1); free(tmp2); free(tmp3); free(y);
                    free(x); free(P); free(sig); free(Wm); free(Wc); free(x2); free(P2);
                    return GC_ERR_SINGULAR;
                }
            }
            for (i = 0; i < dim_x; i++) {
                int j2;
                for (j2 = 0; j2 < dim_z; j2++) K[i * dim_z + j2] = Kt[j2 * dim_x + i];
            }
            free(Pzt); free(one); free(Kt);
        }
        for (i = 0; i < dim_z; i++) y[i] = zs[t * dim_z + i] - zmean[i];
        for (i = 0; i < dim_x; i++) x[i] = x2[i];
        mat_mul(dim_x, dim_z, 1, K, y, tmp1);
        for (i = 0; i < dim_x; i++) x[i] += tmp1[i];
        mat_mul(dim_x, dim_z, dim_x, K, Pz, tmp1);
        mat_transpose(dim_x, dim_z, K, tmp2);
        mat_mul(dim_x, dim_z, dim_x, tmp1, tmp2, tmp3);
        for (i = 0; i < dim_x * dim_x; i++) P[i] = P2[i] - tmp3[i];
        memcpy(&xs[t * dim_x], x, (size_t)dim_x * sizeof(gc_real));
        memcpy(&ps[t * dim_x * dim_x], P, (size_t)dim_x * dim_x * sizeof(gc_real));
        free(zsig); free(zmean); free(Pz); free(Pxz); free(K); free(tmp1); free(tmp2); free(tmp3); free(y);
    }
    free(x); free(P); free(sig); free(Wm); free(Wc); free(x2); free(P2);
    return GC_OK;
}

gc_status gs_kalman_enkf_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                               const gc_real *F, const gc_real *H,
                               const gc_real *Q, const gc_real *R,
                               const gc_real *x0, const gc_real *P0,
                               int nens, unsigned seed, gc_real *xs)
{
    gc_real *ens;
    int t, i, j, e;
    unsigned s = seed ? seed : 12345u;
    if (!zs || !F || !H || !Q || !R || !x0 || !P0 || !xs || n < 1 || nens < 2) return GC_ERR_ARG;
    ens = (gc_real *)malloc((size_t)nens * dim_x * sizeof(gc_real));
    if (!ens) return GC_ERR_NO_MEM;
    /* init: campiona da N(x0, P0) con fattorizzazione di Cholesky */
    {
        gc_real *A = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *L = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
        gc_real *tmp = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
        if (!A || !L || !tmp) { free(A); free(L); free(tmp); free(ens); return GC_ERR_NO_MEM; }
        memcpy(A, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
        if (gm_linalg_cholesky(dim_x, A) != 0) { free(A); free(L); free(tmp); free(ens); return GC_ERR_SINGULAR; }
        for (e = 0; e < nens; e++) {
            for (j = 0; j < dim_x; j++) {
                gc_real rn = (gc_real)((double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0 - 0.5) * 4.0;
                tmp[j] = rn;
            }
            for (i = 0; i < dim_x; i++) {
                gc_real v = x0[i];
                for (j = 0; j <= i; j++) v += A[i * dim_x + j] * tmp[j];
                ens[e * dim_x + i] = v;
            }
        }
        free(A); free(L); free(tmp);
    }
    for (t = 0; t < n; t++) {
        gc_real *mean = (gc_real *)calloc((size_t)dim_x, sizeof(gc_real));
        gc_real *innov = (gc_real *)malloc((size_t)nens * dim_z * sizeof(gc_real));
        gc_real *Ht = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
        if (!mean || !innov || !Ht) { free(mean); free(innov); free(Ht); free(ens); return GC_ERR_NO_MEM; }
        /* predict */
        for (e = 0; e < nens; e++) {
            gc_real *xe = &ens[e * dim_x];
            gc_real *nx = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            gc_real *noise = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            if (!nx || !noise) { free(nx); free(noise); free(mean); free(innov); free(Ht); free(ens); return GC_ERR_NO_MEM; }
            mat_mul(dim_x, dim_x, 1, F, xe, nx);
            for (j = 0; j < dim_x; j++) {
                gc_real rn = (gc_real)((double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0 - 0.5) * 4.0;
                noise[j] = rn * sqrt(Q[j * dim_x + j] > 0 ? Q[j * dim_x + j] : 1.0);
            }
            for (j = 0; j < dim_x; j++) xe[j] = nx[j] + noise[j];
            free(nx); free(noise);
        }
        /* update: ensemble Kalman gain = P H' (H P H' + R)^-1 con R diagonalized
         * approach (perturbato). */
        mat_transpose(dim_z, dim_x, H, Ht);
        for (e = 0; e < nens; e++) {
            gc_real *xe = &ens[e * dim_x];
            gc_real z_pred = 0.0;
            for (i = 0; i < dim_x; i++) z_pred += H[i] * xe[i];
            innov[e * dim_z] = zs[t * dim_z] - z_pred
                + (gc_real)((double)((s = s * 1103515245u + 12345u) % 100000u) / 100000.0 - 0.5) * 4.0 * sqrt(R[0] > 0 ? R[0] : 1.0);
        }
        /* Kalman gain (scalare per dim_z=1): K = P H' / (H P H' + R) */
        {
            gc_real *P = (gc_real *)calloc((size_t)dim_x * dim_x, sizeof(gc_real));
            gc_real *HPH = (gc_real *)calloc((size_t)dim_z * dim_z, sizeof(gc_real));
            gc_real *PHT = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
            gc_real *K = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
            if (!P || !HPH || !PHT || !K) { free(P); free(HPH); free(PHT); free(K); free(mean); free(innov); free(Ht); free(ens); return GC_ERR_NO_MEM; }
            for (e = 0; e < nens; e++) {
                gc_real *xe = &ens[e * dim_x];
                for (i = 0; i < dim_x; i++)
                    for (j = 0; j < dim_x; j++) P[i * dim_x + j] += xe[i] * xe[j];
            }
            for (i = 0; i < dim_x * dim_x; i++) P[i] /= nens;
            mat_mul(dim_x, dim_x, dim_z, P, Ht, PHT);
            mat_mul(dim_z, dim_x, dim_z, H, PHT, HPH);
            for (i = 0; i < dim_z * dim_z; i++) HPH[i] += R[i];
            {
                gc_real *PHTt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
                gc_real *one = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
                gc_real *Kt = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
                int col;
                if (!PHTt || !one || !Kt) {
                    free(PHTt); free(one); free(Kt); free(P); free(HPH); free(PHT); free(K);
                    free(mean); free(innov); free(Ht); free(ens); return GC_ERR_NO_MEM;
                }
                mat_transpose(dim_x, dim_z, PHT, PHTt);
                for (col = 0; col < dim_x; col++) {
                    int r;
                    for (r = 0; r < dim_z; r++) one[r] = PHTt[r * dim_x + col];
                    gm_linalg_solve(dim_z, HPH, one, Kt + col * dim_z);
                }
                for (i = 0; i < dim_x; i++) {
                    int j2;
                    for (j2 = 0; j2 < dim_z; j2++) K[i * dim_z + j2] = Kt[j2 * dim_x + i];
                }
                free(PHTt); free(one); free(Kt);
            }
            for (e = 0; e < nens; e++) {
                gc_real *xe = &ens[e * dim_x];
                for (i = 0; i < dim_x; i++) xe[i] += K[i] * innov[e];
            }
            free(P); free(HPH); free(PHT); free(K);
        }
        for (i = 0; i < dim_x; i++) { mean[i] = 0.0; for (e = 0; e < nens; e++) mean[i] += ens[e * dim_x + i]; mean[i] /= nens; }
        memcpy(&xs[t * dim_x], mean, (size_t)dim_x * sizeof(gc_real));
        free(mean); free(innov); free(Ht);
    }
    free(ens);
    return GC_OK;
}

gc_status gs_kalman_info_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                               const gc_real *F, const gc_real *H,
                               const gc_real *Q, const gc_real *R,
                               const gc_real *x0, const gc_real *P0,
                               gc_real *xs, gc_real *ps)
{
    gc_real *Y, *y, *P, *x, *Fm, *tmp1, *tmp2, *tmp3, *tmp4, *tmp5;
    int i, t;
    if (!zs || !F || !H || !Q || !R || !x0 || !P0 || !xs || !ps) return GC_ERR_ARG;
    P = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    Y = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    y = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    x = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    Fm = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp1 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp2 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp3 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    tmp4 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    tmp5 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    if (!P || !Y || !y || !x || !Fm || !tmp1 || !tmp2 || !tmp3 || !tmp4 || !tmp5) {
        free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5);
        return GC_ERR_NO_MEM;
    }
    memcpy(P, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
    memcpy(x, x0, (size_t)dim_x * sizeof(gc_real));
    for (t = 0; t < n; t++) {
        /* Y = P^-1, y = Y x */
        memcpy(Fm, P, (size_t)dim_x * dim_x * sizeof(gc_real));
        if (gm_linalg_solve(dim_x, Fm, x, tmp4) != 0) { free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5); return GC_ERR_SINGULAR; }
        memcpy(y, tmp4, (size_t)dim_x * sizeof(gc_real));   /* y = P^-1 x */
        memcpy(Fm, P, (size_t)dim_x * dim_x * sizeof(gc_real));
        {
            gc_real *Pinv = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
            gc_real *one = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            gc_real *col = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            int c;
            if (!Pinv || !one || !col) {
                free(Pinv); free(one); free(col); free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5);
                return GC_ERR_NO_MEM;
            }
            for (c = 0; c < dim_x; c++) {
                int r;
                for (r = 0; r < dim_x; r++) one[r] = 0.0;
                one[c] = 1.0;
                gm_linalg_solve(dim_x, Fm, one, col);
                for (r = 0; r < dim_x; r++) Pinv[r * dim_x + c] = col[r];
            }
            memcpy(Y, Pinv, (size_t)dim_x * dim_x * sizeof(gc_real));
            free(Pinv); free(one); free(col);
        }
        /* predict: Yp = (F Y^-1 F' + Q)^-1, yp = Yp F Y^-1 y */
        {
            gc_real *Yinv = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
            gc_real *one = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            gc_real *col = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            gc_real *Ft = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
            int c;
            if (!Yinv || !one || !col || !Ft) {
                free(Yinv); free(one); free(col); free(Ft); free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5);
                return GC_ERR_NO_MEM;
            }
            memcpy(Fm, Y, (size_t)dim_x * dim_x * sizeof(gc_real));
            for (c = 0; c < dim_x; c++) {
                int r;
                for (r = 0; r < dim_x; r++) one[r] = 0.0;
                one[c] = 1.0;
                gm_linalg_solve(dim_x, Fm, one, col);
                for (r = 0; r < dim_x; r++) Yinv[r * dim_x + c] = col[r];
            }
            mat_transpose(dim_x, dim_x, F, Ft);
            mat_mul(dim_x, dim_x, dim_x, F, Yinv, tmp1);
            mat_mul(dim_x, dim_x, dim_x, tmp1, Ft, tmp2);
            for (i = 0; i < dim_x * dim_x; i++) tmp2[i] += Q[i];
            memcpy(Fm, tmp2, (size_t)dim_x * dim_x * sizeof(gc_real));
            for (c = 0; c < dim_x; c++) {
                int r;
                for (r = 0; r < dim_x; r++) one[r] = 0.0;
                one[c] = 1.0;
                gm_linalg_solve(dim_x, Fm, one, col);
                for (r = 0; r < dim_x; r++) Y[r * dim_x + c] = col[r];
            }
            /* yp = Y F Y^-1 y */
            mat_mul(dim_x, dim_x, 1, Yinv, y, tmp4);
            mat_mul(dim_x, dim_x, 1, F, tmp4, tmp5);
            mat_mul(dim_x, dim_x, 1, Y, tmp5, y);
            free(Yinv); free(one); free(col); free(Ft);
        }
        /* update: Y += H' R^-1 H, y += H' R^-1 z */
        {
            gc_real *Rinv = (gc_real *)calloc((size_t)dim_z * dim_z, sizeof(gc_real));
            gc_real *Ht = (gc_real *)malloc((size_t)dim_z * dim_x * sizeof(gc_real));
            gc_real *tmp6 = (gc_real *)malloc((size_t)dim_x * dim_z * sizeof(gc_real));
            gc_real *tmp7 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
            gc_real *tmp8 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
            gc_real *zcol = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
            int c;
            if (!Rinv || !Ht || !tmp6 || !tmp7 || !tmp8 || !zcol) {
                free(Rinv); free(Ht); free(tmp6); free(tmp7); free(tmp8); free(zcol);
                free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5);
                return GC_ERR_NO_MEM;
            }
            for (i = 0; i < dim_z; i++) Rinv[i * dim_z + i] = R[i * dim_z + i] > 0 ? 1.0 / R[i * dim_z + i] : 0.0;
            mat_transpose(dim_z, dim_x, H, Ht);
            mat_mul(dim_x, dim_z, dim_z, Ht, Rinv, tmp6);
            mat_mul(dim_x, dim_z, dim_x, tmp6, H, tmp7);
            for (i = 0; i < dim_x * dim_x; i++) Y[i] += tmp7[i];
            for (i = 0; i < dim_z; i++) zcol[i] = zs[t * dim_z + i];
            mat_mul(dim_x, dim_z, 1, tmp6, zcol, tmp8);
            for (i = 0; i < dim_x; i++) y[i] += tmp8[i];
            /* x = Y^-1 y, P = Y^-1 */
            memcpy(Fm, Y, (size_t)dim_x * dim_x * sizeof(gc_real));
            gm_linalg_solve(dim_x, Fm, y, x);
            memcpy(Fm, Y, (size_t)dim_x * dim_x * sizeof(gc_real));
            for (c = 0; c < dim_x; c++) {
                int r;
                gc_real *one2 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
                gc_real *col2 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
                if (!one2 || !col2) { free(one2); free(col2); free(Rinv); free(Ht); free(tmp6); free(tmp7); free(tmp8); free(zcol); free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5); return GC_ERR_NO_MEM; }
                for (r = 0; r < dim_x; r++) one2[r] = 0.0;
                one2[c] = 1.0;
                gm_linalg_solve(dim_x, Fm, one2, col2);
                for (r = 0; r < dim_x; r++) P[r * dim_x + c] = col2[r];
                free(one2); free(col2);
            }
            free(Rinv); free(Ht); free(tmp6); free(tmp7); free(tmp8); free(zcol);
        }
        memcpy(&xs[t * dim_x], x, (size_t)dim_x * sizeof(gc_real));
        memcpy(&ps[t * dim_x * dim_x], P, (size_t)dim_x * dim_x * sizeof(gc_real));
    }
    free(P); free(Y); free(y); free(x); free(Fm); free(tmp1); free(tmp2); free(tmp3); free(tmp4); free(tmp5);
    return GC_OK;
}

gc_status gs_kalman_imm_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                              const gc_real *F1, const gc_real *F2,
                              const gc_real *H, const gc_real *Q1,
                              const gc_real *Q2, const gc_real *R,
                              const gc_real *x0, const gc_real *P0,
                              gc_real mu1, gc_real *xs, gc_real *ps,
                              gc_real *probs)
{
    gc_real *x1, *P1, *x2, *P2;
    int t;
    if (!zs || !F1 || !F2 || !H || !Q1 || !Q2 || !R || !x0 || !P0 || !xs || !ps) return GC_ERR_ARG;
    x1 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    P1 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    x2 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
    P2 = (gc_real *)malloc((size_t)dim_x * dim_x * sizeof(gc_real));
    if (!x1 || !P1 || !x2 || !P2) { free(x1); free(P1); free(x2); free(P2); return GC_ERR_NO_MEM; }
    memcpy(x1, x0, (size_t)dim_x * sizeof(gc_real));
    memcpy(P1, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
    memcpy(x2, x0, (size_t)dim_x * sizeof(gc_real));
    memcpy(P2, P0, (size_t)dim_x * dim_x * sizeof(gc_real));
    for (t = 0; t < n; t++) {
        gc_real *z1 = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
        gc_real *z2 = (gc_real *)malloc((size_t)dim_z * sizeof(gc_real));
        gc_real *tmp1 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
        gc_real *tmp2 = (gc_real *)malloc((size_t)dim_x * sizeof(gc_real));
        gc_real l1, l2, mu2;
        int i;
        if (!z1 || !z2 || !tmp1 || !tmp2) { free(z1); free(z2); free(tmp1); free(tmp2); free(x1); free(P1); free(x2); free(P2); return GC_ERR_NO_MEM; }
        /* mixing: stato iniziale misto */
        mu2 = 1.0 - mu1;
        for (i = 0; i < dim_x; i++) {
            gc_real x0mix = mu1 * x1[i] + mu2 * x2[i];
            tmp1[i] = x1[i] - x0mix;
            tmp2[i] = x2[i] - x0mix;
            x1[i] = x0mix; x2[i] = x0mix;
        }
        {
            int j;
            for (i = 0; i < dim_x * dim_x; i++) {
                P1[i] += mu1 * mu2 * (tmp1[i / dim_x] * tmp1[i % dim_x]);
                P2[i] += mu1 * mu2 * (tmp2[i / dim_x] * tmp2[i % dim_x]);
            }
            (void)j;
        }
        /* predici + aggiorna con un passo del filtro lineare per modello */
        gs_kalman_batch(&zs[t * dim_z], 1, dim_x, dim_z, F1, H, Q1, R, x1, P1, x1, P1);
        gs_kalman_batch(&zs[t * dim_z], 1, dim_x, dim_z, F2, H, Q2, R, x2, P2, x2, P2);
        /* likelihood gaussiana */
        {
            gc_real S1 = 0.0, S2 = 0.0;
            for (i = 0; i < dim_z; i++) { S1 += R[i * dim_z + i]; S2 += R[i * dim_z + i]; }
            l1 = 1.0 / sqrt(2.0 * 3.14159265358979323846 * S1);
            l2 = 1.0 / sqrt(2.0 * 3.14159265358979323846 * S2);
        }
        (void)z1; (void)z2;
        mu2 = 1.0 - mu1;
        /* prob aggiornate proporzionali a mu_i * lik (qui lik ~ cost, uso mu) */
        {
            gc_real w1 = mu1 * l1, w2 = mu2 * l2;
            gc_real wsum = w1 + w2;
            mu1 = wsum > 0 ? w1 / wsum : mu1;
        }
        for (i = 0; i < dim_x; i++) {
            xs[t * dim_x + i] = mu1 * x1[i] + (1.0 - mu1) * x2[i];
        }
        if (probs) probs[t] = mu1;
        free(z1); free(z2); free(tmp1); free(tmp2);
    }
    free(x1); free(P1); free(x2); free(P2);
    return GC_OK;
}
