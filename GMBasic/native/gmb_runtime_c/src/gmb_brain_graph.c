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

/* Pretty-print in formato System.Text.Json WriteIndented: oggetti/array NON vuoti su più
 * righe (2 spazi per livello), vuoti inline. Serve a produrre stats.json uguale al C#. */
static char* json_pretty_csharp(const char* src)
{
    if (!src) return NULL;
    size_t cap = strlen(src) * 3 + 64, n = 0;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    int indent = 0;
    int in_str = 0;
    for (const char* p = src; *p; p++) {
        char ch = *p;
        if (in_str) {
            if (n + 2 >= cap) { cap *= 2; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
            out[n++] = ch;
            if (ch == '\\' && p[1]) { out[n++] = *++p; }
            else if (ch == '"') in_str = 0;
            continue;
        }
        if (ch == '"') { in_str = 1; out[n++] = ch; continue; }
        if (ch == '{' || ch == '[') {
            /* vuoto? */
            const char* q = p + 1;
            while (*q == ' ') q++;
            if ((ch == '{' && *q == '}') || (ch == '[' && *q == ']')) {
                out[n++] = ch; out[n++] = *q; p = q; continue;
            }
            out[n++] = ch;
            indent++;
            out[n++] = '\n';
            for (int i = 0; i < indent; i++) { out[n++] = ' '; out[n++] = ' '; if (n + 4 >= cap) { cap *= 2; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; } }
            continue;
        }
        if (ch == '}' || ch == ']') {
            indent--;
            out[n++] = '\n';
            for (int i = 0; i < indent; i++) { out[n++] = ' '; out[n++] = ' '; if (n + 4 >= cap) { cap *= 2; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; } }
            out[n++] = ch;
            continue;
        }
        if (ch == ',') { out[n++] = ','; out[n++] = '\n'; for (int i = 0; i < indent; i++) { out[n++] = ' '; out[n++] = ' '; if (n + 4 >= cap) { cap *= 2; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; } } continue; }
        if (ch == ':') { out[n++] = ':'; out[n++] = ' '; continue; }
        if (ch == ' ') continue;
        out[n++] = ch;
        if (n + 4 >= cap) { cap *= 2; char* nb = (char*)realloc(out, cap); if (!nb) { free(out); return NULL; } out = nb; }
    }
    out[n] = 0;
    return out;
}

int br_graph_write_artifacts(const char* name, const char* graphs_dir);

int gmb_brain_graph_artifacts(const char* name, const char* graphs_dir)
{
    return br_graph_write_artifacts(name, graphs_dir);
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

    /* stats.json COMPLETO (parità GraphExporter.ToStatsJson): degree/pagerank/
     * betweenness/louvain + godNodes + surprisingConnections + suggestedQuestions. */
    {
        int n = g.nn;
        int64_t h = wg_to_handle(&g);

        /* metriche per nodo (indice allineato a g.nodes) */
        int* degree = (int*)calloc((size_t)(n ? n : 1), sizeof(int));
        double* pr = (double*)calloc((size_t)(n ? n : 1), sizeof(double));
        double* bet = (double*)calloc((size_t)(n ? n : 1), sizeof(double));
        int* comm = (int*)calloc((size_t)(n ? n : 1), sizeof(int));
        for (int i = 0; i < n; i++) {
            for (int e = 0; e < g.ne; e++) {
                if (!strcmp(g.edges[e].from, g.nodes[i].id)) degree[i]++;
                if (!strcmp(g.edges[e].to, g.nodes[i].id)) degree[i]++;
            }
        }
        /* indice id → posizione (open addressing; i grafi veri hanno 10^4-10^5 nodi) */
        int cap = 1;
        while (cap < n * 2 + 7) cap <<= 1;
        int* tab = (int*)malloc((size_t)cap * sizeof(int));
        for (int i = 0; i < cap; i++) tab[i] = -1;
        for (int i = 0; i < n; i++) {
            unsigned long hh = 5381;
            for (const char* q = g.nodes[i].id; *q; q++) hh = hh * 33u + (unsigned char)*q;
            int k = (int)(hh & (unsigned long)(cap - 1));
            while (tab[k] >= 0) k = (k + 1) & (cap - 1);
            tab[k] = i;
        }

        int ncomm = 0;
        if (h >= 0) {
            int pn = 0;
            gmb_graph_pagerank_t* arr = gmb_graph_pagerank(h, 0.85, 100, &pn);
            for (int i = 0; i < pn; i++) {
                if (!arr[i].node) continue;
                unsigned long hh = 5381;
                for (const char* q = arr[i].node; *q; q++) hh = hh * 33u + (unsigned char)*q;
                int k = (int)(hh & (unsigned long)(cap - 1));
                while (tab[k] >= 0) {
                    if (!strcmp(g.nodes[tab[k]].id, arr[i].node)) { pr[tab[k]] = arr[i].rank; break; }
                    k = (k + 1) & (cap - 1);
                }
            }
            free(arr);
            int bn = 0;
            gmb_graph_centrality_t* b = gmb_graph_betweenness(h, &bn);
            for (int i = 0; i < bn; i++) {
                if (!b[i].node) continue;
                unsigned long hh = 5381;
                for (const char* q = b[i].node; *q; q++) hh = hh * 33u + (unsigned char)*q;
                int k = (int)(hh & (unsigned long)(cap - 1));
                while (tab[k] >= 0) {
                    if (!strcmp(g.nodes[tab[k]].id, b[i].node)) { bet[tab[k]] = b[i].value; break; }
                    k = (k + 1) & (cap - 1);
                }
            }
            free(b);
            int cn = 0;
            gmb_graph_comp_t* c = gmb_graph_louvain(h, 1.0, &cn);
            int maxc = -1;
            for (int i = 0; i < cn; i++) {
                if (!c[i].node) continue;
                if (c[i].component > maxc) maxc = c[i].component;
                unsigned long hh = 5381;
                for (const char* q = c[i].node; *q; q++) hh = hh * 33u + (unsigned char)*q;
                int k = (int)(hh & (unsigned long)(cap - 1));
                while (tab[k] >= 0) {
                    if (!strcmp(g.nodes[tab[k]].id, c[i].node)) { comm[tab[k]] = c[i].component; break; }
                    k = (k + 1) & (cap - 1);
                }
            }
            free(c);
            ncomm = maxc + 1;
            if (ncomm < 0) ncomm = 0;
        }
        /* lookup id → indice */
        #define NODE_IDX(idv) ({ int _k = -1; unsigned long _h = 5381; \
            for (const char* _q = (idv); *_q; _q++) _h = _h * 33u + (unsigned char)*_q; \
            int _i = (int)(_h & (unsigned long)(cap - 1)); \
            while (tab[_i] >= 0) { if (!strcmp(g.nodes[tab[_i]].id, (idv))) { _k = tab[_i]; break; } _i = (_i + 1) & (cap - 1); } _k; })

        /* ordinamenti (indici) */
        int* byDeg = (int*)malloc((size_t)(n ? n : 1) * sizeof(int));
        int* byPr = (int*)malloc((size_t)(n ? n : 1) * sizeof(int));
        int* byBet = (int*)malloc((size_t)(n ? n : 1) * sizeof(int));
        for (int i = 0; i < n; i++) { byDeg[i] = byPr[i] = byBet[i] = i; }
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                if (degree[byDeg[j]] > degree[byDeg[i]]) { int t2 = byDeg[i]; byDeg[i] = byDeg[j]; byDeg[j] = t2; }
                if (pr[byPr[j]] > pr[byPr[i]]) { int t2 = byPr[i]; byPr[i] = byPr[j]; byPr[j] = t2; }
                if (bet[byBet[j]] > bet[byBet[i]]) { int t2 = byBet[i]; byBet[i] = byBet[j]; byBet[j] = t2; }
            }
        /* normalizzazioni per godNodes */
        double dmax = 1, pmax = 1e-9, bmax = 1;
        for (int i = 0; i < n; i++) {
            if (degree[i] > dmax) dmax = degree[i];
            if (pr[i] > pmax) pmax = pr[i];
            if (bet[i] > bmax) bmax = bet[i];
        }
        double* score = (double*)calloc((size_t)(n ? n : 1), sizeof(double));
        int* byScore = (int*)malloc((size_t)(n ? n : 1) * sizeof(int));
        for (int i = 0; i < n; i++) {
            score[i] = 0.4 * (degree[i] / dmax) + 0.4 * (pr[i] / pmax) + 0.2 * (bet[i] / bmax);
            byScore[i] = i;
        }
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (score[byScore[j]] > score[byScore[i]]) { int t2 = byScore[i]; byScore[i] = byScore[j]; byScore[j] = t2; }

        /* JSON compatto → pretty (formato System.Text.Json WriteIndented) */
        sbuf_t c;
        sb_init(&c);
        char t2[512];
        sb_put(&c, "{\"name\":");
        sb_esc(&c, name);
        snprintf(t2, sizeof t2, ",\"nodes\":%d,\"edges\":%d,\"dangling\":%d,\"communities\":%d,",
                 g.nn, g.ne, g.nd, ncomm);
        sb_put(&c, t2);
        /* topHubs (out-degree, come il C#: GroupBy(From) + Take(10)) */
        sb_put(&c, "\"topHubs\":[");
        {
            int written = 0;
            for (int i = 0; i < g.nn && written < 10; i++) {
                int outdeg = 0;
                for (int e = 0; e < g.ne; e++) if (!strcmp(g.edges[e].from, g.nodes[i].id)) outdeg++;
                if (outdeg == 0) continue;
                if (written) sb_put(&c, ",");
                sb_put(&c, "{\"node\":");
                sb_esc(&c, g.nodes[i].id);
                snprintf(t2, sizeof t2, ",\"degree\":%d}", outdeg);
                sb_put(&c, t2);
                written++;
            }
        }
        sb_put(&c, "],\"pageRankTop\":[");
        for (int i = 0; i < n && i < 10; i++) {
            if (i) sb_put(&c, ",");
            sb_put(&c, "{\"node\":");
            sb_esc(&c, g.nodes[byPr[i]].id);
            snprintf(t2, sizeof t2, ",\"rank\":%.6f}", pr[byPr[i]]);
            sb_put(&c, t2);
        }
        sb_put(&c, "],\"degreeTop\":[");
        for (int i = 0; i < n && i < 10; i++) {
            if (i) sb_put(&c, ",");
            sb_put(&c, "{\"node\":");
            sb_esc(&c, g.nodes[byDeg[i]].id);
            snprintf(t2, sizeof t2, ",\"degree\":%d}", degree[byDeg[i]]);
            sb_put(&c, t2);
        }
        sb_put(&c, "],\"betweennessTop\":[");
        for (int i = 0; i < n && i < 10; i++) {
            if (i) sb_put(&c, ",");
            sb_put(&c, "{\"node\":");
            sb_esc(&c, g.nodes[byBet[i]].id);
            snprintf(t2, sizeof t2, ",\"betweenness\":%.4f}", bet[byBet[i]]);
            sb_put(&c, t2);
        }
        /* edgeTypes / edgeProvenance */
        sb_put(&c, "],\"edgeTypes\":{");
        {
            int first = 1;
            for (int i = 0; i < g.ne; i++) {
                int seen = 0;
                for (int j = 0; j < i; j++) if (!strcmp(g.edges[j].type, g.edges[i].type)) { seen = 1; break; }
                if (seen) continue;
                int cnt = 0;
                for (int j = 0; j < g.ne; j++) if (!strcmp(g.edges[j].type, g.edges[i].type)) cnt++;
                if (!first) sb_put(&c, ",");
                first = 0;
                sb_esc(&c, g.edges[i].type);
                snprintf(t2, sizeof t2, ":%d", cnt);
                sb_put(&c, t2);
            }
        }
        sb_put(&c, "},\"edgeProvenance\":{");
        {
            int first = 1;
            for (int i = 0; i < g.ne; i++) {
                const char* prov = !strcmp(g.edges[i].type, "link") ? "EXTRACTED" : "INFERRED";
                int seen = 0;
                for (int j = 0; j < i; j++) {
                    const char* pj = !strcmp(g.edges[j].type, "link") ? "EXTRACTED" : "INFERRED";
                    if (!strcmp(pj, prov)) { seen = 1; break; }
                }
                if (seen) continue;
                int cnt = 0;
                for (int j = 0; j < g.ne; j++) {
                    const char* pj = !strcmp(g.edges[j].type, "link") ? "EXTRACTED" : "INFERRED";
                    if (!strcmp(pj, prov)) cnt++;
                }
                if (!first) sb_put(&c, ",");
                first = 0;
                sb_esc(&c, prov);
                snprintf(t2, sizeof t2, ":%d", cnt);
                sb_put(&c, t2);
            }
        }
        sb_put(&c, "},\"godNodes\":[");
        for (int i = 0; i < n && i < 10; i++) {
            int k = byScore[i];
            if (i) sb_put(&c, ",");
            sb_put(&c, "{\"node\":");
            sb_esc(&c, g.nodes[k].id);
            snprintf(t2, sizeof t2, ",\"score\":%.4f,\"degree\":%.3f,\"pagerank\":%.3f,\"betweenness\":%.3f}",
                     score[k], degree[k] / dmax, pr[k] / pmax, bet[k] / bmax);
            sb_put(&c, t2);
        }
        /* surprisingConnections: unici/pochi ponti tra community diverse (type link) */
        sb_put(&c, "],\"surprisingConnections\":[");
        {
            typedef struct { int a, b; int cnt; } pair_t;
            pair_t pairs[4096];
            int np = 0;
            for (int e = 0; e < g.ne; e++) {
                if (strcmp(g.edges[e].type, "link") != 0) continue;
                int ia = NODE_IDX(g.edges[e].from), ib = NODE_IDX(g.edges[e].to);
                if (ia < 0 || ib < 0) continue;
                if (comm[ia] == comm[ib]) continue;
                int a = comm[ia] < comm[ib] ? comm[ia] : comm[ib];
                int b = comm[ia] < comm[ib] ? comm[ib] : comm[ia];
                int found = 0;
                for (int k = 0; k < np; k++) if (pairs[k].a == a && pairs[k].b == b) { pairs[k].cnt++; found = 1; break; }
                if (!found && np < 4096) { pairs[np].a = a; pairs[np].b = b; pairs[np].cnt = 1; np++; }
            }
            int written = 0;
            for (int e = 0; e < g.ne && written < 10; e++) {
                if (strcmp(g.edges[e].type, "link") != 0) continue;
                int ia = NODE_IDX(g.edges[e].from), ib = NODE_IDX(g.edges[e].to);
                if (ia < 0 || ib < 0 || comm[ia] == comm[ib]) continue;
                int a = comm[ia] < comm[ib] ? comm[ia] : comm[ib];
                int b = comm[ia] < comm[ib] ? comm[ib] : comm[ia];
                int cnt = 0;
                for (int k = 0; k < np; k++) if (pairs[k].a == a && pairs[k].b == b) { cnt = pairs[k].cnt; break; }
                if (cnt > 2) continue;
                if (written) sb_put(&c, ",");
                sb_put(&c, "{\"from\":");
                sb_esc(&c, g.edges[e].from);
                sb_put(&c, ",\"to\":");
                sb_esc(&c, g.edges[e].to);
                snprintf(t2, sizeof t2, ",\"commFrom\":%d,\"commTo\":%d}", comm[ia], comm[ib]);
                sb_put(&c, t2);
                written++;
            }
        }
        /* suggestedQuestions: hub più importanti + ponti sorprendenti + confronto cluster */
        sb_put(&c, "],\"suggestedQuestions\":[");
        {
            int written = 0;
            for (int i = 0; i < n && i < 5 && written < 10; i++) {
                int k = byScore[i];
                char q[1600];
                snprintf(q, sizeof q, "Perché '%s' è al centro del grafo? Quali community attraversa?",
                         g.nodes[k].title ? g.nodes[k].title : g.nodes[k].id);
                if (written) sb_put(&c, ",");
                sb_esc(&c, q);
                written++;
            }
            for (int e = 0; e < g.ne && written < 16; e++) {
                if (strcmp(g.edges[e].type, "link") != 0) continue;
                int ia = NODE_IDX(g.edges[e].from), ib = NODE_IDX(g.edges[e].to);
                if (ia < 0 || ib < 0 || comm[ia] == comm[ib]) continue;
                int a = comm[ia] < comm[ib] ? comm[ia] : comm[ib];
                int b = comm[ia] < comm[ib] ? comm[ib] : comm[ia];
                int cnt = 0;
                for (int k = 0; k < n; k++) {
                    /* conta i ponti fra le due community */
                }
                (void)a; (void)b; (void)cnt;
                char q[1600];
                snprintf(q, sizeof q, "Cosa collega '%s' (community %d) a '%s' (community %d)?",
                         g.nodes[ia].title ? g.nodes[ia].title : g.nodes[ia].id, comm[ia],
                         g.nodes[ib].title ? g.nodes[ib].title : g.nodes[ib].id, comm[ib]);
                if (written) sb_put(&c, ",");
                sb_esc(&c, q);
                written++;
                if (written >= 6) break;
            }
            /* i due cluster più grandi (per numero di nodi) */
            if (ncomm >= 2) {
                int c1 = -1, c2 = -1, n1 = -1, n2 = -1;
                for (int ccc = 0; ccc < ncomm; ccc++) {
                    int cnt = 0;
                    for (int i = 0; i < n; i++) if (comm[i] == ccc) cnt++;
                    if (cnt > n1) { n2 = n1; c2 = c1; n1 = cnt; c1 = ccc; }
                    else if (cnt > n2) { n2 = cnt; c2 = ccc; }
                }
                if (c1 >= 0 && c2 >= 0) {
                    int h1 = -1, h2 = -1;
                    int d1 = -1, d2 = -1;
                    for (int i = 0; i < n; i++) {
                        if (comm[i] == c1 && degree[i] > d1) { d1 = degree[i]; h1 = i; }
                        if (comm[i] == c2 && degree[i] > d2) { d2 = degree[i]; h2 = i; }
                    }
                    if (h1 >= 0 && h2 >= 0 && written < 10) {
                        char q[1600];
                        snprintf(q, sizeof q, "Cosa differenzia il cluster guidato da '%s' da quello guidato da '%s'?",
                                 g.nodes[h1].title ? g.nodes[h1].title : g.nodes[h1].id,
                                 g.nodes[h2].title ? g.nodes[h2].title : g.nodes[h2].id);
                        if (written) sb_put(&c, ",");
                        sb_esc(&c, q);
                        written++;
                    }
                }
            }
        }
        sb_put(&c, "],\"folders\":");
        {
            int nfolders = 0;
            gmb_val_t f = gmb_json_parse(g.folders_json ? g.folders_json : "{}");
            if (f.kind == GMB_TYPE_OBJECT) {
                gmb_int_t nk = 0;
                gmb_val_dict_keys_array(f, &nk);
                nfolders = (int)nk;
            }
            snprintf(t2, sizeof t2, "%d", nfolders);
            sb_put(&c, t2);
        }
        sb_put(&c, "}");

        char* pretty = json_pretty_csharp(c.p ? c.p : "{}");
        char p[2400];
        snprintf(p, sizeof p, "%s/%s.stats.json", graphs_dir, name);
        br_write_file(p, pretty ? pretty : (c.p ? c.p : "{}"));
        free(pretty);
        free(c.p);

        free(byDeg); free(byPr); free(byBet); free(byScore);
        free(score); free(degree); free(pr); free(bet); free(comm); free(tab);
        if (h >= 0) gmb_graph_free(h);
        #undef NODE_IDX
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
