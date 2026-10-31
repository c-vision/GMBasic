/* ============================================================================
 * gmbllm/src/train/llm_train.c — TRAINING (PUL): orchestrazione loss/grad/DPO.
 *
 * Usa l'ABI Apache dei PRIMITIVI (llmengine_mlx_train.h): setup, set_lora_mlx,
 * train_forward_ce, set_eager_sdpa, train_stream. Qui costruisce le closure MLX,
 * mlx_value_and_grad, il readback host e la loss DPO (reference = adapter OFF).
 * ============================================================================ */
#include "train/llm_train.h"
#include "llmengine_mlx.h"
#include "llmengine_mlx_diff_forward.h"
/* PUL: la loss (CE) vive qui, sopra i logits del forward Apache. */
mlx_array llm_train_forward_ce(llm_ai_mlx_model_handle mh, const int32_t* toks, const int32_t* tgt, const uint8_t* mask, int T, bool sum_mode, bool* ok);
/* G5 — loss KL teacher-student (distill) sopra gli stessi logits. */
mlx_array llm_train_forward_kl(llm_ai_mlx_model_handle mh, const int32_t* toks, const float* teacher_logits, int V, int T, float temperature, const uint8_t* mask, bool* ok);

#include <mlx/c/array.h>
#include <mlx/c/vector.h>
#include <mlx/c/closure.h>
#include <mlx/c/ops.h>
#include <mlx/c/transforms.h>   /* mlx_value_and_grad (in questo fork mlx-c) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* payload della closure CE */
typedef struct {
    llm_ai_mlx_model_handle mh; int layer; const char* proj;
    const int32_t* toks; const int32_t* tgt; const uint8_t* mask; int T;
} GtPub;

static int gmbllm_ce_loss_fn(mlx_vector_array* res, const mlx_vector_array input, void* payload) {
    GtPub* c = (GtPub*)payload;
    mlx_array A = mlx_array_new(), B = mlx_array_new();
    mlx_vector_array_get(&A, input, 0);
    mlx_vector_array_get(&B, input, 1);
    llm_ai_mlx_diff_set_lora_mlx(c->mh, c->layer, c->proj, A, B, NULL);
    bool ok = true;
    mlx_array loss = llm_train_forward_ce(c->mh, c->toks, c->tgt, c->mask, c->T, /*sum_mode=*/false, &ok);
    if (ok) mlx_vector_array_set_value(res, loss);
    mlx_array_free(loss);
    return ok ? 0 : 1;
}

/* payload della closure DPO */
typedef struct {
    llm_ai_mlx_model_handle mh; int layer; const char* proj;
    const int32_t *tw, *tgw, *tl, *tgl; const uint8_t *mw, *ml; int Tw, Tl; float beta;
} GdpoPub;

