/*
 * gm_mat.h — matrici 2x2/3x3/4x4 (aggiunte dal gap vecmat).
 *
 * Storage COLUMN-MAJOR: l'elemento (riga r, colonna c) è v[c*N + r].
 * Le funzioni di rotazione usano radianti e sistema destrorso.
 */
#ifndef GMMATH_GM_MAT_H
#define GMMATH_GM_MAT_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_mat2 { gm_real v[4]; } gm_mat2;
typedef struct gm_mat3 { gm_real v[9]; } gm_mat3;
typedef struct gm_mat4 { gm_real v[16]; } gm_mat4;

/* ---- mat2 ---- */
gm_mat2 gm_mat2_identity(void);
gm_mat2 gm_mat2_zero(void);
gm_mat2 gm_mat2_from_cols(gm_vec2 c0, gm_vec2 c1);
gm_real gm_mat2_at(gm_mat2 m, int row, int col);
gm_mat2 gm_mat2_with(gm_mat2 m, int row, int col, gm_real value);
gm_mat2 gm_mat2_mul(gm_mat2 a, gm_mat2 b);
gm_mat2 gm_mat2_transpose(gm_mat2 m);
gm_mat2 gm_mat2_scale(gm_mat2 m, gm_real s);
gm_real gm_mat2_det(gm_mat2 m);
gm_real gm_mat2_trace(gm_mat2 m);
gm_status gm_mat2_inverse(gm_mat2 m, gm_mat2 *out);
gm_vec2 gm_mat2_mul_vec2(gm_mat2 m, gm_vec2 v);

/* ---- mat3 ---- */
gm_mat3 gm_mat3_identity(void);
gm_mat3 gm_mat3_zero(void);
gm_mat3 gm_mat3_from_cols(gm_vec3 c0, gm_vec3 c1, gm_vec3 c2);
gm_real gm_mat3_at(gm_mat3 m, int row, int col);
gm_mat3 gm_mat3_with(gm_mat3 m, int row, int col, gm_real value);
gm_mat3 gm_mat3_mul(gm_mat3 a, gm_mat3 b);
gm_mat3 gm_mat3_transpose(gm_mat3 m);
gm_mat3 gm_mat3_scale(gm_mat3 m, gm_real s);
gm_real gm_mat3_det(gm_mat3 m);
gm_real gm_mat3_trace(gm_mat3 m);
gm_status gm_mat3_inverse(gm_mat3 m, gm_mat3 *out);
gm_vec3 gm_mat3_mul_vec3(gm_mat3 m, gm_vec3 v);
gm_mat3 gm_mat3_rotation_x(gm_real radians);
gm_mat3 gm_mat3_rotation_y(gm_real radians);
gm_mat3 gm_mat3_rotation_z(gm_real radians);
gm_mat3 gm_mat3_normal(gm_mat3 m);

/* ---- mat4 ---- */
gm_mat4 gm_mat4_identity(void);
gm_mat4 gm_mat4_zero(void);
gm_mat4 gm_mat4_from_cols(gm_vec4 c0, gm_vec4 c1, gm_vec4 c2, gm_vec4 c3);
gm_real gm_mat4_at(gm_mat4 m, int row, int col);
gm_mat4 gm_mat4_with(gm_mat4 m, int row, int col, gm_real value);
gm_mat4 gm_mat4_mul(gm_mat4 a, gm_mat4 b);
gm_mat4 gm_mat4_transpose(gm_mat4 m);
gm_mat4 gm_mat4_scale(gm_mat4 m, gm_real s);
gm_real gm_mat4_det(gm_mat4 m);
gm_real gm_mat4_trace(gm_mat4 m);
gm_status gm_mat4_inverse(gm_mat4 m, gm_mat4 *out);
gm_status gm_mat4_inverse_affine(gm_mat4 m, gm_mat4 *out);
gm_vec4 gm_mat4_mul_vec4(gm_mat4 m, gm_vec4 v);
gm_mat4 gm_mat4_translation(gm_real x, gm_real y, gm_real z);
gm_mat4 gm_mat4_scale_matrix(gm_real x, gm_real y, gm_real z);
gm_mat4 gm_mat4_rotation_x(gm_real radians);
gm_mat4 gm_mat4_rotation_y(gm_real radians);
gm_mat4 gm_mat4_rotation_z(gm_real radians);
gm_mat4 gm_mat4_from_mat3(gm_mat3 m);
gm_mat3 gm_mat3_from_mat4(gm_mat4 m);
gm_vec3 gm_mat4_transform_point(gm_mat4 m, gm_vec3 p);
gm_vec3 gm_mat4_transform_dir(gm_mat4 m, gm_vec3 d);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_MAT_H */
