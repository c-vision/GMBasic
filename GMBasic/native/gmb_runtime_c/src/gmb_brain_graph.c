/* gmb_brain_graph.c — modello del grafo wiki (nodi/edge/dangling) ed export:
 * json normalizzato, stats.json, mermaid, graphml, html, vault Obsidian.
 * Usato dal toolchain del Second Brain (dedupe/extract/merge/convert) e da `gmb graph …`.
 */
#include "gmb_brain_tools.h"
#include "gmb_brain_tools_internal.h"

#include "gmb_runtime.h"
#include "gmb_graph.h"
#include "gmb_fsutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* ══════════════════════════════════════════════════════════════════════════
 * JSON minimale: serializzazione di un gmb_val_t (per conservare le parti che
 * non interpretiamo, es. frontmatter dei nodi)
 * ══════════════════════════════════════════════════════════════════════════ */

void json_val(sbuf_t* b, gmb_val_t v)
{
    switch (v.kind) {
    case GMB_TYPE_NIL: sb_put(b, "null"); break;
    case GMB_TYPE_BOOL: sb_put(b, v.v_bool ? "true" : "false"); break;
    case GMB_TYPE_INT: { char t[32]; snprintf(t, sizeof t, "%lld", (long long)v.v_int); sb_put(b, t); break; }
    case GMB_TYPE_DOUBLE: { char t[40]; snprintf(t, sizeof t, "%.6g", v.v_double); sb_put(b, t); break; }
    case GMB_TYPE_STRING: sb_esc(b, v.v_string.data ? v.v_string.data : ""); break;
    case GMB_TYPE_ARRAY: {
        gmb_list_t l = gmb_val_foreach_list(v);
        gmb_int_t n = gmb_list_count(l);
        sb_putc(b, '[');
        for (gmb_int_t i = 0; i < n; i++) { if (i) sb_put(b, ", "); json_val(b, gmb_list_get(l, i)); }
        sb_putc(b, ']');
        gmb_list_free(&l);
        break;
    }
    case GMB_TYPE_OBJECT: {
        gmb_int_t nk = 0;
        gmb_string_t* keys = gmb_val_dict_keys_array(v, &nk);
        sb_putc(b, '{');
        for (int i = 0; keys && i < (int)nk; i++) {
            if (!keys[i].data) continue;
            if (i) sb_put(b, ", ");
            sb_esc(b, keys[i].data);
            sb_put(b, ": ");
            json_val(b, gmb_val_dict_get(v, keys[i].data));
        }
        sb_putc(b, '}');
        break;
    }
    default: sb_put(b, "null"); break;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Modello grafo (WikiGraph: nodi/edge/dangling + raw per round-trip)
 * ══════════════════════════════════════════════════════════════════════════ */

void wg_free(wgraph_t* g)
{
    for (int i = 0; i < g->nn; i++) { free(g->nodes[i].id); free(g->nodes[i].title); free(g->nodes[i].raw); }
    for (int i = 0; i < g->ne; i++) { free(g->edges[i].from); free(g->edges[i].to); free(g->edges[i].type); free(g->edges[i].raw); }
    for (int i = 0; i < g->nd; i++) free(g->dangling[i]);
    free(g->nodes); free(g->edges); free(g->dangling); free(g->folders_json);
    memset(g, 0, sizeof *g);
}

static char* jval_str(gmb_val_t v)
{
    sbuf_t b;
    sb_init(&b);
    json_val(&b, v);
    return b.p;
}

int wg_load(const char* path, wgraph_t* g)
{
    memset(g, 0, sizeof *g);
    g->folders_json = strdup("{}");
    g->louvain = 1;
    char* js = br_read_file(path);
    if (!js) return 0;
    gmb_val_t r = gmb_json_parse(js);
    free(js);
    if (r.kind != GMB_TYPE_OBJECT) return 0;

    gmb_list_t nodes = gmb_val_foreach_list(gmb_val_dict_get(r, "nodes"));
    gmb_int_t nn = gmb_list_count(nodes);
    if (nn > 0) {
        g->nodes = (wgnode_t*)calloc((size_t)nn, sizeof(wgnode_t));
        for (gmb_int_t i = 0; i < nn; i++) {
            gmb_val_t n = gmb_list_get(nodes, i);
            if (n.kind != GMB_TYPE_OBJECT) continue;
            gmb_val_t id = gmb_val_dict_get(n, "id");
            gmb_val_t t = gmb_val_dict_get(n, "title");
            wgnode_t* x = &g->nodes[g->nn++];
            x->id = strdup(id.kind == GMB_TYPE_STRING && id.v_string.data ? id.v_string.data : "");
            x->title = strdup(t.kind == GMB_TYPE_STRING && t.v_string.data ? t.v_string.data : x->id);
            x->raw = jval_str(n);
        }
    }
    gmb_list_free(&nodes);

    gmb_list_t edges = gmb_val_foreach_list(gmb_val_dict_get(r, "edges"));
    gmb_int_t ne = gmb_list_count(edges);
    if (ne > 0) {
        g->edges = (wgedge_t*)calloc((size_t)ne, sizeof(wgedge_t));
        for (gmb_int_t i = 0; i < ne; i++) {
            gmb_val_t e = gmb_list_get(edges, i);
            if (e.kind != GMB_TYPE_OBJECT) continue;
            gmb_val_t f = gmb_val_dict_get(e, "from");
            gmb_val_t t = gmb_val_dict_get(e, "to");
            gmb_val_t ty = gmb_val_dict_get(e, "type");
            wgedge_t* x = &g->edges[g->ne++];
            x->from = strdup(f.kind == GMB_TYPE_STRING && f.v_string.data ? f.v_string.data : "");
            x->to = strdup(t.kind == GMB_TYPE_STRING && t.v_string.data ? t.v_string.data : "");
            x->type = strdup(ty.kind == GMB_TYPE_STRING && ty.v_string.data ? ty.v_string.data : "link");
            x->raw = jval_str(e);
        }
    }
    gmb_list_free(&edges);

    gmb_list_t dang = gmb_val_foreach_list(gmb_val_dict_get(r, "dangling"));
    gmb_int_t nd = gmb_list_count(dang);
    if (nd > 0) {
        g->dangling = (char**)calloc((size_t)nd, sizeof(char*));
        for (gmb_int_t i = 0; i < nd; i++) {
            gmb_val_t d = gmb_list_get(dang, i);
            if (d.kind == GMB_TYPE_STRING && d.v_string.data) g->dangling[g->nd++] = strdup(d.v_string.data);
        }
    }
    gmb_list_free(&dang);

    gmb_val_t fc = gmb_val_dict_get(r, "folderCounts");
    if (fc.kind == GMB_TYPE_OBJECT) {
        free(g->folders_json);
        g->folders_json = jval_str(fc);
    }
    return 1;
}

char* wg_to_json(wgraph_t* g)
{
    sbuf_t b;
    sb_init(&b);
    sb_put(&b, "{\n  \"nodes\": [");
    for (int i = 0; i < g->nn; i++) {
        sb_put(&b, i ? ",\n    " : "\n    ");
        sb_put(&b, g->nodes[i].raw ? g->nodes[i].raw : "{}");
    }
    sb_put(&b, g->nn ? "\n  ],\n  \"edges\": [" : "],\n  \"edges\": [");
    for (int i = 0; i < g->ne; i++) {
        sb_put(&b, i ? ",\n    " : "\n    ");
        sb_put(&b, g->edges[i].raw ? g->edges[i].raw : "{}");
    }
    sb_put(&b, g->ne ? "\n  ],\n  \"dangling\": [" : "],\n  \"dangling\": [");
    for (int i = 0; i < g->nd; i++) {
        sb_put(&b, i ? ", " : "\n    ");
        sb_esc(&b, g->dangling[i]);
    }
    sb_put(&b, g->nd ? "\n  ],\n  \"folderCounts\": " : "],\n  \"folderCounts\": ");
    sb_put(&b, g->folders_json ? g->folders_json : "{}");
    sb_put(&b, "\n}\n");
    return b.p;
}

/* Sanitize (parità GraphExporter.Sanitize): identificatore grafo. */
static void graph_sanitize(const char* in, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = in; *p && o + 1 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        out[o++] = (isalnum(c) || c == '_') ? (char)c : '_';
    }
    out[o] = 0;
    if (!out[0]) snprintf(out, cap, "n");
}

static void xml_escape_into(const char* in, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = in; *p && o + 8 < cap; p++) {
        if (*p == '&') { memcpy(out + o, "&amp;", 5); o += 5; }
        else if (*p == '<') { memcpy(out + o, "&lt;", 4); o += 4; }
        else if (*p == '>') { memcpy(out + o, "&gt;", 4); o += 4; }
        else if (*p == '"') { memcpy(out + o, "&quot;", 6); o += 6; }
        else out[o++] = *p;
    }
    out[o] = 0;
}

