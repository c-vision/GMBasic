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

int ofx_gfx_perspective(double fovy, double aspect, double zn, double zf, double* out)
{ if (!out) return -1; mat4_out(gm_mat4_perspective(fovy, aspect, zn, zf), out); return 0; }

int ofx_gfx_ortho(double l, double r, double b, double t, double zn, double zf, double* out)
{ if (!out) return -1; mat4_out(gm_mat4_ortho(l, r, b, t, zn, zf), out); return 0; }

int ofx_gfx_lookat(const double* eye, const double* center, const double* up, double* out)
{ if (!eye || !center || !up || !out) return -1; mat4_out(gm_mat4_look_at(v3(eye), v3(center), v3(up)), out); return 0; }

int ofx_gfx_aabb_from_points(const double* pts, int nvalues, double* out)
{
    if (!pts || !out || nvalues < 3) return -1;
    int n = nvalues / 3;
    gm_vec3* v = (gm_vec3*)malloc(sizeof(gm_vec3) * (size_t)n);
    if (!v) return -1;
    for (int i = 0; i < n; i++) v[i] = v3(pts + 3 * i);
    gm_aabb b = gm_aabb_from_points(v, n);
    free(v);
    box_out(b, out);
    return 0;
}

int ofx_gfx_aabb_merge(const double* a, const double* b, double* out)
{ if (!a || !b || !out) return -1; box_out(gm_aabb_merge(box_in(a), box_in(b)), out); return 0; }

int ofx_gfx_aabb_center(const double* a, double* out)
{ if (!a || !out) return -1; v3_out(gm_aabb_center(box_in(a)), out); return 0; }

int ofx_gfx_aabb_extents(const double* a, double* out)
{ if (!a || !out) return -1; v3_out(gm_aabb_extents(box_in(a)), out); return 0; }

int ofx_gfx_aabb_test_point(const double* a, const double* p)
{ if (!a || !p) return 0; return gm_aabb_test_point(box_in(a), v3(p)); }

int ofx_gfx_aabb_intersect(const double* a, const double* b)
{ if (!a || !b) return 0; return gm_aabb_intersect(box_in(a), box_in(b)); }

int ofx_gfx_frustum_from(const double* m, double* out)
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

int ofx_gfx_frustum_test_point(const double* planes, const double* p)
{ if (!planes || !p) return 0; return gm_frustum_test_point(frustum_in(planes), v3(p)); }

int ofx_gfx_frustum_test_aabb(const double* planes, const double* box)
{ if (!planes || !box) return 0; return gm_frustum_intersect_aabb(frustum_in(planes), box_in(box)); }

int ofx_gfx_decompose_srt(const double* m, double* scale, double* rot, double* pos)
{
    if (!m || !scale || !rot || !pos) return -1;
    gm_vec3 s, t; gm_quat q;
    int rc = gm_mat4_decompose_srt(mat4_in(m), &s, &q, &t);
    v3_out(s, scale); v3_out(t, pos);
    rot[0] = q.x; rot[1] = q.y; rot[2] = q.z; rot[3] = q.w;
    return rc;
}

int ofx_gfx_camera_view(const double* pos, const double* target, const double* up, double* out)
{
    if (!pos || !target || !up || !out) return -1;
    gm_camera c = gm_camera_make(v3(pos), v3(target), v3(up));
    mat4_out(gm_camera_view(&c), out);
    return 0;
}

int ofx_gfx_camera_projection(double fov, double aspect, double zn, double zf, double* out)
{
    if (!out) return -1;
    gm_camera c = gm_camera_perspective((gm_vec3){0, 0, 0}, (gm_vec3){0, 0, -1}, (gm_vec3){0, 1, 0}, fov, aspect, zn, zf);
    mat4_out(gm_camera_projection(&c), out);
    return 0;
}

int ofx_gfx_camera_view_projection(const double* pos, const double* target, const double* up,
                                   double fov, double aspect, double zn, double zf, double* out)
{
    if (!pos || !target || !up || !out) return -1;
    gm_camera c = gm_camera_perspective(v3(pos), v3(target), v3(up), fov, aspect, zn, zf);
    mat4_out(gm_camera_view_projection(&c), out);
    return 0;
}

/* ── raggi e intersezioni ───────────────────────────────────────────────── */

