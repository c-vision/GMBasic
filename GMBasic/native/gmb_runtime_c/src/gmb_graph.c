/* ============================================================================
 * gmb_graph.c — Grafo orientato pesato (Graph<TNode> del C#).
 * Registry di handle → grafo. Nodi come stringhe. Archi orientati pesati.
 * Implementa BFS/DFS/Dijkstra con la stessa semantica di Graph.cs del C#.
 * ============================================================================ */
#include "gmb_graph.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
typedef struct { volatile LONG locked; } gmb_graph_lock_t;
static void gmb_graph_lock(gmb_graph_lock_t* m) { while (_InterlockedExchange(&m->locked, 1)) { ; } }
static void gmb_graph_unlock(gmb_graph_lock_t* m) { _InterlockedExchange(&m->locked, 0); }
#else
#include <pthread.h>
typedef struct { volatile int locked; } gmb_graph_lock_t;
static void gmb_graph_lock(gmb_graph_lock_t* m) { while (__sync_lock_test_and_set(&m->locked, 1)) { ; } }
static void gmb_graph_unlock(gmb_graph_lock_t* m) { __sync_lock_release(&m->locked); }
#endif

/* ---- Nodo: nome + lista di archi uscenti (vicino, peso) ---- */
typedef struct gmb_graph_edge_internal {
    struct gmb_graph_node* to;
    double weight;
    long seq;   /* sequenza di INSERIMENTO globale (parità C# GraphManager.Edges order) */
    struct gmb_graph_edge_internal* next;
} gmb_graph_edge_internal_t;

typedef struct gmb_graph_node {
    char* name;
    gmb_graph_edge_internal_t* edges;   /* lista uscente */
    struct gmb_graph_node* next; /* lista registry */
} gmb_graph_node_t;

typedef struct gmb_graph {
    gmb_graph_node_t* nodes;   /* lista di nodi (ordine di inserimento) */
    int node_count;
    long edge_seq;             /* contatore di inserimento edge (parità ordine C#) */
} gmb_graph_t;

/* ---- Registry ---- */
#define GMB_GRAPH_MAX 256
static gmb_graph_t* g_registry[GMB_GRAPH_MAX];
static int g_registry_count = 0;
static gmb_graph_lock_t g_registry_lock;
static int g_registry_ready = 0;

void gmb_graph_init(void)
{
    if (g_registry_ready) return;
    g_registry_lock.locked = 0;
    for (int i = 0; i < GMB_GRAPH_MAX; i++) g_registry[i] = NULL;
    g_registry_count = 0;
    g_registry_ready = 1;
}

void gmb_graph_shutdown(void)
{
    if (!g_registry_ready) return;
    for (int i = 0; i < GMB_GRAPH_MAX; i++)
    {
        if (g_registry[i])
        {
            gmb_graph_free(i + 1);
            g_registry[i] = NULL;
        }
    }
    g_registry_count = 0;
    g_registry_ready = 0;
}

/* ---- helper nodo ---- */
static gmb_graph_node_t* graph_find_node(gmb_graph_t* g, const char* name)
{
    for (gmb_graph_node_t* n = g->nodes; n; n = n->next)
        if (n->name && strcmp(n->name, name) == 0)
            return n;
    return NULL;
}

static gmb_graph_edge_internal_t* graph_find_edge(gmb_graph_node_t* from, gmb_graph_node_t* to)
{
    for (gmb_graph_edge_internal_t* e = from->edges; e; e = e->next)
        if (e->to == to)
            return e;
    return NULL;
}

/* ---- Registry API ---- */
int64_t gmb_graph_create(void)
{
    if (!g_registry_ready) gmb_graph_init();
    gmb_graph_t* g = (gmb_graph_t*)calloc(1, sizeof(gmb_graph_t));
    if (!g) return -1;
    gmb_graph_lock(&g_registry_lock);
    int slot = -1;
    for (int i = 0; i < GMB_GRAPH_MAX; i++)
        if (g_registry[i] == NULL) { slot = i; break; }
    if (slot < 0) { gmb_graph_unlock(&g_registry_lock); free(g); return -1; }
    g_registry[slot] = g;
    if (slot >= g_registry_count) g_registry_count = slot + 1;
    gmb_graph_unlock(&g_registry_lock);
    return slot + 1;
}

static gmb_graph_t* graph_get(int64_t h)
{
    if (h <= 0 || h > GMB_GRAPH_MAX) return NULL;
    return g_registry[(int)(h - 1)];
}

void gmb_graph_free(int64_t h)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return;
    /* libera nodi e archi */
    gmb_graph_node_t* n = g->nodes;
    while (n)
    {
        gmb_graph_node_t* nx = n->next;
        gmb_graph_edge_internal_t* e = n->edges;
        while (e)
        {
            gmb_graph_edge_internal_t* ex = e->next;
            free(e);
            e = ex;
        }
        free(n->name);
        free(n);
        n = nx;
    }
    free(g);
    g_registry[(int)(h - 1)] = NULL;
}

/* ---- Nodi/archi ---- */
int gmb_graph_add_node(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node || !node[0]) return -2;
    if (graph_find_node(g, node)) return 0; /* già esiste */
    gmb_graph_node_t* n = (gmb_graph_node_t*)calloc(1, sizeof(gmb_graph_node_t));
    if (!n) return -2;
    n->name = strdup(node);
    if (!n->name) { free(n); return -2; }
    n->next = g->nodes;
    g->nodes = n;
    g->node_count++;
    return -1; /* nuovo */
}

/* F1 Incongruenze (§0bis): AddNodeConsistent — aggiunge il nodo se assente e ritorna
 * -1 (vero BASIC) se il nodo ESISTE dopo la chiamata (nuovo o già presente); 0 su
 * errore. Unica implementazione per VM/C, C99Native. */
int gmb_graph_add_node_consistent(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node || !node[0]) return 0;
    int rc = gmb_graph_add_node(h, node);
    if (rc == -2) return 0;
    return -1; /* esiste a prescindere (già presente o appena aggiunto) */
}

