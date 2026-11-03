/* test_decide_mlx_loss.c — T4: gate NUMERICO del grafo MLX della decision loss.
 *
 * Costruisce logits sintetici [1,T,V], seleziona K token opzione e confronta, per ogni
 * criterio (CE / Brier / RPS / CE+RPS) con/senza maschera e con label smoothing:
 *   - la LOSS del grafo MLX contro `llm_decide_loss_compute` (host, stessa matematica);
 *   - il GRADIENTE del grafo (mlx_value_and_grad) contro il gradiente analitico host.
 * Fuori dall'ultima posizione e fuori dalle opzioni il gradiente deve essere ~0.
 *
 * Model-free: non carica pesi né motore, usa solo mlx-c (simboli esportati dal motore). */
#include "train/llm_train_decide.h"
#include "llm_decide_loss.h"

#include <math.h>
#include <mlx/c/closure.h>
#include <mlx/c/ops.h>
#include <mlx/c/transforms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define V 13
#define T 3
#define K 5

typedef struct {
    const int32_t* ids;
    int k;
    int label;
    int criterion;
    float label_smoothing;
    float rps_weight;
    const uint8_t* mask;
    mlx_stream stream;
} LossCtx;

static int loss_fn(mlx_vector_array* res, const mlx_vector_array input, void* payload) {
    LossCtx* c = (LossCtx*)payload;
    mlx_array logits = mlx_array_new();
    mlx_vector_array_get(&logits, input, 0);
    bool ok = false;
    mlx_array loss = llm_train_decide_loss_from_logits(
        logits, c->ids, c->k, c->label, c->criterion,
        c->label_smoothing, c->rps_weight, c->mask, c->stream, &ok);
    if (ok) mlx_vector_array_set_value(res, loss);
    mlx_array_free(loss);
    mlx_array_free(logits);
    return ok ? 0 : 1;
}

static int failures = 0;

static void fail(const char* what, int criterion, int masked) {
    failures++;
    printf("FAIL [crit=%d mask=%d] %s\n", criterion, masked, what);
}

