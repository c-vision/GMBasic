/*
 * quat_ffi.c — shim ABI per il namespace BASIC QUAT (quaternioni di gmbmath-vector).
 *
 * Il quaternione è il TYPE BASIC `Quat { X, Y, Z, W AS DOUBLE }`, lo stesso layout
 * della struct C `gm_quat` (4 double). Viaggia **per valore** (parametro e ritorno),
 * come GeoPoint di GEO: nessun handle, nessuno stato.
 *
 * Convenzione: (x,y,z) parte vettoriale, w parte scalare; le rotazioni assumono un
 * quaternione normalizzato. Le matrici 3x3 sono scambiate in **row-major** (9 double)
 * e convertite qui dal layout column-major di gm_mat3.
 *
 * Unica funzione con esito: INVERSE, che su quaternione nullo non ha risultato.
 * Lo status arriva in `st()` (1 elemento): 0 = ok, -1 = nullo (e il quaternione
 * restituito è tutto zero).
 */
#include "gm_quat.h"

#include <math.h>
#include <string.h>

typedef long long i64;

/* Stesso layout di gm_quat (4 double). */
typedef struct { double x, y, z, w; } q4;

static gm_quat q_from(q4 a)
{
    gm_quat r;
    r.x = a.x; r.y = a.y; r.z = a.z; r.w = a.w;
    return r;
}

static q4 q_to(gm_quat g)
{
    q4 r;
    r.x = g.x; r.y = g.y; r.z = g.z; r.w = g.w;
    return r;
}

static q4 q_zero(void)
{
    q4 r;
    r.x = 0.0; r.y = 0.0; r.z = 0.0; r.w = 0.0;
    return r;
}

/* ── costruzione ──────────────────────────────────────────────────────────── */

q4 ofx_quat_identity(void)
{
    return q_to(gm_quat_identity());
}

q4 ofx_quat_make(double x, double y, double z, double w)
{
    return q_to(gm_quat_make(x, y, z, w));
}

q4 ofx_quat_from_dir(double x, double y, double z)
{
    gm_vec3 d;
    d.x = x; d.y = y; d.z = z;
    return q_to(gm_quat_from_dir(d));
}

q4 ofx_quat_from_axis_angle(double ax, double ay, double az, double rad)
{
    gm_vec3 v;
    v.x = ax; v.y = ay; v.z = az;
    return q_to(gm_quat_from_axis_angle(v, rad));
}

q4 ofx_quat_from_two_vectors(double fx, double fy, double fz,
                             double tx, double ty, double tz)
{
    gm_vec3 a, b;
    a.x = fx; a.y = fy; a.z = fz;
    b.x = tx; b.y = ty; b.z = tz;
    return q_to(gm_quat_from_two_vectors(a, b));
}

/* m() = 9 double row-major (adattati al layout column-major di gm_mat3).
 * Su lunghezza insufficiente ritorna il quaternione zero. */
q4 ofx_quat_from_mat3(const double *m, i64 m_n)
{
    gm_mat3 g;
    int r, c;
    if (!m || m_n < 9) return q_zero();
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            g.v[c * 3 + r] = m[r * 3 + c];
    return q_to(gm_quat_from_mat3(g));
}

/* ── algebra ──────────────────────────────────────────────────────────────── */

q4 ofx_quat_add(q4 a, q4 b)
{
    return q_to(gm_quat_add(q_from(a), q_from(b)));
}

q4 ofx_quat_scale(q4 a, double s)
{
    return q_to(gm_quat_scale(q_from(a), s));
}

q4 ofx_quat_neg(q4 a)
{
    return q_to(gm_quat_neg(q_from(a)));
}

q4 ofx_quat_mul(q4 a, q4 b)
{
    return q_to(gm_quat_mul(q_from(a), q_from(b)));
}

q4 ofx_quat_conjugate(q4 a)
{
    return q_to(gm_quat_conjugate(q_from(a)));
}

double ofx_quat_dot(q4 a, q4 b)
{
    return (double)gm_quat_dot(q_from(a), q_from(b));
}

double ofx_quat_length(q4 a)
{
    return (double)gm_quat_length(q_from(a));
}

/* gm_quat_normalize restituisce l'identità sul quaternione nullo. */
q4 ofx_quat_normalize(q4 a)
{
    return q_to(gm_quat_normalize(q_from(a)));
}

/* Inversa: coniugato / |q|^2. st(0) = 0 ok, -1 nullo (risultato zero). */
q4 ofx_quat_inverse(q4 a, i64 *st, i64 st_n)
{
    gm_quat r;
    if (st && st_n >= 1) st[0] = 0;
    if (gm_quat_inverse(q_from(a), &r) != GM_OK)
    {
        if (st && st_n >= 1) st[0] = -1;
        return q_zero();
    }
    return q_to(r);
}

/* ── interpolazione ───────────────────────────────────────────────────────── */

q4 ofx_quat_lerp(q4 a, q4 b, double t)
{
    return q_to(gm_quat_lerp(q_from(a), q_from(b), t));
}

q4 ofx_quat_slerp(q4 a, q4 b, double t)
{
    return q_to(gm_quat_slerp(q_from(a), q_from(b), t));
}

/* ── conversione e applicazione ───────────────────────────────────────────── */

/* m() riceve la matrice 3x3 row-major (9 double). 0 = ok, -1 = cap insufficiente. */
i64 ofx_quat_to_mat3(q4 a, double *m, i64 m_n)
{
    gm_mat3 g;
    int r, c;
    if (!m || m_n < 9) return -1;
    g = gm_quat_to_mat3(q_from(a));
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            m[r * 3 + c] = g.v[c * 3 + r];
    return 0;
}

/* out() riceve i 3 componenti del vettore ruotato. 0 = ok, -1 = cap insufficiente. */
i64 ofx_quat_rotate_vec3(q4 a, double x, double y, double z,
                         double *out, i64 out_n)
{
    gm_vec3 v, r;
    if (!out || out_n < 3) return -1;
    v.x = x; v.y = y; v.z = z;
    r = gm_quat_rotate_vec3(q_from(a), v);
    out[0] = r.x; out[1] = r.y; out[2] = r.z;
    return 0;
}
