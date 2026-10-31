/* ============================================================================
 * gmb_dag.c — DAG generico a fan-out/fan-in bounded. Parità C di
 * common/AI/Agents/AgentDag.cs (Fan-out parallelo a concorrenza bounded;
 * fan-in: ogni nodo riceve i risultati dei suoi DependsOn, una riga l'uno,
 * nell'ordine del DependsOn). Regola §0bis: unica implementazione condivisa
 * del runtime C — VM/C, C99Native la CHIAMANO, mai copie.
 * ============================================================================ */
#include "gmb_dag.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char* gmb_dag_strdup_own(const char* s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char* d = (char*)malloc(n + 1);
    if (!d) return NULL;
    memcpy(d, s, n + 1);
    return d;
}

int gmb_dag_init_outcome(gmb_dag_outcome_t* out)
{
    if (!out) return 0;
    out->ok = 0; out->error = NULL; out->n = 0; out->ids = NULL;
    out->results = NULL; out->peak = 0;
    return 1;
}

void gmb_dag_free_outcome(gmb_dag_outcome_t* out)
{
    if (!out) return;
    free(out->error);
    if (out->ids) {
        for (int i = 0; i < out->n; i++) free(out->ids[i]);
        free(out->ids);
    }
    if (out->results) {
        for (int i = 0; i < out->n; i++) free(out->results[i]);
        free(out->results);
    }
    out->ok = 0; out->error = NULL; out->n = 0; out->ids = NULL;
    out->results = NULL; out->peak = 0;
}

/* Concorrenza: esecutore "un thread per nodo" con gate bounded e condition
 * broadcast su un unico mutex (semplice e corretto: n piccolo, DAG aciclico
 * già verificato — nessun attesa circolare). */
typedef struct gmb_dag_exec {
    const gmb_dag_node_t* nodes;
    int n;
    int maxc;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int running;
    int peak;
    int fail;              /* errore interno (thread create) */
    char* failmsg;
    char* done;            /* [n] */
    char** results;        /* [n] risultati prodotti */
    int* idxof;            /* [n] indice reale per thread worker */
} gmb_dag_exec_t;

static int gmb_dag_find_id(const gmb_dag_node_t* nodes, int n, const char* id)
{
    for (int i = 0; i < n; i++)
        if (nodes[i].id && strcasecmp(nodes[i].id, id) == 0) return i;
    return -1;
}

