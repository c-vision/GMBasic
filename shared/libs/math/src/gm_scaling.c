/* scaling.c — equilibratura geometrica righe/colonne con potenze di 2.
 *
 * Poche iterazioni di equilibratura: ogni pass computa per ogni riga
 * (poi colonna) la radice quadrata della media di |a_ij|^2 e arrotonda
 * il fattore 1/rms alla potenza di 2 più vicina, così lo scaling è
 * esatto in floating point.
 */
#include <math.h>
#include <string.h>

#include "gm_scaling.h"

static double pow2_round(double x) {
    /* potenza di 2 più vicina a x (>0), clampata a [2^-24, 2^24] */
    if (!(x > 0.0) || x != x) return 1.0;
    double e = floor(log2(x) + 0.5);
    if (e > 24.0) e = 24.0;
    if (e < -24.0) e = -24.0;
    return ldexp(1.0, (int)e);
}

void scale_equilibrate(int nvar, int ncon,
                       const int *ptr, const int *sub, double *val,
                       double *lc, double *uc, double *lx, double *ux,
                       double *c, double *Q,
                       double *r, double *d) {
    for (int i = 0; i < ncon; i++) r[i] = 1.0;
    for (int j = 0; j < nvar; j++) d[j] = 1.0;

    for (int it = 0; it < 4; it++) {
        /* ---- row pass ---- */
        for (int i = 0; i < ncon; i++) {
            double sum = 0.0;
            int cnt = 0;
            for (int j = 0; j < nvar; j++)
                for (int k = ptr[j]; k < ptr[j + 1]; k++)
                    if (sub[k] == i) { sum += val[k] * val[k]; cnt++; }
            if (cnt == 0) continue;
            double f = pow2_round(1.0 / sqrt(sum / (double)cnt));
            if (f == 1.0) continue;
            r[i] *= f;
            lc[i] *= f; uc[i] *= f;
            for (int j = 0; j < nvar; j++)
                for (int k = ptr[j]; k < ptr[j + 1]; k++)
                    if (sub[k] == i) val[k] *= f;
        }
        /* ---- column pass ---- */
        for (int j = 0; j < nvar; j++) {
            double sum = 0.0;
            int cnt = 0;
            for (int k = ptr[j]; k < ptr[j + 1]; k++) { sum += val[k] * val[k]; cnt++; }
            if (cnt == 0) continue;
            double f = pow2_round(1.0 / sqrt(sum / (double)cnt));
            if (f == 1.0) continue;
            d[j] *= f;
            c[j] *= f;
            lx[j] /= f; ux[j] /= f;
            for (int k = ptr[j]; k < ptr[j + 1]; k++) val[k] *= f;
            if (Q) {
                /* Q' = D Q D: riga j e colonna j scalate da d[j] (parziale) */
                for (int k = 0; k < nvar; k++) {
                    Q[j * nvar + k] *= f;
                    Q[k * nvar + j] *= f;
                }
            }
        }
    }
}