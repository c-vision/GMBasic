/* llm_decide_calib.c — calibrazione a temperatura 2 stadi + bias per slot (T5/T4, PUL). */
#include "llm_decide_calib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T_MIN 0.05
#define T_MAX 20.0
#define T_THRESHOLD 10       /* domande minime per fittare (globale, per kind e per slot) */
#define BIAS_MIN (-8.0)
#define BIAS_MAX 8.0
#define ECE_BINS 10

static const char* const SLOT_NAMES[24] = {
    "false", "true", "0", "1", "2", "3", "4", "5",
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P",
};

typedef struct {
    double brier;
    double nll;
    double accuracy;
    double ece;
    int n;
} calib_metrics_t;

/* Slot canonico dell'opzione i della domanda (ordine del dominio della superficie):
 * NOUL false/true = 0/1 · SCORE 0..5 = 2..7 · CHOICE A..P = 8..23. -1 = non canonico. */
static int slot_of(int kind, int i) {
    if (kind == 0) return (i < 2) ? i : -1;
    if (kind == 2) return (i < 6) ? 2 + i : -1;
    if (kind == 1) return (i < 16) ? 8 + i : -1;
    return -1;
}

static void softmax_at(const llm_decide_sample_t* s, double scale, double T,
                       const float* bias, double* p) {
    double denom = scale * T;
    if (!(denom > 0.0)) denom = 1.0;
    double maxz = -INFINITY;
    for (int k = 0; k < s->n_options; k++) {
        int slot = slot_of(s->kind, k);
        double b = (bias && slot >= 0) ? bias[slot] : 0.0;
        double z = ((double)s->logprob[k] + b) / denom;
        if (z > maxz) maxz = z;
    }
    double sum = 0.0;
    for (int k = 0; k < s->n_options; k++) {
        int slot = slot_of(s->kind, k);
        double b = (bias && slot >= 0) ? bias[slot] : 0.0;
        p[k] = exp(((double)s->logprob[k] + b) / denom - maxz);
        sum += p[k];
    }
    if (!(sum > 0.0)) sum = 1.0;
    for (int k = 0; k < s->n_options; k++) p[k] /= sum;
}

static calib_metrics_t metrics_on(const llm_decide_sample_t* samples, int n,
                                  const int* idx, int nidx, double scale,
                                  const double Tkind[3], const float* bias) {
    calib_metrics_t m = { 0 };
    double brier = 0.0, nll = 0.0;
    int correct = 0;
    int bin_n[ECE_BINS] = { 0 };
    double bin_conf[ECE_BINS] = { 0 }, bin_acc[ECE_BINS] = { 0 };
    int count = idx ? nidx : n;
    for (int ii = 0; ii < count; ii++) {
        int i = idx ? idx[ii] : ii;
        const llm_decide_sample_t* s = &samples[i];
        double p[LLM_DECIDE_MAX_OPTIONS];
        softmax_at(s, scale, Tkind[s->kind], bias, p);
        int best = 0;
        for (int k = 0; k < s->n_options; k++) {
            double y = (k == s->label) ? 1.0 : 0.0;
            double d = p[k] - y;
            brier += d * d;
            if (p[k] > p[best]) best = k;
        }
        nll -= log(p[s->label] > 1e-300 ? p[s->label] : 1e-300);
        if (best == s->label) correct++;

        double conf = p[best];
        int bin = (int)(conf * ECE_BINS);
        if (bin >= ECE_BINS) bin = ECE_BINS - 1;
        if (bin < 0) bin = 0;
        bin_n[bin]++;
        bin_conf[bin] += conf;
        bin_acc[bin] += (best == s->label) ? 1.0 : 0.0;
    }
    if (count > 0) {
        m.brier = brier / count;
        m.nll = nll / count;
        m.accuracy = (double)correct / count;
        double ece = 0.0;
        for (int b = 0; b < ECE_BINS; b++) {
            if (!bin_n[b]) continue;
            double avg_conf = bin_conf[b] / bin_n[b];
            double avg_acc = bin_acc[b] / bin_n[b];
            ece += ((double)bin_n[b] / count) * fabs(avg_acc - avg_conf);
        }
        m.ece = ece;
        m.n = count;
    }
    return m;
}

