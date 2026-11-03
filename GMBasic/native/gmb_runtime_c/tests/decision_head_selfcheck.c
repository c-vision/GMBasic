/* decision_head_selfcheck.c — testa di scoring C (T3): formula, confidence, artifact JSON.
 * Model-free: nessun motore, solo numeri e file piccoli. Exit 0 = PASS. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gmb_ai_decide.h"
#include "gmb_decision_head.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)
#define NEAR(a, b, tol) (fabsf((float)(a) - (float)(b)) <= (tol))

static int write_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

int main(void) {
    gmb_decision_head_t head;
    gmb_decision_head_init(&head);
    CHECK(head.head_T == 1.0f && head.T_kind[0] == 1.0f && head.T_kind[2] == 1.0f, "init T");
    CHECK(head.token_id[0] == -1 && head.calibrated == 0, "init sentinelle");

    /* Slot canonici. */
    CHECK(gmb_decision_head_slot(&gmb_decision_slots_noul[0]) == 0, "slot false");
    CHECK(gmb_decision_head_slot(&gmb_decision_slots_noul[1]) == 1, "slot true");
    CHECK(gmb_decision_head_slot(&gmb_decision_slots_score[5]) == 7, "slot 5");
    CHECK(gmb_decision_head_slot(&gmb_decision_slots_choice[0]) == 8, "slot A");
    CHECK(gmb_decision_head_slot(&gmb_decision_slots_choice[15]) == 23, "slot P");

    /* Softmax identità: lp [0, ln3] -> [0.25, 0.75], conf 0.5. */
    float lp2[2] = { 0.0f, (float)log(3.0) };
    float p2[2] = { 0 };
    int arg = -1;
    double conf = -1;
    CHECK(gmb_decision_head_apply(&head, GMB_DECISION_NOUL, lp2, gmb_decision_slots_noul, 2,
                                  p2, &arg, &conf) == 0, "apply identità");
    CHECK(NEAR(p2[0], 0.25f, 1e-5f) && NEAR(p2[1], 0.75f, 1e-5f), "p identità");
    CHECK(arg == 1 && NEAR(conf, 0.5, 1e-5), "argmax+confidence identità");

    /* Bias true=+1, lp uguali -> p_true=e/(1+e). */
    gmb_decision_head_init(&head);
    head.bias[1] = 1.0f;
    float lp0[2] = { 0.0f, 0.0f };
    CHECK(gmb_decision_head_apply(&head, GMB_DECISION_NOUL, lp0, gmb_decision_slots_noul, 2,
                                  p2, &arg, &conf) == 0, "apply bias");
    float pe = 2.718281828f / (1.0f + 2.718281828f);
    CHECK(NEAR(p2[1], pe, 1e-4f) && arg == 1, "bias true");
    CHECK(NEAR(conf, 2.0 * pe - 1.0, 1e-4), "confidence bias");

    /* head_T=2 su [0, ln3] -> sqrt(3)/(1+sqrt(3)). */
    gmb_decision_head_init(&head);
    head.head_T = 2.0f;
    CHECK(gmb_decision_head_apply(&head, GMB_DECISION_NOUL, lp2, gmb_decision_slots_noul, 2,
                                  p2, &arg, &conf) == 0, "apply head_T");
    float want = 1.7320508f / (1.0f + 1.7320508f);
    CHECK(NEAR(p2[1], want, 1e-4f), "head_T");

    /* T_kind per kind: SCORE=2, NOUL resta 1. */
    gmb_decision_head_init(&head);
    head.T_kind[2] = 2.0f;
    const gmb_decision_option_t score01[2] = {
        { "0", "0", -1, 0, 1 }, { "1", "1", -1, 0, 1 },
    };
    CHECK(gmb_decision_head_apply(&head, GMB_DECISION_SCORE, lp2, score01, 2, p2, &arg, &conf) == 0, "apply T_kind");
    CHECK(NEAR(p2[1], want, 1e-4f), "T_kind score");
    CHECK(gmb_decision_head_apply(&head, GMB_DECISION_NOUL, lp2, gmb_decision_slots_noul, 2, p2, &arg, &conf) == 0, "apply T_kind noul");
    CHECK(NEAR(p2[1], 0.75f, 1e-4f), "T_kind noul resta 1");

    /* Artifact: oggetto bias/T + verbalizer_ids + calibrated:false. */
    char tmpl[] = "/tmp/gmb-head-XXXXXX";
    char* dir = mkdtemp(tmpl);
    CHECK(dir != NULL, "mkdtemp");
    if (dir) {
        char path[512];
        snprintf(path, sizeof path, "%s/scoring_head.json", dir);
        CHECK(write_file(path,
            "{\"bias\":{\"false\":0.0,\"true\":1.0},\"head_T\":2.0,"
            "\"T_kind\":{\"noul\":0.5,\"choice\":1.0,\"score\":1.0},"
            "\"verbalizer_ids\":{\"true\":1904},\"calibrated\":false}") == 0, "write object");
        int rc = gmb_decision_head_load(dir, &head);
        CHECK(rc == 1, "load object");
        CHECK(NEAR(head.bias[1], 1.0f, 1e-6f) && NEAR(head.head_T, 2.0f, 1e-6f), "object bias/T");
        CHECK(NEAR(head.T_kind[0], 0.5f, 1e-6f) && head.token_id[1] == 1904, "object kind/ids");
        CHECK(head.calibrated == 0, "object calibrated false");

        /* Array posizionale + calibrated default. */
        CHECK(write_file(path,
            "{\"bias\":[0.0,1.0],\"head_T\":1.0,\"T_kind\":[1.0,1.0,2.0],"
            "\"calibrated\":true}") == 0, "write array");
        rc = gmb_decision_head_load(dir, &head);
        CHECK(rc == 1 && NEAR(head.bias[1], 1.0f, 1e-6f) && NEAR(head.T_kind[2], 2.0f, 1e-6f), "array bias/T");
        CHECK(head.calibrated == 1, "calibrated default");

        /* File malformato: errore esplicito, non identità silenziosa. */
        CHECK(write_file(path, "{\"bias\":[0,1]") == 0, "write malformed");
        CHECK(gmb_decision_head_load(dir, &head) == -1, "malformed -> -1");

        /* Assente: 0. */
        unlink(path);
        CHECK(gmb_decision_head_load(dir, &head) == 0, "absent -> 0");
        rmdir(dir);
    }

    printf("DECISION-HEAD-SELFCHECK %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
