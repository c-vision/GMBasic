/*
 * xform_ffi.c — shim ABI per il namespace BASIC XFORM (trasformazioni 3D di gmbmath-vector).
 *
 * Le matrici sono array PIATTI di double, in ordine ROW-MAJOR, come in GFX3D e in
 * QUAT.TOMAT3: 9 valori per la 3x3, 16 per la 4x4 omogenea. La struct C gm_mat3/gm_mat4
 * è column-major, quindi la conversione avviene qui (una volta per chiamata).
 *
 * Perché non gli handle di MATRIX: XFORM non è un contenitore di matrici generiche — è la
 * semantica della trasformazione omogenea (rotazione attorno a un asse, traslazione,
 * punto contro direzione, inversa affine, matrice normale). Le operazioni generiche
 * (determinante, inversa completa, trasposta) restano in MATRIX/LINALG.
 *
 * MUL3/MUL4 prendono e restituiscono gli stessi buffer piatti: sono l'operatore di
 * composizione della rappresentazione, non un duplicato di MATRIX.MULTIPLY (che lavora
 * su handle e richiederebbe di materializzare due matrici gestite per ogni prodotto).
 *
 * Ritorno: 0 = ok, -1 = argomento non valido (o matrice singolare per l'inversa).
 */
#include "gm_mat.h"

#include <math.h>
#include <string.h>

typedef long long i64;

static int m3_in(const double *m, i64 n, gm_mat3 *g)
{
    int r, c;
    if (!m || n < 9) return 0;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            g->v[c * 3 + r] = m[r * 3 + c];
    return 1;
}

static void m3_out(gm_mat3 g, double *m)
{
    int r, c;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            m[r * 3 + c] = g.v[c * 3 + r];
}

static int m4_in(const double *m, i64 n, gm_mat4 *g)
{
    int r, c;
    if (!m || n < 16) return 0;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            g->v[c * 4 + r] = m[r * 4 + c];
    return 1;
}

static void m4_out(gm_mat4 g, double *m)
{
    int r, c;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            m[r * 4 + c] = g.v[c * 4 + r];
}

/* ── 3x3 ──────────────────────────────────────────────────────────────────── */

i64 ofx_xf_mat3_identity(double *m, i64 m_n)
{
    if (!m || m_n < 9) return -1;
    m3_out(gm_mat3_identity(), m);
    return 0;
}

i64 ofx_xf_mat3_rotx(double rad, double *m, i64 m_n)
{
    if (!m || m_n < 9) return -1;
    m3_out(gm_mat3_rotation_x(rad), m);
    return 0;
}

i64 ofx_xf_mat3_roty(double rad, double *m, i64 m_n)
{
    if (!m || m_n < 9) return -1;
    m3_out(gm_mat3_rotation_y(rad), m);
    return 0;
}

i64 ofx_xf_mat3_rotz(double rad, double *m, i64 m_n)
{
    if (!m || m_n < 9) return -1;
    m3_out(gm_mat3_rotation_z(rad), m);
    return 0;
}

i64 ofx_xf_mat3_mul(const double *a, i64 a_n, const double *b, i64 b_n,
                    double *out, i64 out_n)
{
    gm_mat3 ga, gb;
    if (!m3_in(a, a_n, &ga) || !m3_in(b, b_n, &gb) || !out || out_n < 9) return -1;
    m3_out(gm_mat3_mul(ga, gb), out);
    return 0;
}

/* Matrice normale = (M^-1)^T: trasforma le normali. -1 se M è singolare. */
i64 ofx_xf_mat3_normal(const double *a, i64 a_n, double *out, i64 out_n)
{
    gm_mat3 ga, inv;
    if (!m3_in(a, a_n, &ga) || !out || out_n < 9) return -1;
    if (gm_mat3_inverse(ga, &inv) != GM_OK) return -1;
    m3_out(gm_mat3_transpose(inv), out);
    return 0;
}