static double brier_at(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                       double scale, int kind, double T, const float* bias) {
    double brier = 0.0;
    int count = idx ? nidx : n;
    for (int ii = 0; ii < count; ii++) {
        int i = idx ? idx[ii] : ii;
        const llm_decide_sample_t* s = &samples[i];
        if (s->kind != kind) continue;
        double p[LLM_DECIDE_MAX_OPTIONS];
        softmax_at(s, scale, T, bias, p);
        for (int k = 0; k < s->n_options; k++) {
            double y = (k == s->label) ? 1.0 : 0.0;
            double d = p[k] - y;
            brier += d * d;
        }
    }
    return brier;
}

static int count_kind(const llm_decide_sample_t* samples, int n, const int* idx, int nidx, int kind) {
    int c = 0, count = idx ? nidx : n;
    for (int ii = 0; ii < count; ii++) {
        int i = idx ? idx[ii] : ii;
        if (samples[i].kind == kind) c++;
    }
    return c;
}

/* Golden section 1D su [lo,hi] per una funzione unimodale. */
static double golden_min(double lo, double hi, int iters,
                         double (*f)(const void*, double), const void* ctx) {
    const double phi = 0.6180339887498949;
    double a = lo, b = hi;
    double c = b - phi * (b - a), d = a + phi * (b - a);
    double fc = f(ctx, c), fd = f(ctx, d);
    for (int it = 0; it < iters && (b - a) > 1e-4; it++) {
        if (fc < fd) {
            b = d; d = c; fd = fc;
            c = b - phi * (b - a); fc = f(ctx, c);
        } else {
            a = c; c = d; fc = fd;
            d = a + phi * (b - a); fd = f(ctx, d);
        }
    }
    return 0.5 * (a + b);
}

typedef struct {
    const llm_decide_sample_t* samples;
    int n;
    const int* idx;
    int nidx;
    double scale;
    int kind;
} fit_ctx_t;

static double fit_t_obj(const void* vctx, double t) {
    const fit_ctx_t* c = (const fit_ctx_t*)vctx;
    return brier_at(c->samples, c->n, c->idx, c->nidx, c->scale, c->kind, t, NULL);
}

static double fit_t(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                    double scale, int kind, int count_kind) {
    if (count_kind < T_THRESHOLD) return 1.0;
    const int grid = 48;
    double best_t = 1.0, best_f = 0.0;
    for (int i = 0; i < grid; i++) {
        double t = T_MIN + (T_MAX - T_MIN) * ((double)i / (grid - 1));
        double f = brier_at(samples, n, idx, nidx, scale, kind, t, NULL);
        if (i == 0 || f < best_f) { best_f = f; best_t = t; }
    }
    double step = (T_MAX - T_MIN) / (grid - 1);
    double lo = best_t - step, hi = best_t + step;
    if (lo < T_MIN) lo = T_MIN;
    if (hi > T_MAX) hi = T_MAX;
    fit_ctx_t ctx = { samples, n, idx, nidx, scale, kind };
    double t = golden_min(lo, hi, 60, fit_t_obj, &ctx);
    return fit_t_obj(&ctx, t) <= best_f ? t : best_t;
}

typedef struct {
    const llm_decide_sample_t* samples;
    int n;
    const int* idx;
    int nidx;
    double scale;
    const double* Tkind;
    const float* bias;   /* base, con lo slot in esame già aggiornato */
    int slot;
} bias_ctx_t;

/* Brier totale del sottoinsieme usando `bias` ma con lo slot `slot` impostato a b. */
static double brier_with_slot(const bias_ctx_t* c, double b) {
    float tmp[24];
    memcpy(tmp, c->bias, sizeof tmp);
    tmp[c->slot] = (float)b;
    double total = 0.0;
    int count = c->idx ? c->nidx : c->n;
    for (int ii = 0; ii < count; ii++) {
        int i = c->idx ? c->idx[ii] : ii;
        const llm_decide_sample_t* s = &c->samples[i];
        double p[LLM_DECIDE_MAX_OPTIONS];
        softmax_at(s, c->scale, c->Tkind[s->kind], tmp, p);
        for (int k = 0; k < s->n_options; k++) {
            double y = (k == s->label) ? 1.0 : 0.0;
            double d = p[k] - y;
            total += d * d;
        }
    }
    return total;
}

static double bias_obj(const void* vctx, double b) {
    return brier_with_slot((const bias_ctx_t*)vctx, b);
}

