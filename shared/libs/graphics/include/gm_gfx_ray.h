/*
 * gm_gfx_ray.h — ray tracing / intersezioni (riscrittura, dominio 3D).
 *
 * Raggio, intersezioni con sfera/piano/triangolo (Möller–Trumbore)/AABB/box,
 * riflessione e rifrazione.
 */
#ifndef GMMATH_GM_GFX_RAY_H
#define GMMATH_GM_GFX_RAY_H

#include "gm_core.h"
#include "gm_vec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gm_ray { gm_vec3 origin, dir; } gm_ray;

gm_ray gm_ray_make(gm_vec3 origin, gm_vec3 dir);
/* punto al parametro t: origin + t*dir. */
gm_vec3 gm_ray_at(gm_ray r, gm_real t);

/* Sfera (center, radius): 1 se colpita, *t = t positiva più vicina. */
int gm_ray_sphere(gm_ray r, gm_vec3 center, gm_real radius, gm_real *t);
/* Piano (point, normal): 1 se colpito (dir non parallela), *t. */
int gm_ray_plane(gm_ray r, gm_vec3 point, gm_vec3 normal, gm_real *t);
/* Triangolo Möller–Trumbore: 1 se colpito, *t, *u, *v baricentriche. */
int gm_ray_triangle(gm_ray r, gm_vec3 v0, gm_vec3 v1, gm_vec3 v2,
                    gm_real *t, gm_real *u, gm_real *v);
/* AABB (slab method): 1 se colpito, imposta tmin e tmax. */
int gm_ray_aabb(gm_ray r, gm_vec3 bmin, gm_vec3 bmax, gm_real *tmin, gm_real *tmax);
/* Box: t della prima faccia colpita. */
int gm_ray_box(gm_ray r, gm_vec3 bmin, gm_vec3 bmax, gm_real *t);

/* --- raycasting su griglia (DDA, stile Wolfenstein 3D; da cub3d42) --- */
typedef struct gm_raycast_hit {
    int hit;            /* 1 se una cella solida è stata colpita */
    int map_x, map_y;   /* cella colpita */
    int side;           /* 0 = parete verticale (x), 1 = orizzontale (y) */
    gm_real perp_dist;  /* distanza perpendicolare (corretta per fisheye) */
    gm_real wall_x;     /* coordinata lungo la parete in [0,1) (per le texture) */
} gm_raycast_hit;

/* Lancia un raggio su una griglia rows×cols (row-major); grid != 0 = solido.
 * (pos_x, pos_y) origine, (dir_x, dir_y) direzione (anche non normalizzata).
 * Ritorna 1 se colpisce, 0 altrimenti (fuori griglia o max_steps). */
int gm_raycast_dda(const int *grid, int rows, int cols, gm_real pos_x, gm_real pos_y,
                   gm_real dir_x, gm_real dir_y, int max_steps, gm_raycast_hit *hit);

/* Riflessione r = v - 2(v·n)n (n normalizzato). */
gm_vec3 gm_ray_reflect(gm_vec3 v, gm_vec3 n);
/* Rifrazione (legge di Snell). Ritorna 0 su riflessione totale interna. */
int gm_ray_refract(gm_vec3 v, gm_vec3 n, gm_real eta, gm_vec3 *out);

/* Cilindro finito (base = centro di una base, axis unitario, radius, height):
 * 1 se colpito, *t = t positiva più vicina (parete + tappi). */
int gm_ray_cylinder(gm_ray r, gm_vec3 base, gm_vec3 axis, gm_real radius,
                    gm_real height, gm_real *t);
/* Cono finito (apice, axis unitario verso la base, radius della base, height):
 * 1 se colpito, *t (superficie laterale + tappo di base). */
int gm_ray_cone(gm_ray r, gm_vec3 apex, gm_vec3 axis, gm_real radius,
                gm_real height, gm_real *t);

/* Normali su cilindro / cono (p sulla superficie, axis unitario). */
gm_vec3 gm_cylinder_normal(gm_vec3 base, gm_vec3 axis, gm_vec3 p);
/* k = radius/height del cono. */
gm_vec3 gm_cone_normal(gm_vec3 apex, gm_vec3 axis, gm_real k, gm_vec3 p);

/* Shading di Phong: ambient + diffuse + specular.
 * p punto, n normale unitaria, view_dir verso l'osservatore, light_pos,
 * light_color, obj_color; ka/kd/ks coefficienti, shininess esponente. */
gm_vec3 gm_shade_phong(gm_vec3 p, gm_vec3 n, gm_vec3 view_dir, gm_vec3 light_pos,
                       gm_vec3 light_color, gm_vec3 obj_color,
                       gm_real ka, gm_real kd, gm_real ks, gm_real shininess);

/* Volume rendering (alpha compositing) lungo un raggio, come in NeRF/f2-nerf.
 * n campioni: sigma[n] densità, rgb[n*3] colori, dt[n] spazi tra campioni.
 * out_rgb[3] colore composito, *out_acc opacità accumulata (se non NULL). */
void gm_gfx_volume_render(int n, const gm_real *sigma, const gm_real *rgb,
                          const gm_real *dt, gm_real *out_rgb, gm_real *out_acc);

#ifdef __cplusplus
}
#endif

#endif /* GMMATH_GM_GFX_RAY_H */