/* Estrae la sottomatrice 3x3 (rotazione) da una 4x4. */
i64 ofx_xf_mat3_from4(const double *m4, i64 m4_n, double *out, i64 out_n)
{
    gm_mat4 g;
    if (!m4_in(m4, m4_n, &g) || !out || out_n < 9) return -1;
    m3_out(gm_mat3_from_mat4(g), out);
    return 0;
}

/* ── 4x4 omogenee ─────────────────────────────────────────────────────────── */

i64 ofx_xf_mat4_identity(double *m, i64 m_n)
{
    if (!m || m_n < 16) return -1;
    m4_out(gm_mat4_identity(), m);
    return 0;
}

i64 ofx_xf_mat4_translate(double x, double y, double z, double *m, i64 m_n)
{
    if (!m || m_n < 16) return -1;
    m4_out(gm_mat4_translation(x, y, z), m);
    return 0;
}

i64 ofx_xf_mat4_scale(double x, double y, double z, double *m, i64 m_n)
{
    if (!m || m_n < 16) return -1;
    m4_out(gm_mat4_scale_matrix(x, y, z), m);
    return 0;
}

i64 ofx_xf_mat4_rotx(double rad, double *m, i64 m_n)
{
    if (!m || m_n < 16) return -1;
    m4_out(gm_mat4_rotation_x(rad), m);
    return 0;
}

i64 ofx_xf_mat4_roty(double rad, double *m, i64 m_n)
{
    if (!m || m_n < 16) return -1;
    m4_out(gm_mat4_rotation_y(rad), m);
    return 0;
}

i64 ofx_xf_mat4_rotz(double rad, double *m, i64 m_n)
{
    if (!m || m_n < 16) return -1;
    m4_out(gm_mat4_rotation_z(rad), m);
    return 0;
}

i64 ofx_xf_mat4_mul(const double *a, i64 a_n, const double *b, i64 b_n,
                    double *out, i64 out_n)
{
    gm_mat4 ga, gb;
    if (!m4_in(a, a_n, &ga) || !m4_in(b, b_n, &gb) || !out || out_n < 16) return -1;
    m4_out(gm_mat4_mul(ga, gb), out);
    return 0;
}

/* Inversa rapida di una trasformazione affine (ultima riga 0,0,0,1). -1 se singolare. */
i64 ofx_xf_mat4_inverse_affine(const double *a, i64 a_n, double *out, i64 out_n)
{
    gm_mat4 ga, inv;
    if (!m4_in(a, a_n, &ga) || !out || out_n < 16) return -1;
    if (gm_mat4_inverse_affine(ga, &inv) != GM_OK) return -1;
    m4_out(inv, out);
    return 0;
}

/* Punto (x,y,z,1) trasformato, con divisione per w se la prospettiva lo cambia. */
i64 ofx_xf_mat4_point(const double *m, i64 m_n, double x, double y, double z,
                      double *out, i64 out_n)
{
    gm_mat4 g;
    gm_vec3 p, r;
    if (!m4_in(m, m_n, &g) || !out || out_n < 3) return -1;
    p.x = x; p.y = y; p.z = z;
    r = gm_mat4_transform_point(g, p);
    out[0] = r.x; out[1] = r.y; out[2] = r.z;
    return 0;
}

/* Direzione (x,y,z,0) trasformata: la traslazione non la tocca. */
i64 ofx_xf_mat4_dir(const double *m, i64 m_n, double x, double y, double z,
                    double *out, i64 out_n)
{
    gm_mat4 g;
    gm_vec3 d, r;
    if (!m4_in(m, m_n, &g) || !out || out_n < 3) return -1;
    d.x = x; d.y = y; d.z = z;
    r = gm_mat4_transform_dir(g, d);
    out[0] = r.x; out[1] = r.y; out[2] = r.z;
    return 0;
}

/* Costruisce la 4x4 omogenea da una 3x3 (traslazione nulla). */
i64 ofx_xf_mat4_from3(const double *m3, i64 m3_n, double *out, i64 out_n)
{
    gm_mat3 g3;
    if (!m3_in(m3, m3_n, &g3) || !out || out_n < 16) return -1;
    m4_out(gm_mat4_from_mat3(g3), out);
    return 0;
}