static int gmbllm_dpo_loss_fn(mlx_vector_array* res, const mlx_vector_array input, void* payload) {
    GdpoPub* c = (GdpoPub*)payload;
    mlx_array A = mlx_array_new(), B = mlx_array_new();
    mlx_vector_array_get(&A, input, 0);
    mlx_vector_array_get(&B, input, 1);
    llm_ai_mlx_diff_set_lora_mlx(c->mh, c->layer, c->proj, A, B, NULL);

    bool ok = true;
    mlx_array ce_w = llm_train_forward_ce(c->mh, c->tw, c->tgw, c->mw, c->Tw, /*sum_mode=*/true, &ok);
    mlx_array ce_l = ok ? llm_train_forward_ce(c->mh, c->tl, c->tgl, c->ml, c->Tl, true, &ok) : mlx_array_new();
    llm_ai_mlx_model_lora_set_enabled(c->mh, false);   /* REFERENCE: adapter OFF (costante) */
    mlx_array rc_w = ok ? llm_train_forward_ce(c->mh, c->tw, c->tgw, c->mw, c->Tw, true, &ok) : mlx_array_new();
    mlx_array rc_l = ok ? llm_train_forward_ce(c->mh, c->tl, c->tgl, c->ml, c->Tl, true, &ok) : mlx_array_new();
    llm_ai_mlx_model_lora_set_enabled(c->mh, true);
    if (!ok) { mlx_array_free(ce_w); mlx_array_free(ce_l); mlx_array_free(rc_w); mlx_array_free(rc_l); return 1; }

    mlx_stream st = llm_ai_mlx_diff_stream(c->mh);
    mlx_array a1 = mlx_array_new(), a2 = mlx_array_new(), a = mlx_array_new(), z = mlx_array_new();
    mlx_array nz = mlx_array_new(), e = mlx_array_new(), e1 = mlx_array_new(), one = mlx_array_new();
    mlx_array sp = mlx_array_new(), b = mlx_array_new_float(c->beta);
    ok = mlx_subtract(&a1, rc_w, ce_w, st) == 0 &&
         mlx_subtract(&a2, rc_l, ce_l, st) == 0 &&
         mlx_subtract(&a, a1, a2, st) == 0 &&
         mlx_multiply(&z, a, b, st) == 0 &&
         mlx_negative(&nz, z, st) == 0 &&
         mlx_exp(&e, nz, st) == 0;
    one = mlx_array_new_float(1.0f);
    if (ok) ok = mlx_add(&e1, e, one, st) == 0;
    if (ok) ok = mlx_log(&sp, e1, st) == 0;      /* softplus(-z) */
    if (ok) mlx_vector_array_set_value(res, sp);
    mlx_array_free(ce_w); mlx_array_free(ce_l); mlx_array_free(rc_w); mlx_array_free(rc_l);
    mlx_array_free(a1); mlx_array_free(a2); mlx_array_free(a); mlx_array_free(z); mlx_array_free(nz);
    mlx_array_free(e); mlx_array_free(e1); mlx_array_free(one); mlx_array_free(sp); mlx_array_free(b);
    return ok ? 0 : 1;
}

/* readback comune: closure + (opzionale) value_and_grad -> loss + dA/dB (host). */
static int gmbllm_apply(mlx_closure cl, mlx_stream st, mlx_vector_array in,
                        int Ar, int Ac, int Br, int Bc,
                        float* out_loss, float* out_dA, float* out_dB) {
    int rc = -1;
    if (out_dA || out_dB) {
        int argnums[2] = { 0, 1 };
        mlx_closure_value_and_grad vg = mlx_closure_value_and_grad_new();
        mlx_vector_array vals = mlx_vector_array_new(), grads = mlx_vector_array_new();
        if (mlx_value_and_grad(&vg, cl, argnums, 2) == 0 &&
            mlx_closure_value_and_grad_apply(&vals, &grads, vg, in) == 0) {
            mlx_array lv = mlx_array_new(), gA = mlx_array_new(), gB = mlx_array_new();
            mlx_vector_array_get(&lv, vals, 0); mlx_vector_array_get(&gA, grads, 0); mlx_vector_array_get(&gB, grads, 1);
            mlx_array lvc = mlx_array_new(), gAc = mlx_array_new(), gBc = mlx_array_new();
            bool eok = mlx_contiguous(&lvc, lv, false, st) == 0 &&
                       mlx_contiguous(&gAc, gA, false, st) == 0 &&
                       mlx_contiguous(&gBc, gB, false, st) == 0 &&
                       mlx_array_eval(lvc) == 0 && mlx_array_eval(gAc) == 0 && mlx_array_eval(gBc) == 0;
            if (eok) {
                if (out_loss) *out_loss = mlx_array_data_float32(lvc)[0];
                if (out_dA) memcpy(out_dA, mlx_array_data_float32(gAc), (size_t)Ar * (size_t)Ac * 4);
                if (out_dB) memcpy(out_dB, mlx_array_data_float32(gBc), (size_t)Br * (size_t)Bc * 4);
                rc = 0;
            }
            mlx_array_free(lvc); mlx_array_free(gAc); mlx_array_free(gBc);
            mlx_array_free(lv); mlx_array_free(gA); mlx_array_free(gB);
        }
        mlx_vector_array_free(vals); mlx_vector_array_free(grads);
        mlx_closure_value_and_grad_free(vg);
    } else {
        mlx_vector_array vals = mlx_vector_array_new();
        if (mlx_closure_apply(&vals, cl, in) == 0 && mlx_vector_array_size(vals) >= 1) {
            mlx_array lv = mlx_array_new(), lvc = mlx_array_new();
            mlx_vector_array_get(&lv, vals, 0);
            if (mlx_contiguous(&lvc, lv, false, st) == 0 && mlx_array_eval(lvc) == 0) {
                if (out_loss) *out_loss = mlx_array_data_float32(lvc)[0];
                rc = 0;
            }
            mlx_array_free(lvc); mlx_array_free(lv);
        }
        mlx_vector_array_free(vals);
    }
    return rc;
}

