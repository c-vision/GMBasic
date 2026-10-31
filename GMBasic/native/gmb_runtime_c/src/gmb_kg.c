/* gmb_kg.c — KnowledgeGraph singleton (parity C# KnowledgeGraphRuntime) per i backend C
 * (usato dall'emissione riusabile dal VM/C). Wrapper su gmb_graph con
 * INIT lazy. Query: "node" → {"node",neighbors}; "path a->b"|"a → b" → {from,to,path}.
 * Ritorni: add_* int 1/0; query char* JSON (heap). Mai stub. */
#include "gmb_runtime.h"
#include "gmb_graph.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int64_t g_kg_h = -1;
static int g_kg_inited = 0;
static int64_t kg_handle(void){
    if (!g_kg_inited) { gmb_graph_init(); g_kg_inited = 1; }
    if (g_kg_h < 0) g_kg_h = gmb_graph_create();
    return g_kg_h;
}
int gmb_kg_add_node(const char* node){ int64_t h = kg_handle(); if (h < 0 || !node || !*node) return 0; while (*node == '/') node++; int r = gmb_graph_add_node(h, node); return (r == -1 || r == 0) ? 1 : 0; }
int gmb_kg_add_edge(const char* from, const char* to){
    int64_t h = kg_handle(); if (h < 0 || !from || !*from || !to || !*to) return 0;
    while (*from == '/') from++; while (*to == '/') to++;
    gmb_graph_add_node(h, from); gmb_graph_add_node(h, to);
    if (gmb_graph_has_edge(h, from, to)) return 1;
    return gmb_graph_add_edge(h, from, to, 1.0) == 0 ? 1 : 0;
}
static void trim(char* s){ char* e = s + strlen(s); while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\r')) *--e=0; char* b=s; while(*b==' '||*b=='\t') b++; if(b!=s) memmove(s,b,strlen(b)+1); }
char* gmb_kg_query(const char* q){
    int64_t h = kg_handle();
    if (h < 0) return strdup("{\"error\":\"grafo non disponibile\"}");
    if (!q || !*q) return strdup("{\"error\":\"query vuota\"}");
    if (strncasecmp(q, "path ", 5) == 0) {
        char spec[512]; snprintf(spec, sizeof(spec), "%s", q + 5);
        char* sep = strstr(spec, "->"); size_t skiplen = 2;
        if (!sep) { char* u = strstr(spec, "\xe2\x86\x92"); if (u) { sep = u; skiplen = 3; } }
        if (!sep) return strdup("{\"error\":\"path richiede 'path a->b'\"}");
        char a[256], b[256];
        /* `a` = la parte PRIMA del separatore (prima si copiava l'intera spec →
         * from="alice->carol"); `b` = la parte DOPO (sep+skiplen, non sep+strlen(skip)
         * che per la freccia UTF-8 saltava 3 byte in più). */
        snprintf(a, sizeof(a), "%.*s", (int)(sep - spec), spec);
        snprintf(b, sizeof(b), "%s", sep + skiplen);
        trim(a); trim(b);
        int n = 0; char** path = gmb_graph_shortest_path(h, a, b, &n);
        char* buf = (char*)malloc(160 + (size_t)(n ? n : 0) * 64);
        if (buf) { size_t len = (size_t)snprintf(buf, 160, "{\"from\":\"%s\",\"to\":\"%s\",\"path\":[", a, b); for (int i = 0; i < n; i++) len += (size_t)snprintf(buf + len, 64, "%s\"%s\"", i ? "," : "", path[i]); snprintf(buf + len, 8, "]}"); }
        if (path) free(path);
        return buf ? buf : strdup("{}");
    }
    char node[256]; snprintf(node, sizeof(node), "%s", q); trim(node); { char* s=node; while(*s=='/') s++; memmove(node,s,strlen(s)+1); }
    if (!gmb_graph_has_node(h, node)) { char* tmp=(char*)malloc(strlen(node)+64); if(tmp) sprintf(tmp,"{\"node\":\"%s\",\"error\":\"nodo non trovato\"}",node); return tmp?tmp:strdup("{}"); }
    int nn = 0; gmb_graph_edge_t* nb = gmb_graph_neighbors(h, node, &nn);
    /* incident: archi che TOCCANO il nodo (src o tgt) — parità C# GraphManager.Incident. */
    int ne = 0; gmb_graph_edge_full_t* el = gmb_graph_edge_list(h, &ne);
    int ninc = 0;
    for (int i = 0; i < ne; i++)
        if ((el[i].from && strcmp(el[i].from, node) == 0) || (el[i].to && strcmp(el[i].to, node) == 0)) ninc++;
    /* neighbors: parità C# GraphManager.Neighbors = uscenti (target) POI entranti (source),
     * Distinct in ordine di edge. gmb_graph_neighbors dà solo gli uscenti → si aggiungono
     * gli entranti non già presenti (prima mancavano: KnowledgeGraphDemo, "Lexer" su Parser). */
    const char* names[1024]; int nnames = 0;
    for (int i = 0; i < nn && nnames < 1024; i++) names[nnames++] = nb[i].node;
    for (int i = 0; i < ne && nnames < 1024; i++) {
        if (!el[i].to || strcmp(el[i].to, node) != 0 || !el[i].from) continue;
        int dup = 0;
        for (int j = 0; j < nnames && !dup; j++) if (strcmp(names[j], el[i].from) == 0) dup = 1;
        if (!dup) names[nnames++] = el[i].from;
    }
    char* buf = (char*)malloc(160 + (size_t)nnames * 64 + (size_t)ninc * 160);
    if (buf) {
        size_t len = (size_t)snprintf(buf, 160, "{\"node\":\"%s\",\"neighbors\":[", node);
        for (int i = 0; i < nnames; i++) len += (size_t)snprintf(buf + len, 64, "%s\"%s\"", i ? "," : "", names[i]);
        len += (size_t)snprintf(buf + len, 20, "],\"incident\":[");
        int w = 0;
        for (int i = 0; i < ne; i++) {
            if (!((el[i].from && strcmp(el[i].from, node) == 0) || (el[i].to && strcmp(el[i].to, node) == 0))) continue;
            len += (size_t)snprintf(buf + len, 160, "%s{\"src\":\"%s\",\"tgt\":\"%s\",\"w\":%g}",
                                    w++ ? "," : "", el[i].from ? el[i].from : "", el[i].to ? el[i].to : "", el[i].weight);
        }
        snprintf(buf + len, 8, "]}");
    }
    if (nb) free(nb);
    if (el) free(el);
    return buf ? buf : strdup("{}");
}
/* Prende possesso di un char* heap (copiato in gmb_string_t e liberato) — per emissione C99. */
gmb_string_t gmb_string_take(char* heap){ gmb_string_t s = gmb_string_alloc(heap ? heap : ""); if (heap) free(heap); return s; }
