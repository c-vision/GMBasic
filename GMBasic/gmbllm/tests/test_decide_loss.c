/* test_decide_loss.c — self-check model-free delle loss JEV (T4).
 * Confronta il gradiente analitico con le differenze finite (eps=1e-4) per CE/Brier/RPS
 * e CE+RPS, verifica label smoothing, maschera e input invalidi. Exit 0 = PASS. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "llm_decide_loss.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static double loss_only(const llm_decide_loss_sample_t* s, const llm_decide_loss_opts_t* o) {
    llm_decide_loss_result_t r;
    if (llm_decide_loss_compute(s, o, &r) != 0) return NAN;
    return r.loss;
}

static void grad_check(const char* name, llm_decide_loss_sample_t s,
                       const llm_decide_loss_opts_t* o) {
    llm_decide_loss_result_t r;
    CHECK(llm_decide_loss_compute(&s, o, &r) == 0, name);
    const double eps = 1e-4;
    for (int k = 0; k < s.n_options; k++) {
        double orig = s.logits[k];
        s.logits[k] = (float)(orig + eps); double lp = loss_only(&s, o);
        s.logits[k] = (float)(orig - eps); double lm = loss_only(&s, o);
        s.logits[k] = (float)orig;
        double num = (lp - lm) / (2.0 * eps);
        if (fabs(num - r.grad[k]) > 2e-3) {
            printf("FAIL: %s grad[%d] analitico=%.6f numerico=%.6f (loss=%.6f)\n",
                   name, k, r.grad[k], num, r.loss);
            fails++;
        }
    }
}

int main(void) {
    llm_decide_loss_sample_t s;
    memset(&s, 0, sizeof s);
    s.n_options = 3;
    s.label = 1;
    s.mask = 0;
    s.logits[0] = 0.7f; s.logits[1] = -0.2f; s.logits[2] = 1.1f;

    llm_decide_loss_opts_t ce = { LLM_DECIDE_CRIT_CE, 0.0, 0.0 };
    grad_check("CE", s, &ce);

    llm_decide_loss_opts_t ce_ls = { LLM_DECIDE_CRIT_CE, 0.1, 0.0 };
    grad_check("CE+LS", s, &ce_ls);

    llm_decide_loss_opts_t brier = { LLM_DECIDE_CRIT_BRIER, 0.0, 0.0 };
    grad_check("Brier", s, &brier);

    llm_decide_loss_opts_t rps = { LLM_DECIDE_CRIT_RPS, 0.0, 0.0 };
    grad_check("RPS", s, &rps);

    llm_decide_loss_opts_t mix = { LLM_DECIDE_CRIT_CE_RPS, 0.0, 0.5 };
    grad_check("CE+RPS", s, &mix);

    /* Maschera: opzione 1 esclusa, label 0; p1=0 e grad1=0. */
    llm_decide_loss_sample_t m = s;
    m.mask = 0b101u;
    m.label = 0;
    llm_decide_loss_result_t mr;
    CHECK(llm_decide_loss_compute(&m, &ce, &mr) == 0, "mask compute");
    CHECK(fabs(mr.grad[1]) < 1e-12, "mask grad escluso = 0");
    grad_check("CE mask", m, &ce);

    /* Loss finita e non negativa. */
    CHECK(mr.loss >= 0.0 && isfinite(mr.loss), "loss finita");

    /* Input invalidi. */
    llm_decide_loss_sample_t bad = s;
    bad.label = 3;
    CHECK(llm_decide_loss_compute(&bad, &ce, &mr) == -1, "label invalido");
    bad = s; bad.n_options = 0;
    CHECK(llm_decide_loss_compute(&bad, &ce, &mr) == -1, "n invalido");
    bad = s; bad.mask = 0b010u; bad.label = 0;
    CHECK(llm_decide_loss_compute(&bad, &ce, &mr) == -1, "label mascherata");

    printf("DECIDE-LOSS-SELFCHECK %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
