/* test_wsp_fixture.c — W4 DoD (2): sulle fixture compilate l'analizzatore ritrova le teste note.
 * Precisione/richiamo riportati su N seed, insieme alla BASELINE ovvia (norma L2 di O). */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_analyze.h"
#include "llmwsp_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { int tp, fp, fn; } PR;
static void pr_add(PR *p, int pred, int truth) { if (pred && truth) p->tp++; else if (pred) p->fp++; else if (truth) p->fn++; }
static double prec(PR p) { return p.tp + p.fp ? (double)p.tp / (p.tp + p.fp) : 1; }
static double rec(PR p) { return p.tp + p.fn ? (double)p.tp / (p.tp + p.fn) : 1; }

int main(void) {
    const char *kinds[2] = {"copy", "induction"};
    const int SEEDS = 40;
    PR copy_pr = {0}, comp_pr = {0}, base_pr = {0};
    for (int ki = 0; ki < 2; ki++)
        for (int seed = 1; seed <= SEEDS; seed++) {
            char dir[] = "/tmp/wsp_fx_XXXXXX", p[300], err[300];
            if (!mkdtemp(dir)) { fails++; continue; }
            llmwsp_fx_truth tr;
            CHECK(llmwsp_fixture_compile(kinds[ki], dir, (unsigned)seed, &tr, err, sizeof err) == 0, "compile: %s", err);
            llmwsp_store *s; llmwsp_doc *d;
            CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open: %s", err);
            snprintf(p, sizeof p, "%s/config.json", dir);
            CHECK(llmwsp_doc_build(s, p, &d, err, sizeof err) == 0 && llmwsp_doc_check(d, err, sizeof err) == 0, "doc: %s", err);
            double wr[4], wi[4], on[4];
            /* copy heads: sum_ratio > 0.9 */
            double best_norm = -1; int best_l = -1, best_h = -1;
            for (int hi = 0; hi < tr.nheads; hi++) {
                int l = tr.hd[hi].layer, h = tr.hd[hi].head;
                llmwsp_an_circuit_spectrum(s, d, l, h, LLMWSP_CIRCUIT_OV, wr, wi);
                double sr, fp; llmwsp_an_copy_scores(wr, wi, 4, &sr, &fp);
                int truth = !strcmp(tr.hd[hi].role, "copy") || !strcmp(tr.hd[hi].role, "induction");
                pr_add(&copy_pr, sr > 0.9, truth);
                llmwsp_an_head_norms(s, d, l, NULL, NULL, NULL, on);
                if (on[h] > best_norm) { best_norm = on[h]; best_l = l; best_h = h; }
            }
            /* baseline: "la testa con ||O|| massima è la copy head" (1 predizione per fixture) */
            for (int hi = 0; hi < tr.nheads; hi++) {
                int truth = !strcmp(tr.hd[hi].role, "copy") || !strcmp(tr.hd[hi].role, "induction");
                pr_add(&base_pr, tr.hd[hi].layer == best_l && tr.hd[hi].head == best_h, truth);
            }
            /* composizione: tutte le coppie (l1<l2) e which in q,k,v con PK >= 3.5 */
            if (tr.layers > 1)
                for (int h1 = 0; h1 < 4; h1++) for (int h2 = 0; h2 < 4; h2++) for (int w = 0; w < 3; w++) {
                    char wh = "qkv"[w]; double c[4], pk;
                    llmwsp_an_compose_cosines(s, d, 0, h1, 1, h2, wh, c, &pk);
                    int truth = 0;
                    for (int e = 0; e < tr.nedges; e++) if (tr.edges[e].h1 == h1 && tr.edges[e].h2 == h2 && tr.edges[e].which == wh) truth = 1;
                    pr_add(&comp_pr, pk >= 3.5, truth);
                }
            llmwsp_doc_free(d); llmwsp_store_close(s);
            snprintf(p, sizeof p, "%s/model.safetensors", dir); unlink(p);
            snprintf(p, sizeof p, "%s/config.json", dir); unlink(p); rmdir(dir);
        }
    printf("copy heads  (sum_ratio>0.9, %d fixture): precisione %.3f richiamo %.3f  [tp=%d fp=%d fn=%d]\n", 2 * SEEDS, prec(copy_pr), rec(copy_pr), copy_pr.tp, copy_pr.fp, copy_pr.fn);
    printf("composizione (PK>=3.5, %d fixture ind.): precisione %.3f richiamo %.3f  [tp=%d fp=%d fn=%d]\n", SEEDS, prec(comp_pr), rec(comp_pr), comp_pr.tp, comp_pr.fp, comp_pr.fn);
    printf("baseline ||O|| max:                      precisione %.3f richiamo %.3f  [tp=%d fp=%d fn=%d]\n", prec(base_pr), rec(base_pr), base_pr.tp, base_pr.fp, base_pr.fn);
    CHECK(prec(copy_pr) >= 0.95 && rec(copy_pr) == 1.0, "copy heads sotto soglia");
    CHECK(prec(comp_pr) >= 0.95 && rec(comp_pr) == 1.0, "composizione sotto soglia");
    printf(fails ? "WSP FIXTURE: %d FALLITI\n" : "WSP FIXTURE: OK\n", fails);
    return fails != 0;
}