int ofx_gfx_ray_sphere(const double* r, const double* sphere, double* t)
{
    if (!r || !sphere || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_sphere(ray_in(r), v3(sphere), sphere[3], &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_plane(const double* r, const double* plane, double* t)
{
    if (!r || !plane || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_plane(ray_in(r), v3(plane), v3(plane + 3), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_triangle(const double* r, const double* tri, double* tuv)
{
    if (!r || !tri || !tuv) return -1;
    gm_real t = 0, u = 0, v = 0;
    int hit = gm_ray_triangle(ray_in(r), v3(tri), v3(tri + 3), v3(tri + 6), &t, &u, &v);
    tuv[0] = t; tuv[1] = u; tuv[2] = v;
    return hit;
}

int ofx_gfx_ray_aabb(const double* r, const double* box, double* t)
{
    if (!r || !box || !t) return -1;
    gm_real tmin = 0, tmax = 0;
    int hit = gm_ray_aabb(ray_in(r), v3(box), v3(box + 3), &tmin, &tmax);
    t[0] = tmin; t[1] = tmax;
    return hit;
}

int ofx_gfx_ray_box(const double* r, const double* box, double* t)
{
    if (!r || !box || !t) return -1;
    gm_real tt = 0;
    int hit = gm_ray_box(ray_in(r), v3(box), v3(box + 3), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_ray_reflect(const double* v, const double* n, double* out)
{ if (!v || !n || !out) return -1; v3_out(gm_ray_reflect(v3(v), v3(n)), out); return 0; }

int ofx_gfx_ray_refract(const double* v, const double* n, double eta, double* out)
{
    if (!v || !n || !out) return -1;
    gm_vec3 o;
    int rc = gm_ray_refract(v3(v), v3(n), eta, &o);
    v3_out(o, out);
    return rc;
}

/* params = [light_rgb(3), obj_rgb(3), ka, kd, ks, shininess] (10 valori): un solo array
 * accessorio evita le chiamate FFI con troppi array (limite pratico dell'emitter). */
int ofx_gfx_shade_phong(const double* p, const double* n, const double* view, const double* light,
                        const double* params, double* out)
{
    if (!p || !n || !view || !light || !params || !out) return -1;
    v3_out(gm_shade_phong(v3(p), v3(n), v3(view), v3(light), v3(params), v3(params + 3),
                          params[6], params[7], params[8], params[9]), out);
    return 0;
}

int ofx_gfx_sphere_sphere(const double* c1, double r1, const double* c2, double r2)
{ if (!c1 || !c2) return 0; return gm_intersect_sphere_sphere(v3(c1), r1, v3(c2), r2); }

int ofx_gfx_sphere_box(const double* c, double r, const double* box)
{ if (!c || !box) return 0; return gm_intersect_sphere_box(v3(c), r, v3(box), v3(box + 3)); }

int ofx_gfx_sphere_plane(const double* c, double r, const double* plane, double* dist)
{
    if (!c || !plane || !dist) return -1;
    gm_real d = 0;
    int hit = gm_intersect_sphere_plane(v3(c), r, v3(plane), v3(plane + 3), &d);
    dist[0] = d;
    return hit;
}

int ofx_gfx_segment_plane(const double* a, const double* b, const double* plane, double* t)
{
    if (!a || !b || !plane || !t) return -1;
    gm_real tt = 0;
    int hit = gm_intersect_segment_plane(v3(a), v3(b), v3(plane), v3(plane + 3), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_segment_sphere(const double* a, const double* b, const double* c, double r, double* t)
{
    if (!a || !b || !c || !t) return -1;
    gm_real tt = 0;
    int hit = gm_intersect_segment_sphere(v3(a), v3(b), v3(c), r, &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_segment_triangle(const double* a, const double* b, const double* tri, double* t)
{
    if (!a || !b || !tri || !t) return -1;
    gm_real tt = 0;
    int hit = gm_intersect_segment_triangle(v3(a), v3(b), v3(tri), v3(tri + 3), v3(tri + 6), &tt);
    t[0] = tt;
    return hit;
}

int ofx_gfx_triangle_triangle(const double* t1, const double* t2)
{
    if (!t1 || !t2) return 0;
    gm_vec3 a[3] = { v3(t1), v3(t1 + 3), v3(t1 + 6) };
    gm_vec3 b[3] = { v3(t2), v3(t2 + 3), v3(t2 + 6) };
    return gm_intersect_triangle_triangle(a, b);
}

int ofx_gfx_volume_render(int n, const double* sigma, const double* rgb, const double* dt,
                          double* out_rgb, double* out_acc)
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

int ofx_gfx_mesh_pos(int h, double* out)
{
    gm_mesh* m = mesh_get(h);
    if (!m || !out) return -1;
    for (int i = 0; i < m->nv; i++) { out[3 * i] = m->pos[i].x; out[3 * i + 1] = m->pos[i].y; out[3 * i + 2] = m->pos[i].z; }
    return 0;
}

int ofx_gfx_mesh_tri(int h, double* out)
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

int ofx_gfx_raster_get(int h, int x, int y, double* out)
{
    gm_raster* r = raster_get(h);
    if (!r || !out || x < 0 || y < 0 || x >= r->w || y >= r->h) return -1;
    gm_real rgb[3];
    gm_raster_get(r, x, y, rgb);
    out[0] = rgb[0]; out[1] = rgb[1]; out[2] = rgb[2];
    return 0;
}

int ofx_gfx_raster_tri(int h, const double* p, const double* c)
{
    gm_raster* r = raster_get(h);
    if (!r || !p || !c) return -1;
    gm_raster_triangle(r, p, p + 3, p + 6, c, c + 3, c + 6);
    return 0;
}

int ofx_gfx_raster_clip_near(const double* tri, double znear, double* out)
{
    if (!tri || !out) return -1;
    return gm_raster_clip_near(tri, znear, out);
}
