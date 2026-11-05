/* gm_core.c — wrapper dei tipi/utility base (delegano a gmbcompute). */
#include "gm_core.h"

#include <math.h>

const char *gm_version(void)
{
    return "0.1.0";
}

const char *gm_status_str(gm_status status)
{
    return gc_status_str(status);
}

gm_real gm_eps(void)
{
    return gc_eps();
}

int gm_is_finite(gm_real x)
{
    return gc_is_finite(x);
}

int gm_near(gm_real a, gm_real b, gm_real tol)
{
    return gc_near(a, b, tol);
}

gm_real gm_smoothstep(gm_real a, gm_real b, gm_real t)
{
    gm_real x;
    if (b == a) return 0.0;
    x = (t - a) / (b - a);
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    return x * x * (3.0 - 2.0 * x);
}

gm_real gm_smootherstep(gm_real a, gm_real b, gm_real t)
{
    gm_real x;
    if (b == a) return 0.0;
    x = (t - a) / (b - a);
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

/* ---- funzioni trigonometriche sicure (da poly3d_omp) ---- */
#define GM_SAFE_ATAN_LIMIT 1e+20
#define GM_SAFE_TAN_LIMIT 1e-11

gm_real gm_safe_tan(gm_real alpha)
{
    gm_real half_pi = 3.14159265358979323846 / 2.0;
    alpha = fmod(alpha, 3.14159265358979323846);
    if (alpha > half_pi) alpha -= 3.14159265358979323846;
    else if (alpha < -half_pi) alpha += 3.14159265358979323846;
    if ((half_pi - fabs(alpha)) < GM_SAFE_TAN_LIMIT)
        alpha = (alpha > 0.0) ? (half_pi - GM_SAFE_TAN_LIMIT) : -(half_pi - GM_SAFE_TAN_LIMIT);
    return tan(alpha);
}

gm_real gm_safe_atan(gm_real y, gm_real x)
{
    if (fabs(x * GM_SAFE_ATAN_LIMIT) < fabs(y))
        return (y >= 0.0) ? 1.5707963267948966 : -1.5707963267948966;
    if (y == 0.0 && x == 0.0) return 0.0;
    return atan(y / x);
}

gm_real gm_safe_atan2(gm_real y, gm_real x)
{
    if (fabs(x * GM_SAFE_ATAN_LIMIT) < fabs(y))
        return (y > 0.0) ? 1.5707963267948966 : -1.5707963267948966;
    if (y == 0.0 && x == 0.0) return (x >= 0.0) ? 0.0 : 3.14159265358979323846;
    return atan2(y, x);
}