int gmb_graph_add_edge(int64_t h, const char* from, const char* to, double weight)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return -2;
    gmb_graph_node_t* f = graph_find_node(g, from);
    gmb_graph_node_t* t = graph_find_node(g, to);
    if (!f || !t) return -2; /* entrambi devono esistere (come C#) */
    gmb_graph_edge_internal_t* e = graph_find_edge(f, t);
    if (e) { e->weight = weight; return 0; } /* aggiorna peso */
    e = (gmb_graph_edge_internal_t*)calloc(1, sizeof(gmb_graph_edge_internal_t));
    if (!e) return -2;
    e->to = t;
    e->weight = weight;
    e->seq = g->edge_seq++;
    e->next = NULL;
    if (!f->edges) f->edges = e;
    else { gmb_graph_edge_internal_t* p = f->edges; while (p->next) p = p->next; p->next = e; }
    return 0;
}

/* Come gmb_graph_add_edge ma NON dedupplica (permette edge paralleli): serve al RichGraph
 * (GraphManager.Build ha edge duplicati (from,to) con tipi diversi, es. FOLDER_CHILD +
 * CONTENT_MENTION) → parità del conteggio edge e di PageRank. */
int gmb_graph_add_edge_multi(int64_t h, const char* from, const char* to, double weight)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return -2;
    gmb_graph_node_t* f = graph_find_node(g, from);
    gmb_graph_node_t* t = graph_find_node(g, to);
    if (!f || !t) return -2;
    gmb_graph_edge_internal_t* e = (gmb_graph_edge_internal_t*)calloc(1, sizeof(gmb_graph_edge_internal_t));
    if (!e) return -2;
    e->to = t;
    e->weight = weight;
    e->seq = g->edge_seq++;
    e->next = NULL;
    if (!f->edges) f->edges = e;
    else { gmb_graph_edge_internal_t* p = f->edges; while (p->next) p = p->next; p->next = e; }
    return 0;
}

int gmb_graph_remove_node(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return 0;
    /* trova il nodo (senza liberarlo ancora) */
    gmb_graph_node_t* target = graph_find_node(g, node);
    if (!target) return 0;

    /* PASSO 1: rimuove gli archi ENTRANTI verso il nodo da tutti gli altri nodi,
     * PRIMA di liberare il target (evita use-after-free su e->to). */
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next)
    {
        if (x == target) continue;
        gmb_graph_edge_internal_t* eprev = NULL;
        gmb_graph_edge_internal_t* e = x->edges;
        while (e)
        {
            if (e->to == target)
            {
                gmb_graph_edge_internal_t* ex = e->next;
                if (eprev) eprev->next = ex;
                else x->edges = ex;
                free(e);
                e = ex;
            }
            else { eprev = e; e = e->next; }
        }
    }

    /* PASSO 2: rimuove il nodo dalla lista */
    gmb_graph_node_t* prev = NULL;
    gmb_graph_node_t* n = g->nodes;
    while (n)
    {
        if (n == target)
        {
            if (prev) prev->next = n->next;
            else g->nodes = n->next;
            gmb_graph_edge_internal_t* e = n->edges;
            while (e) { gmb_graph_edge_internal_t* ex = e->next; free(e); e = ex; }
            free(n->name);
            free(n);
            g->node_count--;
            return -1;
        }
        prev = n;
        n = n->next;
    }
    return 0;
}

int gmb_graph_remove_edge(int64_t h, const char* from, const char* to)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return 0;
    gmb_graph_node_t* f = graph_find_node(g, from);
    gmb_graph_node_t* t = graph_find_node(g, to);
    if (!f || !t) return 0;
    gmb_graph_edge_internal_t* eprev = NULL;
    gmb_graph_edge_internal_t* e = f->edges;
    while (e)
    {
        if (e->to == t)
        {
            if (eprev) eprev->next = e->next;
            else f->edges = e->next;
            free(e);
            return -1;
        }
        eprev = e;
        e = e->next;
    }
    return 0;
}

int gmb_graph_has_node(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return 0;
    return graph_find_node(g, node) ? -1 : 0;
}

int gmb_graph_has_edge(int64_t h, const char* from, const char* to)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return 0;
    gmb_graph_node_t* f = graph_find_node(g, from);
    gmb_graph_node_t* t = graph_find_node(g, to);
    if (!f || !t) return 0;
    return graph_find_edge(f, t) ? -1 : 0;
}

int gmb_graph_get_edge_weight(int64_t h, const char* from, const char* to, double* weight)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !weight) return 0;
    gmb_graph_node_t* f = graph_find_node(g, from);
    gmb_graph_node_t* t = graph_find_node(g, to);
    if (!f || !t) { *weight = 0; return 0; }
    gmb_graph_edge_internal_t* e = graph_find_edge(f, t);
    if (e) { *weight = e->weight; return -1; }
    *weight = 0;
    return 0;
}

int gmb_graph_get_indegree(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return -1;
    if (!graph_find_node(g, node)) return -1;
    int indeg = 0;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next)
    {
        gmb_graph_edge_internal_t* e = x->edges;
        while (e)
        {
            if (e->to && e->to->name && strcmp(e->to->name, node) == 0)
                indeg++;
            e = e->next;
        }
    }
    return indeg;
}

int gmb_graph_node_count(int64_t h)
{
    gmb_graph_t* g = graph_get(h);
    return g ? g->node_count : 0;
}

/* ---- Elenco nodi ---- */
char** gmb_graph_nodes(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || g->node_count == 0) return NULL;
    char** arr = (char**)malloc((size_t)g->node_count * sizeof(char*));
    if (!arr) return NULL;
    int n = g->node_count;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next)
        arr[--n] = x->name;
    *out_len = g->node_count;
    return arr;
}

