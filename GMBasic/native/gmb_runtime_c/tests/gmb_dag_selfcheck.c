/* gmb_dag_selfcheck.c — parità C di tests/AI.Tests/AgentDagTests (AgentDag.cs):
 * fan-out/fan-in a concorrenza bounded + ciclo => errore esplicito.
 * Exit 0 = ok. */
#include "gmb_dag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

/* compute: input già una riga l'uno; appende ";<id>" oppure synth: sostituisce
 * \n con | e incapsula in SYNTH(...). */
static char* comp_append(const char* input, void* user)
{
    const char* tag = (const char*)user;
    size_t n = (input ? strlen(input) : 0) + strlen(tag) + 2;
    char* r = (char*)malloc(n);
    if (!r) return NULL;
    if (input && input[0]) snprintf(r, n, "%s;%s", input, tag);
    else snprintf(r, n, "%s", tag);
    return r;
}

static char* comp_root(const char* input, void* user)
{
    (void)input; (void)user;
    return strdup("base");
}

static char* comp_synth(const char* input, void* user)
{
    (void)user;
    const char* s = input ? input : "";
    size_t n = strlen(s) + 16;
    char* r = (char*)malloc(n);
    if (!r) return NULL;
    snprintf(r, n, "SYNTH(");
    for (const char* p = s; *p; p++) {
        size_t l = strlen(r);
        if (*p == '\n') { r[l] = '|'; r[l + 1] = '\0'; }
        else { r[l] = *p; r[l + 1] = '\0'; }
    }
    strcat(r, ")");
    return r;
}

static void test_fanout_fanin(void)
{
    static const char* rootDep0[] = {};
    static const char* wDep[] = { "root" };
    static const char* synthDep[] = { "w0", "w1", "w2" };
    gmb_dag_node_t nodes[5];
    memset(nodes, 0, sizeof(nodes));
    nodes[0].id = "root"; nodes[0].deps = rootDep0; nodes[0].ndeps = 0; nodes[0].compute = comp_root;
    nodes[1].id = "w0"; nodes[1].deps = wDep; nodes[1].ndeps = 1; nodes[1].compute = comp_append; nodes[1].user = (void*)"w0";
    nodes[2].id = "w1"; nodes[2].deps = wDep; nodes[2].ndeps = 1; nodes[2].compute = comp_append; nodes[2].user = (void*)"w1";
    nodes[3].id = "w2"; nodes[3].deps = wDep; nodes[3].ndeps = 1; nodes[3].compute = comp_append; nodes[3].user = (void*)"w2";
    nodes[4].id = "synth"; nodes[4].deps = synthDep; nodes[4].ndeps = 3; nodes[4].compute = comp_synth;

    gmb_dag_outcome_t o;
    gmb_dag_init_outcome(&o);
    CHECK(gmb_dag_run(nodes, 5, 3, &o) == 1, "fanout: run");
    if (o.ok) {
        CHECK(o.peak <= 3, "fanout: peak<=3");
        int gotRoot = 0, gotW0 = 0, gotSynth = 0;
        for (int i = 0; i < o.n; i++) {
            if (strcmp(o.ids[i], "root") == 0 && strcmp(o.results[i], "base") == 0) gotRoot = 1;
            if (strcmp(o.ids[i], "w0") == 0 && strstr(o.results[i], "w0") != NULL) gotW0 = 1;
            if (strcmp(o.ids[i], "synth") == 0
                && strcmp(o.results[i], "SYNTH(base;w0|base;w1|base;w2)") == 0) gotSynth = 1;
        }
        CHECK(gotRoot, "fanout: root=base");
        CHECK(gotW0, "fanout: w0 contiene w0");
        CHECK(gotSynth, "fanout: synth esatto");
    } else {
        printf("FAIL: fanout ok=false error=%s\n", o.error ? o.error : "?");
        failures++;
    }
    gmb_dag_free_outcome(&o);
}

static void test_cycle(void)
{
    static const char* aDep[] = { "b" };
    static const char* bDep[] = { "a" };
    gmb_dag_node_t nodes[2];
    memset(nodes, 0, sizeof(nodes));
    nodes[0].id = "a"; nodes[0].deps = aDep; nodes[0].ndeps = 1; nodes[0].compute = comp_append; nodes[0].user = (void*)"a";
    nodes[1].id = "b"; nodes[1].deps = bDep; nodes[1].ndeps = 1; nodes[1].compute = comp_append; nodes[1].user = (void*)"b";

    gmb_dag_outcome_t o;
    gmb_dag_init_outcome(&o);
    CHECK(gmb_dag_run(nodes, 2, 8, &o) == 1, "cycle: run");
    CHECK(o.ok == 0, "cycle: ok=false");
    CHECK(o.error && strstr(o.error, "ciclo") != NULL, "cycle: errore contiene 'ciclo'");
    gmb_dag_free_outcome(&o);
}

static void test_duplicate_and_missing_dep(void)
{
    static const char* noDep[] = {};
    static const char* missDep[] = { "zzz" };
    gmb_dag_node_t n1[2];
    memset(n1, 0, sizeof(n1));
    n1[0].id = "x"; n1[0].deps = noDep; n1[0].ndeps = 0; n1[0].compute = comp_root;
    n1[1].id = "X"; n1[1].deps = noDep; n1[1].ndeps = 0; n1[1].compute = comp_root; /* case-insensitive dup */
    gmb_dag_outcome_t o;
    gmb_dag_init_outcome(&o);
    CHECK(gmb_dag_run(n1, 2, 4, &o) == 1 && o.ok == 0
          && o.error && strstr(o.error, "duplicato") != NULL, "dup id");
    gmb_dag_free_outcome(&o);

    gmb_dag_node_t n2[1];
    memset(n2, 0, sizeof(n2));
    n2[0].id = "x"; n2[0].deps = missDep; n2[0].ndeps = 1; n2[0].compute = comp_root;
    gmb_dag_init_outcome(&o);
    CHECK(gmb_dag_run(n2, 1, 4, &o) == 1 && o.ok == 0
          && o.error && strstr(o.error, "inesistente") != NULL, "dep mancante");
    gmb_dag_free_outcome(&o);
}

int main(void)
{
    test_fanout_fanin();
    test_cycle();
    test_duplicate_and_missing_dep();
    if (failures == 0) { printf("gmb_dag_selfcheck: PASS\n"); return 0; }
    printf("gmb_dag_selfcheck: FAIL (%d)\n", failures);
    return 1;
}