long br_graph_edge_count(const char* json_path)
{
    wgraph_t g;
    if (!wg_load(json_path, &g)) return 0;
    long n = g.ne;
    wg_free(&g);
    return n;
}

long br_graph_dangling_count(const char* json_path)
{
    wgraph_t g;
    if (!wg_load(json_path, &g)) return 0;
    long n = g.nd;
    wg_free(&g);
    return n;
}

/* Costruisce un handle gmb_graph dal modello (per pagerank/louvain). */
static int64_t wg_to_handle(wgraph_t* g)
{
    int64_t h = gmb_graph_create();
    if (h < 0) return -1;
    for (int i = 0; i < g->nn; i++) gmb_graph_add_node(h, g->nodes[i].id);
    for (int i = 0; i < g->ne; i++) gmb_graph_add_edge(h, g->edges[i].from, g->edges[i].to, 1.0);
    return h;
}

int br_graph_write_artifacts(const char* name, const char* graphs_dir)
{
    char json_path[2400];
    snprintf(json_path, sizeof json_path, "%s/%s.json", graphs_dir, name);
    wgraph_t g;
    if (!wg_load(json_path, &g)) return 0;

    /* json normalizzato */
    char* js = wg_to_json(&g);
    if (js) { br_write_file(json_path, js); free(js); }

    /* mermaid */
    {
        sbuf_t b;
        sb_init(&b);
        sb_put(&b, "graph TD\n");
        for (int i = 0; i < g.nn; i++) {
            char sid[512];
            graph_sanitize(g.nodes[i].id, sid, sizeof sid);
            sb_put(&b, "    N");
            sb_put(&b, sid);
            sb_put(&b, "[\"");
            sb_put(&b, g.nodes[i].title ? g.nodes[i].title : "");
            sb_put(&b, "\"]\n");
        }
        for (int i = 0; i < g.ne; i++) {
            char a[512], z[512];
            graph_sanitize(g.edges[i].from, a, sizeof a);
            graph_sanitize(g.edges[i].to, z, sizeof z);
            sb_put(&b, "    N");
            sb_put(&b, a);
            sb_put(&b, " --> N");
            sb_put(&b, z);
            sb_put(&b, "\n");
        }
        char p[2400];
        snprintf(p, sizeof p, "%s/%s.mermaid", graphs_dir, name);
        br_write_file(p, b.p ? b.p : "");
        free(b.p);
    }

    /* graphml */
    {
        sbuf_t b;
        sb_init(&b);
        sb_put(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n");
        sb_put(&b, "  <key id=\"title\" for=\"node\" attr.name=\"title\" attr.type=\"string\"/>\n  <graph id=\"G\" edgedefault=\"directed\">\n");
        for (int i = 0; i < g.nn; i++) {
            char id[1200], title[1200];
            xml_escape_into(g.nodes[i].id, id, sizeof id);
            xml_escape_into(g.nodes[i].title ? g.nodes[i].title : "", title, sizeof title);
            sb_put(&b, "    <node id=\"");
            sb_put(&b, id);
            sb_put(&b, "\"><data key=\"title\">");
            sb_put(&b, title);
            sb_put(&b, "</data></node>\n");
        }
        for (int i = 0; i < g.ne; i++) {
            char a[1200], z[1200];
            xml_escape_into(g.edges[i].from, a, sizeof a);
            xml_escape_into(g.edges[i].to, z, sizeof z);
            sb_put(&b, "    <edge source=\"");
            sb_put(&b, a);
            sb_put(&b, "\" target=\"");
            sb_put(&b, z);
            sb_put(&b, "\"/>\n");
        }
        sb_put(&b, "  </graph>\n</graphml>\n");
        char p[2400];
        snprintf(p, sizeof p, "%s/%s.graphml", graphs_dir, name);
        br_write_file(p, b.p ? b.p : "");
        free(b.p);
    }

    /* stats.json: nodi/edge/dangling/community + top hub + pagerank (calcolati dal runtime) */
    {
        int communities = 0;
        int64_t h = wg_to_handle(&g);
        if (h >= 0) {
            int cn = 0;
            gmb_graph_comp_t* comps = gmb_graph_louvain(h, 1.0, &cn);
            if (comps && cn > 0) {
                /* conta le community distinte */
                int maxc = 0;
                for (int i = 0; i < cn; i++) if (comps[i].component > maxc) maxc = comps[i].component;
                communities = maxc + 1;
                free(comps);
            }
        }
        sbuf_t b;
        sb_init(&b);
        char t[256];
        sb_put(&b, "{\n  \"name\": "); sb_esc(&b, name);
        snprintf(t, sizeof t, ",\n  \"nodes\": %d,\n  \"edges\": %d,\n  \"dangling\": %d,\n  \"communities\": %d,\n",
                 g.nn, g.ne, g.nd, communities);
        sb_put(&b, t);
        /* topHubs */
        sb_put(&b, "  \"topHubs\": [");
        int written = 0;
        for (int i = 0; i < g.nn && written < 10; i++) {
            int deg = 0;
            for (int k = 0; k < g.ne; k++) if (!strcmp(g.edges[k].from, g.nodes[i].id)) deg++;
            if (deg == 0) continue;
            if (written) sb_put(&b, ", ");
            snprintf(t, sizeof t, "{\"node\": \"%s\", \"degree\": %d}", g.nodes[i].id, deg);
            sb_put(&b, t);
            written++;
        }
        sb_put(&b, "],\n  \"pageRankTop\": [");
        if (h >= 0) {
            int pn = 0;
            gmb_graph_pagerank_t* pr = gmb_graph_pagerank(h, 0.85, 100, &pn);
            int top = pn < 10 ? pn : 10;
            for (int i = 0; i < top; i++) {
                if (i) sb_put(&b, ", ");
                snprintf(t, sizeof t, "{\"node\": \"%s\", \"rank\": %.6f}", pr[i].node ? pr[i].node : "", pr[i].rank);
                sb_put(&b, t);
            }
            free(pr);
        }
        sb_put(&b, "],\n  \"edgeTypes\": {");
        /* tipi distinti */
        {
            int first = 1;
            for (int i = 0; i < g.ne; i++) {
                int seen = 0;
                for (int j = 0; j < i; j++) if (!strcmp(g.edges[j].type, g.edges[i].type)) { seen = 1; break; }
                if (seen) continue;
                int cnt = 0;
                for (int j = 0; j < g.ne; j++) if (!strcmp(g.edges[j].type, g.edges[i].type)) cnt++;
                if (!first) sb_put(&b, ", ");
                first = 0;
                sb_esc(&b, g.edges[i].type);
                snprintf(t, sizeof t, ": %d", cnt);
                sb_put(&b, t);
            }
        }
        sb_put(&b, "},\n  \"folders\": ");
        {
            int nfolders = 0;
            gmb_val_t f = gmb_json_parse(g.folders_json ? g.folders_json : "{}");
            if (f.kind == GMB_TYPE_OBJECT) {
                gmb_int_t nk = 0;
                gmb_val_dict_keys_array(f, &nk);
                nfolders = (int)nk;
            }
            snprintf(t, sizeof t, "%d", nfolders);
            sb_put(&b, t);
        }
        sb_put(&b, "\n}\n");
        char p[2400];
        snprintf(p, sizeof p, "%s/%s.stats.json", graphs_dir, name);
        br_write_file(p, b.p ? b.p : "");
        free(b.p);
        if (h >= 0) gmb_graph_free(h);
    }

    /* html: viewer autonomo minimale (JSON embedded) */
    {
        char* js2 = wg_to_json(&g);
        sbuf_t b;
        sb_init(&b);
        sb_put(&b, "<!doctype html><meta charset=\"utf-8\"><title>");
        sb_put(&b, name);
        sb_put(&b, "</title><body style=\"font-family:system-ui;margin:2rem\"><h1>");
        sb_put(&b, name);
        sb_put(&b, "</h1><p>Grafo: ");
        char t[128];
        snprintf(t, sizeof t, "%d nodi, %d edge</p><pre id=\"graph\" style=\"white-space:pre-wrap\"></pre>", g.nn, g.ne);
        sb_put(&b, t);
        sb_put(&b, "<script>const g=");
        sb_put(&b, js2 ? js2 : "{}");
        sb_put(&b, ";document.getElementById('graph').textContent=JSON.stringify({nodes:g.nodes.length,edges:g.edges.length});</script>");
        char p[2400];
        snprintf(p, sizeof p, "%s/%s.html", graphs_dir, name);
        br_write_file(p, b.p ? b.p : "");
        free(b.p);
        free(js2);
    }
    wg_free(&g);
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 * GRAPH → vault Obsidian
 * ══════════════════════════════════════════════════════════════════════════ */

gmb_brain_result_t gmb_brain_tool_graph_export_obsidian(const char* graphs_dir, const char* name, const char* out_dir)
{
    if (!name || !*name) return br_fail("serve il nome del grafo");
    char gp[2400];
    if (graphs_dir && *graphs_dir) snprintf(gp, sizeof gp, "%s/%s.json", graphs_dir, name);
    else {
        char gd[1024];
        gmb_paths_graphs_dir(gd, sizeof gd);
        snprintf(gp, sizeof gp, "%s/%s.json", gd, name);
    }
    wgraph_t g;
    if (!wg_load(gp, &g)) return br_fail("grafo non trovato o illeggibile: %s", gp);
    char dir[2048];
    if (out_dir && *out_dir) snprintf(dir, sizeof dir, "%s", out_dir);
    else snprintf(dir, sizeof dir, "/tmp/%s-obsidian", name);
    gmb_fs_mkdir_p(dir);

    int written = 0;
    for (int i = 0; i < g.nn; i++) {
        char fname[1200];
        graph_sanitize(g.nodes[i].id, fname, sizeof fname);
        char path[2400];
        snprintf(path, sizeof path, "%s/%s.md", dir, fname);
        sbuf_t b;
        sb_init(&b);
        sb_put(&b, "# ");
        sb_put(&b, g.nodes[i].title ? g.nodes[i].title : g.nodes[i].id);
        sb_put(&b, "\n\nid: `");
        sb_put(&b, g.nodes[i].id);
        sb_put(&b, "`\n\n## Collegamenti\n\n");
        for (int k = 0; k < g.ne; k++) {
            if (strcmp(g.edges[k].from, g.nodes[i].id)) continue;
            sb_put(&b, "- [[");
            sb_put(&b, g.edges[k].to);
            sb_put(&b, "]]\n");
        }
        for (int k = 0; k < g.ne; k++) {
            if (strcmp(g.edges[k].to, g.nodes[i].id)) continue;
            sb_put(&b, "- ← [[");
            sb_put(&b, g.edges[k].from);
            sb_put(&b, "]]\n");
        }
        if (br_write_file(path, b.p ? b.p : "")) written++;
        free(b.p);
    }
    int nn = g.nn, ne = g.ne;
    wg_free(&g);
    return br_ok("GRAPH export-obsidian: %d pagine (%d nodi, %d edge) → %s", written, nn, ne, dir);
}

/* ══════════════════════════════════════════════════════════════════════════
 * DIFF (differenze RAG/GRAPH fra due cervelli)
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct { char** v; int n, cap; } strset_t;

static void set_add(strset_t* s, const char* v)
{
    for (int i = 0; i < s->n; i++) if (!strcasecmp(s->v[i], v)) return;
    if (s->n == s->cap) {
        int cap = s->cap ? s->cap * 2 : 64;
        char** nv = (char**)realloc(s->v, (size_t)cap * sizeof(char*));
        if (!nv) return;
        s->v = nv;
        s->cap = cap;
    }
    s->v[s->n++] = strdup(v);
}
static int set_has(strset_t* s, const char* v)
{
    for (int i = 0; i < s->n; i++) if (!strcasecmp(s->v[i], v)) return 1;
    return 0;
}
static void set_free(strset_t* s)
{
    for (int i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    memset(s, 0, sizeof *s);
}

static void source_ids(const char* name, strset_t* out)
{
    char p[2048];
    if (!gmb_brain_reg_rag_db(name, p, sizeof p)) return;
    sqlite3* db = br_sql_open(p, 1);
    if (!db) return;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "SELECT source_id FROM vec_documents", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* s = (const char*)sqlite3_column_text(st, 0);
            if (s) set_add(out, s);
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
}

static void node_ids(const char* name, strset_t* out)
{
    char p[2400];
    if (!gmb_brain_reg_graph_json(name, p, sizeof p)) return;
    wgraph_t g;
    if (!wg_load(p, &g)) return;
    for (int i = 0; i < g.nn; i++) set_add(out, g.nodes[i].id);
    wg_free(&g);
}

gmb_brain_result_t gmb_brain_tool_diff(const char* a, const char* b, int as_json)
{
    if (!a || !*a || !b || !*b) return br_fail("uso: BRAIN DIFF <a> <b> [--json]");
    gmb_brain_rec_t ri;
    if (!gmb_brain_reg_info(a, &ri)) return br_fail("cervello '%s' non registrato", a);
    if (!gmb_brain_reg_info(b, &ri)) return br_fail("cervello '%s' non registrato", b);

    strset_t ra, rb, na, nb;
    memset(&ra, 0, sizeof ra); memset(&rb, 0, sizeof rb);
    memset(&na, 0, sizeof na); memset(&nb, 0, sizeof nb);
    source_ids(a, &ra);
    source_ids(b, &rb);
    node_ids(a, &na);
    node_ids(b, &nb);

    int common = 0, only_a = 0, only_b = 0, nonly_a = 0, nonly_b = 0;
    for (int i = 0; i < ra.n; i++) { if (set_has(&rb, ra.v[i])) common++; else only_a++; }
    for (int i = 0; i < rb.n; i++) if (!set_has(&ra, rb.v[i])) only_b++;
    for (int i = 0; i < na.n; i++) if (!set_has(&nb, na.v[i])) nonly_a++;
    for (int i = 0; i < nb.n; i++) if (!set_has(&na, nb.v[i])) nonly_b++;

    char* text = (char*)malloc(16384);
    if (!text) {
        set_free(&ra); set_free(&rb); set_free(&na); set_free(&nb);
        return br_fail("memoria esaurita");
    }
    size_t off = 0;
    if (as_json) {
        off += (size_t)snprintf(text + off, 16384 - off,
            "{\n  \"a\": \"%s\",\n  \"b\": \"%s\",\n  \"rag\": {\"a_documents\": %d, \"b_documents\": %d, \"common\": %d, \"only_a\": %d, \"only_b\": %d, \"sample_only_a\": [",
            a, b, ra.n, rb.n, common, only_a, only_b);
        int k = 0;
        for (int i = 0; i < ra.n && k < 10; i++) {
            if (set_has(&rb, ra.v[i])) continue;
            off += (size_t)snprintf(text + off, 16384 - off, "%s\"%s\"", k ? ", " : "", ra.v[i]);
            k++;
        }
        off += (size_t)snprintf(text + off, 16384 - off, "], \"sample_only_b\": [");
        k = 0;
        for (int i = 0; i < rb.n && k < 10; i++) {
            if (set_has(&ra, rb.v[i])) continue;
            off += (size_t)snprintf(text + off, 16384 - off, "%s\"%s\"", k ? ", " : "", rb.v[i]);
            k++;
        }
        off += (size_t)snprintf(text + off, 16384 - off,
            "]},\n  \"graph\": {\"a_nodes\": %d, \"b_nodes\": %d, \"only_a\": %d, \"only_b\": %d}\n}",
            na.n, nb.n, nonly_a, nonly_b);
    } else {
        off += (size_t)snprintf(text + off, 16384 - off, "BRAIN DIFF — %s vs %s\n", a, b);
        off += (size_t)snprintf(text + off, 16384 - off,
            "  RAG:   %d vs %d documenti · comuni %d · solo-A %d · solo-B %d\n", ra.n, rb.n, common, only_a, only_b);
        int k = 0;
        for (int i = 0; i < ra.n && k < 5; i++) {
            if (set_has(&rb, ra.v[i])) continue;
            off += (size_t)snprintf(text + off, 16384 - off, "    - solo in %s: %s\n", a, ra.v[i]);
            k++;
        }
        k = 0;
        for (int i = 0; i < rb.n && k < 5; i++) {
            if (set_has(&ra, rb.v[i])) continue;
            off += (size_t)snprintf(text + off, 16384 - off, "    + solo in %s: %s\n", b, rb.v[i]);
            k++;
        }
        off += (size_t)snprintf(text + off, 16384 - off,
            "  GRAPH: %d vs %d nodi · solo-A %d · solo-B %d\n", na.n, nb.n, nonly_a, nonly_b);
        k = 0;
        for (int i = 0; i < na.n && k < 5; i++) {
            if (set_has(&nb, na.v[i])) continue;
            off += (size_t)snprintf(text + off, 16384 - off, "    - solo in %s: %s\n", a, na.v[i]);
            k++;
        }
        k = 0;
        for (int i = 0; i < nb.n && k < 5; i++) {
            if (set_has(&na, nb.v[i])) continue;
            off += (size_t)snprintf(text + off, 16384 - off, "    + solo in %s: %s\n", b, nb.v[i]);
            k++;
        }
        if (off && text[off - 1] == '\n') text[off - 1] = 0;
    }
    set_free(&ra); set_free(&rb); set_free(&na); set_free(&nb);
    gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = text;
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * DOCTOR (VERIFY + fix non distruttivi + riverifica)
 * ══════════════════════════════════════════════════════════════════════════ */

gmb_brain_result_t gmb_brain_tool_doctor(const char* name)
{
    if (!name || !*name) return br_fail("serve il nome del cervello");
    gmb_brain_result_t before = gmb_brain_tool_verify(name, 0);
    int clean = before.ok;
    char* before_text = before.text;
    free(before.error);
    if (clean) {
        char* text = (char*)malloc(strlen(before_text ? before_text : "") + 128);
        if (!text) { free(before_text); return br_fail("memoria esaurita"); }
        sprintf(text, "BRAIN DOCTOR — %s: nessun problema\n%s", name, before_text ? before_text : "");
        free(before_text);
        gmb_brain_result_t r; r.ok = 1; r.error = NULL; r.text = text;
        return r;
    }

    sbuf_t out;
    sb_init(&out);
    char t[512];
    snprintf(t, sizeof t, "BRAIN DOCTOR — %s: fix applicati\n", name);
    sb_put(&out, t);

    char p[2048];
    if (gmb_brain_reg_rag_db(name, p, sizeof p)) {
        sqlite3* db = br_sql_open(p, 0);
        if (db) {
            int ok1 = br_sql_exec(db, "PRAGMA optimize");
            int ok2 = br_sql_exec(db, "INSERT INTO fts_chunks(fts_chunks) VALUES('rebuild')");
            sqlite3_close(db);
            db = br_sql_open(p, 0);
            int ok3 = db ? br_sql_exec(db, "VACUUM") : 0;
            if (db) sqlite3_close(db);
            if (ok1 && ok2 && ok3) sb_put(&out, "  RAG: PRAGMA optimize + FTS5 rebuild + VACUUM\n");
            else sb_put(&out, "  RAG: fix parzialmente applicato\n");
        } else {
            snprintf(t, sizeof t, "  RAG: fix non applicabile (db non apribile: %s)\n", p);
            sb_put(&out, t);
        }
    }
    char gp[2400];
    if (gmb_brain_reg_graph_json(name, gp, sizeof gp)) {
        wgraph_t g;
        if (wg_load(gp, &g)) {
            int nd = g.nd;
            for (int i = 0; i < g.nd; i++) free(g.dangling[i]);
            g.nd = 0;
            char* js = wg_to_json(&g);
            if (js) { br_write_file(gp, js); free(js); }
            wg_free(&g);
            char gdir[1024];
            gmb_paths_graphs_dir(gdir, sizeof gdir);
            br_graph_write_artifacts(name, gdir);
            snprintf(t, sizeof t, "  GRAPH: rimossi %d dangling, export ricalcolati\n", nd);
            sb_put(&out, t);
        }
    }
    gmb_brain_result_t after = gmb_brain_tool_verify(name, 0);
    sb_put(&out, after.ok ? "riverifica: OK" : "riverifica: PROBLEMI RESIDUI\n");
    if (!after.ok && after.text) sb_put(&out, after.text);
    free(before_text);
    free(after.text);
    free(after.error);
    gmb_brain_result_t r;
    r.ok = after.ok;
    r.text = out.p;
    r.error = after.ok ? NULL : strdup("problemi residui dopo i fix");
    return r;
}