gmb_graph_edge_t* gmb_graph_neighbors(int64_t h, const char* node, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return NULL;
    gmb_graph_node_t* f = graph_find_node(g, node);
    if (!f) return NULL;
    int cnt = 0;
    for (gmb_graph_edge_internal_t* e = f->edges; e; e = e->next) cnt++;
    if (cnt == 0) return NULL;
    gmb_graph_edge_t* out = (gmb_graph_edge_t*)calloc((size_t)cnt, sizeof(gmb_graph_edge_t));
    if (!out) return NULL;
    int n = 0;
    for (gmb_graph_edge_internal_t* e = f->edges; e; e = e->next)
    {
        out[n].node = e->to->name;
        out[n].weight = e->weight;
        n++;
    }
    *out_len = n;
    return out;
}

/* ---- BFS (stessa semantica C#: coda FIFO, visited set) ---- */
char** gmb_graph_bfs(int64_t h, const char* start, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !start) return NULL;
    gmb_graph_node_t* s = graph_find_node(g, start);
    if (!s) return NULL;
    /* visited: riuso i nodi con flag temporaneo */
    int total = g->node_count;
    char** result = (char**)malloc((size_t)(total ? total : 1) * sizeof(char*));
    if (!result) return NULL;
    char** visited = (char**)calloc((size_t)(total ? total : 1), sizeof(char*));
    if (!visited) { free(result); return NULL; }
    int vcount = 0;
    /* coda FIFO di nodi */
    gmb_graph_node_t** queue = (gmb_graph_node_t**)malloc((size_t)(total ? total : 1) * sizeof(gmb_graph_node_t*));
    if (!queue) { free(result); free(visited); return NULL; }
    int qh = 0, qt = 0;
    queue[qt++] = s;
    visited[vcount++] = s->name;
    result[(*out_len)++] = s->name;
    while (qh < qt)
    {
        gmb_graph_node_t* cur = queue[qh++];
        for (gmb_graph_edge_internal_t* e = cur->edges; e; e = e->next)
        {
            int isv = 0;
            for (int i = 0; i < vcount; i++)
                if (strcmp(visited[i], e->to->name) == 0) { isv = 1; break; }
            if (!isv)
            {
                visited[vcount++] = e->to->name;
                result[(*out_len)++] = e->to->name;
                queue[qt++] = e->to;
            }
        }
    }
    free(queue);
    free(visited);
    return result;
}

/* ---- DFS (stessa semantica C#: stack, vicini in ordine inverso) ---- */
char** gmb_graph_dfs(int64_t h, const char* start, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !start) return NULL;
    gmb_graph_node_t* s = graph_find_node(g, start);
    if (!s) return NULL;
    int total = g->node_count;
    char** result = (char**)malloc((size_t)(total ? total : 1) * sizeof(char*));
    if (!result) return NULL;
    char** visited = (char**)calloc((size_t)(total ? total : 1), sizeof(char*));
    if (!visited) { free(result); return NULL; }
    int vcount = 0;
    gmb_graph_node_t** stack = (gmb_graph_node_t**)malloc((size_t)(total ? total : 1) * sizeof(gmb_graph_node_t*));
    if (!stack) { free(result); free(visited); return NULL; }
    int sp = 0;
    stack[sp++] = s;
    while (sp > 0)
    {
        gmb_graph_node_t* cur = stack[--sp];
        int isv = 0;
        for (int i = 0; i < vcount; i++)
            if (strcmp(visited[i], cur->name) == 0) { isv = 1; break; }
        if (!isv)
        {
            visited[vcount++] = cur->name;
            result[(*out_len)++] = cur->name;
            /* vicini in ordine inverso: raccogli e pusha al contrario */
            gmb_graph_node_t** rev = (gmb_graph_node_t**)malloc((size_t)(total ? total : 1) * sizeof(gmb_graph_node_t*));
            int rn = 0;
            if (rev)
            {
                for (gmb_graph_edge_internal_t* e = cur->edges; e; e = e->next)
                    rev[rn++] = e->to;
                for (int i = rn - 1; i >= 0; i--)
                {
                    int v2 = 0;
                    for (int j = 0; j < vcount; j++)
                        if (strcmp(visited[j], rev[i]->name) == 0) { v2 = 1; break; }
                    if (!v2) stack[sp++] = rev[i];
                }
                free(rev);
            }
        }
    }
    free(stack);
    free(visited);
    return result;
}

/* ---- Dijkstra (stessa semantica C#: distanze iniziali +inf, priority) ---- */
gmb_graph_dist_t* gmb_graph_dijkstra(int64_t h, const char* start, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !start) return NULL;
    gmb_graph_node_t* s = graph_find_node(g, start);
    if (!s) return NULL;
    int total = g->node_count;
    if (total == 0) return NULL;
    /* distanze: per nodo */
    double* dist = (double*)malloc((size_t)total * sizeof(double));
    int* done = (int*)calloc((size_t)total, sizeof(int));
    gmb_graph_node_t** nodes = (gmb_graph_node_t**)malloc((size_t)total * sizeof(gmb_graph_node_t*));
    if (!dist || !done || !nodes) { free(dist); free(done); free(nodes); return NULL; }
    int n = 0;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next) { nodes[n] = x; dist[n] = 1e308; n++; }
    int startIdx = -1;
    for (int i = 0; i < n; i++) if (strcmp(nodes[i]->name, start) == 0) { startIdx = i; break; }
    if (startIdx < 0) { free(dist); free(done); free(nodes); return NULL; }
    dist[startIdx] = 0;
    /* semplice O(V²) con selezione del minimo (equivale a PriorityQueue) */
    for (int iter = 0; iter < n; iter++)
    {
        int u = -1;
        double best = 1e308;
        for (int i = 0; i < n; i++)
            if (!done[i] && dist[i] < best) { best = dist[i]; u = i; }
        if (u < 0) break;
        done[u] = 1;
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            for (int v = 0; v < n; v++)
            {
                if (strcmp(nodes[v]->name, e->to->name) == 0 && dist[v] > dist[u] + e->weight)
                    dist[v] = dist[u] + e->weight;
            }
        }
    }
    /* costruisci risultato (nodi raggiungibili) */
    int cnt = 0;
    for (int i = 0; i < n; i++) if (dist[i] < 1e307) cnt++;
    gmb_graph_dist_t* out = (gmb_graph_dist_t*)calloc((size_t)cnt, sizeof(gmb_graph_dist_t));
    if (out)
    {
        int k = 0;
        for (int i = 0; i < n; i++)
            if (dist[i] < 1e307) { out[k].node = nodes[i]->name; out[k].dist = dist[i]; k++; }
        *out_len = k;
    }
    free(dist);
    free(done);
    free(nodes);
    return out;
}