static int run_case(int criterion, int masked, float smoothing) {
    const int32_t ids[K] = { 1, 4, 7, 9, 11 };
    const int label = 2;                      /* ids[2] = 7 */
    const uint8_t mask[K] = { 1, 0, 1, 1, 1 };
    const uint8_t* maskp = masked ? mask : NULL;

    /* Logits deterministici in [-2,2] su tutte le posizioni. */
    float host[T * V];
    unsigned seed = 12345u;
    for (int i = 0; i < T * V; i++) {
        seed = seed * 1103515245u + 12345u;
        host[i] = ((float)((seed >> 16) & 0x7fff) / 32767.0f) * 4.0f - 2.0f;
    }
    /* Un po' di struttura: l'opzione corretta è favorita. */
    host[(T - 1) * V + ids[label]] += 1.0f;

    llm_decide_loss_sample_t sample;
    memset(&sample, 0, sizeof sample);
    sample.n_options = masked ? K : K;        /* il dominio È l'insieme delle opzioni */
    sample.label = label;
    sample.mask = 0;
    for (int k = 0; k < K; k++) {
        if (!masked || mask[k]) sample.mask |= (1u << k);
        sample.logits[k] = host[(T - 1) * V + ids[k]];
    }
    llm_decide_loss_opts_t opts;
    memset(&opts, 0, sizeof opts);
    opts.criterion = (llm_decide_criterion_t)criterion;
    opts.label_smoothing = smoothing;
    opts.rps_weight = 0.5;
    llm_decide_loss_result_t expected;
    if (llm_decide_loss_compute(&sample, &opts, &expected) != 0) {
        fail("host loss invalida", criterion, masked);
        return -1;
    }

    mlx_stream st = mlx_default_cpu_stream_new();
    LossCtx ctx = { ids, K, label, criterion, smoothing, 0.5f, maskp, st };
    mlx_closure cl = mlx_closure_new_func_payload(loss_fn, &ctx, NULL);
    mlx_closure_value_and_grad vg = mlx_closure_value_and_grad_new();
    int argnum = 0;
    int s[3] = { 1, T, V };
    mlx_array logits = mlx_array_new_data(host, s, 3, MLX_FLOAT32);
    mlx_vector_array in = mlx_vector_array_new();
    mlx_vector_array_append_value(in, logits);

    int rc = 0;
    if (mlx_value_and_grad(&vg, cl, &argnum, 1) != 0) { rc = -1; fail("value_and_grad", criterion, masked); }
    mlx_vector_array vals = mlx_vector_array_new(), grads = mlx_vector_array_new();
    if (rc == 0 && mlx_closure_value_and_grad_apply(&vals, &grads, vg, in) != 0) {
        rc = -1; fail("apply", criterion, masked);
    }

    mlx_array lv = mlx_array_new(), gv = mlx_array_new();
    mlx_array lc = mlx_array_new(), gc = mlx_array_new();
    if (rc == 0) {
        mlx_vector_array_get(&lv, vals, 0);
        mlx_vector_array_get(&gv, grads, 0);
        if (mlx_contiguous(&lc, lv, false, st) != 0) rc = -1;
        if (rc == 0 && mlx_contiguous(&gc, gv, false, st) != 0) rc = -1;
        if (rc == 0 && (mlx_array_eval(lc) != 0 || mlx_array_eval(gc) != 0)) rc = -1;
        if (rc != 0) fail("eval", criterion, masked);
    }

    if (rc == 0) {
        double got = mlx_array_data_float32(lc)[0];
        double diff = fabs(got - expected.loss);
        double scale = fabs(expected.loss) > 1.0 ? fabs(expected.loss) : 1.0;
        if (diff > 2e-4 * scale) {
            printf("  loss got=%.8f want=%.8f (diff=%.3g)\n", got, expected.loss, diff);
            fail("loss", criterion, masked);
        }

        const float* g = mlx_array_data_float32(gc);
        for (int k = 0; k < K; k++) {
            float gotg = g[(size_t)(T - 1) * V + ids[k]];
            double want = expected.grad[k];
            if (fabs((double)gotg - want) > 2e-4 * (fabs(want) > 1.0 ? fabs(want) : 1.0)) {
                printf("  grad[%d] got=%.8f want=%.8f\n", k, (double)gotg, want);
                fail("gradiente opzione", criterion, masked);
            }
        }
        /* Fuori dall'ultima posizione il gradiente deve essere nullo. */
        for (int t = 0; t < T - 1; t++)
            for (int v = 0; v < V; v++)
                if (fabs((double)g[(size_t)t * V + v]) > 1e-5) {
                    printf("  grad fuori posizione [%d,%d]=%.3g\n", t, v,
                           (double)g[(size_t)t * V + v]);
                    fail("gradiente fuori posizione", criterion, masked);
                    break;
                }
        /* Fuori dai token opzione (ultima posizione) deve essere nullo. */
        for (int v = 0; v < V; v++) {
            int is_opt = 0;
            for (int k = 0; k < K; k++) if (ids[k] == v) is_opt = 1;
            if (!is_opt && fabs((double)g[(size_t)(T - 1) * V + v]) > 1e-5) {
                printf("  grad fuori opzione [%d]=%.3g\n", v,
                       (double)g[(size_t)(T - 1) * V + v]);
                fail("gradiente fuori opzione", criterion, masked);
                break;
            }
        }
    }

    mlx_array_free(lc); mlx_array_free(gc); mlx_array_free(lv); mlx_array_free(gv);
    mlx_vector_array_free(vals); mlx_vector_array_free(grads);
    mlx_vector_array_free(in);
    mlx_closure_value_and_grad_free(vg);
    mlx_closure_free(cl);
    mlx_array_free(logits);
    mlx_stream_free(st);
    return rc;
}

int main(void) {
    int cases = 0;
    for (int crit = LLM_DECIDE_CRIT_CE; crit <= LLM_DECIDE_CRIT_CE_RPS; crit++) {
        for (int masked = 0; masked <= 1; masked++) {
            for (int sm = 0; sm <= 1; sm++) {
                run_case(crit, masked, sm ? 0.1f : 0.0f);
                cases++;
            }
        }
    }
    if (failures == 0) {
        printf("PASS decide_mlx_loss: %d casi (CE/Brier/RPS/CE+RPS, mask, label smoothing) "
               "— loss+gradienti identici a llm_decide_loss_compute\n", cases);
        return 0;
    }
    printf("FAIL decide_mlx_loss: %d errori su %d casi\n", failures, cases);
    return 1;
}
