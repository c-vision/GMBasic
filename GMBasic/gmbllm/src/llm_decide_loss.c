/* llm_decide_loss.c — loss decisionali JEV (T4, PUL). Vedi header. */
#include "llm_decide_loss.h"

#include <math.h>
#include <string.h>

static int valid_count(const llm_decide_loss_sample_t* s) {
    if (s->mask == 0) return s->n_options;
    int n = 0;
    for (int k = 0; k < s->n_options; k++) if (s->mask & (1u << k)) n++;
    return n;
}

static void softmax_masked(const llm_decide_loss_sample_t* s, double* p, int* valid, int nval) {
    double maxz = -INFINITY;
    for (int k = 0; k < s->n_options; k++) {
        valid[k] = (s->mask == 0) || (s->mask & (1u << k));
        if (valid[k] && s->logits[k] > maxz) maxz = s->logits[k];
    }
    double sum = 0.0;
    for (int k = 0; k < s->n_options; k++) {
        p[k] = valid[k] ? exp((double)s->logits[k] - maxz) : 0.0;
        sum += p[k];
    }
    if (!(sum > 0.0)) sum = 1.0;
    for (int k = 0; k < s->n_options; k++) p[k] /= sum;
    (void)nval;
}

int llm_decide_loss_compute(const llm_decide_loss_sample_t* s,
                            const llm_decide_loss_opts_t* opts,
                            llm_decide_loss_result_t* out) {
    if (!s || !out) return -1;
    if (s->n_options < 1 || s->n_options > LLM_DECIDE_LOSS_MAX) return -1;
    if (s->label < 0 || s->label >= s->n_options) return -1;
    if (s->mask != 0 && !(s->mask & (1u << s->label))) return -1;
    llm_decide_criterion_t crit = opts ? opts->criterion : LLM_DECIDE_CRIT_CE;
    if (crit < LLM_DECIDE_CRIT_CE || crit > LLM_DECIDE_CRIT_CE_RPS) return -1;
    double eps = opts ? opts->label_smoothing : 0.0;
    if (eps < 0.0) eps = 0.0;
    if (eps > 0.5) eps = 0.5;
    double rps_w = opts ? opts->rps_weight : 0.0;
    if (!(rps_w > 0.0)) rps_w = 0.5;

    int nval = valid_count(s);
    if (nval < 1) return -1;
    double p[LLM_DECIDE_LOSS_MAX];
    int valid[LLM_DECIDE_LOSS_MAX];
    softmax_masked(s, p, valid, nval);

    /* Target con label smoothing SOLO sulle opzioni valide. */
    double y[LLM_DECIDE_LOSS_MAX];
    for (int k = 0; k < s->n_options; k++) {
        if (!valid[k]) { y[k] = 0.0; continue; }
        y[k] = eps / nval + ((k == s->label) ? (1.0 - eps) : 0.0);
    }

    memset(out, 0, sizeof(*out));
    double ce = 0.0, brier = 0.0, rps = 0.0;
    double g_ce[LLM_DECIDE_LOSS_MAX] = { 0 };
    double g_brier[LLM_DECIDE_LOSS_MAX] = { 0 };
    double g_rps[LLM_DECIDE_LOSS_MAX] = { 0 };

    if (crit == LLM_DECIDE_CRIT_CE || crit == LLM_DECIDE_CRIT_CE_RPS) {
        for (int k = 0; k < s->n_options; k++) {
            if (!valid[k]) continue;
            if (p[k] > 1e-300) ce -= y[k] * log(p[k]);
            g_ce[k] = p[k] - y[k];
        }
    }
    if (crit == LLM_DECIDE_CRIT_BRIER) {
        double dot = 0.0;
        for (int k = 0; k < s->n_options; k++) {
            if (!valid[k]) continue;
            double d = p[k] - y[k];
            brier += d * d;
            dot += d * p[k];
        }
        for (int k = 0; k < s->n_options; k++) {
            if (!valid[k]) continue;
            g_brier[k] = 2.0 * p[k] * ((p[k] - y[k]) - dot);
        }
    }
    if (crit == LLM_DECIDE_CRIT_RPS || crit == LLM_DECIDE_CRIT_CE_RPS) {
        /* CDF sulle opzioni valide, nell'ordine del dominio (SCORE ordinale). */
        double cdf = 0.0, ycdf = 0.0;
        double cm[LLM_DECIDE_LOSS_MAX];
        double ym[LLM_DECIDE_LOSS_MAX];
        for (int k = 0; k < s->n_options; k++) {
            if (!valid[k]) { cm[k] = 0.0; ym[k] = 0.0; continue; }
            cdf += p[k];
            ycdf += y[k];
            cm[k] = cdf;
            ym[k] = ycdf;
        }
        double sum_sq = 0.0;
        for (int k = 0; k < s->n_options - 1; k++) {
            if (!valid[k]) continue;
            double d = cm[k] - ym[k];
            sum_sq += d * d;
            for (int j = 0; j < s->n_options; j++) {
                if (!valid[j]) continue;
                double dc = (j <= k) ? p[j] * (1.0 - cm[k]) : -p[j] * cm[k];
                g_rps[j] += 2.0 * d * dc;
            }
        }
        rps = sum_sq;
    }

    if (crit == LLM_DECIDE_CRIT_CE) { out->loss = ce; memcpy(out->grad, g_ce, sizeof g_ce); }
    else if (crit == LLM_DECIDE_CRIT_BRIER) { out->loss = brier; memcpy(out->grad, g_brier, sizeof g_brier); }
    else if (crit == LLM_DECIDE_CRIT_RPS) { out->loss = rps; memcpy(out->grad, g_rps, sizeof g_rps); }
    else {
        out->loss = ce + rps_w * rps;
        for (int k = 0; k < s->n_options; k++)
            out->grad[k] = g_ce[k] + rps_w * g_rps[k];
    }
    return 0;
}