/* ---- PageRank (power iteration, stessa semantica C#: rank normalizzato su tutti i nodi) ---- */
gmb_graph_pagerank_t* gmb_graph_pagerank(int64_t h, double damping, int iterations, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g) return NULL;
    int n = g->node_count;
    if (n == 0) return NULL;

    /* raccogli i nodi in un array indice (ordine di inserzione) */
    gmb_graph_node_t** nodes = (gmb_graph_node_t**)malloc((size_t)n * sizeof(gmb_graph_node_t*));
    if (!nodes) return NULL;
    int idx = 0;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next) nodes[idx++] = x;

    /* indice node->pos per lookup veloce (array di puntatori, scan lineare su n piccolo) */
    double* rank = (double*)malloc((size_t)n * sizeof(double));
    double* next = (double*)malloc((size_t)n * sizeof(double));
    double* outdeg = (double*)calloc((size_t)n, sizeof(double));
    if (!rank || !next || !outdeg) { free(nodes); free(rank); free(next); free(outdeg); return NULL; }

    for (int i = 0; i < n; i++)
    {
        rank[i] = 1.0 / n;
        int od = 0;
        for (gmb_graph_edge_internal_t* e = nodes[i]->edges; e; e = e->next) od++;
        outdeg[i] = od > 0 ? (double)od : 0.0;
    }

    int max_iter = iterations > 0 ? iterations : 100;
    if (damping <= 0.0) damping = 0.85;
    double base = (1.0 - damping) / n;

    for (int it = 0; it < max_iter; it++)
    {
        for (int i = 0; i < n; i++) next[i] = base;
        /* ogni nodo distribuisce il proprio rank ai vicini (solo archi uscenti) */
        for (int i = 0; i < n; i++)
        {
            if (outdeg[i] == 0.0) continue;
            double share = damping * rank[i] / outdeg[i];
            for (gmb_graph_edge_internal_t* e = nodes[i]->edges; e; e = e->next)
            {
                int j = -1;
                for (int k = 0; k < n; k++)
                    if (nodes[k] == e->to) { j = k; break; }
                if (j >= 0) next[j] += share;
            }
        }
        /* i nodi senza archi uscenti ridistribuiscono uniformemente (teleport) */
        for (int i = 0; i < n; i++)
        {
            if (outdeg[i] != 0.0) continue;
            double share = damping * rank[i] / n;
            for (int j = 0; j < n; j++) next[j] += share;
        }
        double* tmp = rank; rank = next; next = tmp;
    }

    /* risultato {node, rank} — nodi puntano alle stringhe del grafo (non copiate) */
    gmb_graph_pagerank_t* out = (gmb_graph_pagerank_t*)malloc((size_t)n * sizeof(gmb_graph_pagerank_t));
    if (out)
    {
        for (int i = 0; i < n; i++)
        {
            out[i].node = nodes[i]->name;
            out[i].rank = rank[i];
        }
        *out_len = n;
    }

    free(nodes);
    free(rank);
    free(next);
    free(outdeg);
    return out;
}

/* ============================================================================
 * ALGORITMI CANONICI (L5) — parità con Graph<TNode> del C# (CommonLib.Collections)
 * ============================================================================ */

/* Helper: posizione di un nodo in un array (scan lineare). */
static int graph_node_pos(gmb_graph_node_t** nodes, int n, const char* name)
{
    for (int i = 0; i < n; i++)
        if (nodes[i]->name && strcmp(nodes[i]->name, name) == 0) return i;
    return -1;
}

/* Helper: array di tutti i nodi (indice stabile, ordine di inserimento). */
static gmb_graph_node_t** graph_nodes_array(gmb_graph_t* g, int* out_n)
{
    *out_n = g->node_count;
    if (g->node_count == 0) return NULL;
    gmb_graph_node_t** nodes = (gmb_graph_node_t**)malloc((size_t)g->node_count * sizeof(gmb_graph_node_t*));
    if (!nodes) return NULL;
    int n = g->node_count;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next) nodes[--n] = x;
    return nodes;
}

int gmb_graph_get_outdegree(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return -1;
    gmb_graph_node_t* f = graph_find_node(g, node);
    if (!f) return -1;
    int cnt = 0;
    for (gmb_graph_edge_internal_t* e = f->edges; e; e = e->next) cnt++;
    return cnt;
}

int gmb_graph_get_degree(int64_t h, const char* node)
{
    int od = gmb_graph_get_outdegree(h, node);
    if (od < 0) return -1;
    return od + gmb_graph_get_indegree(h, node);
}

/* BFS parent: ricostruisce path (a-b-c) da un array prev. */
static char** graph_reconstruct(char** prev_names, gmb_graph_node_t** nodes, int n, const char* from, const char* to, int* out_len)
{
    *out_len = 0;
    int fpos = graph_node_pos(nodes, n, from);
    int tpos = graph_node_pos(nodes, n, to);
    if (fpos < 0 || tpos < 0) return NULL;
    /* ricostruisci invertendo i prev */
    char** rev = (char**)malloc((size_t)(n + 1) * sizeof(char*));
    if (!rev) return NULL;
    int rn = 0;
    int cur = tpos;
    rev[rn++] = nodes[cur]->name;
    while (cur != fpos && prev_names[cur])
    {
        cur = graph_node_pos(nodes, n, prev_names[cur]);
        if (cur < 0) break;
        rev[rn++] = nodes[cur]->name;
        if (rn > n) break; /* sicurezza: evita loop */
    }
    if (cur != fpos) { free(rev); return NULL; }
    char** result = (char**)malloc((size_t)rn * sizeof(char*));
    if (!result) { free(rev); return NULL; }
    for (int i = 0; i < rn; i++) result[i] = rev[rn - 1 - i];
    *out_len = rn;
    free(rev);
    return result;
}

