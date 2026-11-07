/*
 * vec_ffi.c — shim ABI per il namespace BASIC VEC (operazioni vettoriali di gmbmath-vector).
 *
 * Copre SOLO ciò che VECTOR non ha: proiezione e componente perpendicolare, Fresnel di
 * Schlick, bias per il ray tracing, clamp (per componente e di modulo), valore assoluto,
 * radice e potenza per componente, perpendicolare 2D. Somma, differenza, prodotto scalare
 * e vettoriale, norma, distanze, lerp e min/max restano in VECTOR.
 *
 * I vettori sono array di 3 double (2 per PERP2D); gli array di uscita sono dichiarati dal
 * chiamante e la loro lunghezza arriva dall'ABI. Ritorno: 0 = ok, -1 = array troppo corto.
 */
#include "gm_vec.h"

#include <math.h>

typedef long long i64;

static int v3_in(const double *v, i64 n, gm_vec3 *g)
{
    if (!v || n < 3) return 0;
    g->x = v[0]; g->y = v[1]; g->z = v[2];
    return 1;
}

static void v3_out(gm_vec3 g, double *out)
{
    out[0] = g.x; out[1] = g.y; out[2] = g.z;
}

/* Proiezione di v su onto: onto * (v·onto / |onto|^2). onto nullo -> vettore zero. */
i64 ofx_vec_project(const double *v, i64 v_n, const double *on, i64 on_n,
                    double *out, i64 out_n)
{
    gm_vec3 a, b;
    if (!v3_in(v, v_n, &a) || !v3_in(on, on_n, &b) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_project(a, b), out);
    return 0;
}

/* Componente perpendicolare a onto: v - proiezione. */
i64 ofx_vec_reject(const double *v, i64 v_n, const double *on, i64 on_n,
                   double *out, i64 out_n)
{
    gm_vec3 a, b;
    if (!v3_in(v, v_n, &a) || !v3_in(on, on_n, &b) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_reject(a, b), out);
    return 0;
}

/* Fresnel di Schlick per componente: f0*(1-f) + f con f = (1-u)^5.
 * u = coseno dell'angolo di incidenza (1 = perpendicolare alla superficie). */
i64 ofx_vec_schlick(const double *f0, i64 f0_n, double u, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(f0, f0_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_schlick(a, u), out);
    return 0;
}

/* Bias: v + n*eps, con eps proporzionale alla magnitudine (evita l'auto-intersezione
 * dei raggi primari con la superficie che li ha generati). */
i64 ofx_vec_bias(const double *v, i64 v_n, const double *nrm, i64 nrm_n,
                 double *out, i64 out_n)
{
    gm_vec3 a, b;
    if (!v3_in(v, v_n, &a) || !v3_in(nrm, nrm_n, &b) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_bias(a, b), out);
    return 0;
}

/* Clamp per componente nell'intervallo [lo, hi]. */
i64 ofx_vec_clamp(const double *v, i64 v_n, double lo, double hi, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(v, v_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_clamp(a, lo, hi), out);
    return 0;
}

/* Clamp per componente in [0, 1] (colori, normali, texture coordinate). */
i64 ofx_vec_clamp01(const double *v, i64 v_n, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(v, v_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_clamp01(a), out);
    return 0;
}

/* Limita il modulo a `max` lasciando la direzione: |out| <= max. */
i64 ofx_vec_clamp_mag(const double *v, i64 v_n, double maxlen, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(v, v_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_clamp_mag(a, maxlen), out);
    return 0;
}

/* Valore assoluto per componente. */
i64 ofx_vec_abs(const double *v, i64 v_n, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(v, v_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_abs(a), out);
    return 0;
}

/* Radice quadrata per componente (le componenti negative danno NaN, come in C). */
i64 ofx_vec_sqrt(const double *v, i64 v_n, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(v, v_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_sqrt(a), out);
    return 0;
}

/* Potenza per componente. */
i64 ofx_vec_pow(const double *v, i64 v_n, double p, double *out, i64 out_n)
{
    gm_vec3 a;
    if (!v3_in(v, v_n, &a) || !out || out_n < 3) return -1;
    v3_out(gm_vec3_pow(a, p), out);
    return 0;
}

/* Perpendicolare di un vettore 2D (rotazione di 90 gradi: (-y, x)). */
i64 ofx_vec_perp2d(double x, double y, double *out, i64 out_n)
{
    gm_vec2 v, r;
    if (!out || out_n < 2) return -1;
    v.x = x; v.y = y;
    r = gm_vec2_perp(v);
    out[0] = r.x; out[1] = r.y;
    return 0;
}
