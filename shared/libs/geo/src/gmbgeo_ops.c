/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbgeo_ops.c — gmbgeo: proiezioni/geodesia, overlay booleano (Greiner-Hormann),
 * spatial join, GeoJSON. Self-contained (C99 + libm). */
#include "gmbgeo.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define GEO_R 6378137.0   /* raggio sfera (WGS84 semi-asse maggiore), metri */

/* ══════════════════ proiezioni / geodesia ══════════════════ */

void geo_project(double lon, double lat, double* x, double* y) {
    if (x) *x = GEO_R * lon * M_PI / 180.0;
    if (y) *y = GEO_R * log(tan(M_PI / 4.0 + lat * (M_PI / 180.0) / 2.0));
}
void geo_unproject(double x, double y, double* lon, double* lat) {
    if (lon) *lon = x / GEO_R * 180.0 / M_PI;
    if (lat) *lat = (2.0 * atan(exp(y / GEO_R)) - M_PI / 2.0) * 180.0 / M_PI;
}
double geo_haversine(double lon1, double lat1, double lon2, double lat2) {
    double p1 = lat1 * M_PI / 180.0, p2 = lat2 * M_PI / 180.0;
    double dp = (lat2 - lat1) * M_PI / 180.0, dl = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
    return 2.0 * GEO_R * asin(sqrt(a));
}

/* ══════════════════ GeoJSON ══════════════════ */

static const char* gj_ws(const char* p) { while (*p && isspace((unsigned char)*p)) p++; return p; }

int geo_geojson_parse(const char* json, geo_pt** pts, int* n, int* closed) {
    if (!json || !pts || !n) return -1;
    const char* c = strstr(json, "\"coordinates\"");
    if (!c) return -1;
    const char* p = strchr(c, ':');
    if (!p) return -1;
    p++;
    /* type: Polygon/MultiPolygon -> closed (buchi/MultiPolygon: si prende il PRIMO anello). */
    int isPoly = (strstr(json, "Polygon") != NULL);
    int cap = 8, cnt = 0;
    geo_pt* arr = (geo_pt*)malloc((size_t)cap * sizeof(geo_pt));
    if (!arr) return -1;
    /* scanner robusto: raccoglie ogni coppia [x,y] saltando le parentesi non-coppia. */
    for (;;) {
        p = gj_ws(p);
        if (!*p) break;
        if (*p == '[') {
            const char* q = gj_ws(p + 1);
            char* e; double x = strtod(q, &e);
            if (e != q) {
                q = gj_ws(e);
                if (*q == ',') {
                    q = gj_ws(q + 1);
                    double y = strtod(q, &e);
                    if (e != q) { q = gj_ws(e); if (*q == ']') {
                        if (cnt == cap) { cap *= 2; geo_pt* na = (geo_pt*)realloc(arr, (size_t)cap * sizeof(geo_pt)); if (!na) { free(arr); return -1; } arr = na; }
                        arr[cnt].x = x; arr[cnt].y = y; cnt++;
                        p = q + 1; continue;
                    } }
                }
            }
            p++; continue;                      /* '[' non-coppia */
        }
        p++;                                    /* ']', ',', spazi, numeri spuri */
    }
    *pts = arr; *n = cnt;
    if (closed) *closed = isPoly;
    return 0;
}

int geo_geojson_write(const geo_pt* pts, int n, int closed, char* out, int cap) {
    if (!out || cap <= 0) return -1;
    const char* type = closed ? "Polygon" : (n == 1 ? "Point" : "LineString");
    int w = snprintf(out, (size_t)cap, "{\"type\":\"%s\",\"coordinates\":", type);
    if (w < 0 || w >= cap) return -1;
    if (closed) { w += snprintf(out + w, (size_t)(cap - w), "[["); if (w >= cap) return -1; }
    else if (n > 1) { w += snprintf(out + w, (size_t)(cap - w), "["); if (w >= cap) return -1; }
    for (int i = 0; i < n; i++) {
        int need = (closed || n > 1) ? (i == 0 ? 0 : 1) : 0;
        int w2 = snprintf(out + w, (size_t)(cap - w), "%s[%.10g,%.10g]", need ? "," : "", pts[i].x, pts[i].y);
        if (w2 < 0 || w + w2 >= cap) return -1;
        w += w2;
    }
    if (closed || n > 1) { int w2 = snprintf(out + w, (size_t)(cap - w), "]"); if (w2 < 0 || w + w2 >= cap) return -1; w += w2; }
    if (closed) { int w2 = snprintf(out + w, (size_t)(cap - w), "]"); if (w2 < 0 || w + w2 >= cap) return -1; w += w2; }
    int w3 = snprintf(out + w, (size_t)(cap - w), "}"); if (w3 < 0 || w + w3 >= cap) return -1;
    return w + w3;
}

/* ══════════════════ spatial join ══════════════════ */

