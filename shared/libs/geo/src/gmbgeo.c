/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbgeo.c — geometria piana: misure, distanze, intersezione, forma, trasformazioni, WKT, indice. */
#include "gmbgeo.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ── misure ── */
double geo_dist(const geo_pt* a, const geo_pt* b) {
    if (!a || !b) return 0.0;
    double dx = a->x - b->x, dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}
double geo_poly_area(const geo_pt* pts, int n) {
    if (!pts || n < 3) return 0.0;
    double s = 0.0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; s += pts[i].x * pts[j].y - pts[j].x * pts[i].y; }
    return 0.5 * s;
}
double geo_perimeter(const geo_pt* pts, int n, int closed) {
    if (!pts || n < 2) return 0.0;
    double s = 0.0;
    for (int i = 0; i < n - 1; i++) s += geo_dist(&pts[i], &pts[i + 1]);
    if (closed) s += geo_dist(&pts[n - 1], &pts[0]);
    return s;
}
geo_pt geo_centroid(const geo_pt* pts, int n) {
    geo_pt c = { 0, 0 };
    if (!pts || n < 1) return c;
    if (n < 3) { for (int i = 0; i < n; i++) { c.x += pts[i].x; c.y += pts[i].y; } c.x /= n; c.y /= n; return c; }
    double a = 0, cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; double cr = pts[i].x * pts[j].y - pts[j].x * pts[i].y;
        a += cr; cx += (pts[i].x + pts[j].x) * cr; cy += (pts[i].y + pts[j].y) * cr; }
    if (fabs(a) < 1e-15) { for (int i = 0; i < n; i++) { c.x += pts[i].x; c.y += pts[i].y; } c.x /= n; c.y /= n; return c; }
    a *= 0.5; c.x = cx / (6 * a); c.y = cy / (6 * a);
    return c;
}

/* ── predicati / distanze ── */
int geo_point_in_poly(const geo_pt* p, const geo_pt* pts, int n) {
    if (!p || !pts || n < 3) return 0;
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((pts[i].y > p->y) != (pts[j].y > p->y)) &&
            (p->x < (pts[j].x - pts[i].x) * (p->y - pts[i].y) / (pts[j].y - pts[i].y) + pts[i].x))
            inside = !inside;
    }
    return inside;
}
double geo_dist_point_seg(const geo_pt* p, const geo_pt* a, const geo_pt* b) {
    double dx = b->x - a->x, dy = b->y - a->y;
    double L2 = dx * dx + dy * dy;
    if (L2 < 1e-30) return geo_dist(p, a);
    double t = ((p->x - a->x) * dx + (p->y - a->y) * dy) / L2;
    if (t < 0) t = 0; if (t > 1) t = 1;
    geo_pt q = { a->x + t * dx, a->y + t * dy };
    return geo_dist(p, &q);
}
int geo_on_segment(const geo_pt* p, const geo_pt* a, const geo_pt* b) {
    return geo_dist_point_seg(p, a, b) < 1e-9;
}
double geo_dist_seg_seg(const geo_pt* a, const geo_pt* b, const geo_pt* c, const geo_pt* d) {
    if (geo_seg_intersect(a, b, c, d)) return 0.0;
    double d1 = geo_dist_point_seg(a, c, d), d2 = geo_dist_point_seg(b, c, d);
    double d3 = geo_dist_point_seg(c, a, b), d4 = geo_dist_point_seg(d, a, b);
    double m = d1; if (d2 < m) m = d2; if (d3 < m) m = d3; if (d4 < m) m = d4;
    return m;
}

