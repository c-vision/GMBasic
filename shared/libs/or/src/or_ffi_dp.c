/* or_ffi_dp.c — shim ABI per la PROGRAMMAZIONE DINAMICA del namespace OPTIMIZE. */
#include "gor_dp.h"

#include <stdlib.h>
#include <stdint.h>

typedef long long i64;

double ofx_dp_optimal_bst(const double *p, int64_t p_n, i64 n, i64 *root, int64_t root_n)
{
    int *r;
    double cost;
    if (n <= 0 || p == NULL || root == NULL) return -1.0;
    r = (int *)malloc((size_t)(n * n) * sizeof(int));
    if (r == NULL) return -1.0;
    cost = (double)gor_dp_optimal_bst((int)n, (const gor_real *)p, r);
    { i64 i; for (i = 0; i < n * n; i++) root[i] = (i64)r[i]; }
    free(r);
    return cost;
}

double ofx_dp_equipment(i64 years, double buy_cost, i64 max_age, const double *op_cost, int64_t op_cost_n,
                        const double *resale, int64_t resale_n, i64 *schedule, int64_t schedule_n)
{
    int *sch;
    i64 i;
    double cost;
    if (years <= 0 || max_age <= 0 || op_cost == NULL || resale == NULL) return -1.0;
    sch = (int *)malloc((size_t)years * sizeof(int));
    if (sch == NULL) return -1.0;
    cost = (double)gor_dp_equipment_replacement((int)years, (gor_real)buy_cost, (int)max_age,
                                                (const gor_real *)op_cost,
                                                (const gor_real *)resale, sch);
    if (schedule != NULL) for (i = 0; i < years; i++) schedule[i] = (i64)sch[i];
    free(sch);
    return cost;
}

double ofx_dp_sports(int n_games, i64 needed, double p_home, double p_away, const i64 *is_home, int64_t is_home_n)
{
    int *h;
    double r;
    if (n_games <= 0 || needed <= 0 || is_home == NULL) return -1.0;
    h = (int *)malloc((size_t)n_games * sizeof(int));
    if (h == NULL) return -1.0;
    { i64 i; for (i = 0; i < n_games; i++) h[i] = is_home[i] ? 1 : 0; }
    r = (double)gor_dp_sports_series((int)n_games, (int)needed, (gor_real)p_home,
                                     (gor_real)p_away, h);
    free(h);
    return r;
}
