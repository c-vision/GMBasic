/*
 * geo_ffi.c — shim ABI per il binding BASIC del namespace GEO.
 *
 * La struct C `geo_pt { double x, y; }` ha lo stesso layout del TYPE BASIC
 * `GeoPoint { X AS DOUBLE : Y AS DOUBLE }`: un array di GeoPoint viaggia come
 * array di geo_pt contiguo (copy-in/copy-out dell'FFI), niente piu' coppie
 * xs()/ys(). Le funzioni che restituiscono un punto lo fanno per VALORE
 * (struct a 2 double); il bounding box torna come `GeoBox` (4 double).
 *
 * Solo l'indice a griglia ha uno stato (tabella fissa, uso sequenziale).
 */
#include "gmbgeo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

/* GeoBox: struct a 4 double (layout del TYPE BASIC omonimo). */
typedef struct { double minx, miny, maxx, maxy; } geo_box4;

/* ── misure ──────────────────────────────────────────────────────────────── */
double ofx_geo_poly_area(const geo_pt *p, i64 n)
{
    return (p && n >= 3) ? geo_poly_area(p, (int)n) : 0.0;
}

double ofx_geo_perimeter(const geo_pt *p, i64 n, i64 closed)
{
    return (p && n >= 1) ? geo_perimeter(p, (int)n, (int)closed) : 0.0;
}

/* Punto per valore: il chiamante lo riceve come GeoPoint. */
geo_pt ofx_geo_centroid(const geo_pt *p, i64 n)
{
    geo_pt r;
    r.x = 0.0;
    r.y = 0.0;
    return (p && n >= 1) ? geo_centroid(p, (int)n) : r;
}

double ofx_geo_dist(geo_pt a, geo_pt b)
{
    return geo_dist(&a, &b);
}

/* ── predicati e distanze ────────────────────────────────────────────────── */
i64 ofx_geo_point_in_poly(geo_pt q, const geo_pt *p, i64 n)
{
    return (p && n >= 3) ? (i64)geo_point_in_poly(&q, p, (int)n) : 0;
}

i64 ofx_geo_on_segment(geo_pt q, geo_pt a, geo_pt b)
{
    return (i64)geo_on_segment(&q, &a, &b);
}

double ofx_geo_dist_point_seg(geo_pt q, geo_pt a, geo_pt b)
{
    return geo_dist_point_seg(&q, &a, &b);
}

double ofx_geo_dist_seg_seg(geo_pt a, geo_pt b, geo_pt c, geo_pt d)
{
    return geo_dist_seg_seg(&a, &b, &c, &d);
}

/* ── intersezione fra segmenti ───────────────────────────────────────────── */
i64 ofx_geo_seg_intersect(geo_pt a, geo_pt b, geo_pt c, geo_pt d)
{
    return (i64)geo_seg_intersect(&a, &b, &c, &d);
}

/* Punto di intersezione: out(0) del tipo GeoPoint (array di 1 elemento,
 * l'FFI fa il copy-out in place). Ritorna 1 se il punto esiste. */
i64 ofx_geo_seg_intersection(geo_pt a, geo_pt b, geo_pt c, geo_pt d, geo_pt *out)
{
    geo_pt r;
    int ok;
    if (!out) return -1;
    ok = geo_seg_intersection(&a, &b, &c, &d, &r);
    if (ok) { out[0] = r; return 1; }
    return 0;
}

/* ── forma ───────────────────────────────────────────────────────────────── */
i64 ofx_geo_convex_hull(const geo_pt *p, i64 n, geo_pt *out, i64 cap)
{
    int r;
    if (!p || !out || n < 1 || cap < 3) return -1;
    r = geo_convex_hull(p, (int)n, out, (int)cap);
    return (i64)r;
}

geo_box4 ofx_geo_bbox(const geo_pt *p, i64 n)
{
    geo_box4 b;
    geo_bbox t;
    b.minx = b.miny = b.maxx = b.maxy = 0.0;
    if (p && n >= 1 && geo_bbox_of(p, (int)n, &t) == 0) {
        b.minx = t.min.x; b.miny = t.min.y;
        b.maxx = t.max.x; b.maxy = t.max.y;
    }
    return b;
}

i64 ofx_geo_simplify(const geo_pt *p, i64 n, double tol, geo_pt *out, i64 cap)
{
    if (!p || !out || n < 1 || cap < 1) return -1;
    return (i64)geo_simplify(p, (int)n, tol, out, (int)cap);
}

i64 ofx_geo_translate(const geo_pt *p, i64 n, double dx, double dy, geo_pt *out)
{
    if (!p || !out || n < 1) return -1;
    return (i64)geo_translate(p, (int)n, dx, dy, out);
}