char** gmb_graph_shortest_path(int64_t h, const char* from, const char* to, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !from || !to) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }
    int fpos = graph_node_pos(nodes, n, from);
    int tpos = graph_node_pos(nodes, n, to);
    if (fpos < 0 || tpos < 0) { free(nodes); return NULL; }
    if (strcmp(from, to) == 0)
    {
        char** r = (char**)malloc(sizeof(char*));
        if (r) { r[0] = nodes[fpos]->name; *out_len = 1; }
        free(nodes);
        return r;
    }
    char** prev_names = (char**)calloc((size_t)n, sizeof(char*));
    int* visited = (int*)calloc((size_t)n, sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    if (!prev_names || !visited || !queue) { free(nodes); free(prev_names); free(visited); free(queue); return NULL; }
    int qh = 0, qt = 0;
    visited[fpos] = 1;
    queue[qt++] = fpos;
    char** result = NULL;
    while (qh < qt)
    {
        int u = queue[qh++];
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v < 0 || visited[v]) continue;
            visited[v] = 1;
            prev_names[v] = nodes[u]->name;
            if (v == tpos)
            {
                result = graph_reconstruct(prev_names, nodes, n, from, to, out_len);
                goto done;
            }
            queue[qt++] = v;
        }
    }
done:
    free(nodes); free(prev_names); free(visited); free(queue);
    return result;
}

char** gmb_graph_dijkstra_path(int64_t h, const char* from, const char* to, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !from || !to) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }
    int fpos = graph_node_pos(nodes, n, from);
    int tpos = graph_node_pos(nodes, n, to);
    if (fpos < 0 || tpos < 0) { free(nodes); return NULL; }
    if (strcmp(from, to) == 0)
    {
        char** r = (char**)malloc(sizeof(char*));
        if (r) { r[0] = nodes[fpos]->name; *out_len = 1; }
        free(nodes);
        return r;
    }
    double* dist = (double*)malloc((size_t)n * sizeof(double));
    int* done = (int*)calloc((size_t)n, sizeof(int));
    char** prev_names = (char**)calloc((size_t)n, sizeof(char*));
    if (!dist || !done || !prev_names) { free(nodes); free(dist); free(done); free(prev_names); return NULL; }
    for (int i = 0; i < n; i++) dist[i] = 1e308;
    dist[fpos] = 0;
    for (int iter = 0; iter < n; iter++)
    {
        int u = -1; double best = 1e308;
        for (int i = 0; i < n; i++)
            if (!done[i] && dist[i] < best) { best = dist[i]; u = i; }
        if (u < 0) break;
        done[u] = 1;
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v < 0 || dist[u] + e->weight < dist[v])
            {
                if (v >= 0) { dist[v] = dist[u] + e->weight; prev_names[v] = nodes[u]->name; }
            }
        }
    }
    char** result = NULL;
    if (prev_names[tpos])
        result = graph_reconstruct(prev_names, nodes, n, from, to, out_len);
    free(nodes); free(dist); free(done); free(prev_names);
    return result;
}

char** gmb_graph_find_path(int64_t h, const char* from, const char* to, const char* strategy, int* out_len)
{
    if (strategy && (strcmp(strategy, "bfs") == 0 || strcmp(strategy, "breadth") == 0))
        return gmb_graph_shortest_path(h, from, to, out_len);
    if (strategy && (strcmp(strategy, "astar") == 0 || strcmp(strategy, "a*") == 0))
        return gmb_graph_dijkstra_path(h, from, to, out_len); /* A* con euristica 0 = Dijkstra */
    return gmb_graph_dijkstra_path(h, from, to, out_len);
}

gmb_graph_bellman_t* gmb_graph_bellman_ford(int64_t h, const char* start, int* has_neg_cycle, int* out_len)
{
    *out_len = 0;
    if (has_neg_cycle) *has_neg_cycle = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g || !start) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }
    double* dist = (double*)malloc((size_t)n * sizeof(double));
    for (int i = 0; i < n; i++) dist[i] = 1e308;
    int s = graph_node_pos(nodes, n, start);
    if (s < 0) { free(nodes); free(dist); return NULL; }
    dist[s] = 0;
    for (int it = 0; it < n - 1; it++)
    {
        int relaxed = 0;
        for (int u = 0; u < n; u++)
        {
            if (dist[u] >= 1e307) continue;
            for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
            {
                int v = graph_node_pos(nodes, n, e->to->name);
                if (v < 0) continue;
                if (dist[u] + e->weight < dist[v]) { dist[v] = dist[u] + e->weight; relaxed = 1; }
            }
        }
        if (!relaxed) break;
    }
    int neg = 0;
    for (int u = 0; u < n; u++)
    {
        if (dist[u] >= 1e307) continue;
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v >= 0 && dist[u] + e->weight < dist[v]) { neg = 1; break; }
        }
        if (neg) break;
    }
    if (has_neg_cycle) *has_neg_cycle = neg ? -1 : 0;
    int cnt = 0;
    for (int i = 0; i < n; i++) if (dist[i] < 1e307) cnt++;
    gmb_graph_bellman_t* out = (gmb_graph_bellman_t*)calloc((size_t)cnt, sizeof(gmb_graph_bellman_t));
    if (out)
    {
        int k = 0;
        for (int i = 0; i < n; i++)
            if (dist[i] < 1e307) { out[k].node = nodes[i]->name; out[k].dist = dist[i]; k++; }
        *out_len = k;
    }
    free(nodes); free(dist);
    return out;
}

