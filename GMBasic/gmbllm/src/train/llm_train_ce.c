/* ============================================================================
 * llm_train_ce.c — PUL (GMBasic/gmbllm): la LOSS del training.
 *
 * PERIMETRO DI LICENZA (PUL). Costruisce la cross-entropy (masked/DPO) SOPRA i logits del
 * forward DIFFERENZIABILE Apache (`llm_ai_mlx_diff_forward_logits`). La loss/grad/AdamW sono
 * "produce" → PUL (ADR §4). L'engine Apache resta inference-complete (solo forward).
 * ============================================================================ */
#include "llmengine_mlx_diff_forward.h"

#include <mlx/c/mlx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Forward + Cross-Entropy (masked) → loss `mlx_array` nel grafo autograd.
 * `sum_mode`=true somma la CE; false media sui token attivi (mask, se presente). */
mlx_array llm_train_forward_ce(llm_ai_mlx_model_handle mh, const int32_t* toks,
                               const int32_t* tgt, const uint8_t* mask, int T,
                               bool sum_mode, bool* ok) {
    if (ok) *ok = false;
    mlx_array loss = mlx_array_new();
    if (!mh || !toks || !tgt || T <= 0) return loss;

    bool fok = false;
    llm_ai_mlx_session_handle sh = NULL;
    mlx_array capped = llm_ai_mlx_diff_forward_logits(mh, toks, T, &sh, &fok);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    if (!fok) { if (sh) llm_ai_mlx_destroy_session(sh); return loss; }

    /* log_softmax STABILE = x - logsumexp(x) (vjp stabile; NON log(softmax)). */
    mlx_array lse = mlx_array_new(), logp = mlx_array_new();
    bool good = mlx_logsumexp_axis(&lse, capped, -1, /*keepdims=*/true, st) == 0;
    if (good) good = mlx_subtract(&logp, capped, lse, st) == 0;
    mlx_array tv = mlx_array_new(), te = mlx_array_new();
    if (good) { int tshp[2] = { 1, T }; tv = mlx_array_new_data(tgt, tshp, 2, MLX_INT32);
                good = mlx_expand_dims(&te, tv, -1, st) == 0; }
    mlx_array g = mlx_array_new(), nll = mlx_array_new();
    if (good) good = mlx_take_along_axis(&g, logp, te, -1, st) == 0;
    if (good) good = mlx_negative(&nll, g, st) == 0;
    mlx_array pen = nll;
    if (good && mask) {
        float* mf = (float*)malloc((size_t)T * sizeof(float));
        if (mf) {
            for (int i = 0; i < T; i++) mf[i] = mask[i] ? 1.0f : 0.0f;
            int mshp2[2] = { 1, T };
            mlx_array mv = mlx_array_new_data(mf, mshp2, 2, MLX_FLOAT32);
            mlx_array me = mlx_array_new();
            if (mlx_expand_dims(&me, mv, -1, st) == 0) {
                mlx_array sm = mlx_array_new();
                if (mlx_multiply(&sm, nll, me, st) == 0) mlx_array_set(&pen, sm);
                mlx_array_free(sm);
            }
            mlx_array_free(me); mlx_array_free(mv); free(mf);
        }
    }
    mlx_array summed = mlx_array_new();
    if (good) good = mlx_sum(&summed, pen, /*keepdims=*/false, st) == 0;
    if (good) {
        if (sum_mode) { mlx_array_set(&loss, summed); }
        else {
            float denom = (float)T;
            if (mask) { int c = 0; for (int i = 0; i < T; i++) if (mask[i]) c++; denom = c > 0 ? (float)c : 1.0f; }
            mlx_array d = mlx_array_new_float(denom), dq = mlx_array_new();
            if (mlx_divide(&dq, summed, d, st) == 0) mlx_array_set(&loss, dq);
            mlx_array_free(dq); mlx_array_free(d);
        }
    }
    if (good && ok) *ok = true;
    mlx_array_free(summed); mlx_array_free(capped); mlx_array_free(lse); mlx_array_free(logp);
    mlx_array_free(tv); mlx_array_free(te); mlx_array_free(g); mlx_array_free(nll);
    if (sh) llm_ai_mlx_destroy_session(sh);
    return loss;
}

/* G5 — Forward + KL teacher-student (distillazione soft-logit) → loss `mlx_array` nel grafo
 * autograd. `teacher_logits` = [T,V] host (DETACH, teacher fisso); `temperature` = soft targets.
 * loss = media_mascherata( KL( softmax(t/T) || softmax(s/T) ) ) * T^2. */
