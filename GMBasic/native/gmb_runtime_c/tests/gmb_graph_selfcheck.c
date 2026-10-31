/* gmb_graph_selfcheck.c — G4: MST (Kruskal), max-flow (Edmonds-Karp), Louvain. */
#include "gmb_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0, g_fails = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fails++; printf("FAIL %d: %s\n", __LINE__, #c); } } while (0)

static void add(int64_t h, const char* a, const char* b, double w)
{
    gmb_graph_add_node_consistent(h, a);
    gmb_graph_add_node_consistent(h, b);
    gmb_graph_add_edge(h, a, b, w);
}

int main(void)
{
    gmb_graph_init();

    /* ── MST: A-B(1), B-C(2), A-C(3), C-D(1) → A-B, B-C, C-D, peso 4 ── */
    {
        int64_t h = gmb_graph_create();
        add(h, "A", "B", 1); add(h, "B", "C", 2); add(h, "A", "C", 3); add(h, "C", "D", 1);
        int m = 0;
        gmb_graph_edge_full_t* mst = gmb_graph_mst(h, &m);
        CHECK(m == 3);
        CHECK(gmb_graph_mst_weight(h) == 4.0);
        /* gli archi del MST devono essere A-B, B-C, C-D (non A-C) */
        int has_ac = 0, has_ab = 0, has_bc = 0, has_cd = 0;
        for (int i = 0; i < m; i++) {
            const char* f = mst[i].from; const char* t = mst[i].to;
            if ((!strcmp(f, "A") && !strcmp(t, "C")) || (!strcmp(f, "C") && !strcmp(t, "A"))) has_ac = 1;
            if ((!strcmp(f, "A") && !strcmp(t, "B")) || (!strcmp(f, "B") && !strcmp(t, "A"))) has_ab = 1;
            if ((!strcmp(f, "B") && !strcmp(t, "C")) || (!strcmp(f, "C") && !strcmp(t, "B"))) has_bc = 1;
            if ((!strcmp(f, "C") && !strcmp(t, "D")) || (!strcmp(f, "D") && !strcmp(t, "C"))) has_cd = 1;
        }
        CHECK(has_ab && has_bc && has_cd && !has_ac);
        free(mst);
        gmb_graph_free(h);
    }

    /* ── Max-flow: s->a(3), s->b(3), a->t(3), b->t(3) → 6 ── */
    {
        int64_t h = gmb_graph_create();
        add(h, "s", "a", 3); add(h, "s", "b", 3); add(h, "a", "t", 3); add(h, "b", "t", 3);
        CHECK(gmb_graph_maxflow(h, "s", "t") == 6.0);
        /* collo di bottiglia: s->a(3), a->t(1) → 1 (nessun altro percorso) */
        int64_t h2 = gmb_graph_create();
        add(h2, "s", "a", 3); add(h2, "a", "t", 1);
        CHECK(gmb_graph_maxflow(h2, "s", "t") == 1.0);
        CHECK(gmb_graph_maxflow(h, "s", "z") == -1.0);   /* nodo inesistente */
        gmb_graph_free(h); gmb_graph_free(h2);
    }

    /* ── Louvain: due clique {A,B,C} e {D,E,F} unite da C-D(1) → 2 community ── */
    {
        int64_t h = gmb_graph_create();
        add(h, "A", "B", 1); add(h, "B", "C", 1); add(h, "A", "C", 1);
        add(h, "D", "E", 1); add(h, "E", "F", 1); add(h, "D", "F", 1);
        add(h, "C", "D", 1);
        int n = 0;
        gmb_graph_comp_t* cm = gmb_graph_louvain(h, 1.0, &n);
        CHECK(n == 6);
        int cA = -1, cB = -1, cC = -1, cD = -1, cE = -1, cF = -1;
        for (int i = 0; i < n; i++) {
            if (!strcmp(cm[i].node, "A")) cA = cm[i].component;
            else if (!strcmp(cm[i].node, "B")) cB = cm[i].component;
            else if (!strcmp(cm[i].node, "C")) cC = cm[i].component;
            else if (!strcmp(cm[i].node, "D")) cD = cm[i].component;
            else if (!strcmp(cm[i].node, "E")) cE = cm[i].component;
            else if (!strcmp(cm[i].node, "F")) cF = cm[i].component;
        }
        CHECK(cA == cB && cB == cC);          /* clique 1 coesa */
        CHECK(cD == cE && cE == cF);          /* clique 2 coesa */
        CHECK(cA != cD);                       /* le due clique sono separate */
        free(cm);
        gmb_graph_free(h);
    }

    gmb_graph_shutdown();
    printf("gmb_graph_selfcheck: %d check, %d fail\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
