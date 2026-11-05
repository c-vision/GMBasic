/* gm_mat.c — matrici 2x2/3x3/4x4, storage column-major. */
#include "gm_mat.h"

#include <math.h>

#define M2_AT(m, r, c) ((m).v[(c) * 2 + (r)])
#define M3_AT(m, r, c) ((m).v[(c) * 3 + (r)])
#define M4_AT(m, r, c) ((m).v[(c) * 4 + (r)])

/* ============================== mat2 ============================== */

gm_mat2 gm_mat2_identity(void)
{
    gm_mat2 m;
    m.v[0] = 1.0; m.v[1] = 0.0;
    m.v[2] = 0.0; m.v[3] = 1.0;
    return m;
}

gm_mat2 gm_mat2_zero(void)
{
    gm_mat2 m;
    int i;
    for (i = 0; i < 4; i++) m.v[i] = 0.0;
    return m;
}

gm_mat2 gm_mat2_from_cols(gm_vec2 c0, gm_vec2 c1)
{
    gm_mat2 m;
    m.v[0] = c0.x; m.v[1] = c0.y;
    m.v[2] = c1.x; m.v[3] = c1.y;
    return m;
}

gm_real gm_mat2_at(gm_mat2 m, int row, int col)
{
    if (row < 0 || row > 1 || col < 0 || col > 1) return 0.0;
    return M2_AT(m, row, col);
}

gm_mat2 gm_mat2_with(gm_mat2 m, int row, int col, gm_real value)
{
    if (row >= 0 && row <= 1 && col >= 0 && col <= 1) M2_AT(m, row, col) = value;
    return m;
}

gm_mat2 gm_mat2_mul(gm_mat2 a, gm_mat2 b)
{
    gm_mat2 out;
    int c, r, k;
    for (c = 0; c < 2; c++) {
        for (r = 0; r < 2; r++) {
            gm_real s = 0.0;
            for (k = 0; k < 2; k++) s += M2_AT(a, r, k) * M2_AT(b, k, c);
            M2_AT(out, r, c) = s;
        }
    }
    return out;
}

gm_mat2 gm_mat2_transpose(gm_mat2 m)
{
    gm_mat2 out;
    int c, r;
    for (c = 0; c < 2; c++)
        for (r = 0; r < 2; r++) M2_AT(out, r, c) = M2_AT(m, c, r);
    return out;
}

gm_mat2 gm_mat2_scale(gm_mat2 m, gm_real s)
{
    gm_mat2 out;
    int i;
    for (i = 0; i < 4; i++) out.v[i] = m.v[i] * s;
    return out;
}

gm_real gm_mat2_det(gm_mat2 m)
{
    return m.v[0] * m.v[3] - m.v[2] * m.v[1];
}

gm_real gm_mat2_trace(gm_mat2 m)
{
    return m.v[0] + m.v[3];
}

gm_status gm_mat2_inverse(gm_mat2 m, gm_mat2 *out)
{
    gm_real det = gm_mat2_det(m);
    if (out == NULL) return GM_ERR_ARG;
    if (det == 0.0) return GM_ERR_SINGULAR;
    out->v[0] = m.v[3] / det;
    out->v[1] = -m.v[1] / det;
    out->v[2] = -m.v[2] / det;
    out->v[3] = m.v[0] / det;
    return GM_OK;
}

gm_vec2 gm_mat2_mul_vec2(gm_mat2 m, gm_vec2 v)
{
    return gm_vec2_make(m.v[0] * v.x + m.v[2] * v.y,
                        m.v[1] * v.x + m.v[3] * v.y);
}

/* ============================== mat3 ============================== */

gm_mat3 gm_mat3_identity(void)
{
    gm_mat3 m;
    int i;
    for (i = 0; i < 9; i++) m.v[i] = 0.0;
    m.v[0] = 1.0; m.v[4] = 1.0; m.v[8] = 1.0;
    return m;
}

