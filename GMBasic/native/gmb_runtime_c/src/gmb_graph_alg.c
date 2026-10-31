/* ============================================================================
 * gmb_graph_alg.c — algoritmi aggiuntivi su gmb_graph (G4): MST (Kruskal),
 * max-flow (Edmonds-Karp), community (Louvain). Usa SOLO l'API pubblica di
 * gmb_graph.h (nessun accesso agli interni) → una sola sorgente condivisa.
 * ============================================================================ */
#include "gmb_graph.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int ga_index(char** names, int n, const char* s)
{
    for (int i = 0; i < n; i++) if (names[i] && strcmp(names[i], s) == 0) return i;
    return -1;
}

/* ───────────────────────────── MST (Kruskal) ───────────────────────────── */

typedef struct { int a, b; double w; } ga_edge;

static int ga_cmp_edge(const void* x, const void* y)
{
    double dx = ((const ga_edge*)x)->w, dy = ((const ga_edge*)y)->w;
    return dx < dy ? -1 : (dx > dy ? 1 : 0);
}

static int ga_uf_find(int* p, int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }

gmb_graph_edge_full_t* gmb_graph_mst(int64_t h, int* out_len)
{
    if (out_len) *out_len = 0;
    int n = 0, m = 0;
    char** names = gmb_graph_nodes(h, &n);
    if (!names || n == 0) { free(names); return NULL; }
    gmb_graph_edge_full_t* el = gmb_graph_edge_list(h, &m);
    if (!el || m == 0) { free(names); free(el); return NULL; }

    /* lista non orientata unica (i<j), peso minimo tra archi paralleli/opposti */
    ga_edge* es = (ga_edge*)malloc((size_t)m * sizeof(ga_edge));
    if (!es) { free(names); free(el); return NULL; }
    int ne = 0;
    for (int k = 0; k < m; k++) {
        int a = ga_index(names, n, el[k].from), b = ga_index(names, n, el[k].to);
        if (a < 0 || b < 0 || a == b) continue;
        if (a > b) { int t = a; a = b; b = t; }
        int found = -1;
        for (int q = 0; q < ne; q++) if (es[q].a == a && es[q].b == b) { found = q; break; }
        if (found >= 0) { if (el[k].weight < es[found].w) es[found].w = el[k].weight; }
        else { es[ne].a = a; es[ne].b = b; es[ne].w = el[k].weight; ne++; }
    }
    free(el);
    if (ne == 0) { free(names); free(es); return NULL; }
    qsort(es, (size_t)ne, sizeof(ga_edge), ga_cmp_edge);

    int* par = (int*)malloc((size_t)n * sizeof(int));
    gmb_graph_edge_full_t* out = (gmb_graph_edge_full_t*)calloc((size_t)n, sizeof(gmb_graph_edge_full_t));
    if (!par || !out) { free(names); free(es); free(par); free(out); return NULL; }
    for (int i = 0; i < n; i++) par[i] = i;
    int cnt = 0;
    for (int q = 0; q < ne; q++) {
        int ra = ga_uf_find(par, es[q].a), rb = ga_uf_find(par, es[q].b);
        if (ra == rb) continue;
        par[ra] = rb;
        out[cnt].from = names[es[q].a];
        out[cnt].to = names[es[q].b];
        out[cnt].weight = es[q].w;
        out[cnt].seq = cnt;
        cnt++;
        if (cnt == n - 1) break;
    }
    free(names); free(es); free(par);
    if (out_len) *out_len = cnt;
    return out;
}

double gmb_graph_mst_weight(int64_t h)
{
    int n = 0;
    gmb_graph_edge_full_t* mst = gmb_graph_mst(h, &n);
    double s = 0.0;
    for (int i = 0; i < n; i++) s += mst[i].weight;
    free(mst);
    return s;
}

/* ─────────────────────────── Max-flow (Edmonds-Karp) ─────────────────────── */