int gmb_graph_has_cycle(int64_t h)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return 0;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return 0; }
    /* color: 0 bianco, 1 grigio, 2 nero */
    int* color = (int*)calloc((size_t)n, sizeof(int));
    if (!color) { free(nodes); return 0; }
    /* DFS iterativa con stack di (nodo, stato-iter) */
    int has_cycle = 0;
    /* use recursive-free approach: implementiamo DFS iterativa con frame stack */
    typedef struct { int node; gmb_graph_edge_internal_t* edge; } dfs_frame_t;
    dfs_frame_t* stack = (dfs_frame_t*)malloc((size_t)(n + 1) * sizeof(dfs_frame_t));
    if (!stack) { free(nodes); free(color); return 0; }
    for (int start = 0; start < n && !has_cycle; start++)
    {
        if (color[start] != 0) continue;
        int sp = 0;
        color[start] = 1;
        stack[sp].node = start;
        stack[sp].edge = nodes[start]->edges;
        sp++;
        while (sp > 0)
        {
            dfs_frame_t* f = &stack[sp - 1];
            if (!f->edge) { color[f->node] = 2; sp--; continue; }
            gmb_graph_edge_internal_t* e = f->edge;
            f->edge = e->next;
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v < 0) continue;
            if (color[v] == 1) { has_cycle = 1; break; }
            if (color[v] == 0)
            {
                color[v] = 1;
                stack[sp].node = v;
                stack[sp].edge = nodes[v]->edges;
                sp++;
            }
        }
    }
    free(stack); free(nodes); free(color);
    return has_cycle ? -1 : 0;
}

char** gmb_graph_toposort(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }
    int* indeg = (int*)calloc((size_t)n, sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    if (!indeg || !queue) { free(nodes); free(indeg); free(queue); return NULL; }
    for (int u = 0; u < n; u++)
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v >= 0) indeg[v]++;
        }
    int qh = 0, qt = 0;
    for (int i = 0; i < n; i++) if (indeg[i] == 0) queue[qt++] = i;
    char** result = (char**)malloc((size_t)(n ? n : 1) * sizeof(char*));
    if (!result) { free(nodes); free(indeg); free(queue); return NULL; }
    int rn = 0;
    while (qh < qt)
    {
        int u = queue[qh++];
        result[rn++] = nodes[u]->name;
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v >= 0 && --indeg[v] == 0) queue[qt++] = v;
        }
    }
    free(nodes); free(indeg); free(queue);
    if (rn != n) { free(result); return NULL; }
    *out_len = rn;
    return result;
}

int gmb_graph_is_bipartite(int64_t h)
{
    gmb_graph_t* g = graph_get(h);
    if (!g) return 0;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return 0; }
    int* color = (int*)malloc((size_t)n * sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    if (!color || !queue) { free(nodes); free(color); free(queue); return 0; }
    for (int i = 0; i < n; i++) color[i] = -1;
    int ok = 1;
    for (int start = 0; start < n && ok; start++)
    {
        if (color[start] != -1) continue;
        int qh = 0, qt = 0;
        color[start] = 0;
        queue[qt++] = start;
        while (qh < qt)
        {
            int u = queue[qh++];
            /* archi uscenti */
            for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
            {
                int v = graph_node_pos(nodes, n, e->to->name);
                if (v < 0) continue;
                if (color[v] == -1) { color[v] = 1 - color[u]; queue[qt++] = v; }
                else if (color[v] == color[u]) { ok = 0; break; }
            }
            if (!ok) break;
            /* archi ENTRANTI (non orientato, come C# UndirectedNeighbors) */
            for (int x = 0; x < n; x++)
                if (x != u && color[x] == -1)
                {
                    for (gmb_graph_edge_internal_t* e = nodes[x]->edges; e; e = e->next)
                        if (graph_node_pos(nodes, n, e->to->name) == u)
                        {
                            color[x] = 1 - color[u];
                            queue[qt++] = x;
                            break;
                        }
                }
        }
    }
    free(nodes); free(color); free(queue);
    return ok ? -1 : 0;
}

gmb_graph_comp_t* gmb_graph_connected_components(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }
    int* comp = (int*)calloc((size_t)n, sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    if (!comp || !queue) { free(nodes); free(comp); free(queue); return NULL; }
    for (int i = 0; i < n; i++) comp[i] = -1;
    int cid = 0;
    for (int start = 0; start < n; start++)
    {
        if (comp[start] != -1) continue;
        int qh = 0, qt = 0;
        comp[start] = cid;
        queue[qt++] = start;
        while (qh < qt)
        {
            int u = queue[qh++];
            /* archi uscenti */
            for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
            {
                int v = graph_node_pos(nodes, n, e->to->name);
                if (v >= 0 && comp[v] == -1) { comp[v] = cid; queue[qt++] = v; }
            }
            /* archi entranti (weakly connected): scansiona tutti i nodi */
            for (int x = 0; x < n; x++)
                if (comp[x] == -1)
                {
                    for (gmb_graph_edge_internal_t* e = nodes[x]->edges; e; e = e->next)
                        if (graph_node_pos(nodes, n, e->to->name) == u) { comp[x] = cid; queue[qt++] = x; break; }
                }
        }
        cid++;
    }
    gmb_graph_comp_t* out = (gmb_graph_comp_t*)malloc((size_t)n * sizeof(gmb_graph_comp_t));
    if (out)
    {
        for (int i = 0; i < n; i++) { out[i].node = nodes[i]->name; out[i].component = comp[i]; }
        *out_len = n;
    }
    free(nodes); free(comp); free(queue);
    return out;
}