int llm_ai_mlx_train_lora_loss_grad_mask(llm_ai_mlx_model_handle mh,
                                         const int32_t* toks, const int32_t* targets, const uint8_t* mask, int T,
                                         int layer, const char* proj,
                                         const float* A, int Ar, int Ac,
                                         const float* B, int Br, int Bc,
                                         float* out_loss, float* out_dA, float* out_dB) {
    if (!mh || !toks || !targets || T <= 0 || !A || !B || Ac <= 0) return -1;
    const char* pr = proj ? proj : "v_proj";
    llm_ai_mlx_diff_setup(mh);
    llm_ai_mlx_model_lora_clear(mh);
    if (llm_ai_mlx_model_lora_add(mh, layer, pr, Ac, 1.0f, A, B, Ar, Br) != 0) return -1;
    llm_ai_mlx_model_lora_set_enabled(mh, 1);
    GtPub ctx = { mh, layer, pr, toks, targets, mask, T };
    llm_ai_mlx_diff_set_eager_sdpa(1);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    mlx_closure cl = mlx_closure_new_func_payload(gmbllm_ce_loss_fn, &ctx, NULL);
    int sA[2] = { Ar, Ac }, sB[2] = { Br, Bc };
    mlx_array Aa = mlx_array_new_data(A, sA, 2, MLX_FLOAT32);
    mlx_array Ba = mlx_array_new_data(B, sB, 2, MLX_FLOAT32);
    mlx_vector_array in = mlx_vector_array_new();
    mlx_vector_array_append_value(in, Aa);
    mlx_vector_array_append_value(in, Ba);
    int rc = gmbllm_apply(cl, st, in, Ar, Ac, Br, Bc, out_loss, out_dA, out_dB);
    mlx_vector_array_free(in);
    mlx_closure_free(cl);
    llm_ai_mlx_diff_set_eager_sdpa(0);
    return rc;
}

int llm_ai_mlx_train_lora_loss_grad(llm_ai_mlx_model_handle mh,
                                    const int32_t* toks, const int32_t* targets, int T,
                                    int layer, const char* proj,
                                    const float* A, int Ar, int Ac,
                                    const float* B, int Br, int Bc,
                                    float* out_loss, float* out_dA, float* out_dB) {
    return llm_ai_mlx_train_lora_loss_grad_mask(mh, toks, targets, NULL, T, layer, proj,
                                                A, Ar, Ac, B, Br, Bc, out_loss, out_dA, out_dB);
}

int llm_ai_mlx_train_dpo_step(llm_ai_mlx_model_handle mh,
                              const int32_t* tw, const int32_t* tgw, const uint8_t* mw, int Tw,
                              const int32_t* tl, const int32_t* tgl, const uint8_t* ml, int Tl,
                              int layer, const char* proj,
                              const float* A, int Ar, int Ac, const float* B, int Br, int Bc,
                              float beta, float* out_loss, float* out_dA, float* out_dB) {
    if (!mh || !tw || !tgw || !tl || !tgl || Tw <= 0 || Tl <= 0 || !A || !B || Ac <= 0) return -1;
    const char* pr = proj ? proj : "v_proj";
    llm_ai_mlx_diff_setup(mh);
    llm_ai_mlx_model_lora_clear(mh);
    if (llm_ai_mlx_model_lora_add(mh, layer, pr, Ac, 1.0f, A, B, Ar, Br) != 0) return -1;
    llm_ai_mlx_model_lora_set_enabled(mh, 1);
    GdpoPub ctx = { mh, layer, pr, tw, tgw, tl, tgl, mw, ml, Tw, Tl, beta };
    llm_ai_mlx_diff_set_eager_sdpa(1);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    mlx_closure cl = mlx_closure_new_func_payload(gmbllm_dpo_loss_fn, &ctx, NULL);
    int sA[2] = { Ar, Ac }, sB[2] = { Br, Bc };
    mlx_array Aa = mlx_array_new_data(A, sA, 2, MLX_FLOAT32);
    mlx_array Ba = mlx_array_new_data(B, sB, 2, MLX_FLOAT32);
    mlx_vector_array in = mlx_vector_array_new();
    mlx_vector_array_append_value(in, Aa);
    mlx_vector_array_append_value(in, Ba);
    int rc = gmbllm_apply(cl, st, in, Ar, Ac, Br, Bc, out_loss, out_dA, out_dB);
    mlx_vector_array_free(in);
    mlx_closure_free(cl);
    llm_ai_mlx_diff_set_eager_sdpa(0);
    return rc;
}

