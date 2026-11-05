/*
 * gm_quat.h — quaternioni (aggiunte dal gap vecmat).
 *
 * Convenzione: (x, y, z) parte vettoriale, w parte scalare. Unità per le
 * rotazioni; gm_quat_to_mat3/rotate_vec3 assumono un quaternione normalizzato.
 */
#ifndef GMMATH_GM_QUAT_H
#define GMMATH_GM_QUAT_H

#include "gm_core.h"
#include "gm_mat.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_quat { gm_real x, y, z, w; } gm_quat;

gm_quat gm_quat_identity(void);
gm_quat gm_quat_make(gm_real x, gm_real y, gm_real z, gm_real w);
gm_quat gm_quat_add(gm_quat a, gm_quat b);
gm_quat gm_quat_scale(gm_quat q, gm_real s);
gm_quat gm_quat_neg(gm_quat q);
gm_quat gm_quat_mul(gm_quat a, gm_quat b);
gm_quat gm_quat_conjugate(gm_quat q);
gm_real gm_quat_dot(gm_quat a, gm_quat b);
gm_real gm_quat_length(gm_quat q);
gm_quat gm_quat_normalize(gm_quat q);
gm_status gm_quat_inverse(gm_quat q, gm_quat *out);
gm_quat gm_quat_lerp(gm_quat a, gm_quat b, gm_real t);
gm_quat gm_quat_slerp(gm_quat a, gm_quat b, gm_real t);
gm_quat gm_quat_from_dir(gm_vec3 dir);
gm_quat gm_quat_from_axis_angle(gm_vec3 axis, gm_real radians);
gm_mat3 gm_quat_to_mat3(gm_quat q);
gm_quat gm_quat_from_mat3(gm_mat3 m);
gm_vec3 gm_quat_rotate_vec3(gm_quat q, gm_vec3 v);
gm_quat gm_quat_from_two_vectors(gm_vec3 from, gm_vec3 to);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_QUAT_H */