i64 ofx_geo_scale(const geo_pt *p, i64 n, double sx, double sy, geo_pt *out)
{
    if (!p || !out || n < 1) return -1;
    return (i64)geo_scale(p, (int)n, sx, sy, out);
}

i64 ofx_geo_rotate(const geo_pt *p, i64 n, double deg, geo_pt *out)
{
    if (!p || !out || n < 1) return -1;
    return (i64)geo_rotate(p, (int)n, deg, out);
}

/* Sutherland-Hodgman: soggetto ∩ clip convesso; ritorna il n. di vertici. */
i64 ofx_geo_clip(const geo_pt *subj, i64 ns, const geo_pt *clip, i64 nc,
                 geo_pt *out, i64 cap)
{
    if (!subj || !clip || !out || ns < 1 || nc < 3 || cap < 1) return -1;
    return (i64)geo_clip(subj, (int)ns, clip, (int)nc, out, (int)cap);
}

/* ── proiezioni e geodesia ───────────────────────────────────────────────── */
geo_pt ofx_geo_project(double lon, double lat)
{
    geo_pt r;
    geo_project(lon, lat, &r.x, &r.y);
    return r;
}

geo_pt ofx_geo_unproject(double x, double y)
{
    geo_pt r;
    geo_unproject(x, y, &r.x, &r.y);
    return r;
}

double ofx_geo_haversine(double lon1, double lat1, double lon2, double lat2)
{
    return geo_haversine(lon1, lat1, lon2, lat2);
}

/* ── overlay booleano (Greiner-Hormann) ──────────────────────────────────── */
i64 ofx_geo_overlay(const geo_pt *a, i64 na, const geo_pt *b, i64 nb,
                    geo_pt *out, i64 cap, i64 op)
{
    if (!a || !b || !out || na < 3 || nb < 3 || cap < 1) return -1;
    return (i64)geo_overlay(a, (int)na, b, (int)nb, (int)op, out, (int)cap);
}

/* ── spatial join: offset (na+1 / nb+1) su array di GeoPoint ─────────────── */
i64 ofx_geo_spatial_join(const geo_pt *a, const i64 *aoff, i64 na,
                         const geo_pt *b, const i64 *boff, i64 nb,
                         i64 *pairs, i64 cap)
{
    geo_pt **A = NULL, **B = NULL;
    int *nA = NULL, *nB = NULL, *pr = NULL;
    int i, r;
    if (!a || !aoff || !b || !boff || !pairs || na < 1 || nb < 1 || cap < 1) return -1;
    A = (geo_pt **)calloc((size_t)na, sizeof(geo_pt *));
    B = (geo_pt **)calloc((size_t)nb, sizeof(geo_pt *));
    nA = (int *)calloc((size_t)na, sizeof(int));
    nB = (int *)calloc((size_t)nb, sizeof(int));
    pr = (int *)calloc((size_t)cap * 2, sizeof(int));
    if (!A || !B || !nA || !nB || !pr) { free(A); free(B); free(nA); free(nB); free(pr); return -1; }
    for (i = 0; i < (int)na; i++) {
        int lo = (int)aoff[i], hi = (int)aoff[i + 1];
        nA[i] = hi - lo;
        A[i] = (geo_pt *)(a + lo);
    }
    for (i = 0; i < (int)nb; i++) {
        int lo = (int)boff[i], hi = (int)boff[i + 1];
        nB[i] = hi - lo;
        B[i] = (geo_pt *)(b + lo);
    }
    r = geo_spatial_join((const geo_pt *const *)A, nA, (int)na,
                         (const geo_pt *const *)B, nB, (int)nb, pr, (int)cap);
    if (r > 0) { int k; for (k = 0; k < 2 * r; k++) pairs[k] = (i64)pr[k]; }
    free(A); free(B); free(nA); free(nB); free(pr);
    return (i64)r;
}

/* ── I/O GeoJSON / WKT ───────────────────────────────────────────────────── */
i64 ofx_geo_geojson_parse(const char *json, geo_pt *out, i64 cap, i64 *info)
{
    geo_pt *p = NULL;
    int n = 0, closed = 0, r;
    if (!json || !out || !info || cap < 1) return -1;
    r = geo_geojson_parse(json, &p, &n, &closed);
    if (r < 0 || !p) { free(p); return -1; }
    if (n > (int)cap) n = (int)cap;
    memcpy(out, p, (size_t)n * sizeof(geo_pt));
    info[0] = (i64)closed;
    free(p);
    return (i64)n;
}