/* ============================================================================
 * G-opt2 — GRPO (RLHF senza critic). Gruppo di G completamenti dello stesso prompt, vantaggi
 * group-relative. loss = -mean_g[min(r*adv, clip(r,1-clip,1+clip)*adv)] + kl_coef*k3_KL(pi||ref);
 * r = exp(logp_theta - old_logp); ref = adapter OFF. pattern del DPO step (closure + value_and_grad).
 * ============================================================================ */
typedef struct {
    llm_ai_mlx_model_handle mh; int layer; const char* proj;
    const int32_t* toks; const int32_t* tgt; const uint8_t* mask; int T, G;
    const float* old_logp; const float* adv; float clip, kl_coef;
} GgrpoPub;

static int gmbllm_grpo_loss_fn(mlx_vector_array* res, const mlx_vector_array input, void* payload) {
    GgrpoPub* c = (GgrpoPub*)payload;
    mlx_array A = mlx_array_new(), B = mlx_array_new();
    mlx_vector_array_get(&A, input, 0);
    mlx_vector_array_get(&B, input, 1);
    llm_ai_mlx_diff_set_lora_mlx(c->mh, c->layer, c->proj, A, B, NULL);
    mlx_stream st = llm_ai_mlx_diff_stream(c->mh);
    bool ok = true;
    const int G = c->G, T = c->T;
    mlx_array acc = mlx_array_new_float(0.0f);
    mlx_array one = mlx_array_new_float(1.0f);
    mlx_array onec = mlx_array_new_float(1.0f - c->clip);
    mlx_array onecp = mlx_array_new_float(1.0f + c->clip);
    mlx_array klc = mlx_array_new_float(c->kl_coef);
    for (int g = 0; g < G && ok; g++) {
        const int32_t* tg = c->toks + (size_t)g * T;
        const int32_t* tt = c->tgt + (size_t)g * T;
        const uint8_t* mk = c->mask ? c->mask + (size_t)g * T : NULL;
        mlx_array ce = llm_train_forward_ce(c->mh, tg, tt, mk, T, /*sum_mode=*/true, &ok);   /* -sum logp_theta */
        mlx_array lpt = mlx_array_new();
        if (ok) ok = mlx_negative(&lpt, ce, st) == 0;
        mlx_array_free(ce);
        llm_ai_mlx_model_lora_set_enabled(c->mh, false);                                     /* reference */
        mlx_array cer = llm_train_forward_ce(c->mh, tg, tt, mk, T, true, &ok);
        llm_ai_mlx_model_lora_set_enabled(c->mh, true);
        mlx_array lpr = mlx_array_new();
        if (ok) ok = mlx_negative(&lpr, cer, st) == 0;
        mlx_array_free(cer);
        mlx_array oldlp = mlx_array_new_float(c->old_logp[g]);
        mlx_array adv = mlx_array_new_float(c->adv[g]);
        mlx_array diff = mlx_array_new(), ratio = mlx_array_new(), s1 = mlx_array_new(), rc = mlx_array_new();
        mlx_array s2 = mlx_array_new(), surr = mlx_array_new(), d = mlx_array_new(), ed = mlx_array_new();
        mlx_array klt = mlx_array_new(), term = mlx_array_new(), term2 = mlx_array_new();
        if (ok) ok = mlx_subtract(&diff, lpt, oldlp, st) == 0 &&
                     mlx_exp(&ratio, diff, st) == 0 &&
                     mlx_multiply(&s1, ratio, adv, st) == 0 &&
                     mlx_clip(&rc, ratio, onec, onecp, st) == 0 &&
                     mlx_multiply(&s2, rc, adv, st) == 0 &&
                     mlx_minimum(&surr, s1, s2, st) == 0 &&
                     mlx_subtract(&d, lpr, lpt, st) == 0 &&
                     mlx_exp(&ed, d, st) == 0 &&
                     mlx_subtract(&klt, ed, d, st) == 0 &&
                     mlx_subtract(&klt, klt, one, st) == 0 &&                 /* k3 = exp(d)-d-1 */
                     mlx_multiply(&term, klc, klt, st) == 0 &&
                     mlx_subtract(&term2, surr, term, st) == 0 &&
                     mlx_subtract(&acc, acc, term2, st) == 0;                 /* acc -= (surr - kl_coef*kl) */
        mlx_array_free(lpt); mlx_array_free(lpr); mlx_array_free(oldlp); mlx_array_free(adv);
        mlx_array_free(diff); mlx_array_free(ratio); mlx_array_free(s1); mlx_array_free(rc);
        mlx_array_free(s2); mlx_array_free(surr); mlx_array_free(d); mlx_array_free(ed);
        mlx_array_free(klt); mlx_array_free(term); mlx_array_free(term2);
    }
    mlx_array gf = mlx_array_new_float((float)G);
    mlx_array loss = mlx_array_new();
    if (ok) ok = mlx_divide(&loss, acc, gf, st) == 0;
    if (ok) mlx_vector_array_set_value(res, loss);
    mlx_array_free(acc); mlx_array_free(one); mlx_array_free(onec); mlx_array_free(onecp);
    mlx_array_free(klc); mlx_array_free(gf); mlx_array_free(loss);
    return ok ? 0 : 1;
}

