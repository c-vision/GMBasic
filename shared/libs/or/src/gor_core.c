/* gor_core.c — versione, stato, confronto. */
#include "gor_core.h"

#include <math.h>
#include <stdint.h>

const char *gor_version(void)
{
    return "0.1.0";
}

const char *gor_status_str(gor_status status)
{
    switch (status) {
    case GOR_OK: return "ok";
    case GOR_ERR_ARG: return "invalid argument";
    case GOR_ERR_NOMEM: return "allocation failed";
    case GOR_ERR_INFEASIBLE: return "infeasible";
    case GOR_ERR_NOCONV: return "no convergence";
    case GOR_ERR_RANGE: return "index out of range";
    case GOR_ERR_CYCLE: return "cycle detected";
    case GOR_ERR_SINGULAR: return "singular";
    case GOR_ERR_INTERNAL: return "internal error";
    }
    return "unknown status";
}

int gor_near(gor_real a, gor_real b, gor_real tol)
{
    gor_real scale = 1.0;
    if (fabs(a) > scale) scale = fabs(a);
    if (fabs(b) > scale) scale = fabs(b);
    return (fabs(a - b) <= tol * scale) ? 1 : 0;
}

#include "gor_debug.h"
int gor_debug = 0;
int gor_debug_nt = 0;
int gor_no_nt = 0;
void gor_set_debug(int lvl){ gor_debug = lvl; }
void gor_set_debug_nt(int on){ gor_debug_nt = on; }
void gor_set_no_nt(int on){ gor_no_nt = on; }
