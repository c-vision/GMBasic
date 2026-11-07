/* gq_backtest.c — backtest a pesi uguali su matrice periodi x asset.
 * Porting fedele di gmb_tm_backtest / gmb_tm_backtest_matrix (runtime condiviso).
 */
#include "gm_quant_backtest.h"

#include <math.h>

int gq_backtest_run(const double *data, int periods, int assets, double *out)
{
    if (!data || !out || periods <= 0 || assets <= 0) return 0;

    /* Rendimento del periodo = media degli asset della riga; la curva va in out+6. */
    double total = 1.0;
    for (int t = 0; t < periods; t++)
    {
        double s = 0.0;
        for (int a = 0; a < assets; a++) s += data[t * assets + a];
        double r = s / assets;
        out[6 + t] = r;
        total *= (1.0 + r);
    }
    total = (total - 1.0) * 100.0;

    double ann = (pow(total / 100.0 + 1.0, 12.0 / periods) - 1.0) * 100.0;

    double peak = 0.0, maxdd = 0.0, cum = 0.0;
    double mean = 0.0;
    for (int t = 0; t < periods; t++)
    {
        double r = out[6 + t];
        cum += r;
        if (cum > peak) peak = cum;
        double d = cum - peak;
        if (d < maxdd) maxdd = d;
        mean += r;
    }
    mean /= periods;

    double sd = 0.0;
    for (int t = 0; t < periods; t++)
    {
        double d = out[6 + t] - mean;
        sd += d * d;
    }
    sd = periods > 1 ? sqrt(sd / (periods - 1)) : 0.0;

    out[0] = total;
    out[1] = ann;
    out[2] = maxdd;
    out[3] = sd > 0.0 ? (mean / sd) * sqrt(252.0) : 0.0;
    out[4] = (double)periods;
    out[5] = 0.0;
    return periods;
}