int llm_ai_mlx_train_grpo_step(llm_ai_mlx_model_handle mh,
                               const int32_t* toks, const int32_t* tgt, const uint8_t* mask, int T, int G,
                               const float* old_logp, const float* adv, float clip, float kl_coef,
                               int layer, const char* proj,
                               const float* A, int Ar, int Ac, const float* B, int Br, int Bc,
                               float* out_loss, float* out_dA, float* out_dB) {
    if (!mh || !toks || !tgt || T <= 0 || G <= 0 || !old_logp || !adv || !A || !B || Ac <= 0) return -1;
    const char* pr = proj ? proj : "v_proj";
    llm_ai_mlx_diff_setup(mh);
    llm_ai_mlx_model_lora_clear(mh);
    if (llm_ai_mlx_model_lora_add(mh, layer, pr, Ac, 1.0f, A, B, Ar, Br) != 0) return -1;
    llm_ai_mlx_model_lora_set_enabled(mh, 1);
    GgrpoPub ctx = { mh, layer, pr, toks, tgt, mask, T, G, old_logp, adv, clip, kl_coef };
    llm_ai_mlx_diff_set_eager_sdpa(1);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    mlx_closure cl = mlx_closure_new_func_payload(gmbllm_grpo_loss_fn, &ctx, NULL);
    int sA[2] = { Ar, Ac }, sB[2] = { Br, Bc };
    mlx_array Aa = mlx_array_new_data(A, sA, 2, MLX_FLOAT32);
    mlx_array Ba = mlx_array_new_data(B, sB, 2, MLX_FLOAT32);
    mlx_vector_array in = mlx_vector_array_new();
    mlx_vector_array_append_value(in, Aa);
    mlx_vector_array_append_value(in, Ba);
    int rc = gmbllm_apply(cl, st, in, Ar, Ac, Br, Bc, out_loss, out_dA, out_dB);
    mlx_vector_array_free(in);
    mlx_closure_free(cl);
    llm_ai_mlx_diff_set_eager_sdpa(0);
    return rc;
}

