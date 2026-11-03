/* test_decide_calib.c — self-check model-free della calibrazione T5 (PUL).
 * Dati sintetici deterministici (LCG): un modello "overconfident" con T*=2 deve essere
 * recuperato dal fit; Brier/ECE scendono, l'accuracy non cambia. Exit 0 = PASS. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llm_decide_calib.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

static unsigned long long rng_state = 12345678901234567ULL;
static double rng_double(void) {
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)((rng_state >> 11) & 0x1FFFFFFFFFFFFFULL) / (double)0x20000000000000ULL;
}

/* Genera `n` domande (2 opzioni): true_lp=[0,1.5], label dal vero softmax,
 * logprob osservati = true_lp*sharpness + jitter deterministico. Un modello
 * "overconfident" ha sharpness>1 e va corretto con T≈sharpness. */
static void make_samples(llm_decide_sample_t* s, int n, double model_temp, double slot_bias, int bias_kind) {
    double p1 = exp(1.5) / (1.0 + exp(1.5));
    for (int i = 0; i < n; i++) {
        s[i].kind = i % 3;
        s[i].n_options = 2;
        double u = rng_double();
        s[i].label = (u < p1) ? 1 : 0;
        double j0 = (rng_double() - 0.5) * 0.2;
        double j1 = (rng_double() - 0.5) * 0.2;
        s[i].logprob[0] = (float)(0.0 * model_temp + j0);
        /* Bias identificabile: solo metà delle domande del kind riceve lo shift (una T
         * per-kind assorbirebbe uno shift uniforme, rendendo il bias non osservabile). */
        double sb = (bias_kind >= 0 && s[i].kind == bias_kind && (i % 2 == 0)) ? slot_bias : 0.0;
        s[i].logprob[1] = (float)(1.5 * model_temp + sb + j1);
    }
}

/* CHOICE a 3 opzioni: vero softmax([1,0,-1]), logprob osservati con +1 spurio
 * sull'opzione 2. Una T non può correggere uno sbilanciamento NON uniforme: il bias
 * dello slot C (8+2=10) deve tornare ~ -1. */
static void make_choice_bias_samples(llm_decide_sample_t* s, int n) {
    double l[3] = { 1.0, 0.0, -1.0 };
    double mx = 1.0, sum = 0.0, p[3];
    for (int k = 0; k < 3; k++) { p[k] = exp(l[k] - mx); sum += p[k]; }
    for (int k = 0; k < 3; k++) p[k] /= sum;
    for (int i = 0; i < n; i++) {
        double u = rng_double();
        int label = (u < p[0]) ? 0 : (u < p[0] + p[1]) ? 1 : 2;
        s[i].kind = 1;
        s[i].n_options = 3;
        s[i].label = label;
        s[i].logprob[0] = (float)(l[0] + (rng_double() - 0.5) * 0.1);
        s[i].logprob[1] = (float)(l[1] + (rng_double() - 0.5) * 0.1);
        s[i].logprob[2] = (float)(l[2] + 1.0 + (rng_double() - 0.5) * 0.1);
    }
}

