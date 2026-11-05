/*
 * gs_kalman.h — filtri di Kalman multivariati (da Python/Kalman Filters:
 * filterpy, pykalman). Filtro lineare (predict/update, Joseph), smoother RTS,
 * EKF, UKF (sigma points + unscented transform), Ensemble KF, information
 * filter, Interacting Multiple Models (IMM).
 */
#ifndef GMBSTATS_GS_KALMAN_H
#define GMBSTATS_GS_KALMAN_H

#include "gc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Filtro di Kalman lineare in batch. zs = n x dim_z (row-major). F/H/Q/R
 * dim_x/dim_x etc. x0 = stato iniziale, P0 = cov iniziale.
 * xs = n x dim_x, ps = n x dim_x*dim_x (filtrati). Ritorna 0/-1. */
gc_status gs_kalman_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                          const gc_real *F, const gc_real *H, const gc_real *Q,
                          const gc_real *R, const gc_real *x0, const gc_real *P0,
                          gc_real *xs, gc_real *ps);

/* RTS smoother (Rauch-Tung-Striebel) sui filtri xs/ps. Out xs_s/ps_s. */
gc_status gs_kalman_rts(const gc_real *xs, const gc_real *ps, int n, int dim_x,
                        const gc_real *F, const gc_real *Q, gc_real *xs_s,
                        gc_real *ps_s);

/* EKF: funzioni non lineari con Jacobiani. fx/fx_jac = transizione,
 * hx/hx_jac = misura. Stato nx, misura nz. */
typedef void (*gs_kf_fx)(const gc_real *x, const gc_real *u, gc_real *xout, void *ctx);
typedef void (*gs_kf_fx_jac)(const gc_real *x, const gc_real *u, gc_real *F, void *ctx);
typedef void (*gs_kf_hx)(const gc_real *x, gc_real *zout, void *ctx);
typedef void (*gs_kf_hx_jac)(const gc_real *x, gc_real *H, void *ctx);
gc_status gs_kalman_ekf_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                              gs_kf_fx fx, gs_kf_fx_jac fxj,
                              gs_kf_hx hx, gs_kf_hx_jac hxj,
                              const gc_real *Q, const gc_real *R,
                              const gc_real *x0, const gc_real *P0, void *ctx,
                              gc_real *xs, gc_real *ps);

/* UKF: sigma points (merwe: alpha,beta,kappa) + unscented transform + filtro. */
gc_status gs_kalman_ukf_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                              gs_kf_fx fx, gs_kf_hx hx,
                              const gc_real *Q, const gc_real *R,
                              const gc_real *x0, const gc_real *P0,
                              gc_real alpha, gc_real beta, gc_real kappa,
                              void *ctx, gc_real *xs, gc_real *ps);

/* Ensemble Kalman Filter: nens membri; F/H/Q/R lineari. */
gc_status gs_kalman_enkf_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                               const gc_real *F, const gc_real *H,
                               const gc_real *Q, const gc_real *R,
                               const gc_real *x0, const gc_real *P0,
                               int nens, unsigned seed, gc_real *xs);

/* Information filter (forma informativa): Y = P^-1, y = P^-1 x. */
gc_status gs_kalman_info_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                               const gc_real *F, const gc_real *H,
                               const gc_real *Q, const gc_real *R,
                               const gc_real *x0, const gc_real *P0,
                               gc_real *xs, gc_real *ps);

/* IMM (Interacting Multiple Models) con 2 modelli lineari. */
gc_status gs_kalman_imm_batch(const gc_real *zs, int n, int dim_x, int dim_z,
                              const gc_real *F1, const gc_real *F2,
                              const gc_real *H, const gc_real *Q1,
                              const gc_real *Q2, const gc_real *R,
                              const gc_real *x0, const gc_real *P0,
                              gc_real mu1, gc_real *xs, gc_real *ps,
                              gc_real *probs);

#ifdef __cplusplus
}
#endif

#endif /* GMBSTATS_GS_KALMAN_H */