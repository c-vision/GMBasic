/*
 * geo_ffi.c — shim ABI piatto per il binding BASIC del namespace GEO.
 *
 * Le struct C `geo_pt { double x, y; }` non attraversano l'FFI: ogni polilinea o
 * poligono viaggia come DUE array paralleli xs()/ys() con il numero di vertici n.
 * `geo_bbox` idem: out(0..3) = minx, miny, maxx, maxy. Gli handle (griglia spaziale)
 * stanno in una tabella fissa (uso sequenziale, non thread-safe).
 */
#include "gmbgeo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;

/* ── conversione array paralleli <-> geo_pt[] ─────────────────────────────── */
static geo_pt *geo_in(const double *xs, const double *ys, int n)
{
    geo_pt *p;
    int i;
    if (!xs || !ys || n < 1) return NULL;
    p = (geo_pt *)malloc((size_t)n * sizeof(geo_pt));
    if (!p) return NULL;
    for (i = 0; i < n; i++) { p[i].x = xs[i]; p[i].y = ys[i]; }
    return p;
}

static void geo_out(const geo_pt *p, int n, double *xs, double *ys)
{
    int i;
    if (!p) return;
    for (i = 0; i < n; i++) { if (xs) xs[i] = p[i].x; if (ys) ys[i] = p[i].y; }
}

/* ── misure ──────────────────────────────────────────────────────────────── */
double ofx_geo_poly_area(const double *xs, const double *ys, i64 n)
{
    geo_pt *p;
    double r;
    if (n < 3) return 0.0;
    p = geo_in(xs, ys, (int)n);
    if (!p) return 0.0;
    r = geo_poly_area(p, (int)n);
    free(p);
    return r;
}

double ofx_geo_perimeter(const double *xs, const double *ys, i64 n, i64 closed)
{
    geo_pt *p;
    double r;
    if (n < 1) return 0.0;
    p = geo_in(xs, ys, (int)n);
    if (!p) return 0.0;
    r = geo_perimeter(p, (int)n, (int)closed);
    free(p);
    return r;
}