gm_mat3 gm_mat3_zero(void)
{
    gm_mat3 m;
    int i;
    for (i = 0; i < 9; i++) m.v[i] = 0.0;
    return m;
}

gm_mat3 gm_mat3_from_cols(gm_vec3 c0, gm_vec3 c1, gm_vec3 c2)
{
    gm_mat3 m;
    m.v[0] = c0.x; m.v[1] = c0.y; m.v[2] = c0.z;
    m.v[3] = c1.x; m.v[4] = c1.y; m.v[5] = c1.z;
    m.v[6] = c2.x; m.v[7] = c2.y; m.v[8] = c2.z;
    return m;
}

gm_real gm_mat3_at(gm_mat3 m, int row, int col)
{
    if (row < 0 || row > 2 || col < 0 || col > 2) return 0.0;
    return M3_AT(m, row, col);
}

gm_mat3 gm_mat3_with(gm_mat3 m, int row, int col, gm_real value)
{
    if (row >= 0 && row <= 2 && col >= 0 && col <= 2) M3_AT(m, row, col) = value;
    return m;
}

gm_mat3 gm_mat3_mul(gm_mat3 a, gm_mat3 b)
{
    gm_mat3 out;
    int c, r, k;
    for (c = 0; c < 3; c++) {
        for (r = 0; r < 3; r++) {
            gm_real s = 0.0;
            for (k = 0; k < 3; k++) s += M3_AT(a, r, k) * M3_AT(b, k, c);
            M3_AT(out, r, c) = s;
        }
    }
    return out;
}

gm_mat3 gm_mat3_transpose(gm_mat3 m)
{
    gm_mat3 out;
    int c, r;
    for (c = 0; c < 3; c++)
        for (r = 0; r < 3; r++) M3_AT(out, r, c) = M3_AT(m, c, r);
    return out;
}

gm_mat3 gm_mat3_scale(gm_mat3 m, gm_real s)
{
    gm_mat3 out;
    int i;
    for (i = 0; i < 9; i++) out.v[i] = m.v[i] * s;
    return out;
}

gm_real gm_mat3_det(gm_mat3 m)
{
    gm_real a00 = m.v[0], a10 = m.v[1], a20 = m.v[2];
    gm_real a01 = m.v[3], a11 = m.v[4], a21 = m.v[5];
    gm_real a02 = m.v[6], a12 = m.v[7], a22 = m.v[8];
    return a00 * (a11 * a22 - a12 * a21)
         - a01 * (a10 * a22 - a12 * a20)
         + a02 * (a10 * a21 - a11 * a20);
}

gm_real gm_mat3_trace(gm_mat3 m)
{
    return m.v[0] + m.v[4] + m.v[8];
}

gm_status gm_mat3_inverse(gm_mat3 m, gm_mat3 *out)
{
    gm_real a00 = m.v[0], a10 = m.v[1], a20 = m.v[2];
    gm_real a01 = m.v[3], a11 = m.v[4], a21 = m.v[5];
    gm_real a02 = m.v[6], a12 = m.v[7], a22 = m.v[8];
    gm_real det;
    if (out == NULL) return GM_ERR_ARG;
    det = a00 * (a11 * a22 - a12 * a21)
        - a01 * (a10 * a22 - a12 * a20)
        + a02 * (a10 * a21 - a11 * a20);
    if (det == 0.0) return GM_ERR_SINGULAR;
    out->v[0] = (a11 * a22 - a12 * a21) / det;
    out->v[1] = (a12 * a20 - a10 * a22) / det;
    out->v[2] = (a10 * a21 - a11 * a20) / det;
    out->v[3] = (a02 * a21 - a01 * a22) / det;
    out->v[4] = (a00 * a22 - a02 * a20) / det;
    out->v[5] = (a01 * a20 - a00 * a21) / det;
    out->v[6] = (a01 * a12 - a02 * a11) / det;
    out->v[7] = (a02 * a10 - a00 * a12) / det;
    out->v[8] = (a00 * a11 - a01 * a10) / det;
    return GM_OK;
}

