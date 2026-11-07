/*
 * ofx_gfx_3d.c — shim FFI del dominio 3D di `libgmbgraphics` (gfx3d.inc).
 *
 * Tutto passa in array DOUBLE: vettori come [x,y,z], matrici 4x4 come 16 valori
 * (layout di gm_mat4.v, column-major), AABB come [minx,miny,minz,maxx,maxy,maxz],
 * frustum come 6 piani [a,b,c,d] (24 valori), raggi come [ox,oy,oz,dx,dy,dz].
 *
 * Mesh e raster sono STATEFUL (puntatori): handle = indice+1 in una tabella, 0 = errore.
 * I buffer restituiti (MESHPOS/MESHTRI) sono scritti in out() del chiamante.
 */
#include "gm_gfx_geom.h"
#include "gm_gfx_intersect.h"
#include "gm_gfx_ray.h"
#include "gm_gfx_mesh.h"
#include "gm_gfx_raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define OFX_MAX_MESH 64
#define OFX_MAX_RASTER 64
static gm_mesh g_meshes[OFX_MAX_MESH];
static gm_raster g_rasters[OFX_MAX_RASTER];

static gm_vec3 v3(const double* p) { gm_vec3 v; v.x = p[0]; v.y = p[1]; v.z = p[2]; return v; }
static void v3_out(gm_vec3 v, double* out) { out[0] = v.x; out[1] = v.y; out[2] = v.z; }
static gm_aabb box_in(const double* b)
{
    gm_aabb a; a.min = v3(b); a.max = v3(b + 3); return a;
}
static void box_out(gm_aabb a, double* out)
{
    out[0] = a.min.x; out[1] = a.min.y; out[2] = a.min.z;
    out[3] = a.max.x; out[4] = a.max.y; out[5] = a.max.z;
}
static gm_ray ray_in(const double* r) { gm_ray x; x.origin = v3(r); x.dir = v3(r + 3); return x; }
static void mat4_out(gm_mat4 m, double* out) { for (int i = 0; i < 16; i++) out[i] = m.v[i]; }
static gm_mat4 mat4_in(const double* p) { gm_mat4 m; for (int i = 0; i < 16; i++) m.v[i] = p[i]; return m; }
static gm_frustum frustum_in(const double* p)
{
    gm_frustum f;
    for (int i = 0; i < 6; i++)
    {
        f.planes[i].x = p[4 * i + 0]; f.planes[i].y = p[4 * i + 1];
        f.planes[i].z = p[4 * i + 2]; f.planes[i].w = p[4 * i + 3];
    }
    return f;
}

/* ── geometria ──────────────────────────────────────────────────────────── */

int ofx_gfx_perspective(double fovy, double aspect, double zn, double zf, double* out, int64_t out_n)
{ if (!out) return -1; mat4_out(gm_mat4_perspective(fovy, aspect, zn, zf), out); return 0; }

int ofx_gfx_ortho(double l, double r, double b, double t, double zn, double zf, double* out, int64_t out_n)
{ if (!out) return -1; mat4_out(gm_mat4_ortho(l, r, b, t, zn, zf), out); return 0; }

int ofx_gfx_lookat(const double* eye, int64_t eye_n, const double* center, int64_t center_n, const double* up, int64_t up_n, double* out, int64_t out_n)
{ if (!eye || !center || !up || !out) return -1; mat4_out(gm_mat4_look_at(v3(eye), v3(center), v3(up)), out); return 0; }

int ofx_gfx_aabb_from_points(const double* pts, int64_t pts_n, double* out, int64_t out_n)
{
    /* il numero di valori e' l'estensione dell'array (n esplicito tolto dalla DECLARE) */
    if (!pts || !out || pts_n < 3 || out_n < 6) return -1;
    int n = (int)(pts_n / 3);
    gm_vec3* v = (gm_vec3*)malloc(sizeof(gm_vec3) * (size_t)n);
    if (!v) return -1;
    for (int i = 0; i < n; i++) v[i] = v3(pts + 3 * i);
    gm_aabb b = gm_aabb_from_points(v, n);
    free(v);
    box_out(b, out);
    return 0;
}

