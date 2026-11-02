/* gmb_brain_tools_internal.h — helper condivisi fra i file del Second Brain C.
 * NON è API pubblica: i consumatori usano gmb_brain_tools.h. */
#ifndef GMB_BRAIN_TOOLS_INTERNAL_H
#define GMB_BRAIN_TOOLS_INTERNAL_H

#include "gmb_brain_tools.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "gmb_runtime.h"

/* String builder minimale (condiviso fra i file del Second Brain C). */
typedef struct { char* p; size_t n, cap; } sbuf_t;

static inline void sb_init(sbuf_t* b)
{
    b->cap = 1024;
    b->n = 0;
    b->p = (char*)malloc(b->cap);
    if (b->p) b->p[0] = 0;
}
static inline void sb_reserve(sbuf_t* b, size_t extra)
{
    if (!b->p) return;
    if (b->n + extra + 1 <= b->cap) return;
    while (b->n + extra + 1 > b->cap) b->cap *= 2;
    char* np = (char*)realloc(b->p, b->cap);
    if (!np) { free(b->p); b->p = NULL; }
    else b->p = np;
}
static inline void sb_put(sbuf_t* b, const char* s)
{
    if (!b->p || !s) return;
    size_t l = strlen(s);
    sb_reserve(b, l);
    if (!b->p) return;
    memcpy(b->p + b->n, s, l);
    b->n += l;
    b->p[b->n] = 0;
}
static inline void sb_putc(sbuf_t* b, char c)
{
    if (!b->p) return;
    sb_reserve(b, 1);
    if (!b->p) return;
    b->p[b->n++] = c;
    b->p[b->n] = 0;
}
static inline void sb_esc(sbuf_t* b, const char* s)
{
    sb_putc(b, '"');
    for (const char* q = s; q && *q; q++) {
        unsigned char c = (unsigned char)*q;
        if (c == '"' || c == '\\') { sb_putc(b, '\\'); sb_putc(b, (char)c); }
        else if (c == '\n') sb_put(b, "\\n");
        else if (c == '\r') sb_put(b, "\\r");
        else if (c == '\t') sb_put(b, "\\t");
        else if (c < 0x20) { char tmp[8]; snprintf(tmp, sizeof tmp, "\\u%04x", c); sb_put(b, tmp); }
        else sb_putc(b, (char)c);
    }
    sb_putc(b, '"');
}

gmb_brain_result_t br_ok(const char* fmt, ...);
gmb_brain_result_t br_fail(const char* fmt, ...);

char* br_read_file(const char* path);
int   br_write_file(const char* path, const char* text);
long  br_file_size(const char* path);
void  br_now_utc_iso(char* out, size_t cap);
void  br_now_local_stamp(char* out, size_t cap);
long  br_count_md(const char* dir, long limit);

sqlite3* br_sql_open(const char* path, int readonly);
long     br_sql_scalar(sqlite3* db, const char* sql);
char*    br_sql_text(sqlite3* db, const char* sql);
int      br_sql_exec(sqlite3* db, const char* sql);

/* Grafo (gmb_brain_graph.c): modello WikiGraph + export. */
typedef struct { char* id; char* title; char* raw; } wgnode_t;
typedef struct { char* from; char* to; char* type; char* raw; } wgedge_t;
typedef struct {
    wgnode_t* nodes; int nn;
    wgedge_t* edges; int ne;
    char** dangling; int nd;
    char* folders_json;   /* oggetto JSON, "{}" se assente */
    int louvain;
} wgraph_t;

int   wg_load(const char* path, wgraph_t* g);
char* wg_to_json(wgraph_t* g);
void  wg_free(wgraph_t* g);
void  json_val(sbuf_t* b, gmb_val_t v);

/* Scrive gli export accessori di <graphs_dir>/<name>.json (stats/mermaid/graphml/html)
 * e conta edge/dangling. */
int  br_graph_write_artifacts(const char* name, const char* graphs_dir);
long br_graph_edge_count(const char* json_path);
long br_graph_dangling_count(const char* json_path);

#endif /* GMB_BRAIN_TOOLS_INTERNAL_H */
