/* cbfield_ext.c — libreria di test FFI per le forme di callback "vettoriali"
 * (2026-11-02 / 2026-11-07).
 *
 * Copre le ABI che il marshalling BASIC riconosce:
 *   144  f(v[]) -> out[]            (double* v, int64_t n, double* out)
 *   145  f(a[], b[]) -> out[]       (double* a, int64_t na, double* b, int64_t nb, double* out)
 *   148  f(v[], x) -> out[]         (double* v, int64_t n, double x, double* out)
 *   193  f(t) -> vec3               (double t, double* out)
 *   194  f(u, v) -> vec3            (double u, double v, double* out)
 *   195  f(x, y, z) -> vec3         (double x, double y, double z, double* out)
 *
 * In tutte le forme con ritorno array il valore restituito dal callback e' il
 * numero di elementi scritti in out (come fanno i trampolini di tutti i backend).
 */
#include <stdint.h>

typedef double (*cb_a144)(double *v, int64_t n, double *out);
typedef double (*cb_a145)(double *a, int64_t na, double *b, int64_t nb, double *out);
typedef double (*cb_a148)(double *v, int64_t n, double s, double *out);
typedef double (*cb_c193)(double t, double *out);
typedef double (*cb_c194)(double u, double v, double *out);
typedef double (*cb_c195)(double x, double y, double z, double *out);

/* Impacchetta un vettore in un solo numero, per un EXPECT leggibile:
 * x*100 + y*10 + z. */
static double pack3(const double *o)
{
    return o[0] * 100.0 + o[1] * 10.0 + o[2];
}

double cbf_curve193(cb_c193 f, double t)
{
    double o[3] = { 0.0, 0.0, 0.0 };
    if (!f) return -1.0;
    (void)f(t, o);
    return pack3(o);
}

double cbf_patch194(cb_c194 f, double u, double v)
{
    double o[3] = { 0.0, 0.0, 0.0 };
    if (!f) return -1.0;
    (void)f(u, v, o);
    return pack3(o);
}

double cbf_field195(cb_c195 f, double x, double y, double z)
{
    double o[3] = { 0.0, 0.0, 0.0 };
    if (!f) return -1.0;
    (void)f(x, y, z, o);
    return pack3(o);
}

double cbf_map144(cb_a144 f, double *v, int64_t n, double *out)
{
    if (!f) return -1.0;
    return f(v, n, out);
}

double cbf_map145(cb_a145 f, double *a, int64_t na, double *b, int64_t nb, double *out)
{
    if (!f) return -1.0;
    return f(a, na, b, nb, out);
}

double cbf_scale148(cb_a148 f, double *v, int64_t n, double s, double *out)
{
    if (!f) return -1.0;
    return f(v, n, s, out);
}