int ofx_gfx_aabb_merge(const double* a, int64_t a_n, const double* b, int64_t b_n, double* out, int64_t out_n)
{ if (!a || !b || !out) return -1; box_out(gm_aabb_merge(box_in(a), box_in(b)), out); return 0; }

int ofx_gfx_aabb_center(const double* a, int64_t a_n, double* out, int64_t out_n)
{ if (!a || !out) return -1; v3_out(gm_aabb_center(box_in(a)), out); return 0; }

int ofx_gfx_aabb_extents(const double* a, int64_t a_n, double* out, int64_t out_n)
{ if (!a || !out) return -1; v3_out(gm_aabb_extents(box_in(a)), out); return 0; }

int ofx_gfx_aabb_test_point(const double* a, int64_t a_n, const double* p, int64_t p_n)
{ if (!a || !p) return 0; return gm_aabb_test_point(box_in(a), v3(p)); }

int ofx_gfx_aabb_intersect(const double* a, int64_t a_n, const double* b, int64_t b_n)
{ if (!a || !b) return 0; return gm_aabb_intersect(box_in(a), box_in(b)); }

int ofx_gfx_frustum_from(const double* m, int64_t m_n, double* out, int64_t out_n)
{
    if (!m || !out) return -1;
    gm_frustum f = gm_frustum_from_matrix(mat4_in(m));
    for (int i = 0; i < 6; i++)
    {
        out[4 * i + 0] = f.planes[i].x; out[4 * i + 1] = f.planes[i].y;
        out[4 * i + 2] = f.planes[i].z; out[4 * i + 3] = f.planes[i].w;
    }
    return 0;
}

int ofx_gfx_frustum_test_point(const double* planes, int64_t planes_n, const double* p, int64_t p_n)
{ if (!planes || !p) return 0; return gm_frustum_test_point(frustum_in(planes), v3(p)); }

int ofx_gfx_frustum_test_aabb(const double* planes, int64_t planes_n, const double* box, int64_t box_n)
{ if (!planes || !box) return 0; return gm_frustum_intersect_aabb(frustum_in(planes), box_in(box)); }

int ofx_gfx_decompose_srt(const double* m, int64_t m_n, double* scale, int64_t scale_n, double* rot, int64_t rot_n, double* pos, int64_t pos_n)
{
    if (!m || !scale || !rot || !pos) return -1;
    gm_vec3 s, t; gm_quat q;
    int rc = gm_mat4_decompose_srt(mat4_in(m), &s, &q, &t);
    v3_out(s, scale); v3_out(t, pos);
    rot[0] = q.x; rot[1] = q.y; rot[2] = q.z; rot[3] = q.w;
    return rc;
}

int ofx_gfx_camera_view(const double* pos, int64_t pos_n, const double* target, int64_t target_n, const double* up, int64_t up_n, double* out, int64_t out_n)
{
    if (!pos || !target || !up || !out) return -1;
    gm_camera c = gm_camera_make(v3(pos), v3(target), v3(up));
    mat4_out(gm_camera_view(&c), out);
    return 0;
}

int ofx_gfx_camera_projection(double fov, double aspect, double zn, double zf, double* out, int64_t out_n)
{
    if (!out) return -1;
    gm_camera c = gm_camera_perspective((gm_vec3){0, 0, 0}, (gm_vec3){0, 0, -1}, (gm_vec3){0, 1, 0}, fov, aspect, zn, zf);
    mat4_out(gm_camera_projection(&c), out);
    return 0;
}

int ofx_gfx_camera_view_projection(const double* pos, int64_t pos_n, const double* target, int64_t target_n, const double* up, int64_t up_n,
                                   double fov, double aspect, double zn, double zf, double* out, int64_t out_n)
{
    if (!pos || !target || !up || !out) return -1;
    gm_camera c = gm_camera_perspective(v3(pos), v3(target), v3(up), fov, aspect, zn, zf);
    mat4_out(gm_camera_view_projection(&c), out);
    return 0;
}

/* ── raggi e intersezioni ───────────────────────────────────────────────── */