double gmb_graph_maxflow(int64_t h, const char* source, const char* sink)
{
    int n = 0, m = 0;
    char** names = gmb_graph_nodes(h, &n);
    if (!names || n == 0) { free(names); return -1.0; }
    int s = ga_index(names, n, source), t = ga_index(names, n, sink);
    if (s < 0 || t < 0) { free(names); return -1.0; }
    gmb_graph_edge_full_t* el = gmb_graph_edge_list(h, &m);
    double* cap = (double*)calloc((size_t)n * n, sizeof(double));
    if (!cap) { free(names); free(el); return -1.0; }
    for (int k = 0; k < m; k++) {
        int a = ga_index(names, n, el[k].from), b = ga_index(names, n, el[k].to);
        if (a < 0 || b < 0 || a == b) continue;
        if (el[k].weight > 0) cap[(size_t)a * n + b] += el[k].weight;   /* capacità = peso (archi paralleli sommati) */
    }
    free(el);
    int* prev = (int*)malloc((size_t)n * sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    if (!prev || !queue || s == t) { free(names); free(cap); free(prev); free(queue); return s == t ? 0.0 : -1.0; }
    double flow = 0.0;
    for (;;) {
        for (int i = 0; i < n; i++) prev[i] = -1;
        int qh = 0, qt = 0; queue[qt++] = s; prev[s] = s;
        while (qh < qt && prev[t] == -1) {
            int u = queue[qh++];
            for (int v = 0; v < n; v++)
                if (prev[v] == -1 && cap[(size_t)u * n + v] > 1e-12) { prev[v] = u; queue[qt++] = v; if (v == t) break; }
        }
        if (prev[t] == -1) break;
        double aug = 1e300;
        for (int v = t; v != s; v = prev[v]) { double c = cap[(size_t)prev[v] * n + v]; if (c < aug) aug = c; }
        for (int v = t; v != s; v = prev[v]) { cap[(size_t)prev[v] * n + v] -= aug; cap[(size_t)v * n + prev[v]] += aug; }
        flow += aug;
    }
    free(names); free(cap); free(prev); free(queue);
    return flow;
}

/* ───────────────────────────── Louvain (community) ───────────────────────── */

/* Un livello di local-moving su un super-grafo (matrici dense, n piccolo). Ritorna 1 se
 * almeno un nodo si è spostato. */
static int ga_local_move(int n, const double* W, const double* selfw, const double* deg,
                         double m2, double resolution, int* comm, double* stot)
{
    int moved = 0;
    double* kintoc = (double*)calloc((size_t)n, sizeof(double));
    int* touched = (int*)malloc((size_t)n * sizeof(int));
    if (!kintoc || !touched) { free(kintoc); free(touched); return 0; }
    for (int i = 0; i < n; i++) { comm[i] = i; stot[i] = deg[i]; }
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < n; i++) {
            int ci = comm[i];
            int nt = 0;
            for (int j = 0; j < n; j++) {
                double w = W[(size_t)i * n + j];
                if (w > 0.0) { int cj = comm[j]; if (kintoc[cj] == 0.0) touched[nt++] = cj; kintoc[cj] += w; }
            }
            stot[ci] -= deg[i];
            int best_c = ci; double best_gain = 0.0;
            /* stay */
            double k_ci = kintoc[ci];
            double gain_ci = k_ci - resolution * stot[ci] * deg[i] / m2;
            best_gain = gain_ci; best_c = ci;
            for (int q = 0; q < nt; q++) {
                int c = touched[q];
                if (c == ci) continue;
                double gain = kintoc[c] - resolution * stot[c] * deg[i] / m2;
                if (gain > best_gain) { best_gain = gain; best_c = c; }
            }
            stot[best_c] += deg[i];
            if (best_c != ci) { comm[i] = best_c; changed = 1; moved = 1; }
            for (int q = 0; q < nt; q++) kintoc[touched[q]] = 0.0;
        }
    }
    free(kintoc); free(touched);
    return moved;
}