gmb_graph_centrality_t* gmb_graph_betweenness(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }
    double* cb = (double*)calloc((size_t)n, sizeof(double));
    if (!cb) { free(nodes); return NULL; }
    for (int s = 0; s < n; s++)
    {
        int* dist = (int*)malloc((size_t)n * sizeof(int));
        double* sigma = (double*)calloc((size_t)n, sizeof(double));
        double* delta = (double*)calloc((size_t)n, sizeof(double));
        int* stack = (int*)malloc((size_t)n * sizeof(int));
        int* queue = (int*)malloc((size_t)n * sizeof(int));
        /* pred: lista di predecessori per nodo (n*n worst) */
        int* pred = (int*)calloc((size_t)n * n, sizeof(int));
        int* pred_count = (int*)calloc((size_t)n, sizeof(int));
        if (!dist || !sigma || !delta || !stack || !queue || !pred || !pred_count)
        {
            free(dist); free(sigma); free(delta); free(stack); free(queue); free(pred); free(pred_count);
            continue;
        }
        for (int i = 0; i < n; i++) dist[i] = -1;
        sigma[s] = 1.0;
        dist[s] = 0;
        int qh = 0, qt = 0, sp = 0;
        queue[qt++] = s;
        while (qh < qt)
        {
            int v = queue[qh++];
            stack[sp++] = v;
            for (gmb_graph_edge_internal_t* e = nodes[v]->edges; e; e = e->next)
            {
                int w = graph_node_pos(nodes, n, e->to->name);
                if (w < 0) continue;
                if (dist[w] < 0) { dist[w] = dist[v] + 1; queue[qt++] = w; }
                if (dist[w] == dist[v] + 1)
                {
                    sigma[w] += sigma[v];
                    pred[w * n + pred_count[w]++] = v;
                }
            }
        }
        while (sp > 0)
        {
            int w = stack[--sp];
            for (int i = 0; i < pred_count[w]; i++)
            {
                int v = pred[w * n + i];
                delta[v] += sigma[v] / sigma[w] * (1.0 + delta[w]);
            }
            if (w != s) cb[w] += delta[w];
        }
        free(dist); free(sigma); free(delta); free(stack); free(queue); free(pred); free(pred_count);
    }
    double norm = n <= 2 ? 0.0 : 1.0 / ((n - 1) * (n - 2));
    gmb_graph_centrality_t* out = (gmb_graph_centrality_t*)malloc((size_t)n * sizeof(gmb_graph_centrality_t));
    if (out)
    {
        for (int i = 0; i < n; i++) { out[i].node = nodes[i]->name; out[i].value = cb[i] * norm; }
        *out_len = n;
    }
    free(nodes); free(cb);
    return out;
}

double gmb_graph_diameter(int64_t h)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || g->node_count < 2) return 0;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return 0; }
    /* FloydWarshall sui pesi */
    double** d = (double**)malloc((size_t)n * sizeof(double*));
    for (int i = 0; i < n; i++)
    {
        d[i] = (double*)malloc((size_t)n * sizeof(double));
        for (int j = 0; j < n; j++) d[i][j] = 1e308;
        d[i][i] = 0;
    }
    for (int u = 0; u < n; u++)
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v >= 0 && e->weight < d[u][v]) d[u][v] = e->weight;
        }
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (d[i][k] + d[k][j] < d[i][j]) d[i][j] = d[i][k] + d[k][j];
    double max = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (d[i][j] < 1e307 && d[i][j] > max) max = d[i][j];
    for (int i = 0; i < n; i++) free(d[i]);
    free(d);
    free(nodes);
    return max;
}

double gmb_graph_eccentricity(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return -1;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return -1; }
    int s = graph_node_pos(nodes, n, node);
    if (s < 0) { free(nodes); return -1; }
    int* dist = (int*)malloc((size_t)n * sizeof(int));
    int* queue = (int*)malloc((size_t)n * sizeof(int));
    if (!dist || !queue) { free(nodes); free(dist); free(queue); return -1; }
    for (int i = 0; i < n; i++) dist[i] = -1;
    dist[s] = 0;
    int qh = 0, qt = 0;
    queue[qt++] = s;
    while (qh < qt)
    {
        int u = queue[qh++];
        for (gmb_graph_edge_internal_t* e = nodes[u]->edges; e; e = e->next)
        {
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v >= 0 && dist[v] < 0) { dist[v] = dist[u] + 1; queue[qt++] = v; }
        }
    }
    int maxd = 0, reached = 0;
    for (int i = 0; i < n; i++)
        if (dist[i] > maxd) maxd = dist[i];
    free(nodes); free(dist); free(queue);
    return maxd > 0 ? (double)maxd : 0;
}

double gmb_graph_clustering_coefficient(int64_t h, const char* node)
{
    gmb_graph_t* g = graph_get(h);
    if (!g || !node) return 0;
    gmb_graph_node_t* f = graph_find_node(g, node);
    if (!f) return 0;
    /* vicini (uscenti) */
    int cnt = 0;
    for (gmb_graph_edge_internal_t* e = f->edges; e; e = e->next) cnt++;
    if (cnt < 2) return 0;
    /* coppie di vicini collegate tra loro (arco in entrambe le direzioni) */
    double links = 0;
    for (gmb_graph_edge_internal_t* e1 = f->edges; e1; e1 = e1->next)
        for (gmb_graph_edge_internal_t* e2 = e1->next; e2; e2 = e2->next)
        {
            if (graph_find_edge(e1->to, e2->to) || graph_find_edge(e2->to, e1->to)) links++;
        }
    double possible = (double)cnt * (cnt - 1) / 2;
    return possible > 0 ? links / possible : 0;
}

gmb_graph_edge_full_t* gmb_graph_edge_list(int64_t h, int* out_len)
{
    *out_len = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g) return NULL;
    int cnt = 0;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next)
        for (gmb_graph_edge_internal_t* e = x->edges; e; e = e->next) cnt++;
    if (cnt == 0) return NULL;
    gmb_graph_edge_full_t* out = (gmb_graph_edge_full_t*)calloc((size_t)cnt, sizeof(gmb_graph_edge_full_t));
    if (!out) return NULL;
    int k = 0;
    for (gmb_graph_node_t* x = g->nodes; x; x = x->next)
        for (gmb_graph_edge_internal_t* e = x->edges; e; e = e->next)
        {
            out[k].from = x->name;
            out[k].to = e->to->name;
            out[k].weight = e->weight;
            out[k].seq = e->seq;
            k++;
        }
    /* ordina per sequenza di inserimento (parità C# GraphManager.Edges: la lista nodi
     * è prepend → senza sort l'ordine era invertito per nodo). */
    for (int i = 0; i < k - 1; i++)
        for (int j = i + 1; j < k; j++)
            if (out[j].seq < out[i].seq) { gmb_graph_edge_full_t t = out[i]; out[i] = out[j]; out[j] = t; }
    *out_len = k;
    return out;
}

/* SCC (Tarjan) — iterativo, con stack di frame. */
typedef struct { int node; gmb_graph_edge_internal_t* edge; } gmb_scc_frame_t;