/* ============================================================================
 * G5 — DISTILLAZIONE teacher-student (KL soft-logit). teacher_logits [T,V] host (detach);
 * closure -> llm_train_forward_kl; value_and_grad w.r.t. LoRA A/B (pattern del DPO step).
 * ============================================================================ */
typedef struct {
    llm_ai_mlx_model_handle mh; int layer; const char* proj;
    const int32_t* toks; const float* tlog; int V; int T; float temp; const uint8_t* mask;
} GklPub;

static int gmbllm_kl_loss_fn(mlx_vector_array* res, const mlx_vector_array input, void* payload) {
    GklPub* c = (GklPub*)payload;
    mlx_array A = mlx_array_new(), B = mlx_array_new();
    mlx_vector_array_get(&A, input, 0);
    mlx_vector_array_get(&B, input, 1);
    llm_ai_mlx_diff_set_lora_mlx(c->mh, c->layer, c->proj, A, B, NULL);
    bool ok = true;
    mlx_array loss = llm_train_forward_kl(c->mh, c->toks, c->tlog, c->V, c->T, c->temp, c->mask, &ok);
    if (ok) mlx_vector_array_set_value(res, loss);
    mlx_array_free(loss);
    return ok ? 0 : 1;
}

int llm_ai_mlx_train_kl_step(llm_ai_mlx_model_handle mh,
                             const int32_t* toks, int T,
                             const float* teacher_logits, int V, float temperature,
                             const uint8_t* mask,
                             int layer, const char* proj,
                             const float* A, int Ar, int Ac,
                             const float* B, int Br, int Bc,
                             float* out_loss, float* out_dA, float* out_dB) {
    if (!mh || !toks || T <= 0 || !teacher_logits || V <= 0 || temperature <= 0 || !A || !B || Ac <= 0) return -1;
    const char* pr = proj ? proj : "v_proj";
    llm_ai_mlx_diff_setup(mh);
    llm_ai_mlx_model_lora_clear(mh);
    if (llm_ai_mlx_model_lora_add(mh, layer, pr, Ac, 1.0f, A, B, Ar, Br) != 0) return -1;
    llm_ai_mlx_model_lora_set_enabled(mh, 1);
    GklPub ctx = { mh, layer, pr, toks, teacher_logits, V, T, temperature, mask };
    llm_ai_mlx_diff_set_eager_sdpa(1);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    mlx_closure cl = mlx_closure_new_func_payload(gmbllm_kl_loss_fn, &ctx, NULL);
    int sA[2] = { Ar, Ac }, sB[2] = { Br, Bc };
    mlx_array Aa = mlx_array_new_data(A, sA, 2, MLX_FLOAT32);
    mlx_array Ba = mlx_array_new_data(B, sB, 2, MLX_FLOAT32);
    mlx_vector_array in = mlx_vector_array_new();
    mlx_vector_array_append_value(in, Aa);
    mlx_vector_array_append_value(in, Ba);
    int rc = gmbllm_apply(cl, st, in, Ar, Ac, Br, Bc, out_loss, out_dA, out_dB);
    mlx_vector_array_free(in);
    mlx_closure_free(cl);
    llm_ai_mlx_diff_set_eager_sdpa(0);
    return rc;
}

/* ============================================================================
 * G6 — FULL-PARAMETER: value_and_grad della masked-CE rispetto a TUTTI i pesi (ABI
 * llm_ai_mlx_diff_{get,set}_params). `params` = vettore pesi (ordine di get_params); `out_grads` =
 * gradi (stesso ordine, ownership al chiamante). FP16/BF16 (pesi plain). 0 ok / -1.
 * ============================================================================ */
typedef struct { llm_ai_mlx_model_handle mh; const int32_t* toks; const int32_t* tgt;
                 const uint8_t* mask; int T; const char (*names)[64]; int n; } GfullCtx;