int main(void) {
    enum { N = 300 };
    llm_decide_sample_t* s = (llm_decide_sample_t*)calloc((size_t)N, sizeof(*s));
    CHECK(s != NULL, "alloc");
    if (!s) return 1;

    /* 1) Overconfidence T*=2: il fit recupera ~2 e migliora le metriche. */
    rng_state = 12345678901234567ULL;
    make_samples(s, N, 2.0, 0.0, -1);
    llm_decide_calib_result_t r;
    CHECK(llm_decide_calib_fit(s, N, &r, "/tmp/gmb-decide-calib.json") == 0, "fit T=2");
    printf("fit T=2: head_T=%.4f T_kind=[%.4f %.4f %.4f] brier %.6f->%.6f ece %.6f->%.6f acc %.4f (oof brier %.6f->%.6f)\n",
           r.head_T, r.T_kind[0], r.T_kind[1], r.T_kind[2], r.brier_before, r.brier_after,
           r.ece_before, r.ece_after, r.accuracy, r.brier_oof_before, r.brier_oof_after);
    CHECK(r.head_T > 1.5f && r.head_T < 2.5f, "head_T ~ 2");
    CHECK(r.T_kind[0] > 0.6f && r.T_kind[0] < 1.6f, "T_kind noul ~ 1");
    CHECK(r.brier_after < r.brier_before, "Brier scende");
    CHECK(r.ece_after <= r.ece_before + 0.02, "ECE non peggiora");
    CHECK(r.cv_folds == 2, "2-fold");
    CHECK(r.brier_oof_after <= r.brier_oof_before + 1e-9, "Brier OOF non peggiora");
    CHECK(r.accuracy > 0.0 && r.accuracy <= 1.0, "accuracy valida");

    /* L'artefatto prodotto è un manifest leggibile dal loader T3. */
    FILE* f = fopen("/tmp/gmb-decide-calib.json", "rb");
    CHECK(f != NULL, "artifact scritto");
    if (f) {
        char buf[2048];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        buf[n] = 0;
        fclose(f);
        CHECK(strstr(buf, "\"head_T\"") != NULL, "artifact head_T");
        CHECK(strstr(buf, "\"T_kind\"") != NULL, "artifact T_kind");
        CHECK(strstr(buf, "\"brier_before\"") != NULL, "artifact metriche");
    }
    remove("/tmp/gmb-decide-calib.json");

    /* 2) Dati già calibrati (T*=1): il fit non deve stravolgere T. */
    rng_state = 98765432109876543ULL;
    make_samples(s, N, 1.0, 0.0, -1);
    CHECK(llm_decide_calib_fit(s, N, &r, NULL) == 0, "fit T=1");
    printf("fit T=1: head_T=%.4f brier %.6f->%.6f\n", r.head_T, r.brier_before, r.brier_after);
    CHECK(r.head_T > 0.6f && r.head_T < 1.6f, "head_T ~ 1");
    CHECK(r.brier_after <= r.brier_before + 1e-6, "Brier T=1 non peggiora");

    /* 3) Bias per slot (T4): CHOICE 3 opzioni con +1 spurio sullo slot C.
     * La T non può correggere uno sbilanciamento non uniforme: il bias deve tornare ~ -1. */
    rng_state = 55555555555555555ULL;
    make_choice_bias_samples(s, N);
    CHECK(llm_decide_calib_fit_ex(s, N, &r, "/tmp/gmb-decide-bias.json", 1) == 0, "fit bias");
    printf("fit bias: head_T=%.4f Tchoice=%.4f bias_C=%.4f brier %.6f->%.6f (bias %.6f) ece %.6f->%.6f (bias %.6f)\n",
           r.head_T, r.T_kind[1], r.bias[10], r.brier_before, r.brier_after, r.brier_bias,
           r.ece_before, r.ece_after, r.ece_bias);
    CHECK(r.bias[10] < -0.15f && r.bias[10] > -0.95f, "bias C negativo e non degenere");
    CHECK(r.brier_bias <= r.brier_after + 1e-6, "Brier con bias non peggiore del solo T");
    CHECK(r.bias_slots >= 1, "bias_slots");
    {
        FILE* bf = fopen("/tmp/gmb-decide-bias.json", "rb");
        CHECK(bf != NULL, "artifact bias scritto");
        if (bf) {
            char bb[4096];
            size_t bn = fread(bb, 1, sizeof bb - 1, bf);
            bb[bn] = 0;
            fclose(bf);
            CHECK(strstr(bb, "\"bias\"") != NULL && strstr(bb, "\"C\"") != NULL, "artifact bias JSON");
        }
        remove("/tmp/gmb-decide-bias.json");
    }

    /* 4) Soglia: <10 domande → identità. */
    CHECK(llm_decide_calib_fit(s, 5, &r, NULL) == 0, "fit soglia");
    CHECK(r.identity == 1 && r.head_T == 1.0f && r.T_kind[2] == 1.0f, "soglia T=1");
    CHECK(r.cv_folds == 1, "soglia cv 1");

    /* 5) Input invalido. */
    llm_decide_sample_t bad = { 3, 2, 0, { 0, 0 } };
    CHECK(llm_decide_calib_fit(&bad, 1, &r, NULL) == -1, "kind invalido");
    bad.kind = 0; bad.label = 2;
    CHECK(llm_decide_calib_fit(&bad, 1, &r, NULL) == -1, "label invalido");

    free(s);
    printf("DECIDE-CALIB-SELFCHECK %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