char*** gmb_graph_scc(int64_t h, int* out_count)
{
    *out_count = 0;
    gmb_graph_t* g = graph_get(h);
    if (!g) return NULL;
    int n = 0;
    gmb_graph_node_t** nodes = graph_nodes_array(g, &n);
    if (!nodes || n == 0) { free(nodes); return NULL; }

    int* index = (int*)malloc((size_t)n * sizeof(int));
    int* low = (int*)malloc((size_t)n * sizeof(int));
    int* on_stack = (int*)calloc((size_t)n, sizeof(int));
    int* stack = (int*)malloc((size_t)(n + 1) * sizeof(int));
    gmb_scc_frame_t* frames = (gmb_scc_frame_t*)malloc((size_t)(n + 1) * sizeof(gmb_scc_frame_t));
    if (!index || !low || !on_stack || !stack || !frames)
    {
        free(nodes); free(index); free(low); free(on_stack); free(stack); free(frames);
        return NULL;
    }
    for (int i = 0; i < n; i++) index[i] = low[i] = -1;

    /* risultati: lista di array */
    char*** result = NULL;
    int res_count = 0, res_cap = 0;
    int counter = 0, sp_stack = 0;

    for (int start = 0; start < n; start++)
    {
        if (index[start] != -1) continue;
        int sp = 0;
        index[start] = low[start] = counter++;
        stack[sp_stack++] = start;
        on_stack[start] = 1;
        frames[sp].node = start;
        frames[sp].edge = nodes[start]->edges;
        sp++;

        while (sp > 0)
        {
            gmb_scc_frame_t* f = &frames[sp - 1];
            int u = f->node;
            if (!f->edge)
            {
                sp--;
                if (low[u] == index[u])
                {
                    /* pop SCC */
                    int scc_size = 0;
                    for (int t = sp_stack - 1; t >= 0 && stack[t] != u; t--) scc_size++;
                    scc_size++;
                    char** scc = (char**)malloc((size_t)(scc_size + 1) * sizeof(char*));
                    if (!scc) goto cleanup;
                    int k = 0;
                    int w;
                    do
                    {
                        w = stack[--sp_stack];
                        on_stack[w] = 0;
                        scc[k++] = nodes[w]->name;
                    } while (w != u);
                    scc[k] = NULL;
                    if (res_count == res_cap)
                    {
                        res_cap = res_cap ? res_cap * 2 : 4;
                        char*** nr = (char***)realloc(result, (size_t)res_cap * sizeof(char**));
                        if (!nr) { free(scc); goto cleanup; }
                        result = nr;
                    }
                    result[res_count++] = scc;
                }
                continue;
            }
            gmb_graph_edge_internal_t* e = f->edge;
            f->edge = e->next;
            int v = graph_node_pos(nodes, n, e->to->name);
            if (v < 0) continue;
            if (index[v] == -1)
            {
                index[v] = low[v] = counter++;
                stack[sp_stack++] = v;
                on_stack[v] = 1;
                frames[sp].node = v;
                frames[sp].edge = nodes[v]->edges;
                sp++;
            }
            else if (on_stack[v])
                low[u] = low[u] < index[v] ? low[u] : index[v];
        }
    }

    /* ordina per taglia descrescente */
    for (int i = 0; i < res_count; i++)
        for (int j = i + 1; j < res_count; j++)
        {
            int ci = 0, cj = 0;
            while (result[i][ci]) ci++;
            while (result[j][cj]) cj++;
            if (cj > ci) { char** tmp = result[i]; result[i] = result[j]; result[j] = tmp; }
        }
    *out_count = res_count;
    free(nodes); free(index); free(low); free(on_stack); free(stack); free(frames);
    return result;

cleanup:
    free(nodes); free(index); free(low); free(on_stack); free(stack); free(frames);
    return NULL;
}

void gmb_graph_free_str_array_array(char*** arr, int count)
{
    if (!arr) return;
    for (int i = 0; i < count; i++) free(arr[i]);
    free(arr);
}

/* ── Val-boxed (C99Native: membri GRAPH che ritornano collezioni) — §0bis,
 * unica sorgente; formato identico al materializzato del VM/C (OBJ_LIST/OBJ_DICT). */

gmb_val_t gmb_graph_bfs_val(int64_t h, const char* start)
{
    int n = 0;
    char** res = gmb_graph_bfs(h, start, &n);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_string(res[i]));
    free(res);
    return gmb_make_val_list_take(&l);
}

gmb_val_t gmb_graph_dfs_val(int64_t h, const char* start)
{
    int n = 0;
    char** res = gmb_graph_dfs(h, start, &n);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++) gmb_list_add(&l, gmb_make_val_string(res[i]));
    free(res);
    return gmb_make_val_list_take(&l);
}

gmb_val_t gmb_graph_neighbors_val(int64_t h, const char* node)
{
    int n = 0;
    gmb_graph_edge_t* es = gmb_graph_neighbors(h, node, &n);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++)
    {
        gmb_dict_t d = gmb_dict_new();
        gmb_dict_set(&d, "node", gmb_make_val_string(es[i].node));
        gmb_dict_set(&d, "weight", gmb_make_val_double(es[i].weight));
        gmb_list_add(&l, gmb_make_val_dict_take(&d));
    }
    free(es);
    return gmb_make_val_list_take(&l);
}

gmb_val_t gmb_graph_dijkstra_val(int64_t h, const char* start)
{
    int n = 0;
    gmb_graph_dist_t* ds = gmb_graph_dijkstra(h, start, &n);
    gmb_list_t l = gmb_list_new();
    for (int i = 0; i < n; i++)
    {
        gmb_dict_t d = gmb_dict_new();
        gmb_dict_set(&d, "node", gmb_make_val_string(ds[i].node));
        gmb_dict_set(&d, "dist", gmb_make_val_double(ds[i].dist));
        gmb_list_add(&l, gmb_make_val_dict_take(&d));
    }
    free(ds);
    return gmb_make_val_list_take(&l);
}