/* Fit dei bias per slot (solo slot con >= T_THRESHOLD campioni), 2 passate coordinate. */
static int fit_bias_slots(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                          double scale, const double Tkind[3], float bias[24]) {
    memset(bias, 0, 24 * sizeof(float));
    int counts[24] = { 0 };
    int count = idx ? nidx : n;
    for (int ii = 0; ii < count; ii++) {
        int i = idx ? idx[ii] : ii;
        const llm_decide_sample_t* s = &samples[i];
        for (int k = 0; k < s->n_options; k++) {
            int slot = slot_of(s->kind, k);
            if (slot >= 0) counts[slot]++;
        }
    }
    int used = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int slot = 0; slot < 24; slot++) {
            if (counts[slot] < T_THRESHOLD) continue;
            bias_ctx_t ctx = { samples, n, idx, nidx, scale, Tkind, bias, slot };
            double b = golden_min(BIAS_MIN, BIAS_MAX, 50, bias_obj, &ctx);
            bias[slot] = (float)b;
            if (pass == 0) used++;
        }
    }
    return used;
}

static void fit_model(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                      int fit_bias, float* head_T, float T_kind[3], float bias[24]) {
    int count = idx ? nidx : n;
    int nk[3] = {
        count_kind(samples, n, idx, nidx, 0),
        count_kind(samples, n, idx, nidx, 1),
        count_kind(samples, n, idx, nidx, 2),
    };
    memset(bias, 0, 24 * sizeof(float));
    if (count < T_THRESHOLD) {
        *head_T = 1.0f;
        T_kind[0] = T_kind[1] = T_kind[2] = 1.0f;
        return;
    }
    /* Stage 1: head_T con T_kind=1, bias=0. */
    const int grid = 48;
    double best_t = 1.0, best_f = 0.0;
    for (int i = 0; i < grid; i++) {
        double t = T_MIN + (T_MAX - T_MIN) * ((double)i / (grid - 1));
        double f = 0.0;
        for (int kind = 0; kind < 3; kind++)
            if (nk[kind] >= T_THRESHOLD) f += brier_at(samples, n, idx, nidx, 1.0, kind, t, NULL);
        if (i == 0 || f < best_f) { best_f = f; best_t = t; }
    }
    double step = (T_MAX - T_MIN) / (grid - 1);
    double lo = best_t - step, hi = best_t + step;
    if (lo < T_MIN) lo = T_MIN;
    if (hi > T_MAX) hi = T_MAX;
    {
        /* Golden section globale: somma dei kind eleggibili. */
        const double phi = 0.6180339887498949;
        double a = lo, b = hi;
        double c = b - phi * (b - a), d = a + phi * (b - a);
        double fc = 0.0, fd = 0.0;
        for (int kind = 0; kind < 3; kind++) {
            if (nk[kind] < T_THRESHOLD) continue;
            fc += brier_at(samples, n, idx, nidx, 1.0, kind, c, NULL);
            fd += brier_at(samples, n, idx, nidx, 1.0, kind, d, NULL);
        }
        for (int it = 0; it < 60 && (b - a) > 1e-4; it++) {
            if (fc < fd) {
                b = d; d = c; fd = fc; c = b - phi * (b - a);
                fc = 0.0;
                for (int kind = 0; kind < 3; kind++) if (nk[kind] >= T_THRESHOLD)
                    fc += brier_at(samples, n, idx, nidx, 1.0, kind, c, NULL);
            } else {
                a = c; c = d; fc = fd; d = a + phi * (b - a);
                fd = 0.0;
                for (int kind = 0; kind < 3; kind++) if (nk[kind] >= T_THRESHOLD)
                    fd += brier_at(samples, n, idx, nidx, 1.0, kind, d, NULL);
            }
        }
        *head_T = (float)(0.5 * (a + b));
    }
    if (!(*head_T > 0.0f)) *head_T = 1.0f;

    /* Stage 2: T per kind sui logits già divisi per head_T. */
    for (int kind = 0; kind < 3; kind++) {
        T_kind[kind] = (float)fit_t(samples, n, idx, nidx, *head_T, kind, nk[kind]);
        if (!(T_kind[kind] > 0.0f)) T_kind[kind] = 1.0f;
    }

    /* Stage 3 (T4, opzionale): bias per slot, T fissate. */
    if (fit_bias) {
        double tk[3] = { T_kind[0], T_kind[1], T_kind[2] };
        fit_bias_slots(samples, n, idx, nidx, *head_T, tk, bias);
    }
}

