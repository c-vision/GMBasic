/*
 * conic_sdp.c — shim FFI del solver SDP/conico unificato (sdp_ipm).
 *
 * Forma:  min c'x + sum_j <C_j, X_j> + sum_i <c_i, z_i>
 *         s.t.  A x + sum_j <A_kj, X_j> + sum_i <a_ki, z_i> = b_k
 *               x >= 0, X_j PSD, z_i in cono (SOC o esponenziale/potenza).
 *
 * Tutti i blocchi viaggiano in array piatti, con offset deducibili dalle
 * dimensioni (nessuna tabella di puntatori da costruire in BASIC):
 *   dims(j)      = ordine d_j del blocco PSD j
 *   Cbar         = concatenazione delle matrici d_j x d_j (row-major)
 *   Abar         = per k = 0..m-1, per j = 0..nb-1: matrice d_j x d_j
 *                  (lunghezza m * somma_j d_j^2)
 *   socdims(i)   = dimensione k_i del blocco SOC i
 *   Csoc         = concatenazione dei vettori k_i
 *   Asoc         = per k, per i: vettore k_i (lunghezza m * somma_i k_i)
 *   ekind(i)     = 0 PEXP, 1 PPOW, 2 RPOW (blocchi esponenziali, 3 componenti)
 *   ealpha(i)    = parametro alpha del blocco
 *   Cexp         = 3 valori per blocco (lunghezza 3*nep)
 *   Aexp         = per k, per i: 3 valori (lunghezza 3*m*nep)
 * Le uscite hanno lo stesso layout di Cbar/Abar/Csoc/Asoc/Cexp/Aexp.
 *
 * Ritorno: 0 ottimo, 1 max iter, 2 memoria, 3 singolare, -1 argomenti.
 * fb_ok(0) = 1 se e' stato salvato un punto near-optimal di fallback.
 */
#include "sdp.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;

static int *cvt_i(const i64 *src, i64 n)
{
    int *d;
    i64 i;
    if (!src || n <= 0) return NULL;
    d = (int *)malloc((size_t)n * sizeof(int));
    if (!d) return NULL;
    for (i = 0; i < n; i++) d[i] = (int)src[i];
    return d;
}