gm_vec3 gm_mat3_mul_vec3(gm_mat3 m, gm_vec3 v)
{
    return gm_vec3_make(m.v[0] * v.x + m.v[3] * v.y + m.v[6] * v.z,
                        m.v[1] * v.x + m.v[4] * v.y + m.v[7] * v.z,
                        m.v[2] * v.x + m.v[5] * v.y + m.v[8] * v.z);
}

gm_mat3 gm_mat3_rotation_x(gm_real radians)
{
    gm_mat3 m = gm_mat3_identity();
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    M3_AT(m, 1, 1) = c; M3_AT(m, 1, 2) = -s;
    M3_AT(m, 2, 1) = s; M3_AT(m, 2, 2) = c;
    return m;
}

gm_mat3 gm_mat3_rotation_y(gm_real radians)
{
    gm_mat3 m = gm_mat3_identity();
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    M3_AT(m, 0, 0) = c; M3_AT(m, 0, 2) = s;
    M3_AT(m, 2, 0) = -s; M3_AT(m, 2, 2) = c;
    return m;
}

gm_mat3 gm_mat3_rotation_z(gm_real radians)
{
    gm_mat3 m = gm_mat3_identity();
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    M3_AT(m, 0, 0) = c; M3_AT(m, 0, 1) = -s;
    M3_AT(m, 1, 0) = s; M3_AT(m, 1, 1) = c;
    return m;
}

/* Matrice delle normali = transpose(inverse(m)). Matrice singolare -> zero. */
gm_mat3 gm_mat3_normal(gm_mat3 m)
{
    gm_mat3 inv;
    if (gm_mat3_inverse(m, &inv) != GM_OK) return gm_mat3_zero();
    return gm_mat3_transpose(inv);
}

/* ============================== mat4 ============================== */

/* Determinante del 3x3 della sottomatrice (righe r0,r1,r2 / colonne c0,c1,c2). */
static gm_real m4_minor3(gm_mat4 m, int r0, int r1, int r2, int c0, int c1, int c2)
{
    gm_real a = M4_AT(m, r0, c0), b = M4_AT(m, r0, c1), c = M4_AT(m, r0, c2);
    gm_real d = M4_AT(m, r1, c0), e = M4_AT(m, r1, c1), f = M4_AT(m, r1, c2);
    gm_real g = M4_AT(m, r2, c0), h = M4_AT(m, r2, c1), i = M4_AT(m, r2, c2);
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

gm_mat4 gm_mat4_identity(void)
{
    gm_mat4 m;
    int i;
    for (i = 0; i < 16; i++) m.v[i] = 0.0;
    m.v[0] = 1.0; m.v[5] = 1.0; m.v[10] = 1.0; m.v[15] = 1.0;
    return m;
}

gm_mat4 gm_mat4_zero(void)
{
    gm_mat4 m;
    int i;
    for (i = 0; i < 16; i++) m.v[i] = 0.0;
    return m;
}

gm_mat4 gm_mat4_from_cols(gm_vec4 c0, gm_vec4 c1, gm_vec4 c2, gm_vec4 c3)
{
    gm_mat4 m;
    m.v[0] = c0.x; m.v[1] = c0.y; m.v[2] = c0.z; m.v[3] = c0.w;
    m.v[4] = c1.x; m.v[5] = c1.y; m.v[6] = c1.z; m.v[7] = c1.w;
    m.v[8] = c2.x; m.v[9] = c2.y; m.v[10] = c2.z; m.v[11] = c2.w;
    m.v[12] = c3.x; m.v[13] = c3.y; m.v[14] = c3.z; m.v[15] = c3.w;
    return m;
}

gm_real gm_mat4_at(gm_mat4 m, int row, int col)
{
    if (row < 0 || row > 3 || col < 0 || col > 3) return 0.0;
    return M4_AT(m, row, col);
}

gm_mat4 gm_mat4_with(gm_mat4 m, int row, int col, gm_real value)
{
    if (row >= 0 && row <= 3 && col >= 0 && col <= 3) M4_AT(m, row, col) = value;
    return m;
}

gm_mat4 gm_mat4_mul(gm_mat4 a, gm_mat4 b)
{
    gm_mat4 out;
    int c, r, k;
    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            gm_real s = 0.0;
            for (k = 0; k < 4; k++) s += M4_AT(a, r, k) * M4_AT(b, k, c);
            M4_AT(out, r, c) = s;
        }
    }
    return out;
}