static int poly_intersect(const geo_pt* a, int na, const geo_pt* b, int nb) {
    geo_bbox ba, bb;
    if (geo_bbox_of(a, na, &ba) != 0 || geo_bbox_of(b, nb, &bb) != 0) return 0;
    if (ba.max.x < bb.min.x || bb.max.x < ba.min.x || ba.max.y < bb.min.y || bb.max.y < ba.min.y) return 0;
    for (int i = 0; i < na; i++) if (geo_point_in_poly(&a[i], b, nb)) return 1;
    for (int i = 0; i < nb; i++) if (geo_point_in_poly(&b[i], a, na)) return 1;
    for (int i = 0, ii = na - 1; i < na; ii = i++)
        for (int j = 0, jj = nb - 1; j < nb; jj = j++)
            if (geo_seg_intersect(&a[ii], &a[i], &b[jj], &b[j])) return 1;
    return 0;
}

int geo_spatial_join(const geo_pt* const* A, const int* nA, int na,
                     const geo_pt* const* B, const int* nB, int nb,
                     int* pairs, int cap) {
    if (!A || !nA || !B || !nB || !pairs) return -1;
    int cnt = 0;
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nb; j++)
            if (poly_intersect(A[i], nA[i], B[j], nB[j])) {
                if (cnt < cap) { pairs[2 * cnt] = i; pairs[2 * cnt + 1] = j; }
                cnt++;
            }
    return cnt;
}

/* ══════════════════ overlay booleano (Greiner-Hormann) ══════════════════
 * Poligoni SEMPLICI (senza auto-intersezioni). I casi degeneri (vertici/edge
 * collineari sovrapposti) NON sono gestiti: il chiamante deve evitare geometrie
 * degeneri. Ritorna i vertici del risultato (0 = vuoto, -1 = errore). */

typedef struct ghv { double x, y, alpha; int inter, entry; struct ghv *next, *prev, *neigh; } ghv;

static ghv* gh_node(double x, double y) { ghv* v = (ghv*)calloc(1, sizeof(ghv)); if (v) { v->x = x; v->y = y; v->next = v; v->prev = v; } return v; }

/* punto su un bordo del poligono? (tolleranza) */
static int geo_on_boundary_any(const geo_pt* p, const geo_pt* poly, int n) {
    for (int i = 0, j = n - 1; i < n; j = i++) if (geo_on_segment(p, &poly[j], &poly[i])) return 1;
    return 0;
}

static ghv* gh_list(const geo_pt* p, int n) {
    ghv *first = NULL, *last = NULL;
    for (int i = 0; i < n; i++) {
        if (i == n - 1 && n > 1 && fabs(p[i].x - p[0].x) < 1e-12 && fabs(p[i].y - p[0].y) < 1e-12) break; /* chiudi anello */
        ghv* v = gh_node(p[i].x, p[i].y);
        if (!v) return first;
        if (!first) { first = last = v; }
        else { v->prev = last; v->next = first; last->next = v; first->prev = v; last = v; }
    }
    return first;
}
static void gh_free(ghv* first) {
    if (!first) return;
    ghv* p = first;
    do { ghv* nx = p->next; free(p); p = nx; if (p == first) break; } while (p);
    /* i nodi intersezione inseriti appartengono alla stessa lista: già liberati */
}
static void gh_insert(ghv* e1, ghv* e2, double a1, double a2) {
    /* inserisce un'intersezione tra e1 e e1->next (alpha a1) e tra e2 e e2->next (a2) */
    ghv* n1 = gh_node(0, 0); ghv* n2 = gh_node(0, 0);
    if (!n1 || !n2) { free(n1); free(n2); return; }
    n1->inter = n2->inter = 1; n1->alpha = a1; n2->alpha = a2; n1->neigh = n2; n2->neigh = n1;
    /* posiziona sulla geometria */
    n1->x = e1->x + (e1->next->x - e1->x) * a1; n1->y = e1->y + (e1->next->y - e1->y) * a1;
    n2->x = e2->x + (e2->next->x - e2->x) * a2; n2->y = e2->y + (e2->next->y - e2->y) * a2;
    /* A: inserisci n1 ordinato per alpha nella sequenza di intersezioni dell'edge e1 */
    ghv* q = e1; while (q->next->inter && q->next->alpha < a1 && q->next != e1->next && q != e1) q = q->next;
    if (q == e1) q = e1; /* inizio edge */
    { ghv* nx = q->next; n1->prev = q; n1->next = nx; q->next = n1; nx->prev = n1; }
    q = e2; while (q->next->inter && q->next->alpha < a2) q = q->next;
    { ghv* nx = q->next; n2->prev = q; n2->next = nx; q->next = n2; nx->prev = n2; }
}