i64 ofx_sdp_solve(i64 m, i64 n,
                  const double *A, int64_t A_n, const double *b, int64_t b_n, const double *c, int64_t c_n,
                  i64 nb, const i64 *dims, int64_t dims_n, const double *Cbar, int64_t Cbar_n, const double *Abar, int64_t Abar_n,
                  i64 nsoc, const i64 *socdims, int64_t socdims_n, const double *Csoc, int64_t Csoc_n, const double *Asoc, int64_t Asoc_n,
                  i64 nep, const i64 *ekind, int64_t ekind_n, const double *ealpha, int64_t ealpha_n,
                  const double *Cexp, int64_t Cexp_n, const double *Aexp, int64_t Aexp_n,
                  i64 max_iter, double tol, double rtol_pri, double rtol_dual,
                  double rtol_gap, double near_rel,
                  double *x, int64_t x_n, double *Xbar, int64_t Xbar_n, double *y, int64_t y_n, double *Sbar, int64_t Sbar_n,
                  double *Zsoc, int64_t Zsoc_n, double *Ssoc, int64_t Ssoc_n, double *Zexp, int64_t Zexp_n, double *Sexp, int64_t Sexp_n,
                  i64 *fb_ok, int64_t fb_ok_n)
{
    const double **CbarP = NULL, **AbarP = NULL, **CsocP = NULL, **AsocP = NULL;
    const double **CexpP = NULL, **AexpP = NULL;
    double **XbarP = NULL, **SbarP = NULL, **ZsocP = NULL, **SsocP = NULL;
    double **ZexpP = NULL, **SexpP = NULL;
    int *dimsI = NULL, *socdimsI = NULL, *ekindI = NULL;
    i64 j, k, i, cum;
    i64 sumSq = 0, sumSoc = 0;
    int r, fbok = 0;

    if (m < 0 || n < 0 || nb < 0 || nsoc < 0 || nep < 0) return -1;
    if (m > 0 && (!A || !b)) return -1;
    if (n > 0 && !c) return -1;
    if (nb > 0 && (!dims || !Cbar || !Xbar || !Sbar)) return -1;
    if (nb > 0 && m > 0 && !Abar) return -1;
    if (nsoc > 0 && (!socdims || !Csoc || !Zsoc || !Ssoc)) return -1;
    if (nsoc > 0 && m > 0 && !Asoc) return -1;
    if (nep > 0 && (!ekind || !ealpha || !Cexp || !Zexp || !Sexp)) return -1;
    if (nep > 0 && m > 0 && !Aexp) return -1;
    if (m > 0 && !y) return -1;
    if (n > 0 && !x) return -1;

    for (j = 0; j < nb; j++) {
        if (dims[j] < 1) return -1;
        sumSq += dims[j] * dims[j];
    }
    for (i = 0; i < nsoc; i++) {
        if (socdims[i] < 1) return -1;
        sumSoc += socdims[i];
    }

    dimsI = cvt_i(dims, nb);
    socdimsI = cvt_i(socdims, nsoc);
    ekindI = cvt_i(ekind, nep);
    CbarP = (const double **)calloc((size_t)(nb > 0 ? nb : 1), sizeof(double *));
    AbarP = (const double **)calloc((size_t)(nb * m > 0 ? nb * m : 1), sizeof(double *));
    CsocP = (const double **)calloc((size_t)(nsoc > 0 ? nsoc : 1), sizeof(double *));
    AsocP = (const double **)calloc((size_t)(nsoc * m > 0 ? nsoc * m : 1), sizeof(double *));
    CexpP = (const double **)calloc((size_t)(nep > 0 ? nep : 1), sizeof(double *));
    AexpP = (const double **)calloc((size_t)(nep * m > 0 ? nep * m : 1), sizeof(double *));
    XbarP = (double **)calloc((size_t)(nb > 0 ? nb : 1), sizeof(double *));
    SbarP = (double **)calloc((size_t)(nb > 0 ? nb : 1), sizeof(double *));
    ZsocP = (double **)calloc((size_t)(nsoc > 0 ? nsoc : 1), sizeof(double *));
    SsocP = (double **)calloc((size_t)(nsoc > 0 ? nsoc : 1), sizeof(double *));
    ZexpP = (double **)calloc((size_t)(nep > 0 ? nep : 1), sizeof(double *));
    SexpP = (double **)calloc((size_t)(nep > 0 ? nep : 1), sizeof(double *));

    if ((nb > 0 && (!CbarP || !XbarP || !SbarP)) ||
        (nb > 0 && m > 0 && !AbarP) ||
        (nsoc > 0 && (!CsocP || !ZsocP || !SsocP)) ||
        (nsoc > 0 && m > 0 && !AsocP) ||
        (nep > 0 && (!CexpP || !ZexpP || !SexpP)) ||
        (nep > 0 && m > 0 && !AexpP) ||
        (nb > 0 && !dimsI) || (nsoc > 0 && !socdimsI) || (nep > 0 && !ekindI)) {
        r = -1;
        goto done;
    }

    cum = 0;
    for (j = 0; j < nb; j++) {
        CbarP[j] = Cbar + cum;
        XbarP[j] = Xbar + cum;
        SbarP[j] = Sbar + cum;
        cum += dims[j] * dims[j];
    }
    for (k = 0; k < m; k++) {
        cum = 0;
        for (j = 0; j < nb; j++) {
            AbarP[k * nb + j] = Abar + k * sumSq + cum;
            cum += dims[j] * dims[j];
        }
    }
    cum = 0;
    for (i = 0; i < nsoc; i++) {
        CsocP[i] = Csoc + cum;
        ZsocP[i] = Zsoc + cum;
        SsocP[i] = Ssoc + cum;
        cum += socdims[i];
    }
    for (k = 0; k < m; k++) {
        cum = 0;
        for (i = 0; i < nsoc; i++) {
            AsocP[k * nsoc + i] = Asoc + k * sumSoc + cum;
            cum += socdims[i];
        }
    }
    for (i = 0; i < nep; i++) {
        CexpP[i] = Cexp + 3 * i;
        ZexpP[i] = Zexp + 3 * i;
        SexpP[i] = Sexp + 3 * i;
    }
    for (k = 0; k < m; k++)
        for (i = 0; i < nep; i++)
            AexpP[k * nep + i] = Aexp + 3 * (k * nep + i);

    r = sdp_ipm((int)m, (int)n, A, b, c, (int)nb, dimsI,
                (const double *const *)CbarP, (const double *const *)AbarP,
                (int)nsoc, socdimsI,
                (const double *const *)CsocP, (const double *const *)AsocP,
                (int)nep, ekindI, ealpha,
                (const double *const *)CexpP, (const double *const *)AexpP,
                (int)max_iter, tol, rtol_pri, rtol_dual, rtol_gap, near_rel,
                x, (double *const *)XbarP, y, (double *const *)SbarP,
                (double *const *)ZsocP, (double *const *)SsocP,
                (double *const *)ZexpP, (double *const *)SexpP,
                NULL, NULL, &fbok);
    if (fb_ok) fb_ok[0] = (i64)fbok;

done:
    free((void *)CbarP); free((void *)AbarP);
    free((void *)CsocP); free((void *)AsocP);
    free((void *)CexpP); free((void *)AexpP);
    free(XbarP); free(SbarP); free(ZsocP); free(SsocP); free(ZexpP); free(SexpP);
    free(dimsI); free(socdimsI); free(ekindI);
    return (i64)r;
}