gmb_graph_comp_t* gmb_graph_louvain(int64_t h, double resolution, int* out_len)
{
    if (out_len) *out_len = 0;
    int n0 = 0, m0 = 0;
    char** names = gmb_graph_nodes(h, &n0);
    if (!names || n0 == 0) { free(names); return NULL; }
    gmb_graph_edge_full_t* el = gmb_graph_edge_list(h, &m0);
    if (resolution <= 0.0) resolution = 1.0;

    /* grafo non orientato (somma dei due versi), self-loop ignorati in ingresso */
    double* W = (double*)calloc((size_t)n0 * n0, sizeof(double));
    double* selfw = (double*)calloc((size_t)n0, sizeof(double));
    double* deg = (double*)calloc((size_t)n0, sizeof(double));
    if (!W || !selfw || !deg) { free(names); free(el); free(W); free(selfw); free(deg); return NULL; }
    for (int k = 0; k < m0; k++) {
        int a = ga_index(names, n0, el[k].from), b = ga_index(names, n0, el[k].to);
        if (a < 0 || b < 0) continue;
        if (a == b) continue;
        double w = el[k].weight > 0 ? el[k].weight : 0.0;
        W[(size_t)a * n0 + b] += w;
        W[(size_t)b * n0 + a] += w;
        deg[a] += w; deg[b] += w;
    }
    free(el);

    /* livello 0 → mappatura originale */
    int* node_comm = (int*)malloc((size_t)n0 * sizeof(int));   /* community finale per nodo originalе */
    if (!node_comm) { free(names); free(W); free(selfw); free(deg); return NULL; }
    for (int i = 0; i < n0; i++) node_comm[i] = i;

    int n = n0;
    double* cW = W; double* cself = selfw; double* cdeg = deg;
    double* stot = (double*)malloc((size_t)n * sizeof(double));
    int* comm = (int*)malloc((size_t)n * sizeof(int));
    if (!stot || !comm) { free(names); free(node_comm); free(W); free(selfw); free(deg); free(stot); free(comm); return NULL; }

    for (int level = 0; level < 64; level++) {
        double m2 = 0.0; for (int i = 0; i < n; i++) m2 += cdeg[i];
        if (m2 <= 0.0) break;
        int moved = ga_local_move(n, cW, cself, cdeg, m2, resolution, comm, stot);
        /* rimappa community → indici compatti (super-nodi) */
        int* remap = (int*)malloc((size_t)n * sizeof(int));
        for (int i = 0; i < n; i++) remap[i] = -1;
        int nc = 0;
        for (int i = 0; i < n; i++) { int c = comm[i]; if (remap[c] < 0) remap[c] = nc++; }
        /* mappa i nodi originali sull'indice del super-nodo di questo livello */
        for (int i = 0; i < n0; i++) node_comm[i] = remap[comm[node_comm[i]]];
        if (!moved || nc == n) { free(remap); break; }

        /* costruisci super-grafo */
        double* nW = (double*)calloc((size_t)nc * nc, sizeof(double));
        double* nself = (double*)calloc((size_t)nc, sizeof(double));
        double* ndeg = (double*)calloc((size_t)nc, sizeof(double));
        if (!nW || !nself || !ndeg) { free(remap); free(nW); free(nself); free(ndeg); break; }
        for (int i = 0; i < n; i++) {
            int ri = remap[comm[i]];
            nself[ri] += cself[i];
            ndeg[ri] += cdeg[i];
            for (int j = 0; j < n; j++) {
                double w = cW[(size_t)i * n + j];
                if (w <= 0.0) continue;
                int rj = remap[comm[j]];
                if (ri == rj) nself[ri] += w * 0.5;   /* archi interni → self-loop */
                else nW[(size_t)ri * nc + rj] += w;   /* i due versi sommati */
            }
        }
        free(remap);
        if (cW != W) { free(cW); free(cself); free(cdeg); }
        cW = nW; cself = nself; cdeg = ndeg; n = nc;
        stot = (double*)realloc(stot, (size_t)n * sizeof(double));
        comm = (int*)realloc(comm, (size_t)n * sizeof(int));
        if (!stot || !comm) break;
    }

    /* normalizza id community e costruisci output */
    gmb_graph_comp_t* out = (gmb_graph_comp_t*)malloc((size_t)n0 * sizeof(gmb_graph_comp_t));
    if (out) {
        int* seen = (int*)malloc((size_t)n0 * sizeof(int));
        for (int i = 0; i < n0; i++) seen[i] = -1;
        int cid = 0;
        for (int i = 0; i < n0; i++) { int c = node_comm[i]; if (seen[c] < 0) seen[c] = cid++; out[i].node = names[i]; out[i].component = seen[c]; }
        free(seen);
        if (out_len) *out_len = n0;
    }
    free(names); free(node_comm); free(stot); free(comm);
    if (cW != W) { free(cW); free(cself); free(cdeg); }
    free(W); free(selfw); free(deg);
    return out;
}