/* ── intersezione ── */
static int orient(const geo_pt* a, const geo_pt* b, const geo_pt* c) {
    double v = (b->y - a->y) * (c->x - b->x) - (b->x - a->x) * (c->y - b->y);
    return v > 0 ? 1 : (v < 0 ? -1 : 0);
}
static int on_seg_box(const geo_pt* a, const geo_pt* b, const geo_pt* c) {
    return b->x <= fmax(a->x, c->x) && b->x >= fmin(a->x, c->x) &&
           b->y <= fmax(a->y, c->y) && b->y >= fmin(a->y, c->y);
}
int geo_seg_intersect(const geo_pt* a, const geo_pt* b, const geo_pt* c, const geo_pt* d) {
    if (!a || !b || !c || !d) return 0;
    int o1 = orient(a, b, c), o2 = orient(a, b, d), o3 = orient(c, d, a), o4 = orient(c, d, b);
    if (o1 != o2 && o3 != o4) return 1;
    if (o1 == 0 && on_seg_box(a, c, b)) return 1;
    if (o2 == 0 && on_seg_box(a, d, b)) return 1;
    if (o3 == 0 && on_seg_box(c, a, d)) return 1;
    if (o4 == 0 && on_seg_box(c, b, d)) return 1;
    return 0;
}
int geo_seg_intersection(const geo_pt* a, const geo_pt* b, const geo_pt* c, const geo_pt* d, geo_pt* out) {
    double r_x = b->x - a->x, r_y = b->y - a->y, s_x = d->x - c->x, s_y = d->y - c->y;
    double den = r_x * s_y - r_y * s_x;
    if (fabs(den) < 1e-15) return 0;   /* paralleli/collineari */
    double t = ((c->x - a->x) * s_y - (c->y - a->y) * s_x) / den;
    double u = ((c->x - a->x) * r_y - (c->y - a->y) * r_x) / den;
    if (t < 0 || t > 1 || u < 0 || u > 1) return 0;
    if (out) { out->x = a->x + t * r_x; out->y = a->y + t * r_y; }
    return 1;
}