i64 ofx_geo_wkt_parse(const char *wkt, geo_pt *out, i64 cap, i64 *info)
{
    geo_pt *p = NULL;
    int n = 0, closed = 0, r;
    if (!wkt || !out || !info || cap < 1) return -1;
    r = geo_wkt_parse(wkt, &p, &n, &closed);
    if (r < 0 || !p) { free(p); return -1; }
    if (n > (int)cap) n = (int)cap;
    memcpy(out, p, (size_t)n * sizeof(geo_pt));
    info[0] = (i64)closed;
    free(p);
    return (i64)n;
}

geo_pt ofx_geo_wkt_point(const char *wkt)
{
    geo_pt r;
    r.x = 0.0;
    r.y = 0.0;
    if (wkt) geo_wkt_point(wkt, &r);
    return r;
}

/* Serializzazione: buffer statico riusato (il runtime copia subito la stringa). */
static char *g_geo_buf = NULL;
static size_t g_geo_cap = 0;

static const char *geo_serialize(const geo_pt *p, i64 n, i64 closed, int is_json)
{
    size_t need;
    int r;
    if (!p || n < 1) return NULL;
    need = (size_t)n * 64 + 128;
    if (need > g_geo_cap) {
        char *nb = (char *)realloc(g_geo_buf, need);
        if (!nb) return NULL;
        g_geo_buf = nb;
        g_geo_cap = need;
    }
    if (is_json) {
        r = geo_geojson_write(p, (int)n, (int)closed, g_geo_buf, (int)g_geo_cap);
    } else {
        int i, off = 0;
        off += snprintf(g_geo_buf + off, g_geo_cap - (size_t)off,
                        closed ? "POLYGON ((" : "LINESTRING (");
        for (i = 0; i < (int)n && (size_t)off < g_geo_cap; i++) {
            off += snprintf(g_geo_buf + off, g_geo_cap - (size_t)off, "%s%g %g",
                            i ? ", " : "", p[i].x, p[i].y);
            if ((size_t)off >= g_geo_cap) break;
        }
        if ((size_t)off < g_geo_cap)
            off += snprintf(g_geo_buf + off, g_geo_cap - (size_t)off, closed ? "))" : ")");
        r = off;
    }
    return r > 0 ? g_geo_buf : NULL;
}

const char *ofx_geo_geojson_write(const geo_pt *p, i64 n, i64 closed)
{
    return geo_serialize(p, n, closed, 1);
}

const char *ofx_geo_wkt_write(const geo_pt *p, i64 n, i64 closed)
{
    return geo_serialize(p, n, closed, 0);
}

/* ── indice spaziale a griglia — handle 7301+ ────────────────────────────── */
#define NN_GEO_GRID_MAX 16
#define NN_GEO_GRID_BASE 7300

typedef struct { int used; geo_grid *g; } geo_grid_e;
static geo_grid_e g_geo_grid[NN_GEO_GRID_MAX];

static geo_grid_e *grid_get(i64 h)
{
    if (h <= NN_GEO_GRID_BASE || h > NN_GEO_GRID_BASE + NN_GEO_GRID_MAX) return NULL;
    if (!g_geo_grid[h - NN_GEO_GRID_BASE - 1].used) return NULL;
    return &g_geo_grid[h - NN_GEO_GRID_BASE - 1];
}

i64 ofx_geo_grid_new(const geo_pt *p, i64 n, double cell)
{
    int i;
    if (!p || n < 1) return -1;
    for (i = 0; i < NN_GEO_GRID_MAX; i++) {
        geo_grid *g;
        if (g_geo_grid[i].used) continue;
        g = geo_grid_new(p, (int)n, cell);
        if (!g) continue;
        g_geo_grid[i].g = g;
        g_geo_grid[i].used = 1;
        return (i64)(NN_GEO_GRID_BASE + i + 1);
    }
    return -1;
}

i64 ofx_geo_grid_query(i64 h, geo_box4 box, i64 *idx, i64 cap)
{
    geo_grid_e *e = grid_get(h);
    geo_bbox b;
    int *tmp;
    int i, r;
    if (!e || !idx || cap < 1) return -1;
    b.min.x = box.minx; b.min.y = box.miny;
    b.max.x = box.maxx; b.max.y = box.maxy;
    tmp = (int *)calloc((size_t)cap, sizeof(int));
    if (!tmp) return -1;
    r = geo_grid_query_bbox(e->g, b, tmp, (int)cap);
    if (r > 0) for (i = 0; i < r; i++) idx[i] = (i64)tmp[i];
    free(tmp);
    return (i64)r;
}