static char* gmb_dag_build_input(gmb_dag_exec_t* e, int nodeIdx)
{
    /* Risultati dei DependsOn, una riga l'uno, nell'ordine del DependsOn. */
    const gmb_dag_node_t* spec = &e->nodes[nodeIdx];
    size_t cap = 64, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (int d = 0; d < spec->ndeps; d++) {
        int di = gmb_dag_find_id(e->nodes, e->n, spec->deps[d]);
        const char* r = (di >= 0 && e->results[di]) ? e->results[di] : "";
        size_t rl = strlen(r);
        if (len + rl + 2 > cap) {
            cap = (len + rl + 2) * 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        if (len > 0) buf[len++] = '\n';
        memcpy(buf + len, r, rl);
        len += rl;
        buf[len] = '\0';
    }
    return buf;
}

static int gmb_dag_deps_done(gmb_dag_exec_t* e, int nodeIdx)
{
    const gmb_dag_node_t* spec = &e->nodes[nodeIdx];
    for (int d = 0; d < spec->ndeps; d++) {
        int di = gmb_dag_find_id(e->nodes, e->n, spec->deps[d]);
        if (di < 0 || !e->done[di]) return 0;
    }
    return 1;
}

typedef struct gmb_dag_worker_arg {
    gmb_dag_exec_t* e;
    int idx;
} gmb_dag_worker_arg_t;

static void* gmb_dag_worker_run(void* p)
{
    gmb_dag_worker_arg_t* a = (gmb_dag_worker_arg_t*)p;
    gmb_dag_exec_t* e = a->e;
    int idx = a->idx;

    pthread_mutex_lock(&e->mu);
    while (!e->fail) {
        if (gmb_dag_deps_done(e, idx)) break;
        pthread_cond_wait(&e->cv, &e->mu);
    }
    if (e->fail) { pthread_mutex_unlock(&e->mu); free(p); return NULL; }
    while (e->running >= e->maxc) pthread_cond_wait(&e->cv, &e->mu);
    e->running++;
    if (e->running > e->peak) e->peak = e->running;
    pthread_mutex_unlock(&e->mu);

    char* input = gmb_dag_build_input(e, idx);
    char* out = e->nodes[idx].compute ? e->nodes[idx].compute(input ? input : "", e->nodes[idx].user) : NULL;
    free(input);
    if (!out) out = gmb_dag_strdup_own("");

    pthread_mutex_lock(&e->mu);
    e->results[idx] = out;
    e->done[idx] = 1;
    e->running--;
    pthread_cond_broadcast(&e->cv);
    pthread_mutex_unlock(&e->mu);
    free(p);
    return NULL;
}

int gmb_dag_run(const gmb_dag_node_t* nodes, int n, int max_concurrency,
                gmb_dag_outcome_t* out)
{
    if (!nodes || n < 1 || !out) return 0;
    if (max_concurrency < 1) max_concurrency = 1;

    /* 1. duplicati (case-insensitive) */
    for (int i = 0; i < n; i++) {
        if (!nodes[i].id || !nodes[i].compute)
            { out->ok = 0; return 1; }
        for (int j = 0; j < i; j++)
            if (strcasecmp(nodes[i].id, nodes[j].id) == 0) {
                char msg[160];
                snprintf(msg, sizeof(msg), "id duplicato %s", nodes[i].id);
                out->ok = 0; out->error = gmb_dag_strdup_own(msg); return 1;
            }
    }
    /* 2. esistenza dipendenze */
    for (int i = 0; i < n; i++)
        for (int d = 0; d < nodes[i].ndeps; d++)
            if (gmb_dag_find_id(nodes, n, nodes[i].deps[d]) < 0) {
                char msg[200];
                snprintf(msg, sizeof(msg), "dipendenza '%s' di '%s' inesistente",
                         nodes[i].deps[d], nodes[i].id);
                out->ok = 0; out->error = gmb_dag_strdup_own(msg); return 1;
            }
    /* 3. Kahn (ordine topologico) */
    int* indeg = (int*)calloc((size_t)n, sizeof(int));
    int* q = (int*)malloc((size_t)n * sizeof(int));
    int* order = (int*)malloc((size_t)n * sizeof(int));
    if (!indeg || !q || !order) { free(indeg); free(q); free(order);
        out->ok = 0; return 1; }
    int qh = 0, qt = 0, oc = 0;
    for (int i = 0; i < n; i++) indeg[i] = nodes[i].ndeps;
    for (int i = 0; i < n; i++) if (indeg[i] == 0) q[qt++] = i;
    while (qh < qt) {
        int c = q[qh++];
        order[oc++] = c;
        for (int i = 0; i < n; i++)
            if (indeg[i] > 0 && gmb_dag_find_id(nodes, n, nodes[c].id) >= 0
                && i != c) {
                for (int d = 0; d < nodes[i].ndeps; d++)
                    if (strcasecmp(nodes[i].deps[d], nodes[c].id) == 0) {
                        if (--indeg[i] == 0) q[qt++] = i;
                        break;
                    }
            }
    }
    if (oc != n) {
        free(indeg); free(q); free(order);
        out->ok = 0; out->error = gmb_dag_strdup_own("ciclo nel DAG");
        return 1;
    }
    free(indeg); free(q);

    /* 4. esecuzione con thread per nodo, gate bounded */
    gmb_dag_exec_t e;
    memset(&e, 0, sizeof(e));
    e.nodes = nodes; e.n = n; e.maxc = max_concurrency;
    e.done = (char*)calloc((size_t)n, 1);
    e.results = (char**)calloc((size_t)n, sizeof(char*));
    if (!e.done || !e.results) {
        free(e.done); free(e.results); free(order);
        out->ok = 0; out->error = gmb_dag_strdup_own("memoria esaurita");
        return 1;
    }
    pthread_mutex_init(&e.mu, NULL);
    pthread_cond_init(&e.cv, NULL);

    pthread_t* th = (pthread_t*)malloc((size_t)n * sizeof(pthread_t));
    int started = 0;
    for (int k = 0; k < n; k++) {
        gmb_dag_worker_arg_t* a = (gmb_dag_worker_arg_t*)malloc(sizeof(*a));
        if (!a) { e.fail = 1; break; }
        a->e = &e; a->idx = order[k];
        if (pthread_create(&th[started], NULL, gmb_dag_worker_run, a) != 0) {
            free(a); e.fail = 1; break;
        }
        started++;
    }
    if (e.fail) {
        pthread_mutex_lock(&e.mu);
        e.failmsg = gmb_dag_strdup_own("thread: avvio fallito");
        pthread_cond_broadcast(&e.cv);
        pthread_mutex_unlock(&e.mu);
    }
    for (int k = 0; k < started; k++) pthread_join(th[k], NULL);
    free(th);

    if (e.fail) {
        pthread_mutex_destroy(&e.mu);
        pthread_cond_destroy(&e.cv);
        for (int i = 0; i < n; i++) free(e.results[i]);
        free(e.results); free(e.done); free(order);
        out->ok = 0; out->error = e.failmsg ? e.failmsg : gmb_dag_strdup_own("thread: avvio fallito");
        return 1;
    }

    out->ok = 1;
    out->peak = e.peak;
    out->n = n;
    out->ids = (char**)calloc((size_t)n, sizeof(char*));
    out->results = (char**)calloc((size_t)n, sizeof(char*));
    if (out->ids && out->results) {
        for (int i = 0; i < n; i++) {
            out->ids[i] = gmb_dag_strdup_own(nodes[i].id);
            out->results[i] = e.results[i] ? e.results[i] : gmb_dag_strdup_own("");
            e.results[i] = NULL; /* trasferita */
        }
    }
    for (int i = 0; i < n; i++) free(e.results[i]);
    free(e.results); free(e.done); free(order);
    pthread_mutex_destroy(&e.mu);
    pthread_cond_destroy(&e.cv);
    return 1;
}