gm_mat4 gm_mat4_transpose(gm_mat4 m)
{
    gm_mat4 out;
    int c, r;
    for (c = 0; c < 4; c++)
        for (r = 0; r < 4; r++) M4_AT(out, r, c) = M4_AT(m, c, r);
    return out;
}

gm_mat4 gm_mat4_scale(gm_mat4 m, gm_real s)
{
    gm_mat4 out;
    int i;
    for (i = 0; i < 16; i++) out.v[i] = m.v[i] * s;
    return out;
}

gm_real gm_mat4_det(gm_mat4 m)
{
    return M4_AT(m, 0, 0) * m4_minor3(m, 1, 2, 3, 1, 2, 3)
         - M4_AT(m, 0, 1) * m4_minor3(m, 1, 2, 3, 0, 2, 3)
         + M4_AT(m, 0, 2) * m4_minor3(m, 1, 2, 3, 0, 1, 3)
         - M4_AT(m, 0, 3) * m4_minor3(m, 1, 2, 3, 0, 1, 2);
}

gm_real gm_mat4_trace(gm_mat4 m)
{
    return m.v[0] + m.v[5] + m.v[10] + m.v[15];
}

/* I tre indici 0..3 diversi da `exclude`, in ordine crescente. */
static void m4_exclude3(int exclude, int *a, int *b, int *c)
{
    int kept[3];
    int n = 0;
    int i;
    for (i = 0; i < 4; i++) {
        if (i != exclude) kept[n++] = i;
    }
    *a = kept[0];
    *b = kept[1];
    *c = kept[2];
}

gm_status gm_mat4_inverse(gm_mat4 m, gm_mat4 *out)
{
    gm_real det = gm_mat4_det(m);
    gm_real cof[16];
    int r, c;
    if (out == NULL) return GM_ERR_ARG;
    if (det == 0.0) return GM_ERR_SINGULAR;
    /* cofattore C[r][c] = sign(r+c) * minor(riga r e colonna c rimosse) */
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            int r0, r1, r2, c0, c1, c2;
            gm_real sign;
            m4_exclude3(r, &r0, &r1, &r2);
            m4_exclude3(c, &c0, &c1, &c2);
            sign = ((r + c) % 2 == 0) ? 1.0 : -1.0;
            cof[r * 4 + c] = sign * m4_minor3(m, r0, r1, r2, c0, c1, c2);
        }
    }
    /* inverse = transpose(C)/det; il flat column-major di out coincide con
     * l'indice row-major del cofattore trasposto. */
    for (r = 0; r < 16; r++) out->v[r] = cof[r] / det;
    return GM_OK;
}

gm_status gm_mat4_inverse_affine(gm_mat4 m, gm_mat4 *out)
{
    gm_mat3 a;
    gm_mat3 ainv;
    gm_vec3 t;
    gm_vec3 tinv;
    gm_status st;
    if (out == NULL) return GM_ERR_ARG;
    a = gm_mat3_from_mat4(m);
    st = gm_mat3_inverse(a, &ainv);
    if (st != GM_OK) return st;
    t = gm_vec3_make(m.v[12], m.v[13], m.v[14]);
    tinv = gm_mat3_mul_vec3(ainv, t);
    *out = gm_mat4_from_mat3(ainv);
    out->v[12] = -tinv.x;
    out->v[13] = -tinv.y;
    out->v[14] = -tinv.z;
    return GM_OK;
}