int llm_decide_calib_fit_ex(const llm_decide_sample_t* samples, int n,
                            llm_decide_calib_result_t* result, const char* out_path,
                            int fit_bias) {
    if (!samples || n < 1 || !result) return -1;
    memset(result, 0, sizeof(*result));
    for (int i = 0; i < n; i++) {
        const llm_decide_sample_t* s = &samples[i];
        if (s->kind < 0 || s->kind > 2 || s->n_options < 1 || s->n_options > LLM_DECIDE_MAX_OPTIONS
            || s->label < 0 || s->label >= s->n_options) return -1;
        result->n_kind[s->kind]++;
    }
    result->n_questions = n;
    result->fit_bias = fit_bias ? 1 : 0;

    double T_identity[3] = { 1.0, 1.0, 1.0 };
    calib_metrics_t before = metrics_on(samples, n, NULL, 0, 1.0, T_identity, NULL);
    result->accuracy = before.accuracy;

    if (n < T_THRESHOLD) {
        result->head_T = 1.0f;
        result->T_kind[0] = result->T_kind[1] = result->T_kind[2] = 1.0f;
        result->identity = 1;
        result->cv_folds = 1;
        result->brier_before = result->brier_after = result->brier_bias = before.brier;
        result->nll_before = result->nll_after = before.nll;
        result->ece_before = result->ece_after = result->ece_bias = before.ece;
        result->brier_oof_before = result->brier_oof_after = before.brier;
        result->ece_oof_before = result->ece_oof_after = before.ece;
    } else {
        fit_model(samples, n, NULL, 0, fit_bias, &result->head_T, result->T_kind, result->bias);
        result->identity = (result->head_T == 1.0f
                            && result->T_kind[0] == 1.0f && result->T_kind[1] == 1.0f
                            && result->T_kind[2] == 1.0f) ? 1 : 0;
        double tk[3] = { result->T_kind[0], result->T_kind[1], result->T_kind[2] };
        calib_metrics_t after = metrics_on(samples, n, NULL, 0, result->head_T, tk, NULL);
        result->brier_before = before.brier;
        result->brier_after = after.brier;
        result->nll_before = before.nll;
        result->nll_after = after.nll;
        result->ece_before = before.ece;
        result->ece_after = after.ece;
        if (fit_bias) {
            calib_metrics_t with = metrics_on(samples, n, NULL, 0, result->head_T, tk, result->bias);
            result->brier_bias = with.brier;
            result->ece_bias = with.ece;
            int used = 0;
            for (int s = 0; s < 24; s++) if (result->bias[s] != 0.0f) used++;
            result->bias_slots = used;
        } else {
            result->brier_bias = after.brier;
            result->ece_bias = after.ece;
        }

        /* 2-fold out-of-fold: fit sul fold opposto, valutazione sul fold tenuto fuori. */
        if (n >= 2 * T_THRESHOLD) {
            int* idxA = (int*)malloc((size_t)n * sizeof(int));
            int* idxB = (int*)malloc((size_t)n * sizeof(int));
            int na = 0, nb = 0;
            if (idxA && idxB) {
                for (int i = 0; i < n; i++) { if (i % 2 == 0) idxA[na++] = i; else idxB[nb++] = i; }
                float hT, tK[3], bf[24];
                double b_before = 0.0, b_after = 0.0, e_before = 0.0, e_after = 0.0;

                fit_model(samples, n, idxA, na, fit_bias, &hT, tK, bf);
                double tk2[3] = { tK[0], tK[1], tK[2] };
                calib_metrics_t id = metrics_on(samples, n, idxB, nb, 1.0, T_identity, NULL);
                calib_metrics_t fit = metrics_on(samples, n, idxB, nb, hT, tk2, fit_bias ? bf : NULL);
                b_before += id.brier * nb; e_before += id.ece * nb;
                b_after += fit.brier * nb; e_after += fit.ece * nb;

                fit_model(samples, n, idxB, nb, fit_bias, &hT, tK, bf);
                double tk3[3] = { tK[0], tK[1], tK[2] };
                id = metrics_on(samples, n, idxA, na, 1.0, T_identity, NULL);
                fit = metrics_on(samples, n, idxA, na, hT, tk3, fit_bias ? bf : NULL);
                b_before += id.brier * na; e_before += id.ece * na;
                b_after += fit.brier * na; e_after += fit.ece * na;

                result->brier_oof_before = b_before / n;
                result->brier_oof_after = b_after / n;
                result->ece_oof_before = e_before / n;
                result->ece_oof_after = e_after / n;
                result->cv_folds = 2;
            } else {
                result->brier_oof_before = before.brier;
                result->brier_oof_after = result->brier_bias;
                result->ece_oof_before = before.ece;
                result->ece_oof_after = result->ece_bias;
                result->cv_folds = 1;
            }
            free(idxA); free(idxB);
        } else {
            result->brier_oof_before = before.brier;
            result->brier_oof_after = result->brier_bias;
            result->ece_oof_before = before.ece;
            result->ece_oof_after = result->ece_bias;
            result->cv_folds = 1;
        }
    }

    if (out_path && *out_path) {
        FILE* f = fopen(out_path, "wb");
        if (!f) return -1;
        fprintf(f,
            "{\n"
            "  \"format\": \"gmb-scoring-head/1\",\n"
            "  \"head_T\": %.6f,\n"
            "  \"T_kind\": {\"noul\": %.6f, \"choice\": %.6f, \"score\": %.6f},\n",
            result->head_T, result->T_kind[0], result->T_kind[1], result->T_kind[2]);
        if (fit_bias) {
            fprintf(f, "  \"bias\": {");
            for (int s = 0; s < 24; s++)
                fprintf(f, "%s\"%s\": %.6f", s ? ", " : "", SLOT_NAMES[s], result->bias[s]);
            fprintf(f, "},\n");
        }
        fprintf(f,
            "  \"calibration\": {\"method\": \"temperature-2stage-brier\", \"n_questions\": %d,"
            " \"cv_folds\": %d, \"accuracy\": %.6f, \"brier_before\": %.6f, \"brier_after\": %.6f,"
            " \"ece_before\": %.6f, \"ece_after\": %.6f, \"brier_with_bias\": %.6f,"
            " \"ece_with_bias\": %.6f, \"brier_oof_before\": %.6f, \"brier_oof_after\": %.6f,"
            " \"ece_oof_before\": %.6f, \"ece_oof_after\": %.6f}\n"
            "}\n",
            result->n_questions, result->cv_folds, result->accuracy,
            result->brier_before, result->brier_after, result->ece_before, result->ece_after,
            result->brier_bias, result->ece_bias,
            result->brier_oof_before, result->brier_oof_after,
            result->ece_oof_before, result->ece_oof_after);
        fclose(f);
    }
    return 0;
}

