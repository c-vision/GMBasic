/* FILE: native/gmb_runtime_c/src/gmb_vecquant.c
 * Quantizzazione SQ8 + scoring nel dominio int8 (unica implementazione §0bis).
 * Il percorso caldo NON dequantizza in FP32: dot/cosine/L2 lavorano sugli int8. */
#include "gmb_vecquant.h"

#include <math.h>
#include <stddef.h>

void gmb_sq8_encode(const float* x, int n, int8_t* q, float* scale)
{
    float m = 0.0f;
    float s;
    int i;
    if (!x || !q || !scale || n <= 0) return;
    for (i = 0; i < n; i++) {
        float a = x[i] < 0.0f ? -x[i] : x[i];
        if (a > m) m = a;
    }
    s = (m == 0.0f) ? 0.0f : m / 127.0f;
    *scale = s;
    if (s == 0.0f) {
        for (i = 0; i < n; i++) q[i] = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        float v = x[i] / s;
        long r = (long)(v + (v >= 0.0f ? 0.5f : -0.5f));
        if (r > 127) r = 127;
        if (r < -127) r = -127;
        q[i] = (int8_t)r;
    }
}

void gmb_sq8_decode(const int8_t* q, float scale, int n, float* x)
{
    int i;
    if (!q || !x || n <= 0) return;
    for (i = 0; i < n; i++) x[i] = (float)q[i] * scale;
}

int32_t gmb_i8_dot(const int8_t* a, const int8_t* b, int n)
{
    int32_t d = 0;
    int i;
    if (!a || !b || n <= 0) return 0;
    for (i = 0; i < n; i++) d += (int32_t)a[i] * (int32_t)b[i];
    return d;
}

float gmb_sq8_cosine(const int8_t* qa, float sa, const int8_t* qb, float sb, int n)
{
    double dot = 0.0, na = 0.0, nb = 0.0;
    int i;
    if (!qa || !qb || n <= 0) return 0.0f;
    for (i = 0; i < n; i++) {
        double a = (double)qa[i];
        double b = (double)qb[i];
        dot += a * b;
        na += a * a;
        nb += b * b;
    }
    if (na == 0.0 || nb == 0.0) return 0.0f;
    /* Le scale si elidono nel rapporto: dot*sa*sb/(|qa|*sa*|qb|*sb). */
    (void)sa;
    (void)sb;
    return (float)(dot / (sqrt(na) * sqrt(nb)));
}

float gmb_sq8_l2(const int8_t* qa, float sa, const int8_t* qb, float sb, int n)
{
    double sum = 0.0;
    int i;
    if (!qa || !qb || n <= 0) return 0.0f;
    for (i = 0; i < n; i++) {
        double d = (double)qa[i] * (double)sa - (double)qb[i] * (double)sb;
        sum += d * d;
    }
    return (float)sqrt(sum);
}

static int nearf(float a, float b, float tol)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= tol;
}

int gmb_vecquant_selfcheck(void)
{
    int fail = 0;
    /* Orac pattern: scale = max/127, q = round(x/scale). */
    {
        float x[4] = { 1.0f, -2.0f, 0.5f, 0.0f };
        int8_t q[4];
        float s = -1.0f;
        float back[4];
        gmb_sq8_encode(x, 4, q, &s);
        if (!nearf(s, 2.0f / 127.0f, 1e-7f)) fail++;
        if (q[0] != 64 || q[1] != -127 || q[2] != 32 || q[3] != 0) fail++;
        gmb_sq8_decode(q, s, 4, back);
        if (!nearf(back[1], -2.0f, 1e-6f) || !nearf(back[0], 1.0f, 0.01f)) fail++;
    }
    /* Vettore nullo: scale 0, decode zeri. */
    {
        float x[3] = { 0.0f, 0.0f, 0.0f };
        int8_t q[3] = { 5, 5, 5 };
        float s = -1.0f;
        float back[3] = { 9.0f, 9.0f, 9.0f };
        gmb_sq8_encode(x, 3, q, &s);
        if (s != 0.0f || q[0] != 0 || q[1] != 0 || q[2] != 0) fail++;
        gmb_sq8_decode(q, s, 3, back);
        if (back[0] != 0.0f || back[1] != 0.0f || back[2] != 0.0f) fail++;
    }
    /* i8dot esatto su interi piccoli. */
    {
        int8_t a[4] = { 1, 2, 3, 4 };
        int8_t b[4] = { 4, 3, 2, 1 };
        if (gmb_i8_dot(a, b, 4) != 20) fail++;
        if (gmb_i8_dot(NULL, b, 4) != 0) fail++;
    }
    /* Coseno: paralleli = 1, ortogonali = 0, nullo = 0. */
    {
        int8_t a[2] = { 100, 0 };
        int8_t b[2] = { 50, 0 };
        int8_t c[2] = { 0, 100 };
        int8_t z[2] = { 0, 0 };
        if (!nearf(gmb_sq8_cosine(a, 0.01f, b, 0.02f, 2), 1.0f, 1e-5f)) fail++;
        if (!nearf(gmb_sq8_cosine(a, 0.01f, c, 0.01f, 2), 0.0f, 1e-5f)) fail++;
        if (gmb_sq8_cosine(a, 0.01f, z, 0.01f, 2) != 0.0f) fail++;
    }
    /* L2: (3,4) vs (0,0) con scale 1 = 5. */
    {
        int8_t a[2] = { 3, 4 };
        int8_t z[2] = { 0, 0 };
        if (!nearf(gmb_sq8_l2(a, 1.0f, z, 1.0f, 2), 5.0f, 1e-5f)) fail++;
    }
    return fail;
}