gm_vec4 gm_mat4_mul_vec4(gm_mat4 m, gm_vec4 v)
{
    return gm_vec4_make(m.v[0] * v.x + m.v[4] * v.y + m.v[8] * v.z + m.v[12] * v.w,
                        m.v[1] * v.x + m.v[5] * v.y + m.v[9] * v.z + m.v[13] * v.w,
                        m.v[2] * v.x + m.v[6] * v.y + m.v[10] * v.z + m.v[14] * v.w,
                        m.v[3] * v.x + m.v[7] * v.y + m.v[11] * v.z + m.v[15] * v.w);
}

gm_mat4 gm_mat4_translation(gm_real x, gm_real y, gm_real z)
{
    gm_mat4 m = gm_mat4_identity();
    m.v[12] = x;
    m.v[13] = y;
    m.v[14] = z;
    return m;
}

gm_mat4 gm_mat4_scale_matrix(gm_real x, gm_real y, gm_real z)
{
    gm_mat4 m = gm_mat4_identity();
    m.v[0] = x;
    m.v[5] = y;
    m.v[10] = z;
    return m;
}

gm_mat4 gm_mat4_rotation_x(gm_real radians)
{
    gm_mat4 m = gm_mat4_identity();
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    M4_AT(m, 1, 1) = c; M4_AT(m, 1, 2) = -s;
    M4_AT(m, 2, 1) = s; M4_AT(m, 2, 2) = c;
    return m;
}

gm_mat4 gm_mat4_rotation_y(gm_real radians)
{
    gm_mat4 m = gm_mat4_identity();
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    M4_AT(m, 0, 0) = c; M4_AT(m, 0, 2) = s;
    M4_AT(m, 2, 0) = -s; M4_AT(m, 2, 2) = c;
    return m;
}

gm_mat4 gm_mat4_rotation_z(gm_real radians)
{
    gm_mat4 m = gm_mat4_identity();
    gm_real c = cos(radians);
    gm_real s = sin(radians);
    M4_AT(m, 0, 0) = c; M4_AT(m, 0, 1) = -s;
    M4_AT(m, 1, 0) = s; M4_AT(m, 1, 1) = c;
    return m;
}

gm_mat4 gm_mat4_from_mat3(gm_mat3 m)
{
    gm_mat4 out = gm_mat4_identity();
    int c, r;
    for (c = 0; c < 3; c++)
        for (r = 0; r < 3; r++) M4_AT(out, r, c) = M3_AT(m, r, c);
    return out;
}

gm_mat3 gm_mat3_from_mat4(gm_mat4 m)
{
    gm_mat3 out;
    int c, r;
    for (c = 0; c < 3; c++)
        for (r = 0; r < 3; r++) M3_AT(out, r, c) = M4_AT(m, r, c);
    return out;
}

gm_vec3 gm_mat4_transform_point(gm_mat4 m, gm_vec3 p)
{
    gm_vec4 h = gm_mat4_mul_vec4(m, gm_vec4_make(p.x, p.y, p.z, 1.0));
    if (h.w != 0.0 && h.w != 1.0) {
        return gm_vec3_make(h.x / h.w, h.y / h.w, h.z / h.w);
    }
    return gm_vec3_make(h.x, h.y, h.z);
}

gm_vec3 gm_mat4_transform_dir(gm_mat4 m, gm_vec3 d)
{
    gm_vec4 h = gm_mat4_mul_vec4(m, gm_vec4_make(d.x, d.y, d.z, 0.0));
    return gm_vec3_make(h.x, h.y, h.z);
}