static int gmbllm_full_loss_fn(mlx_vector_array* res, const mlx_vector_array input, void* payload) {
    GfullCtx* c = (GfullCtx*)payload;
    if (llm_ai_mlx_diff_set_params(c->mh, input, c->names, c->n) != 0) return 1;
    bool ok = true;
    mlx_array loss = llm_train_forward_ce(c->mh, c->toks, c->tgt, c->mask, c->T, /*sum_mode=*/false, &ok);
    if (ok) mlx_vector_array_set_value(res, loss);
    mlx_array_free(loss);
    return ok ? 0 : 1;
}

int llm_ai_mlx_train_full_grad(llm_ai_mlx_model_handle mh,
                               const int32_t* toks, const int32_t* tgt, const uint8_t* mask, int T,
                               mlx_vector_array params, const char names[][64], int n,
                               float* out_loss, mlx_vector_array* out_grads) {
    if (!mh || !toks || !tgt || T <= 0 || n <= 0 || !names || !out_grads) return -1;
    llm_ai_mlx_diff_setup(mh);
    GfullCtx ctx = { mh, toks, tgt, mask, T, names, n };
    llm_ai_mlx_diff_set_eager_sdpa(1);
    mlx_stream st = llm_ai_mlx_diff_stream(mh);
    mlx_closure cl = mlx_closure_new_func_payload(gmbllm_full_loss_fn, &ctx, NULL);
    int* argnums = (int*)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) argnums[i] = i;
    mlx_closure_value_and_grad vg = mlx_closure_value_and_grad_new();
    mlx_vector_array vals = mlx_vector_array_new(), grads = mlx_vector_array_new();
    int rc = -1;
    if (argnums && mlx_value_and_grad(&vg, cl, argnums, n) == 0 &&
        mlx_closure_value_and_grad_apply(&vals, &grads, vg, params) == 0) {
        mlx_array lv = mlx_array_new(), lvc = mlx_array_new();
        mlx_vector_array_get(&lv, vals, 0);
        if (mlx_contiguous(&lvc, lv, false, st) == 0 && mlx_array_eval(lvc) == 0) {
            if (out_loss) *out_loss = mlx_array_data_float32(lvc)[0];
            *out_grads = mlx_vector_array_new();
            for (int i = 0; i < n; i++) {
                mlx_array g = mlx_array_new(), gc = mlx_array_new();
                mlx_vector_array_get(&g, grads, i);
                if (mlx_contiguous(&gc, g, false, st) == 0 && mlx_array_eval(gc) == 0)
                    mlx_vector_array_append_value(*out_grads, gc);
                else mlx_array_free(gc);
                mlx_array_free(g);
            }
            rc = 0;
        }
        mlx_array_free(lvc); mlx_array_free(lv);
    }
    mlx_vector_array_free(vals); mlx_vector_array_free(grads);
    mlx_closure_value_and_grad_free(vg); free(argnums); mlx_closure_free(cl);
    llm_ai_mlx_diff_set_eager_sdpa(0);
    return rc;
}

/* G6 — driver FULL-PARAMETER: loop di training (Adam) su TUTTI i pesi + salvataggio. Salva
 * `out_prefix`_params/NNN.raw + out_prefix_names.txt. 0 ok / -1. (FP16/BF16.) */