i64 ofx_geo_grid_free(i64 h)
{
    geo_grid_e *e = grid_get(h);
    if (!e) return -1;
    geo_grid_free(e->g);
    e->g = NULL;
    e->used = 0;
    return 0;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
geo_box4 ofx_geo_bbox__ffi(const geo_pt *p, int64_t p_ext, i64 n) { return ofx_geo_bbox(p, n); }
geo_pt ofx_geo_centroid__ffi(const geo_pt *p, int64_t p_ext, i64 n) { return ofx_geo_centroid(p, n); }
i64 ofx_geo_clip__ffi(const geo_pt *subj, int64_t subj_ext, i64 ns, const geo_pt *clip, int64_t clip_ext, i64 nc, geo_pt *out, int64_t out_ext, i64 cap) { return ofx_geo_clip(subj, ns, clip, nc, out, cap); }
i64 ofx_geo_convex_hull__ffi(const geo_pt *p, int64_t p_ext, i64 n, geo_pt *out, int64_t out_ext, i64 cap) { return ofx_geo_convex_hull(p, n, out, cap); }
i64 ofx_geo_geojson_parse__ffi(const char *json, geo_pt *out, int64_t out_ext, i64 cap, i64 *info, int64_t info_ext) { return ofx_geo_geojson_parse(json, out, cap, info); }
const char * ofx_geo_geojson_write__ffi(const geo_pt *p, int64_t p_ext, i64 n, i64 closed) { return ofx_geo_geojson_write(p, n, closed); }
i64 ofx_geo_grid_new__ffi(const geo_pt *p, int64_t p_ext, i64 n, double cell) { return ofx_geo_grid_new(p, n, cell); }
i64 ofx_geo_grid_query__ffi(i64 h, geo_box4 box, i64 *idx, int64_t idx_ext, i64 cap) { return ofx_geo_grid_query(h, box, idx, cap); }
i64 ofx_geo_overlay__ffi(const geo_pt *a, int64_t a_ext, i64 na, const geo_pt *b, int64_t b_ext, i64 nb, geo_pt *out, int64_t out_ext, i64 cap, i64 op) { return ofx_geo_overlay(a, na, b, nb, out, cap, op); }
double ofx_geo_perimeter__ffi(const geo_pt *p, int64_t p_ext, i64 n, i64 closed) { return ofx_geo_perimeter(p, n, closed); }
i64 ofx_geo_point_in_poly__ffi(geo_pt q, const geo_pt *p, int64_t p_ext, i64 n) { return ofx_geo_point_in_poly(q, p, n); }
double ofx_geo_poly_area__ffi(const geo_pt *p, int64_t p_ext, i64 n) { return ofx_geo_poly_area(p, n); }
i64 ofx_geo_rotate__ffi(const geo_pt *p, int64_t p_ext, i64 n, double deg, geo_pt *out, int64_t out_ext) { return ofx_geo_rotate(p, n, deg, out); }
i64 ofx_geo_scale__ffi(const geo_pt *p, int64_t p_ext, i64 n, double sx, double sy, geo_pt *out, int64_t out_ext) { return ofx_geo_scale(p, n, sx, sy, out); }
i64 ofx_geo_seg_intersection__ffi(geo_pt a, geo_pt b, geo_pt c, geo_pt d, geo_pt *out, int64_t out_ext) { return ofx_geo_seg_intersection(a, b, c, d, out); }
i64 ofx_geo_simplify__ffi(const geo_pt *p, int64_t p_ext, i64 n, double tol, geo_pt *out, int64_t out_ext, i64 cap) { return ofx_geo_simplify(p, n, tol, out, cap); }
i64 ofx_geo_spatial_join__ffi(const geo_pt *a, int64_t a_ext, const i64 *aoff, int64_t aoff_ext, i64 na, const geo_pt *b, int64_t b_ext, const i64 *boff, int64_t boff_ext, i64 nb, i64 *pairs, int64_t pairs_ext, i64 cap) { return ofx_geo_spatial_join(a, aoff, na, b, boff, nb, pairs, cap); }
i64 ofx_geo_translate__ffi(const geo_pt *p, int64_t p_ext, i64 n, double dx, double dy, geo_pt *out, int64_t out_ext) { return ofx_geo_translate(p, n, dx, dy, out); }
i64 ofx_geo_wkt_parse__ffi(const char *wkt, geo_pt *out, int64_t out_ext, i64 cap, i64 *info, int64_t info_ext) { return ofx_geo_wkt_parse(wkt, out, cap, info); }
const char * ofx_geo_wkt_write__ffi(const geo_pt *p, int64_t p_ext, i64 n, i64 closed) { return ofx_geo_wkt_write(p, n, closed); }
