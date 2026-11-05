/*
 * gm_gfx_geom.h — geometria 3D (riscrittura, dominio 3D).
 *
 * AABB, frustum (estrazione piani + test), matrici di proiezione/vista,
 * decomposizione SRT di una matrice affine.
 */
#ifndef GMMATH_GM_GFX_GEOM_H
#define GMMATH_GM_GFX_GEOM_H

#include "gm_core.h"
#include "gm_vec.h"
#include "gm_mat.h"
#include "gm_quat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_aabb { gm_vec3 min, max; } gm_aabb;

gm_aabb gm_aabb_make(gm_vec3 min, gm_vec3 max);
gm_aabb gm_aabb_from_points(const gm_vec3 *pts, int n);
int gm_aabb_valid(gm_aabb b);
gm_aabb gm_aabb_merge(gm_aabb a, gm_aabb b);
gm_aabb gm_aabb_transform(gm_aabb b, gm_mat4 m);
gm_vec3 gm_aabb_center(gm_aabb b);
gm_vec3 gm_aabb_extents(gm_aabb b);
int gm_aabb_test_point(gm_aabb b, gm_vec3 p);
int gm_aabb_intersect(gm_aabb a, gm_aabb b);

/* Proiezioni/vista (mat4 column-major, sistema destrorso, clip z in [-1,1]). */
gm_mat4 gm_mat4_perspective(gm_real fovy_rad, gm_real aspect, gm_real znear, gm_real zfar);
gm_mat4 gm_mat4_ortho(gm_real left, gm_real right, gm_real bottom, gm_real top,
                      gm_real znear, gm_real zfar);
gm_mat4 gm_mat4_look_at(gm_vec3 eye, gm_vec3 center, gm_vec3 up);

/* Frustum: 6 piani (a,b,c,d) con a*x+b*y+c*z+d >= 0 dentro. */
typedef struct gm_frustum { gm_vec4 planes[6]; } gm_frustum;
gm_frustum gm_frustum_from_matrix(gm_mat4 m);
int gm_frustum_test_point(gm_frustum f, gm_vec3 p);
int gm_frustum_intersect_aabb(gm_frustum f, gm_aabb b);

/* Decomposizione SRT: m = T * R * S. Ritorna 0 se la matrice è degenere. */
int gm_mat4_decompose_srt(gm_mat4 m, gm_vec3 *scale, gm_quat *rotation, gm_vec3 *translation);

/* ---- camera (da ZINC) ---- */
typedef struct gm_camera {
    gm_vec3 position, right, up, forward;
    gm_real fov, aspect, znear, zfar;
} gm_camera;

/* Camera prospettica da posizione, target e up. */
gm_camera gm_camera_make(gm_vec3 position, gm_vec3 target, gm_vec3 up);
gm_camera gm_camera_perspective(gm_vec3 position, gm_vec3 target, gm_vec3 up,
                                gm_real fov_rad, gm_real aspect, gm_real znear, gm_real zfar);
gm_mat4 gm_camera_view(const gm_camera *c);
gm_mat4 gm_camera_projection(const gm_camera *c);
gm_mat4 gm_camera_view_projection(const gm_camera *c);
/* Orbita attorno a target: yaw/pitch (radianti), radius. */
void gm_camera_orbit(gm_camera *c, gm_vec3 target, gm_real yaw, gm_real pitch, gm_real radius);
void gm_camera_translate(gm_camera *c, gm_vec3 delta);
/* Direzione del raggio dal NDC ([-1,1]²) nel mondo. */
gm_vec3 gm_camera_ndc_ray_dir(const gm_camera *c, gm_real ndc_x, gm_real ndc_y);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_GEOM_H */