int geo_overlay(const geo_pt* a, int na, const geo_pt* b, int nb, int op, geo_pt* out, int cap) {
    if (!a || !b || !out || na < 3 || nb < 3 || cap < 3) return -1;
    ghv* la = gh_list(a, na);
    ghv* lb = gh_list(b, nb);
    if (!la || !lb) { gh_free(la); gh_free(lb); return -1; }

    /* 1) intersezioni + inserimento */
    int any = 0;
    for (ghv* ea = la; ; ea = ea->next) {
        if (!ea->inter) {
            for (ghv* eb = lb; ; eb = eb->next) {
                if (!eb->inter) {
                    geo_pt r1 = { ea->x, ea->y }, r2 = { ea->next->x, ea->next->y };
                    geo_pt s1 = { eb->x, eb->y }, s2 = { eb->next->x, eb->next->y };
                    double rx = r2.x - r1.x, ry = r2.y - r1.y, sx = s2.x - s1.x, sy = s2.y - s1.y;
                    double den = rx * sy - ry * sx;
                    if (fabs(den) < 1e-15) { /* paralleli: ignora (no casi degeneri) */ }
                    else {
                        double t = ((s1.x - r1.x) * sy - (s1.y - r1.y) * sx) / den;
                        double u = ((s1.x - r1.x) * ry - (s1.y - r1.y) * rx) / den;
                        if (t > 1e-12 && t < 1 - 1e-12 && u > 1e-12 && u < 1 - 1e-12) { gh_insert(ea, eb, t, u); any = 1; }
                    }
                }
                if (eb->next == lb) break;
            }
        }
        if (ea->next == la) break;
    }
    if (!any) { /* nessuna intersezione: casi semplici */
        gh_free(la); gh_free(lb);
        if (op == GEO_INTERSECTION) return 0;
        if (op == GEO_UNION) {
            int n = 0;
            if (geo_point_in_poly(&a[0], b, nb)) {           /* B dentro A → A */
                for (int i = 0; i < na && n < cap; i++) out[n++] = a[i];
            } else if (geo_point_in_poly(&b[0], a, na)) {    /* A dentro B → B */
                for (int i = 0; i < nb && n < cap; i++) out[n++] = b[i];
            } else {                                          /* disgiunti → A + B (unione non connessa) */
                for (int i = 0; i < na && n < cap; i++) out[n++] = a[i];
                for (int i = 0; i < nb && n < cap; i++) out[n++] = b[i];
            }
            return n;
        }
        if (op == GEO_DIFFERENCE) {   /* A - B: se disgiunti = A; se A dentro B = vuoto */
            if (geo_point_in_poly(&a[0], b, nb)) return 0;
            int n = 0; for (int i = 0; i < na && n < cap; i++) out[n++] = a[i];
            return n;
        }
        return -1;
    }

    /* 2) entry/exit marking (midpoint del successivo)
       per il poligono A: entry=1 se, dopo l'intersezione, A entra in B (prossimo punto FUORI → hmm)
       Standard GH: status = inside(punto corrente). entry = !inside_before && inside_after. */
    /* entry=1 se, subito DOPO l'intersezione, il punto è INTERNO all'altro poligono
       (il poligono "entra"). Per DIFFERENCE si nega (il soggetto è percorso al contrario). */
    for (ghv* x = la; ; x = x->next) {
        if (x->inter) {
            geo_pt mp = { (x->x + x->next->x) / 2.0, (x->y + x->next->y) / 2.0 };
            int inb = geo_point_in_poly(&mp, b, nb) || geo_on_boundary_any(&mp, b, nb);
            x->entry = (op == GEO_DIFFERENCE) ? !inb : inb;
        }
        if (x->next == la) break;
    }
    for (ghv* x = lb; ; x = x->next) {
        if (x->inter) {
            geo_pt mp = { (x->x + x->next->x) / 2.0, (x->y + x->next->y) / 2.0 };
            int ina = geo_point_in_poly(&mp, a, na) || geo_on_boundary_any(&mp, a, na);
            x->entry = ina;
        }
        if (x->next == lb) break;
    }

    /* 3) trace */
    ghv* start = NULL;
    for (ghv* x = la; ; x = x->next) { if (x->inter) { start = x; break; } if (x->next == la) break; }
    if (!start) { gh_free(la); gh_free(lb); return 0; }
    int n = 0;
    ghv* cur = start;
    do {
        ghv* s = cur; int steps = 0;
        int fwd = (op == GEO_UNION) ? !cur->entry : cur->entry;
        if (fwd) {
            do { if (n < cap) out[n++] = (geo_pt){ s->x, s->y }; s = s->next; steps++; if (steps > (na + nb) * 4 + 16) break; } while (!s->inter);
            cur = s->neigh;
        } else {
            do { if (n < cap) out[n++] = (geo_pt){ s->x, s->y }; s = s->prev; steps++; if (steps > (na + nb) * 4 + 16) break; } while (!s->inter);
            cur = s->neigh;
        }
    } while (cur != start && n < cap);
    gh_free(la); gh_free(lb);
    return n;
}