int llm_ai_mlx_train_full_fit(void* mh, const int32_t* toks, const int32_t* tgt, const uint8_t* mask,
                              int T, int steps, float lr, const char* out_prefix) {
    if (!mh || !toks || !tgt || T <= 0 || steps <= 0 || !out_prefix) return -1;
    int n = llm_ai_mlx_diff_num_params(mh);
    if (n <= 0 || n > 4096) return -1;
    char (*names)[64] = calloc((size_t)n, 64);
    mlx_vector_array params = mlx_vector_array_new();
    int got = llm_ai_mlx_diff_get_params(mh, &params, names, n);
    if (got <= 0) { free(names); return -1; }
    n = got;
    float** phost = calloc((size_t)n, sizeof(float*)); int* pnum = calloc((size_t)n, sizeof(int));
    int** pdims = calloc((size_t)n, sizeof(int*)); int* pnd = calloc((size_t)n, sizeof(int));
    float** pm = calloc((size_t)n, sizeof(float*)); float** pv = calloc((size_t)n, sizeof(float*));
    int rc = 0;
    for (int i = 0; i < n && rc == 0; i++) {
        mlx_array a = mlx_array_new(); mlx_vector_array_get(&a, params, i);
        int nd = mlx_array_ndim(a); int* dl = (int*)malloc((size_t)(nd > 0 ? nd : 1) * sizeof(int));
        int num = 1; for (int d = 0; d < nd; d++) { dl[d] = (int)mlx_array_shape(a)[d]; num *= dl[d]; }
        mlx_array ac = mlx_array_new(), af = mlx_array_new();
        /* i pesi possono essere F16/BF16: `mlx_array_data_float32` assume F32 -> cast esplicito. */
        if (mlx_contiguous(&ac, a, false, llm_ai_mlx_diff_stream(mh)) != 0 ||
            mlx_astype(&af, ac, MLX_FLOAT32, llm_ai_mlx_diff_stream(mh)) != 0 || mlx_array_eval(af) != 0) rc = -1;
        else { phost[i] = (float*)malloc((size_t)num * 4); memcpy(phost[i], mlx_array_data_float32(af), (size_t)num * 4);
               pnum[i] = num; pdims[i] = dl; pnd[i] = nd; pm[i] = calloc((size_t)num, 4); pv[i] = calloc((size_t)num, 4); }
        mlx_array_free(ac); mlx_array_free(af); mlx_array_free(a);
    }
    for (int step = 0; step < steps && rc == 0; step++) {
        float loss = 0; mlx_vector_array grads = mlx_vector_array_new();
        int grc = llm_ai_mlx_train_full_grad(mh, toks, tgt, mask, T, params, names, n, &loss, &grads);
        if (grc != 0) { mlx_vector_array_free(grads); rc = -1; break; }
        mlx_vector_array nparams = mlx_vector_array_new();
        for (int i = 0; i < n; i++) {
            mlx_array g = mlx_array_new(), gf = mlx_array_new(); mlx_vector_array_get(&g, grads, i);
            mlx_astype(&gf, g, MLX_FLOAT32, llm_ai_mlx_diff_stream(mh)); mlx_array_eval(gf);
            const float* gh = mlx_array_data_float32(gf);
            for (int k = 0; k < pnum[i]; k++) {
                float gi = gh[k];
                pm[i][k] = 0.9f * pm[i][k] + 0.1f * gi; pv[i][k] = 0.999f * pv[i][k] + 0.001f * gi * gi;
                float mh2 = pm[i][k] / (1.0f - powf(0.9f, (float)step + 1));
                float vh = pv[i][k] / (1.0f - powf(0.999f, (float)step + 1));
                phost[i][k] -= lr * mh2 / (sqrtf(vh) + 1e-8f);
            }
            mlx_array na = mlx_array_new_data(phost[i], pdims[i], pnd[i], MLX_FLOAT32);
            mlx_vector_array_append_value(nparams, na);
            mlx_array_free(gf); mlx_array_free(g);
        }
        mlx_vector_array_free(grads);
        llm_ai_mlx_diff_set_params(mh, nparams, names, n);
        mlx_vector_array_free(params); params = nparams;
    }
    if (rc == 0) {   /* salva: <out_prefix>.names (names) + <out_prefix>.NNN.raw (pesi F32) */
        char p[1200];
        snprintf(p, sizeof p, "%s.names", out_prefix); FILE* fn = fopen(p, "w");
        if (fn) { for (int i = 0; i < n; i++) fprintf(fn, "%s\n", names[i]); fclose(fn); } else rc = -1;
        for (int i = 0; i < n && rc == 0; i++) {
            snprintf(p, sizeof p, "%s.%03d.raw", out_prefix, i); FILE* fp = fopen(p, "wb");
            if (!fp) { rc = -1; break; }
            fwrite(phost[i], 4, (size_t)pnum[i], fp); fclose(fp);
        }
    }
    for (int i = 0; i < n; i++) { free(phost[i]); free(pdims[i]); free(pm[i]); free(pv[i]); }
    free(phost); free(pnum); free(pdims); free(pnd); free(pm); free(pv); free(names);
    mlx_vector_array_free(params);
    return rc;
}
