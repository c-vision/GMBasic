/* llm_decide_calib.c — calibrazione a temperatura 2 stadi (T5, PUL). Vedi header. */
#include "llm_decide_calib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T_MIN 0.05
#define T_MAX 20.0
#define T_THRESHOLD 10       /* domande minime per fittare (globale e per kind) */
#define ECE_BINS 10

typedef struct {
    double brier;
    double nll;
    double accuracy;
    double ece;
    int n;
} calib_metrics_t;

static void softmax_at(const llm_decide_sample_t* s, double scale, double T, double* p) {
    double denom = scale * T;
    if (!(denom > 0.0)) denom = 1.0;
    double maxz = -INFINITY;
    for (int k = 0; k < s->n_options; k++) {
        double z = (double)s->logprob[k] / denom;
        if (z > maxz) maxz = z;
    }
    double sum = 0.0;
    for (int k = 0; k < s->n_options; k++) {
        p[k] = exp((double)s->logprob[k] / denom - maxz);
        sum += p[k];
    }
    if (!(sum > 0.0)) sum = 1.0;
    for (int k = 0; k < s->n_options; k++) p[k] /= sum;
}

/* Metriche per il sottoinsieme `idx` (se NULL, tutte). `scale` = head_T, `Tkind` = T del kind. */
static calib_metrics_t metrics_on(const llm_decide_sample_t* samples, int n,
                                  const int* idx, int nidx, double scale,
                                  const double Tkind[3]) {
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
        softmax_at(s, scale, Tkind[s->kind], p);
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

/* Brier sul sottoinsieme per la temperatura T (scale = head_T fisso, kind singolo). */
static double brier_at(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                       double scale, int kind, double T) {
    double brier = 0.0;
    int count = idx ? nidx : n;
    for (int ii = 0; ii < count; ii++) {
        int i = idx ? idx[ii] : ii;
        const llm_decide_sample_t* s = &samples[i];
        if (s->kind != kind) continue;
        double p[LLM_DECIDE_MAX_OPTIONS];
        softmax_at(s, scale, T, p);
        for (int k = 0; k < s->n_options; k++) {
            double y = (k == s->label) ? 1.0 : 0.0;
            double d = p[k] - y;
            brier += d * d;
        }
    }
    return brier;
}

/* Golden section su T in [T_MIN,T_MAX], dopo una griglia grossolana per il bracket. */
static double fit_t(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                    double scale, int kind, int count_kind) {
    if (count_kind < T_THRESHOLD) return 1.0;
    const int grid = 48;
    double best_t = 1.0, best_f = 0.0;
    int best_i = 0;
    for (int i = 0; i < grid; i++) {
        double t = T_MIN + (T_MAX - T_MIN) * ((double)i / (grid - 1));
        double f = brier_at(samples, n, idx, nidx, scale, kind, t);
        if (i == 0 || f < best_f) { best_f = f; best_t = t; best_i = i; }
    }
    double step = (T_MAX - T_MIN) / (grid - 1);
    double lo = best_t - step, hi = best_t + step;
    if (lo < T_MIN) lo = T_MIN;
    if (hi > T_MAX) hi = T_MAX;
    const double phi = 0.6180339887498949;
    double a = lo, b = hi;
    double c = b - phi * (b - a), d = a + phi * (b - a);
    double fc = brier_at(samples, n, idx, nidx, scale, kind, c);
    double fd = brier_at(samples, n, idx, nidx, scale, kind, d);
    for (int it = 0; it < 60 && (b - a) > 1e-4; it++) {
        if (fc < fd) {
            b = d; d = c; fd = fc;
            c = b - phi * (b - a); fc = brier_at(samples, n, idx, nidx, scale, kind, c);
        } else {
            a = c; c = d; fc = fd;
            d = a + phi * (b - a); fd = brier_at(samples, n, idx, nidx, scale, kind, d);
        }
    }
    double t = 0.5 * (a + b);
    double ft = brier_at(samples, n, idx, nidx, scale, kind, t);
    (void)best_i;
    return ft <= best_f ? t : best_t;
}

static int count_kind(const llm_decide_sample_t* samples, int n, const int* idx, int nidx, int kind) {
    int c = 0, count = idx ? nidx : n;
    for (int ii = 0; ii < count; ii++) {
        int i = idx ? idx[ii] : ii;
        if (samples[i].kind == kind) c++;
    }
    return c;
}

/* Fit 2 stadi su un sottoinsieme (idx NULL = tutte). */
static void fit_model(const llm_decide_sample_t* samples, int n, const int* idx, int nidx,
                      float* head_T, float T_kind[3]) {
    int count = idx ? nidx : n;
    int nk[3] = {
        count_kind(samples, n, idx, nidx, 0),
        count_kind(samples, n, idx, nidx, 1),
        count_kind(samples, n, idx, nidx, 2),
    };
    if (count < T_THRESHOLD) {
        *head_T = 1.0f;
        T_kind[0] = T_kind[1] = T_kind[2] = 1.0f;
        return;
    }
    /* Stage 1: head_T con T_kind=1. */
    double best_t = 1.0, best_f = 0.0;
    const int grid = 48;
    for (int i = 0; i < grid; i++) {
        double t = T_MIN + (T_MAX - T_MIN) * ((double)i / (grid - 1));
        double f = 0.0;
        for (int kind = 0; kind < 3; kind++) {
            if (nk[kind] < T_THRESHOLD) continue;
            f += brier_at(samples, n, idx, nidx, 1.0, kind, t);
        }
        if (i == 0 || f < best_f) { best_f = f; best_t = t; }
    }
    double lo = T_MIN, hi = T_MAX;
    {
        double step = (T_MAX - T_MIN) / (grid - 1);
        lo = best_t - step; if (lo < T_MIN) lo = T_MIN;
        hi = best_t + step; if (hi > T_MAX) hi = T_MAX;
    }
    const double phi = 0.6180339887498949;
    double a = lo, b = hi;
    double c = b - phi * (b - a), d = a + phi * (b - a);
    double fc = 0.0, fd = 0.0;
    for (int kind = 0; kind < 3; kind++) {
        if (nk[kind] < T_THRESHOLD) continue;
        fc += brier_at(samples, n, idx, nidx, 1.0, kind, c);
        fd += brier_at(samples, n, idx, nidx, 1.0, kind, d);
    }
    for (int it = 0; it < 60 && (b - a) > 1e-4; it++) {
        if (fc < fd) {
            b = d; d = c; fd = fc; c = b - phi * (b - a);
            fc = 0.0;
            for (int kind = 0; kind < 3; kind++) if (nk[kind] >= T_THRESHOLD) fc += brier_at(samples, n, idx, nidx, 1.0, kind, c);
        } else {
            a = c; c = d; fc = fd; d = a + phi * (b - a);
            fd = 0.0;
            for (int kind = 0; kind < 3; kind++) if (nk[kind] >= T_THRESHOLD) fd += brier_at(samples, n, idx, nidx, 1.0, kind, d);
        }
    }
    *head_T = (float)(0.5 * (a + b));
    if (!(*head_T > 0.0f)) *head_T = 1.0f;

    /* Stage 2: T per kind sui logits già divisi per head_T. */
    for (int kind = 0; kind < 3; kind++) {
        T_kind[kind] = (float)fit_t(samples, n, idx, nidx, *head_T, kind, nk[kind]);
        if (!(T_kind[kind] > 0.0f)) T_kind[kind] = 1.0f;
    }
}

int llm_decide_calib_fit(const llm_decide_sample_t* samples, int n,
                         llm_decide_calib_result_t* result, const char* out_path) {
    if (!samples || n < 1 || !result) return -1;
    memset(result, 0, sizeof(*result));
    for (int i = 0; i < n; i++) {
        const llm_decide_sample_t* s = &samples[i];
        if (s->kind < 0 || s->kind > 2 || s->n_options < 1 || s->n_options > LLM_DECIDE_MAX_OPTIONS
            || s->label < 0 || s->label >= s->n_options) return -1;
        result->n_kind[s->kind]++;
    }
    result->n_questions = n;

    double T_identity[3] = { 1.0, 1.0, 1.0 };
    calib_metrics_t before = metrics_on(samples, n, NULL, 0, 1.0, T_identity);
    result->accuracy = before.accuracy;

    if (n < T_THRESHOLD) {
        result->head_T = 1.0f;
        result->T_kind[0] = result->T_kind[1] = result->T_kind[2] = 1.0f;
        result->identity = 1;
        result->cv_folds = 1;
        result->brier_before = result->brier_after = before.brier;
        result->nll_before = result->nll_after = before.nll;
        result->ece_before = result->ece_after = before.ece;
        result->brier_oof_before = result->brier_oof_after = before.brier;
        result->ece_oof_before = result->ece_oof_after = before.ece;
    } else {
        fit_model(samples, n, NULL, 0, &result->head_T, result->T_kind);
        result->identity = (result->head_T == 1.0f
                            && result->T_kind[0] == 1.0f && result->T_kind[1] == 1.0f
                            && result->T_kind[2] == 1.0f) ? 1 : 0;
        double tk[3] = { result->T_kind[0], result->T_kind[1], result->T_kind[2] };
        calib_metrics_t after = metrics_on(samples, n, NULL, 0, result->head_T, tk);
        result->brier_before = before.brier;
        result->brier_after = after.brier;
        result->nll_before = before.nll;
        result->nll_after = after.nll;
        result->ece_before = before.ece;
        result->ece_after = after.ece;

        /* 2-fold out-of-fold: fit sul fold opposto, valutazione sul fold tenuto fuori. */
        if (n >= 2 * T_THRESHOLD) {
            int* idxA = (int*)malloc((size_t)n * sizeof(int));
            int* idxB = (int*)malloc((size_t)n * sizeof(int));
            int na = 0, nb = 0;
            if (idxA && idxB) {
                for (int i = 0; i < n; i++) { if (i % 2 == 0) idxA[na++] = i; else idxB[nb++] = i; }
                float hT; float tK[3];
                double b_before = 0.0, b_after = 0.0, e_before = 0.0, e_after = 0.0;

                fit_model(samples, n, idxA, na, &hT, tK);
                double tk2[3] = { tK[0], tK[1], tK[2] };
                calib_metrics_t id = metrics_on(samples, n, idxB, nb, 1.0, T_identity);
                calib_metrics_t fit = metrics_on(samples, n, idxB, nb, hT, tk2);
                b_before += id.brier * nb; e_before += id.ece * nb;
                b_after += fit.brier * nb; e_after += fit.ece * nb;

                fit_model(samples, n, idxB, nb, &hT, tK);
                double tk3[3] = { tK[0], tK[1], tK[2] };
                id = metrics_on(samples, n, idxA, na, 1.0, T_identity);
                fit = metrics_on(samples, n, idxA, na, hT, tk3);
                b_before += id.brier * na; e_before += id.ece * na;
                b_after += fit.brier * na; e_after += fit.ece * na;

                result->brier_oof_before = b_before / n;
                result->brier_oof_after = b_after / n;
                result->ece_oof_before = e_before / n;
                result->ece_oof_after = e_after / n;
                result->cv_folds = 2;
            } else {
                result->brier_oof_before = before.brier;
                result->brier_oof_after = after.brier;
                result->ece_oof_before = before.ece;
                result->ece_oof_after = after.ece;
                result->cv_folds = 1;
            }
            free(idxA); free(idxB);
        } else {
            result->brier_oof_before = before.brier;
            result->brier_oof_after = after.brier;
            result->ece_oof_before = before.ece;
            result->ece_oof_after = after.ece;
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
            "  \"T_kind\": {\"noul\": %.6f, \"choice\": %.6f, \"score\": %.6f},\n"
            "  \"calibration\": {\"method\": \"temperature-2stage-brier\", \"n_questions\": %d,"
            " \"cv_folds\": %d, \"accuracy\": %.6f, \"brier_before\": %.6f, \"brier_after\": %.6f,"
            " \"ece_before\": %.6f, \"ece_after\": %.6f, \"brier_oof_before\": %.6f,"
            " \"brier_oof_after\": %.6f, \"ece_oof_before\": %.6f, \"ece_oof_after\": %.6f}\n"
            "}\n",
            result->head_T, result->T_kind[0], result->T_kind[1], result->T_kind[2],
            result->n_questions, result->cv_folds, result->accuracy,
            result->brier_before, result->brier_after, result->ece_before, result->ece_after,
            result->brier_oof_before, result->brier_oof_after,
            result->ece_oof_before, result->ece_oof_after);
        fclose(f);
    }
    return 0;
}
