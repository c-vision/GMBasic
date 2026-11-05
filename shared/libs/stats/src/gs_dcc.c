/* gs_dcc.c — DCC-GARCH step (finance). Estratto da gs_stats2.c. */
#include "gs_stats2.h"

#include <math.h>

int gs_dcc_step(const double *zprev, const double *Qprev, const double *Qbar, int n,
                double a, double b, double *Qout, double *Rout)
{
    int i, j;
    if (!zprev || !Qprev || !Qbar || !Qout || !Rout || n < 1) return -1;
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) {
        Qout[(size_t)i*n+j] = (1.0-a-b)*Qbar[(size_t)i*n+j] + a*zprev[i]*zprev[j] + b*Qprev[(size_t)i*n+j];
    }
    for (i = 0; i < n; i++) {
        double qi = Qout[(size_t)i*n+i]; double si = (qi>1e-300)? sqrt(qi):1e-300;
        for (j = 0; j < n; j++) {
            double qj = Qout[(size_t)j*n+j]; double sj = (qj>1e-300)? sqrt(qj):1e-300;
            Rout[(size_t)i*n+j] = Qout[(size_t)i*n+j]/(si*sj);
        }
    }
    return 0;
}

