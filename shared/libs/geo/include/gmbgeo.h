/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbgeo.h — geometria piana / GIS (ispirato a shapely/geopandas). Self-contained (C99 + libm).
 * Nessun tipo di dominio (fondazione gmbmaths). */
#ifndef GMBGEO_H
#define GMBGEO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { double x, y; } geo_pt;
typedef struct { geo_pt min, max; } geo_bbox;

/* ── misure ── */
double geo_dist(const geo_pt* a, const geo_pt* b);
double geo_poly_area(const geo_pt* pts, int n);            /* area firmata (shoelace) */
double geo_perimeter(const geo_pt* pts, int n, int closed);/* perimetro (closed=1) o lunghezza */
geo_pt geo_centroid(const geo_pt* pts, int n);             /* centroide (area) o media vertici */

/* ── predicati / distanze ── */
int    geo_point_in_poly(const geo_pt* p, const geo_pt* pts, int n);
int    geo_on_segment(const geo_pt* p, const geo_pt* a, const geo_pt* b);
double geo_dist_point_seg(const geo_pt* p, const geo_pt* a, const geo_pt* b);
double geo_dist_seg_seg(const geo_pt* a, const geo_pt* b, const geo_pt* c, const geo_pt* d);

/* ── intersezione ── */
int    geo_seg_intersect(const geo_pt* a, const geo_pt* b, const geo_pt* c, const geo_pt* d);
int    geo_seg_intersection(const geo_pt* a, const geo_pt* b, const geo_pt* c, const geo_pt* d, geo_pt* out);

/* ── forma ── */
int    geo_convex_hull(const geo_pt* pts, int n, geo_pt* out, int cap);
int    geo_bbox_of(const geo_pt* pts, int n, geo_bbox* out);
int    geo_simplify(const geo_pt* pts, int n, double tol, geo_pt* out, int cap);   /* Douglas-Peucker */
int    geo_translate(const geo_pt* in, int n, double dx, double dy, geo_pt* out);
int    geo_scale(const geo_pt* in, int n, double sx, double sy, geo_pt* out);
int    geo_rotate(const geo_pt* in, int n, double deg, geo_pt* out);
/* intersezione soggetto∩clip (Sutherland-Hodgman, clip convesso): n. vertici o -1. */
int    geo_clip(const geo_pt* subj, int ns, const geo_pt* clip, int nc, geo_pt* out, int cap);

/* ── proiezioni / geodesia (self-contained, sferico) ── */
void   geo_project(double lon, double lat, double* x, double* y);   /* Web Mercator (lon/lat gradi -> x/y metri) */
void   geo_unproject(double x, double y, double* lon, double* lat); /* inversa */
double geo_haversine(double lon1, double lat1, double lon2, double lat2); /* distanza geodesica (metri) */

/* ── overlay booleano generale di poligoni semplici (Greiner-Hormann) ── */
#define GEO_INTERSECTION 0
#define GEO_UNION        1
#define GEO_DIFFERENCE   2
/* Accetta poligoni (non solo convessi) NON degeneri. Ritorna il n. di vertici del risultato, 0 se vuoto, -1 err. */
int    geo_overlay(const geo_pt* a, int na, const geo_pt* b, int nb, int op, geo_pt* out, int cap);

/* ── spatial join: coppie (i,j) di poligoni di due insiemi che si toccano/intersecano ── */
/* A/B: array di n poligoni (A[i] = pts, nA[i] = vertici). Scrive le coppie (i,j) in `pairs` (2*count int).
   Ritorna il numero di coppie, -1 su errore. */
int    geo_spatial_join(const geo_pt* const* A, const int* nA, int na,
                        const geo_pt* const* B, const int* nB, int nb,
                        int* pairs, int cap);

/* ── I/O GeoJSON ── */
/* Parsa {"type":"Point|LineString|Polygon","coordinates":[...]} -> alloca *pts (free). closed=1 per Polygon. */
int    geo_geojson_parse(const char* json, geo_pt** pts, int* n, int* closed);
/* Serializza in GeoJSON. Ritorna la lunghezza scritta (>0) o -1. */
int    geo_geojson_write(const geo_pt* pts, int n, int closed, char* out, int cap);

/* ── I/O WKT ── */
/* Parsa POINT / LINESTRING / POLYGON; alloca *pts (da liberare con free). closed=1 per POLYGON. */
int    geo_wkt_parse(const char* wkt, geo_pt** pts, int* n, int* closed);
int    geo_wkt_point(const char* wkt, geo_pt* out);       /* scorciatoia POINT */

/* ── indice spaziale (griglia uniforme) ── */
typedef struct geo_grid geo_grid;
geo_grid* geo_grid_new(const geo_pt* pts, int n, double cell);   /* NULL se n<1 */
int       geo_grid_query_bbox(const geo_grid*, geo_bbox box, int* idx_out, int cap); /* n. trovati, -1 err */
void      geo_grid_free(geo_grid*);

#ifdef __cplusplus
}
#endif
#endif /* GMBGEO_H */
