/* socp.h - primal-dual interior point for conic problems
 *   min  c'x   s.t.  E x = d,  G_k x + h_k in K_k  (K_k = R_+ or SOC)
 * Cone representation: cones[k] lists member variable indices for SOC cones
 * (dim = nmem, first member is t). R_+ cones are dim-1 cones given by the
 * single row of G (G row selects the affine expression; h its constant).
 * G is stacked (sum of cone dims) x n row-major; h stacked.
 * Duals: y (p), lambda (sum dims, in K*). Stationarity c + E'y - G'lam = 0.
 * statuses: 0 optimal, 1 no convergence, 2 memory, 3 singular */
#ifndef SOCP_H
#define SOCP_H

typedef struct {
    int type;   /* 0 = R_+ (dim from G rows), 1 = QUAD (socp), 2 = RQUAD */
    int nmem;   /* cone dimension */
    int *mem;   /* SOC: member variable indices (mem[0] = t) */
} SocpCone;

int socp_solve(int n, int p,
               const double *E, const double *d, const double *c,
               int ncones, const SocpCone *cones,
               const double *G, const double *h,
               double tol_gap, double tol_feas, int max_iter,
               double *x /*n*/, double *y /*p*/, double *lam /*K*/);

/* Same problem and SAME augmented KKT system as socp_solve, but M is assembled
 * sparsely and factored with the sparse LU (splu_factor) instead of a dense
 * N x N LU, so the O(N^3) factorization becomes O(fill). Results are identical
 * to socp_solve. Use for large sparse conic problems. */
int socp_solve_sparse(int n, int p,
                      const double *E, const double *d, const double *c,
                      int ncones, const SocpCone *cones,
                      const double *G, const double *h,
                      double tol_gap, double tol_feas, int max_iter,
                      double *x /*n*/, double *y /*p*/, double *lam /*K*/);

#endif /* SOCP_H */