/* ── forma ── */
static int cmp_pt(const void* a, const void* b) {
    const geo_pt* pa = (const geo_pt*)a; const geo_pt* pb = (const geo_pt*)b;
    if (pa->x < pb->x) return -1; if (pa->x > pb->x) return 1;
    return pa->y < pb->y ? -1 : (pa->y > pb->y ? 1 : 0);
}
static double cross3(const geo_pt* o, const geo_pt* a, const geo_pt* b) {
    return (a->x - o->x) * (b->y - o->y) - (a->y - o->y) * (b->x - o->x);
}
int geo_convex_hull(const geo_pt* pts, int n, geo_pt* out, int cap) {
    if (!pts || !out || n < 1) return -1;
    if (n < 3) { for (int i = 0; i < n; i++) out[i] = pts[i]; return n; }
    geo_pt* s = (geo_pt*)malloc((size_t)n * sizeof(geo_pt));
    if (!s) return -1;
    memcpy(s, pts, (size_t)n * sizeof(geo_pt));
    qsort(s, (size_t)n, sizeof(geo_pt), cmp_pt);
    int k = 0;
    for (int i = 0; i < n; i++) { while (k >= 2 && cross3(&out[k - 2], &out[k - 1], &s[i]) <= 0) k--; if (k < cap) out[k++] = s[i]; }
    int lower = k + 1;
    for (int i = n - 2; i >= 0; i--) { while (k >= lower && cross3(&out[k - 2], &out[k - 1], &s[i]) <= 0) k--; if (k < cap) out[k++] = s[i]; }
    free(s);
    return k - 1;
}
int geo_bbox_of(const geo_pt* pts, int n, geo_bbox* out) {
    if (!pts || n < 1 || !out) return -1;
    out->min = out->max = pts[0];
    for (int i = 1; i < n; i++) {
        if (pts[i].x < out->min.x) out->min.x = pts[i].x;
        if (pts[i].y < out->min.y) out->min.y = pts[i].y;
        if (pts[i].x > out->max.x) out->max.x = pts[i].x;
        if (pts[i].y > out->max.y) out->max.y = pts[i].y;
    }
    return 0;
}
/* Douglas-Peucker ricorsivo */
static int dp(const geo_pt* pts, int n, double tol, geo_pt* out, int cap, int* oi) {
    int idx = -1; double maxd = tol;
    for (int i = 1; i < n - 1; i++) { double d = geo_dist_point_seg(&pts[i], &pts[0], &pts[n - 1]); if (d > maxd) { maxd = d; idx = i; } }
    if (idx < 0) { if (*oi < cap) out[(*oi)++] = pts[0]; return 0; }
    dp(pts, idx + 1, tol, out, cap, oi);
    dp(pts + idx, n - idx, tol, out, cap, oi);
    return 0;
}
int geo_simplify(const geo_pt* pts, int n, double tol, geo_pt* out, int cap) {
    if (!pts || !out || n < 2) return -1;
    int oi = 0;
    dp(pts, n, tol, out, cap, &oi);
    if (oi < cap) out[oi++] = pts[n - 1];
    return oi;
}
int geo_translate(const geo_pt* in, int n, double dx, double dy, geo_pt* out) {
    if (!in || !out || n < 0) return -1;
    for (int i = 0; i < n; i++) { out[i].x = in[i].x + dx; out[i].y = in[i].y + dy; }
    return 0;
}
int geo_scale(const geo_pt* in, int n, double sx, double sy, geo_pt* out) {
    if (!in || !out || n < 0) return -1;
    for (int i = 0; i < n; i++) { out[i].x = in[i].x * sx; out[i].y = in[i].y * sy; }
    return 0;
}
int geo_rotate(const geo_pt* in, int n, double deg, geo_pt* out) {
    if (!in || !out || n < 0) return -1;
    double r = deg * M_PI / 180.0, c = cos(r), s = sin(r);
    for (int i = 0; i < n; i++) { out[i].x = in[i].x * c - in[i].y * s; out[i].y = in[i].x * s + in[i].y * c; }
    return 0;
}
static int inside_edge(const geo_pt* a, const geo_pt* b, const geo_pt* p) {
    return (b->x - a->x) * (p->y - a->y) - (b->y - a->y) * (p->x - a->x) >= 0;   /* clip CCW */
}
static geo_pt inter_line(const geo_pt* a, const geo_pt* b, const geo_pt* p, const geo_pt* q) {
    double Ax = b->x - a->x, Ay = b->y - a->y, Bx = q->x - p->x, By = q->y - p->y;
    double den = Ax * By - Ay * Bx;
    geo_pt r = *p;
    if (fabs(den) < 1e-15) return r;
    double t = ((p->x - a->x) * By - (p->y - a->y) * Bx) / den;
    r.x = a->x + t * Ax; r.y = a->y + t * Ay;
    return r;
}
int geo_clip(const geo_pt* subj, int ns, const geo_pt* clip, int nc, geo_pt* out, int cap) {
    if (!subj || !clip || !out || ns < 3 || nc < 3) return -1;
    geo_pt* in = (geo_pt*)malloc((size_t)(ns + nc + 4) * sizeof(geo_pt));
    geo_pt* tmp = (geo_pt*)malloc((size_t)(ns + nc + 4) * sizeof(geo_pt));
    if (!in || !tmp) { free(in); free(tmp); return -1; }
    for (int i = 0; i < ns; i++) in[i] = subj[i];
    int n = ns;
    for (int e = 0; e < nc && n > 0; e++) {
        const geo_pt *A = &clip[e], *B = &clip[(e + 1) % nc];
        int m = 0;
        for (int i = 0; i < n; i++) {
            const geo_pt *P = &in[i], *Q = &in[(i + 1) % n];
            int pin = inside_edge(A, B, P), qin = inside_edge(A, B, Q);
            if (pin) tmp[m++] = *P;
            if (pin != qin) tmp[m++] = inter_line(A, B, P, Q);
        }
        memcpy(in, tmp, (size_t)m * sizeof(geo_pt));
        n = m;
    }
    if (n > cap) n = cap;
    for (int i = 0; i < n; i++) out[i] = in[i];
    free(in); free(tmp);
    return n;
}