int ofx_gfx_ray_sphere(const double* r, int64_t r_n, const double* sphere, int64_t sphere_n, double* t, int64_t t_n)
{
    if (!r || !sphere || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_sphere(ray_in(r), v3(sphere), sphere[3], &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_plane(const double* r, int64_t r_n, const double* plane, int64_t plane_n, double* t, int64_t t_n)
{
    if (!r || !plane || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_plane(ray_in(r), v3(plane), v3(plane + 3), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_triangle(const double* r, int64_t r_n, const double* tri, int64_t tri_n, double* tuv, int64_t tuv_n)
{
    if (!r || !tri || !tuv) return -1;
    gm_real t = 0, u = 0, v = 0;
    int hit = gm_ray_triangle(ray_in(r), v3(tri), v3(tri + 3), v3(tri + 6), &t, &u, &v);
    tuv[0] = t; tuv[1] = u; tuv[2] = v;
    return hit;
}

int ofx_gfx_ray_aabb(const double* r, int64_t r_n, const double* box, int64_t box_n, double* t, int64_t t_n)
{
    if (!r || !box || !t) return -1;
    gm_real tmin = 0, tmax = 0;
    int hit = gm_ray_aabb(ray_in(r), v3(box), v3(box + 3), &tmin, &tmax);
    t[0] = tmin; t[1] = tmax;
    return hit;
}

int ofx_gfx_ray_box(const double* r, int64_t r_n, const double* box, int64_t box_n, double* t, int64_t t_n)
{
    if (!r || !box || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_box(ray_in(r), v3(box), v3(box + 3), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_reflect(const double* v, int64_t v_n, const double* n, int64_t n_n, double* out, int64_t out_n)
{ if (!v || !n || !out) return -1; v3_out(gm_ray_reflect(v3(v), v3(n)), out); return 0; }

int ofx_gfx_ray_refract(const double* v, int64_t v_n, const double* n, int64_t n_n, double eta, double* out, int64_t out_n)
{
    if (!v || !n || !out) return -1;
    gm_vec3 o;
    int rc = gm_ray_refract(v3(v), v3(n), eta, &o);
    v3_out(o, out);
    return rc;
}

/* params = [light_rgb(3), obj_rgb(3), ka, kd, ks, shininess] (10 valori): un solo array
 * accessorio evita le chiamate FFI con troppi array (limite pratico dell'emitter). */
int ofx_gfx_shade_phong(const double* p, int64_t p_n, const double* n, int64_t n_n, const double* view, int64_t view_n, const double* light, int64_t light_n,
                        const double* params, int64_t params_n, double* out, int64_t out_n)
{
    if (!p || !n || !view || !light || !params || !out) return -1;
    v3_out(gm_shade_phong(v3(p), v3(n), v3(view), v3(light), v3(params), v3(params + 3),
                          params[6], params[7], params[8], params[9]), out);
    return 0;
}

int ofx_gfx_sphere_sphere(const double* c1, int64_t c1_n, double r1, const double* c2, int64_t c2_n, double r2)
{ if (!c1 || !c2) return 0; return gm_intersect_sphere_sphere(v3(c1), r1, v3(c2), r2); }

int ofx_gfx_sphere_box(const double* c, int64_t c_n, double r, const double* box, int64_t box_n)
{ if (!c || !box) return 0; return gm_intersect_sphere_box(v3(c), r, v3(box), v3(box + 3)); }

int ofx_gfx_sphere_plane(const double* c, int64_t c_n, double r, const double* plane, int64_t plane_n, double* dist, int64_t dist_n)
{
    if (!c || !plane || !dist) return -1;
    gm_real d = 0;
    int hit = gm_intersect_sphere_plane(v3(c), r, v3(plane), v3(plane + 3), &d);
    dist[0] = d;
    return hit;
}

int ofx_gfx_segment_plane(const double* a, int64_t a_n, const double* b, int64_t b_n, const double* plane, int64_t plane_n, double* t, int64_t t_n)
{
    if (!a || !b || !plane || !t) return -1;
    gm_real tt = 0;
    int hit = gm_intersect_segment_plane(v3(a), v3(b), v3(plane), v3(plane + 3), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_segment_sphere(const double* a, int64_t a_n, const double* b, int64_t b_n, const double* c, int64_t c_n, double r, double* t, int64_t t_n)
{
    if (!a || !b || !c || !t) return -1;
    gm_real tt = 0;
    int hit = gm_intersect_segment_sphere(v3(a), v3(b), v3(c), r, &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_segment_triangle(const double* a, int64_t a_n, const double* b, int64_t b_n, const double* tri, int64_t tri_n, double* t, int64_t t_n)
{
    if (!a || !b || !tri || !t) return -1;
    gm_real tt = 0;
    int hit = gm_intersect_segment_triangle(v3(a), v3(b), v3(tri), v3(tri + 3), v3(tri + 6), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_triangle_triangle(const double* t1, int64_t t1_n, const double* t2, int64_t t2_n)
{
    if (!t1 || !t2) return 0;
    gm_vec3 a[3] = { v3(t1), v3(t1 + 3), v3(t1 + 6) };
    gm_vec3 b[3] = { v3(t2), v3(t2 + 3), v3(t2 + 6) };
    return gm_intersect_triangle_triangle(a, b);
}

int ofx_gfx_volume_render(int n, const double* sigma, int64_t sigma_n, const double* rgb, int64_t rgb_n, const double* dt, int64_t dt_n,
                          double* out_rgb, int64_t out_rgb_n, double* out_acc, int64_t out_acc_n)
{
    if (n <= 0 || !sigma || !rgb || !dt || !out_rgb) return -1;
    gm_real acc = 0;
    gm_gfx_volume_render(n, sigma, rgb, dt, out_rgb, &acc);
    if (out_acc) out_acc[0] = acc;
    return 0;
}

/* ── mesh (handle) ──────────────────────────────────────────────────────── */

static int mesh_store(gm_mesh m)
{
    if (m.nv <= 0 || m.nt <= 0) { gm_mesh_free(&m); return 0; }
    for (int i = 0; i < OFX_MAX_MESH; i++)
    {
        if (g_meshes[i].nv == 0 && g_meshes[i].pos == NULL)
        {
            g_meshes[i] = m;
            return i + 1;
        }
    }
    gm_mesh_free(&m);
    return 0;
}

static gm_mesh* mesh_get(int h)
{
    int i = h - 1;
    if (i < 0 || i >= OFX_MAX_MESH) return NULL;
    if (g_meshes[i].pos == NULL) return NULL;
    return &g_meshes[i];
}

int ofx_gfx_mesh_sphere(int slices, int stacks, double radius)
{ return mesh_store(gm_mesh_uv_sphere(slices, stacks, radius)); }
int ofx_gfx_mesh_box(double sx, double sy, double sz)
{ return mesh_store(gm_mesh_box(sx, sy, sz)); }
int ofx_gfx_mesh_plane(int nx, int ny, double sx, double sy)
{ return mesh_store(gm_mesh_plane(nx, ny, sx, sy)); }
int ofx_gfx_mesh_cylinder(int slices, double radius, double height)
{ return mesh_store(gm_mesh_cylinder(slices, radius, height)); }

int ofx_gfx_mesh_nv(int h) { gm_mesh* m = mesh_get(h); return m ? m->nv : -1; }
int ofx_gfx_mesh_nt(int h) { gm_mesh* m = mesh_get(h); return m ? m->nt : -1; }

int ofx_gfx_mesh_pos(int h, double* out, int64_t out_n)
{
    gm_mesh* m = mesh_get(h);
    if (!m || !out) return -1;
    for (int i = 0; i < m->nv; i++) { out[3 * i] = m->pos[i].x; out[3 * i + 1] = m->pos[i].y; out[3 * i + 2] = m->pos[i].z; }
    return 0;
}

int ofx_gfx_mesh_tri(int h, double* out, int64_t out_n)
{
    gm_mesh* m = mesh_get(h);
    if (!m || !out) return -1;
    for (int i = 0; i < m->nt * 3; i++) out[i] = (double)m->tri[i];
    return 0;
}

int ofx_gfx_mesh_free(int h)
{
    gm_mesh* m = mesh_get(h);
    if (!m) return -1;
    gm_mesh_free(m);
    memset(m, 0, sizeof(*m));
    return 0;
}

/* ── raster (handle) ────────────────────────────────────────────────────── */

static gm_raster* raster_get(int h)
{
    int i = h - 1;
    if (i < 0 || i >= OFX_MAX_RASTER) return NULL;
    if (g_rasters[i].color == NULL) return NULL;
    return &g_rasters[i];
}

int ofx_gfx_raster_new(int w, int h)
{
    if (w <= 0 || h <= 0) return 0;
    for (int i = 0; i < OFX_MAX_RASTER; i++)
    {
        if (g_rasters[i].color == NULL)
        {
            if (gm_raster_alloc(&g_rasters[i], w, h) != 0) return 0;
            return i + 1;
        }
    }
    return 0;
}

int ofx_gfx_raster_free(int h)
{
    gm_raster* r = raster_get(h);
    if (!r) return -1;
    gm_raster_free(r);
    memset(r, 0, sizeof(*r));
    return 0;
}

int ofx_gfx_raster_clear(int h, double depth)
{
    gm_raster* r = raster_get(h);
    if (!r) return -1;
    gm_raster_clear(r, depth);
    return 0;
}

int ofx_gfx_raster_get(int h, int x, int y, double* out, int64_t out_n)
{
    gm_raster* r = raster_get(h);
    if (!r || !out || x < 0 || y < 0 || x >= r->w || y >= r->h) return -1;
    gm_real rgb[3];
    gm_raster_get(r, x, y, rgb);
    out[0] = rgb[0]; out[1] = rgb[1]; out[2] = rgb[2];
    return 0;
}

int ofx_gfx_raster_tri(int h, const double* p, int64_t p_n, const double* c, int64_t c_n)
{
    gm_raster* r = raster_get(h);
    if (!r || !p || !c) return -1;
    gm_raster_triangle(r, p, p + 3, p + 6, c, c + 3, c + 6);
    return 0;
}

int ofx_gfx_raster_clip_near(const double* tri, int64_t tri_n, double znear, double* out, int64_t out_n)
{
    if (!tri || !out) return -1;
    return gm_raster_clip_near(tri, znear, out);
}

/* ── superficie: mesh da campo di altezza + raycast DDA (gfx3d.inc) ──────── */

#include "gm_gfx_surface.h"

int ofx_gfx_mesh_surface(const double* z, int64_t z_n, int nx, int ny, double sx, double sy, double zscale)
{
    if (!z || nx < 2 || ny < 2) return 0;
    gm_mesh m = { 0, NULL, 0, NULL };
    if (gm_mesh_surface(z, nx, ny, sx, sy, zscale, &m) != 0) return 0;
    return mesh_store(m);
}

/* Raycasting su griglia: hit() = [hit,map_x,map_y,side,perp_dist,wall_x].
 * `steps` <= 0 -> 256 (default storico); oltre il limite la cella non e' colpita. */
static int raycast_dda_steps(const double* grid, int rows, int cols, double px, double py,
                             double dx, double dy, int steps, double* out)
{
    if (!grid || !out || rows <= 0 || cols <= 0) return -1;
    if (steps <= 0) steps = 256;
    int* g = (int*)malloc(sizeof(int) * (size_t)rows * (size_t)cols);
    if (!g) return -1;
    for (int i = 0; i < rows * cols; i++) g[i] = (grid[i] != 0.0) ? 1 : 0;
    gm_raycast_hit h = { 0, 0, 0, 0, 0.0, 0.0 };
    int rc = gm_raycast_dda(g, rows, cols, px, py, dx, dy, steps, &h);
    free(g);
    out[0] = h.hit; out[1] = h.map_x; out[2] = h.map_y;
    out[3] = h.side; out[4] = h.perp_dist; out[5] = h.wall_x;
    return rc;
}

/* Compat: 256 passi (massimo storico). */
int ofx_gfx_raycast_dda(const double* grid, int64_t grid_n, int rows, int cols, double px, double py,
                        double dx, double dy, double* out, int64_t out_n)
{
    return raycast_dda_steps(grid, rows, cols, px, py, dx, dy, 256, out);
}

/* Variante con limite di passi configurabile (gfx3d.inc: RAYCASTDDAEX).
 * ray() = [px, py, dx, dy, maxsteps]: con 9 parametri scalari si supererebbero gli 8 slot
 * nativi ammessi dall'FFI, quindi gli argomenti del raggio viaggiano in un array. */
int ofx_gfx_raycast_dda_ex(const double* grid, int64_t grid_n, int rows, int cols, const double* ray, int64_t ray_n, double* out, int64_t out_n)
{
    if (!ray) return -1;
    int steps = (int)ray[4];
    return raycast_dda_steps(grid, rows, cols, ray[0], ray[1], ray[2], ray[3], steps, out);
}

/* ── cilindro/cono + AABB transform (gfx3d.inc) ──────────────────────────── */

int ofx_gfx_ray_cylinder(const double* r, int64_t r_n, const double* base, int64_t base_n, const double* axis, int64_t axis_n,
                         double radius, double height, double* t, int64_t t_n)
{
    if (!r || !base || !axis || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_cylinder(ray_in(r), v3(base), v3(axis), radius, height, &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_cone(const double* r, int64_t r_n, const double* apex, int64_t apex_n, const double* axis, int64_t axis_n,
                     double radius, double height, double* t, int64_t t_n)
{
    if (!r || !apex || !axis || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_cone(ray_in(r), v3(apex), v3(axis), radius, height, &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_cylinder_normal(const double* base, int64_t base_n, const double* axis, int64_t axis_n, const double* p, int64_t p_n, double* out, int64_t out_n)
{
    if (!base || !axis || !p || !out) return -1;
    v3_out(gm_cylinder_normal(v3(base), v3(axis), v3(p)), out);
    return 0;
}

int ofx_gfx_cone_normal(const double* apex, int64_t apex_n, const double* axis, int64_t axis_n, double k, const double* p, int64_t p_n, double* out, int64_t out_n)
{
    if (!apex || !axis || !p || !out) return -1;
    v3_out(gm_cone_normal(v3(apex), v3(axis), k, v3(p)), out);
    return 0;
}

int ofx_gfx_aabb_transform(const double* a, int64_t a_n, const double* m, int64_t m_n, double* out, int64_t out_n)
{
    if (!a || !m || !out) return -1;
    box_out(gm_aabb_transform(box_in(a), mat4_in(m)), out);
    return 0;
}

/* ── costruttori e camera orbit/translate/ndc (gfx3d.inc) ────────────────── */

int ofx_gfx_aabb_make(const double* mn, int64_t mn_n, const double* mx, int64_t mx_n, double* out, int64_t out_n)
{
    if (!mn || !mx || !out) return -1;
    box_out(gm_aabb_make(v3(mn), v3(mx)), out);
    return 0;
}

int ofx_gfx_ray_at(const double* r, int64_t r_n, double t, double* out, int64_t out_n)
{
    if (!r || !out) return -1;
    v3_out(gm_ray_at(ray_in(r), t), out);
    return 0;
}

int ofx_gfx_camera_orbit(const double* eye, int64_t eye_n, const double* target, int64_t target_n, double yaw, double pitch,
                         double radius, double* out, int64_t out_n)
{
    if (!eye || !target || !out) return -1;
    gm_camera c = gm_camera_make(v3(eye), v3(target), (gm_vec3){0, 1, 0});
    gm_camera_orbit(&c, v3(target), yaw, pitch, radius);
    v3_out(c.position, out);
    return 0;
}

int ofx_gfx_camera_translate(const double* eye, int64_t eye_n, const double* target, int64_t target_n, const double* delta, int64_t delta_n, double* out, int64_t out_n)
{
    if (!eye || !target || !delta || !out) return -1;
    gm_camera c = gm_camera_make(v3(eye), v3(target), (gm_vec3){0, 1, 0});
    gm_camera_translate(&c, v3(delta));
    v3_out(c.position, out);
    return 0;
}

int ofx_gfx_camera_ndc_ray(const double* pos, int64_t pos_n, const double* target, int64_t target_n, const double* up, int64_t up_n,
                           double fov, double aspect, double ndcx, double ndcy, double* out, int64_t out_n)
{
    if (!pos || !target || !up || !out) return -1;
    gm_camera c = gm_camera_perspective(v3(pos), v3(target), v3(up), fov, aspect, 0.1, 100.0);
    v3_out(gm_camera_ndc_ray_dir(&c, ndcx, ndcy), out);
    return 0;
}