int llm_decide_calib_fit(const llm_decide_sample_t* samples, int n,
                         llm_decide_calib_result_t* result, const char* out_path) {
    return llm_decide_calib_fit_ex(samples, n, result, out_path, 0);
}

int llm_decide_eval(const llm_decide_sample_t* samples, int n,
                    float head_T, const float T_kind[3],
                    llm_decide_metrics_t* overall, llm_decide_metrics_t per_kind[3]) {
    if (!samples || n < 1) return -1;
    for (int i = 0; i < n; i++) {
        const llm_decide_sample_t* s = &samples[i];
        if (s->kind < 0 || s->kind > 2 || s->n_options < 1 || s->n_options > LLM_DECIDE_MAX_OPTIONS
            || s->label < 0 || s->label >= s->n_options) return -1;
    }
    double tk[3] = { T_kind ? T_kind[0] : 1.0, T_kind ? T_kind[1] : 1.0, T_kind ? T_kind[2] : 1.0 };
    double scale = (head_T > 0.0f) ? head_T : 1.0;
    calib_metrics_t all = metrics_on(samples, n, NULL, 0, scale, tk, NULL);
    if (overall) {
        overall->n = all.n;
        overall->accuracy = all.accuracy;
        overall->brier = all.brier;
        overall->nll = all.nll;
        overall->ece = all.ece;
    }
    if (per_kind) {
        for (int k = 0; k < 3; k++) {
            int* idx = (int*)malloc((size_t)n * sizeof(int));
            int ni = 0;
            if (!idx) return -1;
            for (int i = 0; i < n; i++) if (samples[i].kind == k) idx[ni++] = i;
            calib_metrics_t m = metrics_on(samples, n, idx, ni, scale, tk, NULL);
            per_kind[k].n = m.n;
            per_kind[k].accuracy = m.accuracy;
            per_kind[k].brier = m.brier;
            per_kind[k].nll = m.nll;
            per_kind[k].ece = m.ece;
            free(idx);
        }
    }
    return 0;
}