mlx_array llm_train_forward_kl(llm_ai_mlx_model_handle mh, const int32_t* toks,
                               const float* teacher_logits, int V, int T,
                               float temperature, const uint8_t* mask, bool* ok) {
    if (ok) *ok = false;
    mlx_array loss = mlx_array_new();
    if (!mh || !toks || !teacher_logits || V <= 0 || T <= 0 || temperature <= 0) return loss;

    bool fok = false;
    llm_ai_mlx_session_handle sh = NULL;
    mlx_array capped = llm_ai_mlx_diff_forward_logits(mh, toks, T, &sh, &fok);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    if (!fok) { if (sh) llm_ai_mlx_destroy_session(sh); return loss; }

    /* teacher logits [1,T,V] (costante, host) + scala 1/T su entrambi. */
    int tsh[3] = { 1, T, V };
    mlx_array tl = mlx_array_new_data(teacher_logits, tsh, 3, MLX_FLOAT32);
    mlx_array invT = mlx_array_new_float(1.0f / temperature);
    mlx_array ss = mlx_array_new(), ts = mlx_array_new();
    bool good = mlx_multiply(&ss, capped, invT, st) == 0 && mlx_multiply(&ts, tl, invT, st) == 0;
    /* log_softmax stabile = x - logsumexp(x). */
    mlx_array lse_s = mlx_array_new(), lps = mlx_array_new(), lse_t = mlx_array_new(), lpt = mlx_array_new(), pt = mlx_array_new();
    if (good) good = mlx_logsumexp_axis(&lse_s, ss, -1, /*keepdims=*/true, st) == 0;
    if (good) good = mlx_subtract(&lps, ss, lse_s, st) == 0;
    if (good) good = mlx_logsumexp_axis(&lse_t, ts, -1, true, st) == 0;
    if (good) good = mlx_subtract(&lpt, ts, lse_t, st) == 0;
    if (good) good = mlx_exp(&pt, lpt, st) == 0;
    /* per-token KL = sum_v pt*(log_pt - log_ps) -> [1,T] */
    mlx_array diff = mlx_array_new(), prod = mlx_array_new(), klt = mlx_array_new();
    if (good) good = mlx_subtract(&diff, lpt, lps, st) == 0;
    if (good) good = mlx_multiply(&prod, pt, diff, st) == 0;
    if (good) good = mlx_sum_axis(&klt, prod, -1, /*keepdims=*/false, st) == 0;
    mlx_array pen = klt;
    if (good && mask) {
        float* mf = (float*)malloc((size_t)T * sizeof(float));
        if (mf) {
            for (int i = 0; i < T; i++) mf[i] = mask[i] ? 1.0f : 0.0f;
            int mshp2[2] = { 1, T };
            mlx_array mv = mlx_array_new_data(mf, mshp2, 2, MLX_FLOAT32);
            mlx_array sm = mlx_array_new();
            if (mlx_multiply(&sm, klt, mv, st) == 0) mlx_array_set(&pen, sm);
            mlx_array_free(sm); mlx_array_free(mv); free(mf);
        }
    }
    mlx_array summed = mlx_array_new();
    if (good) good = mlx_sum(&summed, pen, /*keepdims=*/false, st) == 0;
    if (good) {
        float denom = (float)T;
        if (mask) { int c = 0; for (int i = 0; i < T; i++) if (mask[i]) c++; denom = c > 0 ? (float)c : 1.0f; }
        mlx_array d = mlx_array_new_float(denom), dq = mlx_array_new(), t2 = mlx_array_new_float(temperature * temperature);
        if (mlx_divide(&dq, summed, d, st) == 0) { mlx_multiply(&loss, dq, t2, st); }
        mlx_array_free(dq); mlx_array_free(d); mlx_array_free(t2);
    }
    if (good && ok) *ok = true;
    mlx_array_free(summed); mlx_array_free(capped); mlx_array_free(tl); mlx_array_free(invT);
    mlx_array_free(ss); mlx_array_free(ts); mlx_array_free(lse_s); mlx_array_free(lps);
    mlx_array_free(lse_t); mlx_array_free(lpt); mlx_array_free(pt);
    mlx_array_free(diff); mlx_array_free(prod); mlx_array_free(klt);
    if (sh) llm_ai_mlx_destroy_session(sh);
    return loss;
}

/* G5 — logits del TEACHER per la distillazione: forward differenziabile -> copia host [T,V] (detach).
 * Isola mlx-c in gmbllm (la dylib finetune non include headers mlx). `out_logits` = T*V float. */
int llm_train_teacher_logits(llm_ai_mlx_model_handle mh, const int32_t* toks, int T,
                             float* out_logits, int V) {
    if (!mh || !toks || T <= 0 || !out_logits || V <= 0) return -1;
    llm_ai_mlx_diff_setup(mh);          /* il forward differenziabile richiede il setup (eager/composable) */
    llm_ai_mlx_diff_set_eager_sdpa(1);
    bool fok = false;
    llm_ai_mlx_session_handle sh = NULL;
    mlx_array lg = llm_ai_mlx_diff_forward_logits(mh, toks, T, &sh, &fok);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    int rc = -1;
    if (fok) {
        mlx_array c = mlx_array_new(), cf = mlx_array_new();
        /* I logits possono essere F16/BF16 (modello F16): `mlx_array_data_float32` legge QUALSIASI
         * dtype come float32 -> su F16 leggerebbe 2x i byte (overflow!). Cast esplicito a F32. */
        if (mlx_contiguous(&c, lg, false, st) == 0 && mlx_astype(&cf, c, MLX_FLOAT32, st) == 0 &&
            mlx_array_eval(cf) == 0)
            { memcpy(out_logits, mlx_array_data_float32(cf), (size_t)T * (size_t)V * sizeof(float)); rc = 0; }
        mlx_array_free(c); mlx_array_free(cf);
    }
    if (sh) llm_ai_mlx_destroy_session(sh);
    mlx_array_free(lg);
    return rc;
}
