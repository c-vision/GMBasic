/*
 * conic_socp.c — shim FFI del solver SOCP (cono del secondo ordine).
 *
 *   min c'x   s.t.  E x = d,   G x + h in K
 *
 * E : p x n row-major; d : p; c : n.
 * G : K x n row-major; h : K, con K = somma delle dimensioni dei coni.
 * I coni sono descritti da tre array paralleli:
 *   ctype(k) = 0  R+   (cnm(k) = 1, una riga di G)
 *              1  QUAD (cono del secondo ordine, cnm = dimensione)
 *              2  RQUAD (cono quadratico ruotato)
 *   cnm(k)   = dimensione del cono k
 *   cmem()   = concatenazione degli indici dei membri dei soli coni SOC,
 *              in ordine di cono (cnm(k) valori per ogni cono con ctype <> 0)
 *
 * Uscite: x (n), y (p), lam (K). Ritorno: 0 ottimo, 1 non convergenza,
 * 2 memoria, 3 singolare, -1 argomenti.
 *
 * SOCPSOLVESP risolve lo stesso problema con la stessa matrice KKT aumentata,
 * ma assembla e fattorizza M con LU sparsa (splu): risultati identici, costo
 * O(fill) invece di O(N^3). E/G/h restano densi in ingresso.
 */
#include "socp.h"

#include <stdlib.h>

typedef long long i64;

static i64 socp_run(i64 sparse, i64 n, i64 p,
                    const double *E, const double *d, const double *c,
                    i64 ncones, const i64 *ctype, const i64 *cnm, const i64 *cmem,
                    const double *G, const double *h,
                    double tol_gap, double tol_feas, i64 max_iter,
                    double *x, double *y, double *lam)
{
    SocpCone *cones;
    i64 k, cur = 0;
    int r;

    if (n < 0 || p < 0 || ncones < 0 || !x || !y || !lam) return -1;
    if (ncones > 0 && (!ctype || !cnm)) return -1;
    if (n > 0 && !c) return -1;
    if (p > 0 && (!E || !d)) return -1;
    cones = (SocpCone *)calloc((size_t)(ncones > 0 ? ncones : 1), sizeof(SocpCone));
    if (!cones) return -1;

    for (k = 0; k < ncones; k++) {
        i64 i, nm = cnm[k];
        cones[k].type = (int)ctype[k];
        cones[k].nmem = (int)nm;
        cones[k].mem = NULL;
        if (cnm[k] < 1) { free(cones); return -1; }
        if (ctype[k] != 0) {
            if (!cmem) { free(cones); return -1; }
            cones[k].mem = (int *)malloc((size_t)nm * sizeof(int));
            if (!cones[k].mem) { free(cones); return -1; }
            for (i = 0; i < nm; i++) cones[k].mem[i] = (int)cmem[cur + i];
            cur += nm;
        }
    }
    r = sparse
            ? socp_solve_sparse((int)n, (int)p, E, d, c, (int)ncones, cones,
                                G, h, tol_gap, tol_feas, (int)max_iter, x, y, lam)
            : socp_solve((int)n, (int)p, E, d, c, (int)ncones, cones,
                         G, h, tol_gap, tol_feas, (int)max_iter, x, y, lam);
    for (k = 0; k < ncones; k++) free(cones[k].mem);
    free(cones);
    return (i64)r;
}

i64 ofx_socp_solve(i64 n, i64 p,
                   const double *E, const double *d, const double *c,
                   i64 ncones, const i64 *ctype, const i64 *cnm, const i64 *cmem,
                   const double *G, const double *h,
                   double tol_gap, double tol_feas, i64 max_iter,
                   double *x, double *y, double *lam)
{
    return socp_run(0, n, p, E, d, c, ncones, ctype, cnm, cmem, G, h,
                    tol_gap, tol_feas, max_iter, x, y, lam);
}

i64 ofx_socp_solve_sparse(i64 n, i64 p,
                          const double *E, const double *d, const double *c,
                          i64 ncones, const i64 *ctype, const i64 *cnm, const i64 *cmem,
                          const double *G, const double *h,
                          double tol_gap, double tol_feas, i64 max_iter,
                          double *x, double *y, double *lam)
{
    return socp_run(1, n, p, E, d, c, ncones, ctype, cnm, cmem, G, h,
                    tol_gap, tol_feas, max_iter, x, y, lam);
}