/* out(0) = x, out(1) = y. */
i64 ofx_geo_centroid(const double *xs, const double *ys, i64 n, double *out)
{
    geo_pt *p;
    geo_pt c;
    if (!out || n < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    if (!p) return -1;
    c = geo_centroid(p, (int)n);
    out[0] = c.x;
    out[1] = c.y;
    free(p);
    return 0;
}

/* Distanza euclidea scalare: le distanze BATCH restano SPATIAL.PDIST/CDIST. */
double ofx_geo_dist(double ax, double ay, double bx, double by)
{
    geo_pt a, b;
    a.x = ax; a.y = ay; b.x = bx; b.y = by;
    return geo_dist(&a, &b);
}

/* ── predicati / distanze ────────────────────────────────────────────────── */
i64 ofx_geo_point_in_poly(double px, double py, const double *xs, const double *ys, i64 n)
{
    geo_pt p, *poly;
    int r;
    if (n < 3) return 0;
    poly = geo_in(xs, ys, (int)n);
    if (!poly) return 0;
    p.x = px; p.y = py;
    r = geo_point_in_poly(&p, poly, (int)n);
    free(poly);
    return (i64)r;
}

i64 ofx_geo_on_segment(double px, double py, double ax, double ay, double bx, double by)
{
    geo_pt p, a, b;
    p.x = px; p.y = py; a.x = ax; a.y = ay; b.x = bx; b.y = by;
    return (i64)geo_on_segment(&p, &a, &b);
}

double ofx_geo_dist_point_seg(double px, double py, double ax, double ay, double bx, double by)
{
    geo_pt p, a, b;
    p.x = px; p.y = py; a.x = ax; a.y = ay; b.x = bx; b.y = by;
    return geo_dist_point_seg(&p, &a, &b);
}

double ofx_geo_dist_seg_seg(double ax, double ay, double bx, double by,
                            double cx, double cy, double dx, double dy)
{
    geo_pt a, b, c, d;
    a.x = ax; a.y = ay; b.x = bx; b.y = by;
    c.x = cx; c.y = cy; d.x = dx; d.y = dy;
    return geo_dist_seg_seg(&a, &b, &c, &d);
}

/* ── intersezione segmenti ───────────────────────────────────────────────── */
i64 ofx_geo_seg_intersect(double ax, double ay, double bx, double by,
                          double cx, double cy, double dx, double dy)
{
    geo_pt a, b, c, d;
    a.x = ax; a.y = ay; b.x = bx; b.y = by;
    c.x = cx; c.y = cy; d.x = dx; d.y = dy;
    return (i64)geo_seg_intersect(&a, &b, &c, &d);
}

/* out(0..1) = punto di intersezione; ritorna 1 se esiste. */
i64 ofx_geo_seg_intersection(double *out, double ax, double ay, double bx, double by,
                             double cx, double cy, double dx, double dy)
{
    geo_pt a, b, c, d, o;
    int r;
    if (!out) return -1;
    a.x = ax; a.y = ay; b.x = bx; b.y = by;
    c.x = cx; c.y = cy; d.x = dx; d.y = dy;
    r = geo_seg_intersection(&a, &b, &c, &d, &o);
    if (r) { out[0] = o.x; out[1] = o.y; }
    return (i64)r;
}

/* ── forma ───────────────────────────────────────────────────────────────── */
/* Bounding box: out(0)=minx out(1)=miny out(2)=maxx out(3)=maxy. */
i64 ofx_geo_bbox(const double *xs, const double *ys, i64 n, double *out)
{
    geo_pt *p;
    geo_bbox b;
    int r;
    if (!out || n < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    if (!p) return -1;
    r = geo_bbox_of(p, (int)n, &b);
    if (r == 0) { out[0] = b.min.x; out[1] = b.min.y; out[2] = b.max.x; out[3] = b.max.y; }
    free(p);
    return (i64)r;
}

/* Douglas-Peucker: ritorna il numero di vertici semplificati (0/-1 errore). */
i64 ofx_geo_simplify(const double *xs, const double *ys, i64 n, double tol,
                     double *oxs, double *oys, i64 cap)
{
    geo_pt *p, *o;
    int r;
    if (n < 1 || cap < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    o = (geo_pt *)calloc((size_t)cap, sizeof(geo_pt));
    if (!p || !o) { free(p); free(o); return -1; }
    r = geo_simplify(p, (int)n, tol, o, (int)cap);
    if (r >= 0) geo_out(o, r, oxs, oys);
    free(p);
    free(o);
    return (i64)r;
}

i64 ofx_geo_translate(const double *xs, const double *ys, i64 n, double dx, double dy,
                      double *oxs, double *oys)
{
    geo_pt *p, *o;
    int r;
    if (n < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    o = (geo_pt *)calloc((size_t)n, sizeof(geo_pt));
    if (!p || !o) { free(p); free(o); return -1; }
    r = geo_translate(p, (int)n, dx, dy, o);
    if (r >= 0) geo_out(o, (int)n, oxs, oys);
    free(p);
    free(o);
    return (i64)r;
}

i64 ofx_geo_scale(const double *xs, const double *ys, i64 n, double sx, double sy,
                  double *oxs, double *oys)
{
    geo_pt *p, *o;
    int r;
    if (n < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    o = (geo_pt *)calloc((size_t)n, sizeof(geo_pt));
    if (!p || !o) { free(p); free(o); return -1; }
    r = geo_scale(p, (int)n, sx, sy, o);
    if (r >= 0) geo_out(o, (int)n, oxs, oys);
    free(p);
    free(o);
    return (i64)r;
}

i64 ofx_geo_rotate(const double *xs, const double *ys, i64 n, double deg,
                   double *oxs, double *oys)
{
    geo_pt *p, *o;
    int r;
    if (n < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    o = (geo_pt *)calloc((size_t)n, sizeof(geo_pt));
    if (!p || !o) { free(p); free(o); return -1; }
    r = geo_rotate(p, (int)n, deg, o);
    if (r >= 0) geo_out(o, (int)n, oxs, oys);
    free(p);
    free(o);
    return (i64)r;
}

/* Sutherland-Hodgman: soggetto ∩ clip convesso; ritorna il n. di vertici del risultato. */
i64 ofx_geo_clip(const double *sxs, const double *sys, i64 ns,
                 const double *cxs, const double *cys, i64 nc,
                 double *oxs, double *oys, i64 cap)
{
    geo_pt *s, *c, *o;
    int r;
    if (ns < 1 || nc < 3 || cap < 1) return -1;
    s = geo_in(sxs, sys, (int)ns);
    c = geo_in(cxs, cys, (int)nc);
    o = (geo_pt *)calloc((size_t)cap, sizeof(geo_pt));
    if (!s || !c || !o) { free(s); free(c); free(o); return -1; }
    r = geo_clip(s, (int)ns, c, (int)nc, o, (int)cap);
    if (r > 0) geo_out(o, r, oxs, oys);
    free(s);
    free(c);
    free(o);
    return (i64)r;
}

/* ── proiezioni e geodesia ───────────────────────────────────────────────── */
/* out(0)=x, out(1)=y (Web Mercator, metri). */
i64 ofx_geo_project(double lon, double lat, double *out)
{
    if (!out) return -1;
    geo_project(lon, lat, &out[0], &out[1]);
    return 0;
}

/* out(0)=lon, out(1)=lat (gradi). */
i64 ofx_geo_unproject(double x, double y, double *out)
{
    if (!out) return -1;
    geo_unproject(x, y, &out[0], &out[1]);
    return 0;
}

double ofx_geo_haversine(double lon1, double lat1, double lon2, double lat2)
{
    return geo_haversine(lon1, lat1, lon2, lat2);
}

/* ── overlay booleano (Greiner-Hormann) ──────────────────────────────────── */
i64 ofx_geo_overlay(const double *axs, const double *ays, i64 na,
                    const double *bxs, const double *bys, i64 nb,
                    double *oxs, double *oys, i64 cap, i64 op)
{
    geo_pt *a, *b, *o;
    int r;
    if (na < 3 || nb < 3 || cap < 1) return -1;
    a = geo_in(axs, ays, (int)na);
    b = geo_in(bxs, bys, (int)nb);
    o = (geo_pt *)calloc((size_t)cap, sizeof(geo_pt));
    if (!a || !b || !o) { free(a); free(b); free(o); return -1; }
    r = geo_overlay(a, (int)na, b, (int)nb, (int)op, o, (int)cap);
    if (r > 0) geo_out(o, r, oxs, oys);
    free(a);
    free(b);
    free(o);
    return (i64)r;
}

/* Coppie (i,j) di poligoni che si intersecano. aoff/boff: offset na+1 (nb+1) nei
 * due array piatti: il poligono k e' [off(k) .. off(k+1)). pairs() riceve 2*count
 * indici. Ritorna il numero di coppie (-1 errore). */
i64 ofx_geo_spatial_join(const double *axs, const double *ays, const i64 *aoff, i64 *pairs,
                         const double *bxs, const double *bys, const i64 *boff,
                         i64 cap, i64 na, i64 nb)
{
    geo_pt **A = NULL, **B = NULL;
    int *nA = NULL, *nB = NULL, *pr = NULL;
    int i, r;
    if (!axs || !ays || !aoff || !bxs || !bys || !boff || !pairs || na < 1 || nb < 1) return -1;
    A = (geo_pt **)calloc((size_t)na, sizeof(geo_pt *));
    B = (geo_pt **)calloc((size_t)nb, sizeof(geo_pt *));
    nA = (int *)calloc((size_t)na, sizeof(int));
    nB = (int *)calloc((size_t)nb, sizeof(int));
    pr = (int *)calloc((size_t)(cap > 0 ? cap : 1) * 2, sizeof(int));
    if (!A || !B || !nA || !nB || !pr) { free(A); free(B); free(nA); free(nB); free(pr); return -1; }
    for (i = 0; i < (int)na; i++) {
        int lo = (int)aoff[i], hi = (int)aoff[i + 1];
        nA[i] = hi - lo;
        A[i] = geo_in(axs + lo, ays + lo, nA[i]);
    }
    for (i = 0; i < (int)nb; i++) {
        int lo = (int)boff[i], hi = (int)boff[i + 1];
        nB[i] = hi - lo;
        B[i] = geo_in(bxs + lo, bys + lo, nB[i]);
    }
    r = geo_spatial_join((const geo_pt *const *)A, nA, (int)na,
                         (const geo_pt *const *)B, nB, (int)nb, pr, (int)cap);
    if (r > 0) {
        int k;
        for (k = 0; k < 2 * r; k++) pairs[k] = (i64)pr[k];
    }
    for (i = 0; i < (int)na; i++) free(A[i]);
    for (i = 0; i < (int)nb; i++) free(B[i]);
    free(A); free(B); free(nA); free(nB); free(pr);
    return (i64)r;
}

/* ── I/O GeoJSON / WKT ───────────────────────────────────────────────────── */
/* Parse: ox/oy ricevono fino a cap vertici, info(0)=closed. Ritorna n (-1 errore). */
i64 ofx_geo_geojson_parse(const char *json, double *oxs, double *oys, i64 cap, i64 *info)
{
    geo_pt *p = NULL;
    int n = 0, closed = 0, r;
    if (!json || !oxs || !oys || !info || cap < 1) return -1;
    r = geo_geojson_parse(json, &p, &n, &closed);
    if (r < 0 || !p) { free(p); return -1; }
    if (n > (int)cap) n = (int)cap;
    geo_out(p, n, oxs, oys);
    info[0] = (i64)closed;
    free(p);
    return (i64)n;
}

i64 ofx_geo_wkt_parse(const char *wkt, double *oxs, double *oys, i64 cap, i64 *info)
{
    geo_pt *p = NULL;
    int n = 0, closed = 0, r;
    if (!wkt || !oxs || !oys || !info || cap < 1) return -1;
    r = geo_wkt_parse(wkt, &p, &n, &closed);
    if (r < 0 || !p) { free(p); return -1; }
    if (n > (int)cap) n = (int)cap;
    geo_out(p, n, oxs, oys);
    info[0] = (i64)closed;
    free(p);
    return (i64)n;
}

i64 ofx_geo_wkt_point(const char *wkt, double *out)
{
    geo_pt p;
    if (!wkt || !out) return -1;
    if (geo_wkt_point(wkt, &p) != 0) return -1;
    out[0] = p.x;
    out[1] = p.y;
    return 0;
}

/* Serializzazione: buffer statico riusato (il runtime copia subito la stringa). */
static char *g_geo_buf = NULL;
static size_t g_geo_cap = 0;

static const char *geo_serialize(const double *xs, const double *ys, i64 n, i64 closed, int is_json)
{
    geo_pt *p;
    size_t need;
    int r;
    if (n < 1) return NULL;
    p = geo_in(xs, ys, (int)n);
    if (!p) return NULL;
    need = (size_t)n * 64 + 128;
    if (need > g_geo_cap) {
        char *nb = (char *)realloc(g_geo_buf, need);
        if (!nb) { free(p); return NULL; }
        g_geo_buf = nb;
        g_geo_cap = need;
    }
    r = is_json ? geo_geojson_write(p, (int)n, (int)closed, g_geo_buf, (int)g_geo_cap)
                : 0;
    if (!is_json) {
        /* WKT: niente writer nella lib (solo parse) -> composizione diretta. */
        int i, off = 0;
        off += snprintf(g_geo_buf + off, g_geo_cap - (size_t)off, closed ? "POLYGON ((" : "LINESTRING (");
        for (i = 0; i < (int)n && (size_t)off < g_geo_cap; i++) {
            off += snprintf(g_geo_buf + off, g_geo_cap - (size_t)off, "%s%g %g",
                            i ? ", " : "", p[i].x, p[i].y);
            if ((size_t)off >= g_geo_cap) break;
        }
        if ((size_t)off < g_geo_cap)
            off += snprintf(g_geo_buf + off, g_geo_cap - (size_t)off, closed ? "))" : ")");
        r = off;
    }
    free(p);
    return r > 0 ? g_geo_buf : NULL;
}

const char *ofx_geo_geojson_write(const double *xs, const double *ys, i64 n, i64 closed)
{
    return geo_serialize(xs, ys, n, closed, 1);
}

const char *ofx_geo_wkt_write(const double *xs, const double *ys, i64 n, i64 closed)
{
    return geo_serialize(xs, ys, n, closed, 0);
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

i64 ofx_geo_grid_new(const double *xs, const double *ys, i64 n, double cell)
{
    int i;
    geo_pt *p;
    if (n < 1) return -1;
    p = geo_in(xs, ys, (int)n);
    if (!p) return -1;
    for (i = 0; i < NN_GEO_GRID_MAX; i++) {
        geo_grid *g;
        if (g_geo_grid[i].used) continue;
        g = geo_grid_new(p, (int)n, cell);
        if (!g) continue;
        g_geo_grid[i].g = g;
        g_geo_grid[i].used = 1;
        free(p);
        return (i64)(NN_GEO_GRID_BASE + i + 1);
    }
    free(p);
    return -1;
}

i64 ofx_geo_grid_query(i64 h, double minx, double miny, double maxx, double maxy,
                       i64 *idx, i64 cap)
{
    geo_grid_e *e = grid_get(h);
    geo_bbox b;
    int *tmp;
    int i, r;
    if (!e || !idx || cap < 1) return -1;
    b.min.x = minx; b.min.y = miny;
    b.max.x = maxx; b.max.y = maxy;
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