/* ── WKT ── */
static const char* skipws(const char* p) { while (*p && isspace((unsigned char)*p)) p++; return p; }
static int read_pt(const char** pp, geo_pt* out) {
    const char* p = skipws(*pp);
    char* end;
    double x = strtod(p, &end); if (end == p) return -1; p = end;
    double y = strtod(p, &end); if (end == p) return -1; p = end;
    out->x = x; out->y = y; *pp = p; return 0;
}
int geo_wkt_parse(const char* wkt, geo_pt** pts, int* n, int* closed) {
    if (!wkt || !pts || !n) return -1;
    const char* p = skipws(wkt);
    int cls = 0;
    if (strncasecmp(p, "POINT", 5) == 0) { cls = 1; p += 5; }
    else if (strncasecmp(p, "LINESTRING", 10) == 0) { cls = 2; p += 10; }
    else if (strncasecmp(p, "POLYGON", 7) == 0) { cls = 3; p += 7; }
    else return -1;
    p = skipws(p);
    if (*p != '(') return -1;
    p++;
    if (cls == 3) { p = skipws(p); if (*p != '(') return -1; p++; }   /* anello esterno */
    int cap = 8, cnt = 0;
    geo_pt* arr = (geo_pt*)malloc((size_t)cap * sizeof(geo_pt));
    if (!arr) return -1;
    for (;;) {
        p = skipws(p);
        if (*p == ')') break;
        geo_pt q;
        if (read_pt(&p, &q) != 0) { free(arr); return -1; }
        if (cnt == cap) { cap *= 2; geo_pt* na = (geo_pt*)realloc(arr, (size_t)cap * sizeof(geo_pt)); if (!na) { free(arr); return -1; } arr = na; }
        arr[cnt++] = q;
        p = skipws(p);
        if (*p == ',') { p++; continue; }
        break;
    }
    *pts = arr; *n = cnt;
    if (closed) *closed = (cls == 3);
    return 0;
}
int geo_wkt_point(const char* wkt, geo_pt* out) {
    geo_pt* a = NULL; int n = 0, cl = 0;
    if (geo_wkt_parse(wkt, &a, &n, &cl) != 0 || n < 1) { free(a); return -1; }
    *out = a[0]; free(a); return 0;
}

/* ── indice a griglia ── */
struct geo_grid { double cell, minx, miny; int nx, ny, *start, *items; };
geo_grid* geo_grid_new(const geo_pt* pts, int n, double cell) {
    if (!pts || n < 1 || cell <= 0) return NULL;
    geo_bbox bb;
    if (geo_bbox_of(pts, n, &bb) != 0) return NULL;
    geo_grid* g = (geo_grid*)calloc(1, sizeof(geo_grid));
    if (!g) return NULL;
    g->cell = cell; g->minx = bb.min.x; g->miny = bb.min.y;
    g->nx = (int)((bb.max.x - bb.min.x) / cell) + 1;
    g->ny = (int)((bb.max.y - bb.min.y) / cell) + 1;
    long ncells = (long)g->nx * g->ny;
    g->start = (int*)calloc((size_t)ncells + 1, sizeof(int));
    g->items = (int*)malloc((size_t)n * sizeof(int));
    if (!g->start || !g->items) { free(g->start); free(g->items); free(g); return NULL; }
    int* cnt = (int*)calloc((size_t)ncells, sizeof(int));
    for (int i = 0; i < n; i++) { int cx = (int)((pts[i].x - g->minx) / cell), cy = (int)((pts[i].y - g->miny) / cell); cnt[cy * g->nx + cx]++; }
    for (long c = 0; c < ncells; c++) g->start[c + 1] = g->start[c] + cnt[c];
    int* pos = (int*)malloc((size_t)ncells * sizeof(int));
    for (long c = 0; c < ncells; c++) pos[c] = g->start[c];
    for (int i = 0; i < n; i++) { int cx = (int)((pts[i].x - g->minx) / cell), cy = (int)((pts[i].y - g->miny) / cell); long c = (long)cy * g->nx + cx; g->items[pos[c]++] = i; }
    free(cnt); free(pos);
    return g;
}
int geo_grid_query_bbox(const geo_grid* g, geo_bbox box, int* idx_out, int cap) {
    if (!g || !idx_out) return -1;
    int x0 = (int)((box.min.x - g->minx) / g->cell), x1 = (int)((box.max.x - g->minx) / g->cell);
    int y0 = (int)((box.min.y - g->miny) / g->cell), y1 = (int)((box.max.y - g->miny) / g->cell);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= g->nx) x1 = g->nx - 1; if (y1 >= g->ny) y1 = g->ny - 1;
    int n = 0;
    for (int cy = y0; cy <= y1; cy++) for (int cx = x0; cx <= x1; cx++) {
        long c = (long)cy * g->nx + cx;
        for (int k = g->start[c]; k < g->start[c + 1]; k++) if (n < cap) idx_out[n++] = g->items[k];
    }
    return n;
}
void geo_grid_free(geo_grid* g) { if (!g) return; free(g->start); free(g->items); free(g); }
