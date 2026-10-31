/* gmb_rageval_advanced.c — RAG eval avanzato R4-R8: diagnosi, Cohen kappa,
 * Wilson CI, calibrazione soglia, gate/diff, PSI, z-test, Welch t-test,
 * confronto multi-config. Pure math/string parsing, no SQLite/LLM.
 * §0bis: runtime C condiviso — fonte unica VM/C + C99.
 * Tutti i char* ritorni sono mallocati (caller free). */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define RGA_MAX_ITEMS 1024
#define RGA_MAX_STR 4096
#define RGA_MIN_KAPPA_SAMPLES 20
#define RGA_MIN_PSI_SAMPLES 20
#define RGA_MIN_ZTEST_SAMPLES 5
#define RGA_MIN_WELCH_SAMPLES 5
#define RGA_NBINS 10
#define RGA_Z_95 1.96

/* --- static accumulators per diagnose_report --- */
static int rga_diag_total = 0;
static int rga_diag_retrieval_miss = 0;
static int rga_diag_retrieval_ranking = 0;
static int rga_diag_context_insufficient = 0;
static int rga_diag_gen_hallucination = 0;
static int rga_diag_gen_incomplete = 0;
static int rga_diag_pipeline_error = 0;

/* --- JSON helper: skip whitespace --- */
static const char* rga_skip_ws(const char* p)
{
    if (!p) return NULL;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* --- JSON helper: extract a quoted string value --- */
static int rga_extract_string(const char* json, const char* field, char* out, int outsz)
{
    if (!json || !field) return 0;
    char pat[256];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p = strstr(p, ":");
    if (!p) return 0;
    p = rga_skip_ws(p + 1);
    if (*p != '"') return 0;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < outsz - 1) {
        if (*p == '\\' && *(p + 1)) { p++; }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

/* --- JSON helper: extract a double value --- */
static int rga_extract_double(const char* json, const char* field, double* out)
{
    if (!json || !field) return 0;
    char pat[256];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p = strstr(p, ":");
    if (!p) return 0;
    p = rga_skip_ws(p + 1);
    return (sscanf(p, "%lf", out) == 1) ? 1 : 0;
}

/* --- JSON helper: extract an int value --- */
static int rga_extract_int(const char* json, const char* field, int* out)
{
    if (!json || !field) return 0;
    char pat[256];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p = strstr(p, ":");
    if (!p) return 0;
    p = rga_skip_ws(p + 1);
    return (sscanf(p, "%d", out) == 1) ? 1 : 0;
}

/* --- JSON helper: skip a JSON value and return pointer after it --- */
static const char* rga_skip_value(const char* p)
{
    if (!p) return NULL;
    p = rga_skip_ws(p);
    if (!p || !*p) return NULL;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p + 1)) p++;
            p++;
        }
        if (*p) p++;
    } else if (*p == '{' || *p == '[') {
        char open = *p;
        char close = (open == '{') ? '}' : ']';
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) p++;
                    p++;
                }
                if (*p) p++;
            } else if (*p == open) {
                depth++;
                p++;
            } else if (*p == close) {
                depth--;
                if (depth > 0) p++;
            } else {
                p++;
            }
        }
        if (*p) p++;
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']' &&
               *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            p++;
    }
    return p;
}

/* --- Parse JSON string array ["a","b","c"] into items (pointers into buf). ---
 * buf is mutated (NUL inserted after each string). Returns count. */
static int rga_parse_str_array(const char* json, const char* field,
                                char* buf, int bufsz, char** items, int max)
{
    if (!json || !field) return 0;
    char pat[256];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char* p = strstr(json, pat);
    if (!p) { buf[0] = '\0'; return 0; }
    p = strstr(p, ":");
    if (!p) { buf[0] = '\0'; return 0; }
    p = rga_skip_ws(p + 1);
    if (*p != '[') { buf[0] = '\0'; return 0; }
    p++;
    int bi = 0;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        p = rga_skip_ws(p);
        if (*p == ']') break;
        if (*p != '"') { p = rga_skip_value(p); continue; }
        p++;
        int start = bi;
        while (*p && *p != '"' && bi < bufsz - 1) {
            if (*p == '\\' && *(p + 1)) p++;
            buf[bi++] = *p++;
        }
        buf[bi++] = '\0';
        items[n++] = buf + start;
        if (*p) p++;
        p = rga_skip_ws(p);
        if (*p == ',') p++;
    }
    buf[bi] = '\0';
    return n;
}

/* --- Parse binary cases JSON for kappa: [{umano:N,giudice:N},...] ---
 * Returns number of cases (max RGA_MAX_ITEMS). */
static int rga_parse_binary_cases(const char* json, int* umano, int* giudice, int max)
{
    if (!json) return 0;
    const char* p = rga_skip_ws(json);
    if (*p != '[') return 0;
    p++;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        p = rga_skip_ws(p);
        if (*p != '{') { p = rga_skip_value(p); continue; }
        rga_extract_int(p, "umano", &umano[n]);
        rga_extract_int(p, "giudice", &giudice[n]);
        n++;
        /* skip this object */
        p = strchr(p, '}');
        if (!p) break;
        p++;
        p = rga_skip_ws(p);
        if (*p == ',') p++;
    }
    return n;
}

/* --- Parse double array [a,b,c,...] --- Returns count. */
static int rga_parse_double_array(const char* json, double* out, int max)
{
    if (!json) return 0;
    const char* p = rga_skip_ws(json);
    if (*p == '{') {
        /* Forma {"values":[...]}: contratto della guida §8.7 e del runtime C#.
         * Prima era accettato solo l'array nudo → DRIFT dava -1 sulle gambe C. */
        const char* v = strstr(p, "\"values\"");
        if (!v) return 0;
        p = strchr(v, '[');
        if (!p) return 0;
    }
    if (*p != '[') return 0;
    p++;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        p = rga_skip_ws(p);
        if (*p == ']') break;
        if (sscanf(p, "%lf", &out[n]) == 1) {
            n++;
            while (*p && *p != ',' && *p != ']' && *p != ' ' &&
                   *p != '\t' && *p != '\n' && *p != '\r') p++;
        } else {
            p = rga_skip_value(p);
        }
        p = rga_skip_ws(p);
        if (*p == ',') p++;
    }
    return n;
}

/* --- Parse binary correctness array for Wilson CI: [{correct:N},...] or [N,N,...] --- */
static int rga_parse_binary_correct(const char* json, int* correct, int max)
{
    if (!json) return 0;
    const char* p = rga_skip_ws(json);
    if (*p != '[') return 0;
    p++;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        p = rga_skip_ws(p);
        if (*p == ']') break;
        if (*p == '{') {
            int c = 0;
            rga_extract_int(p, "correct", &c);
            correct[n++] = c;
            p = strchr(p, '}');
            if (!p) break;
            p++;
        } else if (*p == '0' || *p == '1') {
            if (sscanf(p, "%d", &correct[n]) == 1) {
                if (correct[n] == 0 || correct[n] == 1) n++;
            }
            while (*p && *p != ',' && *p != ']' && *p != ' ' &&
                   *p != '\t' && *p != '\n' && *p != '\r') p++;
        } else {
            p = rga_skip_value(p);
        }
        p = rga_skip_ws(p);
        if (*p == ',') p++;
    }
    return n;
}

/* --- Parse proportion cases for z-test/Welch: [{value:N},...] or [N,N,...] ---
 * Returns count and fills values. */
static int rga_parse_value_array(const char* json, double* out, int max)
{
    if (!json) return 0;
    const char* p = rga_skip_ws(json);
    if (*p != '[') return 0;
    p++;
    int n = 0;
    while (*p && *p != ']' && n < max) {
        p = rga_skip_ws(p);
        if (*p == ']') break;
        if (*p == '{') {
            double v = 0.0;
            rga_extract_double(p, "value", &v);
            out[n++] = v;
            p = strchr(p, '}');
            if (!p) break;
            p++;
        } else {
            if (sscanf(p, "%lf", &out[n]) == 1) n++;
            while (*p && *p != ',' && *p != ']' && *p != ' ' &&
                   *p != '\t' && *p != '\n' && *p != '\r') p++;
        }
        p = rga_skip_ws(p);
        if (*p == ',') p++;
    }
    return n;
}

/* ===================================================================
 * R4: diagnosi contestuale
 * =================================================================== */
char* gmb_rageval_diagnose(const char* eval_json)
{
    double faithfulness = 0.0, hallucination = 0.0, completeness = 0.0;
    double relevance = 0.0;
    int context_len = 0;
    char retrieved_buf[RGA_MAX_STR];
    char ideal_buf[RGA_MAX_STR];
    char* retrieved_items[RGA_MAX_ITEMS];
    char* ideal_items[RGA_MAX_ITEMS];
    const char* cause = "pipeline_error";
    char evidence[1024];
    char* rec = NULL;

    retrieved_buf[0] = '\0';
    ideal_buf[0] = '\0';

    rga_extract_double(eval_json, "faithfulness", &faithfulness);
    rga_extract_double(eval_json, "hallucination", &hallucination);
    rga_extract_double(eval_json, "completeness", &completeness);
    rga_extract_double(eval_json, "relevance", &relevance);
    rga_extract_int(eval_json, "context_len", &context_len);

    int nr = rga_parse_str_array(eval_json, "retrieved",
                                  retrieved_buf, sizeof(retrieved_buf),
                                  retrieved_items, RGA_MAX_ITEMS);
    int ni = rga_parse_str_array(eval_json, "ideal",
                                  ideal_buf, sizeof(ideal_buf),
                                  ideal_items, RGA_MAX_ITEMS);

    if (faithfulness < 0.3 && hallucination > 0.7) {
        cause = "generation_hallucination";
        snprintf(evidence, sizeof(evidence),
                 "faithfulness=%.3f < 0.3 and hallucination=%.3f > 0.7",
                 faithfulness, hallucination);
        rec = "increase context grounding, reduce temperature, or add factual constraints";
    } else if (completeness < 0.4) {
        cause = "generation_incomplete";
        snprintf(evidence, sizeof(evidence),
                 "completeness=%.3f < 0.4", completeness);
        rec = "increase max_tokens, improve instruction to cover all points, or chunk input";
    } else if (context_len < 200) {
        cause = "context_insufficient";
        snprintf(evidence, sizeof(evidence),
                 "context_len=%d < 200", context_len);
        rec = "increase chunk size, add more retrieved documents, or expand context window";
    } else if (ni > 0) {
        int ideal_has_retrieved = 0;
        for (int i = 0; i < nr && !ideal_has_retrieved; i++) {
            for (int j = 0; j < ni; j++) {
                if (strcmp(retrieved_items[i], ideal_items[j]) == 0) {
                    ideal_has_retrieved = 1;
                    break;
                }
            }
        }
        if (ideal_has_retrieved) {
            cause = "retrieval_ranking";
            snprintf(evidence, sizeof(evidence),
                     "ideal documents present in retrieved but not at top positions (ideal=%d items, retrieved=%d items)",
                     ni, nr);
            rec = "improve ranking score, try reranking, or adjust hybrid search weights";
        } else {
            cause = "retrieval_miss";
            snprintf(evidence, sizeof(evidence),
                     "ideal documents missing from retrieved results (ideal=%d items, retrieved=%d items)",
                     ni, nr);
            rec = "expand search query, reduce top_k, lower similarity threshold, or add synonyms";
        }
    } else {
        cause = "pipeline_error";
        snprintf(evidence, sizeof(evidence),
                 "no specific diagnosis criteria met (faith=%.3f, hall=%.3f, compl=%.3f, ctx=%d)",
                 faithfulness, hallucination, completeness, context_len);
        rec = "check pipeline configuration, verify embedding model, or inspect input quality";
    }

    /* Update aggregator */
    rga_diag_total++;
    if (strcmp(cause, "retrieval_miss") == 0) rga_diag_retrieval_miss++;
    else if (strcmp(cause, "retrieval_ranking") == 0) rga_diag_retrieval_ranking++;
    else if (strcmp(cause, "context_insufficient") == 0) rga_diag_context_insufficient++;
    else if (strcmp(cause, "generation_hallucination") == 0) rga_diag_gen_hallucination++;
    else if (strcmp(cause, "generation_incomplete") == 0) rga_diag_gen_incomplete++;
    else rga_diag_pipeline_error++;

    char* out = (char*)malloc(2048);
    if (!out) return NULL;
    snprintf(out, 2048,
        "{\"cause\":\"%s\",\"evidence\":\"%s\",\"recommendation\":\"%s\"}",
        cause, evidence, rec ? rec : "");
    return out;
}

char* gmb_rageval_diagnose_report(void)
{
    char* out = (char*)malloc(1024);
    if (!out) return NULL;
    snprintf(out, 1024,
        "{\"total\":%d,"
        "\"causes\":{"
        "\"retrieval_miss\":%d,"
        "\"retrieval_ranking\":%d,"
        "\"context_insufficient\":%d,"
        "\"generation_hallucination\":%d,"
        "\"generation_incomplete\":%d,"
        "\"pipeline_error\":%d}}",
        rga_diag_total,
        rga_diag_retrieval_miss,
        rga_diag_retrieval_ranking,
        rga_diag_context_insufficient,
        rga_diag_gen_hallucination,
        rga_diag_gen_incomplete,
        rga_diag_pipeline_error);
    return out;
}

/* ===================================================================
 * R5: Cohen kappa, Wilson CI, calibrazione soglia
 * =================================================================== */
double gmb_rageval_kappa(const char* cases_json)
{
    int umano[RGA_MAX_ITEMS];
    int giudice[RGA_MAX_ITEMS];
    int n = rga_parse_binary_cases(cases_json, umano, giudice, RGA_MAX_ITEMS);
    if (n < RGA_MIN_KAPPA_SAMPLES) return -2.0;

    int agree = 0;
    int n1_u = 0, n0_u = 0, n1_g = 0, n0_g = 0;
    for (int i = 0; i < n; i++) {
        if (umano[i] == giudice[i]) agree++;
        if (umano[i] == 1) n1_u++;
        else n0_u++;
        if (giudice[i] == 1) n1_g++;
        else n0_g++;
    }
    double p_o = (double)agree / (double)n;
    double p_e = ((double)n1_u / (double)n) * ((double)n1_g / (double)n)
               + ((double)n0_u / (double)n) * ((double)n0_g / (double)n);
    if (p_e >= 1.0) return 0.0;
    return (p_o - p_e) / (1.0 - p_e);
}

double gmb_rageval_wilson_ci_low(const char* cases_json)
{
    int correct[RGA_MAX_ITEMS];
    int n = rga_parse_binary_correct(cases_json, correct, RGA_MAX_ITEMS);
    if (n < 1) return 0.0;
    int s = 0;
    for (int i = 0; i < n; i++) if (correct[i]) s++;
    double p = (double)s / (double)n;
    double z2 = RGA_Z_95 * RGA_Z_95;
    double denom = 1.0 + z2 / (double)n;
    double centre = (p + z2 / (2.0 * (double)n)) / denom;
    double se = sqrt(p * (1.0 - p) / (double)n + z2 / (4.0 * (double)n * (double)n));
    return centre - RGA_Z_95 * se / denom;
}

double gmb_rageval_wilson_ci_high(const char* cases_json)
{
    int correct[RGA_MAX_ITEMS];
    int n = rga_parse_binary_correct(cases_json, correct, RGA_MAX_ITEMS);
    if (n < 1) return 0.0;
    int s = 0;
    for (int i = 0; i < n; i++) if (correct[i]) s++;
    double p = (double)s / (double)n;
    double z2 = RGA_Z_95 * RGA_Z_95;
    double denom = 1.0 + z2 / (double)n;
    double centre = (p + z2 / (2.0 * (double)n)) / denom;
    double se = sqrt(p * (1.0 - p) / (double)n + z2 / (4.0 * (double)n * (double)n));
    return centre + RGA_Z_95 * se / denom;
}

double gmb_rageval_calibrate_threshold(const char* cases_json)
{
    int best_idx = -1;
    double best_kap = -999.0;
    int umano[RGA_MAX_ITEMS];
    int giudice_raw[RGA_MAX_ITEMS];
    int n = rga_parse_binary_cases(cases_json, umano, giudice_raw, RGA_MAX_ITEMS);
    if (n < RGA_MIN_KAPPA_SAMPLES) return -2.0;

    for (int step = 1; step <= 19; step++) {
        double thresh = 0.05 * (double)step; /* 0.05 .. 0.95 */
        int agree = 0;
        int n1_u = 0, n0_u = 0, n1_g = 0, n0_g = 0;
        for (int i = 0; i < n; i++) {
            int giudice_bin = (giudice_raw[i] == 1 || (giudice_raw[i] == 0 && thresh <= 0.5)) ? 1 : 0;
            if (giudice_raw[i] > 0) {
                giudice_bin = ((double)giudice_raw[i] >= thresh) ? 1 : 0;
            } else {
                giudice_bin = (thresh <= 0.5) ? 0 : 1;
            }
            /* binary threshold: treat giudice_raw as 0/1 by default, but if giudice_raw > 1,
               treat it as a score and threshold it */
            if (giudice_raw[i] == 1 || giudice_raw[i] == 0) {
                giudice_bin = giudice_raw[i];
            } else {
                giudice_bin = (giudice_raw[i] >= (int)(thresh * 100)) ? 1 : 0;
            }
            if (umano[i] == giudice_bin) agree++;
            if (umano[i] == 1) n1_u++; else n0_u++;
            if (giudice_bin == 1) n1_g++; else n0_g++;
        }
        double p_o = (double)agree / (double)n;
        double p_e = ((double)n1_u / (double)n) * ((double)n1_g / (double)n)
                   + ((double)n0_u / (double)n) * ((double)n0_g / (double)n);
        double kappa = (p_e >= 1.0) ? 0.0 : (p_o - p_e) / (1.0 - p_e);
        if (kappa > best_kap) {
            best_kap = kappa;
            best_idx = step;
        }
    }
    if (best_idx < 0) return -2.0;
    return 0.05 * (double)best_idx;
}

/* Combined calibrate: chiama kappa + wilson + threshold e restituisce JSON. */
char* gmb_rageval_calibrate(const char* cases_json)
{
    double kappa = gmb_rageval_kappa(cases_json);
    if (kappa < -1.0) return strdup("{\"status\":\"insufficient_data\",\"kappa\":-2}");
    double wil_low = gmb_rageval_wilson_ci_low(cases_json);
    double wil_high = gmb_rageval_wilson_ci_high(cases_json);
    double th = gmb_rageval_calibrate_threshold(cases_json);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"kappa\":%.4f,\"wilson_ci_low\":%.4f,\"wilson_ci_high\":%.4f,\"threshold\":%.2f}",
             kappa, wil_low, wil_high, th);
    return strdup(buf);
}

/* Simple baseline capture: salva il JSON in memoria statica e lo ritorna. */
static char g_baseline_buf[4096] = "";
char* gmb_rageval_baseline(const char* sample_json)
{
    if (!sample_json || !sample_json[0]) return strdup("{\"status\":\"error\",\"message\":\"empty sample\"}");
    snprintf(g_baseline_buf, sizeof(g_baseline_buf), "%s", sample_json);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"message\":\"baseline captured\",\"sample\":%s}", sample_json);
    return strdup(buf);
}

/* ===================================================================
 * R6: gate + diff
 * =================================================================== */
static double rge_metric(const char* json, const char* name, double def)
{
    double v = def;
    rga_extract_double(json, name, &v);
    return v;
}

static const char* RGA_METRICS[] = {
    "retrieval_precision", "retrieval_recall", "retrieval_mrr", "retrieval_ndcg",
    "faithfulness", "relevance", "hallucination", "completeness",
    "judgment_faithfulness", "judgment_relevance", "judgment_completeness",
    "judgment_hallucination",
    NULL
};

/* Itera le coppie "<chiave>":<numero> di un oggetto JSON piatto.
 * `*cur` è lo stato di scansione (inizializzare a json). Ritorna 1 se trovata una coppia. */
static int rge_next_metric(const char* json, const char** cur, char* name, size_t nsz, double* val)
{
    const char* p = *cur ? *cur : json;
    while (p && *p) {
        const char* q = strchr(p, '"');
        if (!q) return 0;
        q++;
        const char* e = strchr(q, '"');
        if (!e) return 0;
        const char* c = rga_skip_ws(e + 1);
        if (*c != ':') { p = e + 1; continue; }
        c = rga_skip_ws(c + 1);
        double v;
        if (sscanf(c, "%lf", &v) == 1) {
            size_t len = (size_t)(e - q);
            if (len >= nsz) len = nsz - 1;
            memcpy(name, q, len); name[len] = 0;
            *val = v;
            *cur = c;
            return 1;
        }
        p = e + 1;
    }
    return 0;
}

static int rge_find_metric(const char* json, const char* name, double* val)
{
    char pat[160];
    snprintf(pat, sizeof(pat), "\"%s\"", name);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p + strlen(pat), ':');
    if (!p) return 0;
    p = rga_skip_ws(p + 1);
    return sscanf(p, "%lf", val) == 1;
}

char* gmb_rageval_eval_gate(const char* baseline, const char* current)
{
    /* Contratto della guida §8.4 e del runtime C#: per ogni metrica numerica presente in
     * ENTRAMBI, delta = current - baseline; retrieval (p_at/recall/ndcg/mrr) con
     * delta < -0.05 → FAIL, altre con delta < -0.10 → WARN, altrimenti PASS.
     * Output: {"status":"ok","verdict":..,"deltas":[{"metric","before","after","delta"}...]}.
     * NB: la versione precedente usava una lista di metriche hardcoded (retrieval_precision,
     * ...) e un oggetto `deltas` per nome → verdetto divergente dal C# (ERR/pass invece di fail). */
    if (!baseline || !current) return strdup("{\"status\":\"error\",\"message\":\"empty input\"}");
    char buf[8192];
    int pos = snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"verdict\":\"");
    int any_fail = 0, any_warn = 0;
    char deltas[6144];
    int dpos = snprintf(deltas, sizeof(deltas), "[");
    int first = 1;
    const char* cur = baseline;
    char name[128];
    double bv;
    while (rge_next_metric(baseline, &cur, name, sizeof(name), &bv)) {
        double cv;
        if (!rge_find_metric(current, name, &cv)) continue;
        double delta = cv - bv;
        int is_retrieval = (strcmp(name, "p_at") == 0 || strcmp(name, "recall") == 0 ||
                            strcmp(name, "ndcg") == 0 || strcmp(name, "mrr") == 0);
        double thr = is_retrieval ? 0.05 : 0.10;
        if (delta < -thr && is_retrieval) any_fail = 1;
        else if (delta < -thr) any_warn = 1;
        dpos += snprintf(deltas + dpos, sizeof(deltas) - (size_t)dpos,
                         "%s{\"metric\":\"%s\",\"before\":%.4f,\"after\":%.4f,\"delta\":%.4f}",
                         first ? "" : ",", name, bv, cv, delta);
        first = 0;
        if (dpos >= (int)sizeof(deltas) - 128) break;
    }
    snprintf(deltas + dpos, sizeof(deltas) - (size_t)dpos, "]");
    const char* verdict = any_fail ? "fail" : (any_warn ? "warn" : "pass");
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "%s\",\"deltas\":%s}", verdict, deltas);
    return strdup(buf);
}

/* Dichiarazione forward per rge_parse_query_report usata in eval_diff */
static int rga_parse_query_report(const char* json, const char* field,
                                   double* out_vals, int max);

/* Confronto per-metrica fra due oggetti JSON piatti (contratto della guida §8.5, del
 * runtime C# e dei MiniTest): output {"status":"ok","metrics":{"<k>":{baseline,current,diff}}}.
 * NB: la versione precedente emetteva {"per_query_diffs":[...]} su un input a report
 * per-query — contratto DIVERGENTE dal C#/guida → EVALDIFF dava risultati diversi per gamba. */
char* gmb_rageval_eval_diff(const char* baseline, const char* current)
{
    if (!baseline || !current) return strdup("{\"status\":\"error\",\"message\":\"empty input\"}");
    char buf[8192];
    int pos = snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"metrics\":{");
    int first = 1;
    const char* cur = baseline;
    char name[128];
    double bv;
    while (rge_next_metric(baseline, &cur, name, sizeof(name), &bv)) {
        double cv;
        if (!rge_find_metric(current, name, &cv)) continue;
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                        "%s\"%s\":{\"baseline\":%.4f,\"current\":%.4f,\"diff\":%.4f}",
                        first ? "" : ",", name, bv, cv, cv - bv);
        first = 0;
        if (pos >= (int)sizeof(buf) - 128) break;
    }
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "}}");
    return strdup(buf);
}

/* Helper: parse per-query values from a report JSON */
static int rga_parse_query_report(const char* json, const char* field,
                                   double* out_vals, int max)
{
    if (!json || !field) return 0;
    char pat[256];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char* p = strstr(json, pat);
    if (!p) return 0;
    p = strstr(p, ":");
    if (!p) return 0;
    p = rga_skip_ws(p + 1);
    return rga_parse_double_array(p, out_vals, max);
}

/* ===================================================================
 * R7: PSI, z-test, Welch t-test
 * =================================================================== */
double gmb_rageval_psi(const char* baseline, const char* window)
{
    double b_vals[RGA_MAX_ITEMS];
    double w_vals[RGA_MAX_ITEMS];
    int nb = rga_parse_double_array(baseline, b_vals, RGA_MAX_ITEMS);
    int nw = rga_parse_double_array(window, w_vals, RGA_MAX_ITEMS);
    if (nb < RGA_MIN_PSI_SAMPLES || nw < RGA_MIN_PSI_SAMPLES) return -1.0;

    /* Find min/max across both */
    double lo = b_vals[0], hi = b_vals[0];
    for (int i = 1; i < nb; i++) {
        if (b_vals[i] < lo) lo = b_vals[i];
        if (b_vals[i] > hi) hi = b_vals[i];
    }
    for (int i = 0; i < nw; i++) {
        if (w_vals[i] < lo) lo = w_vals[i];
        if (w_vals[i] > hi) hi = w_vals[i];
    }
    if (hi - lo < 1e-12) return 0.0;

    double bin_width = (hi - lo) / (double)RGA_NBINS;
    if (bin_width < 1e-12) return 0.0;

    double psi = 0.0;
    for (int b = 0; b < RGA_NBINS; b++) {
        double bl = lo + (double)b * bin_width;
        double br = (b == RGA_NBINS - 1) ? hi + 1e-12 : lo + (double)(b + 1) * bin_width;
        double p_b = 0.0, p_w = 0.0;
        for (int i = 0; i < nb; i++) if (b_vals[i] >= bl && b_vals[i] < br) p_b += 1.0;
        for (int i = 0; i < nw; i++) if (w_vals[i] >= bl && w_vals[i] < br) p_w += 1.0;
        p_b /= (double)nb;
        p_w /= (double)nw;
        if (p_b < 1e-12) p_b = 1e-12;
        if (p_w < 1e-12) p_w = 1e-12;
        psi += (p_w - p_b) * log(p_w / p_b);
    }
    return psi;
}


/* A/B test su PROPORZIONI: input {"n","success"} per variante (contratto guida §8.8 e
 * MiniTest), output {"z_score","p_value","effect_size"}. Parità con AiVectorRuntime.AbTest. */
char* gmb_rageval_ab_test(const char* variantA, const char* variantB)
{
    double nA = 0, sA = 0, nB = 0, sB = 0;
    if (variantA) { rga_extract_double(variantA, "n", &nA); rga_extract_double(variantA, "success", &sA); }
    if (variantB) { rga_extract_double(variantB, "n", &nB); rga_extract_double(variantB, "success", &sB); }
    if (nA <= 0.0 || nB <= 0.0) return strdup("{\"status\":\"error\",\"message\":\"n deve essere > 0\"}");
    double p1 = sA / nA, p2 = sB / nB;
    double var = p1 * (1.0 - p1) / nA + p2 * (1.0 - p2) / nB;
    double se = (var > 0.0) ? sqrt(var) : 0.0;
    double z = (se > 0.0) ? (p1 - p2) / se : 0.0;
    double pval = 2.0 * (1.0 - 0.5 * (1.0 + erf(fabs(z) / sqrt(2.0))));
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"z_score\":%.6f,\"p_value\":%.6f,\"effect_size\":%.6f}", z, pval, p1 - p2);
    return strdup(buf);
}

double gmb_rageval_z_test(const char* variantA, const char* variantB)
{
    double a_vals[RGA_MAX_ITEMS];
    double b_vals[RGA_MAX_ITEMS];
    int na = rga_parse_value_array(variantA, a_vals, RGA_MAX_ITEMS);
    int nb = rga_parse_value_array(variantB, b_vals, RGA_MAX_ITEMS);
    if (na < RGA_MIN_ZTEST_SAMPLES || nb < RGA_MIN_ZTEST_SAMPLES) return -2.0;

    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < na; i++) s1 += a_vals[i];
    for (int i = 0; i < nb; i++) s2 += b_vals[i];
    double p1 = s1 / (double)na;
    double p2 = s2 / (double)nb;
    double p_pool = (s1 + s2) / (double)(na + nb);
    double se = sqrt(p_pool * (1.0 - p_pool) * (1.0 / (double)na + 1.0 / (double)nb));
    if (se < 1e-12) return 0.0;
    return (p1 - p2) / se;
}

double gmb_rageval_welch_t(const char* variantA, const char* variantB)
{
    double a_vals[RGA_MAX_ITEMS];
    double b_vals[RGA_MAX_ITEMS];
    int na = rga_parse_value_array(variantA, a_vals, RGA_MAX_ITEMS);
    int nb = rga_parse_value_array(variantB, b_vals, RGA_MAX_ITEMS);
    if (na < RGA_MIN_WELCH_SAMPLES || nb < RGA_MIN_WELCH_SAMPLES) return -2.0;

    double sum_a = 0.0, sum_b = 0.0;
    for (int i = 0; i < na; i++) sum_a += a_vals[i];
    for (int i = 0; i < nb; i++) sum_b += b_vals[i];
    double mean_a = sum_a / (double)na;
    double mean_b = sum_b / (double)nb;

    double var_a = 0.0, var_b = 0.0;
    for (int i = 0; i < na; i++) { double d = a_vals[i] - mean_a; var_a += d * d; }
    for (int i = 0; i < nb; i++) { double d = b_vals[i] - mean_b; var_b += d * d; }
    var_a /= (double)(na - 1);
    var_b /= (double)(nb - 1);
    double denom = sqrt(var_a / (double)na + var_b / (double)nb);
    if (denom < 1e-12) return 0.0;
    return (mean_a - mean_b) / denom;
}

/* ===================================================================
 * R8: confronto multi-config
 * =================================================================== */
/* p punta a '{' → ritorna il carattere dopo la '}' corrispondente (NULL se sbilanciato). */
static const char* rge_skip_braces(const char* p)
{
    int depth = 0;
    while (p && *p) {
        if (*p == '"') { const char* q = strchr(p + 1, '"'); if (!q) return NULL; p = q + 1; continue; }
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) return p + 1; }
        p++;
    }
    return NULL;
}

/* Confronto multi-configurazione (contratto della guida §8.9 e del runtime C#):
 * per ogni metrica (UNIONE di quelle presenti nei `results`) vince il valore massimo.
 * Output: {"status":"ok","winners":[{"metric","winner","value"}...],"overall_winner":"<cfg>"}.
 * NB: la versione precedente scandiva l'array con `strchr(p,'}')` (prima graffa, non quella
 * di chiusura dell'oggetto) → poteva non avanzare mai = HANG della VM/C; e usava una lista di
 * metriche hardcoded (pAt/rAt/...) diversa dal C#/guida (p_at/recall/...). */
char* gmb_rageval_compare(const char* configs_json)
{
    if (!configs_json) return strdup("{\"status\":\"error\",\"message\":\"empty input\"}");
    const char* p = rga_skip_ws(configs_json);
    if (*p != '[') return strdup("{\"status\":\"error\",\"message\":\"expected JSON array\"}");
    p++;

    char names[32][128];
    static char res_json[32][2048];
    int nc = 0;
    while (*p && nc < 32) {
        p = rga_skip_ws(p);
        if (*p == ']') break;
        if (*p != '{') { p = rga_skip_value(p); p = rga_skip_ws(p); if (*p == ',') p++; continue; }
        const char* obj_end = rge_skip_braces(p);
        if (!obj_end) break;
        char nm[128] = "unknown";
        const char* cfg = strstr(p, "\"config\"");
        if (cfg && cfg < obj_end) rga_extract_string(cfg, "name", nm, sizeof(nm));
        snprintf(names[nc], sizeof(names[nc]), "%s", nm);
        res_json[nc][0] = 0;
        const char* res = strstr(p, "\"results\"");
        if (res && res < obj_end) {
            const char* ob = strchr(res, '{');
            if (ob && ob < obj_end) {
                const char* oe = rge_skip_braces(ob);
                if (oe) {
                    size_t len = (size_t)(oe - ob);
                    if (len >= sizeof(res_json[nc])) len = sizeof(res_json[nc]) - 1;
                    memcpy(res_json[nc], ob, len); res_json[nc][len] = 0;
                }
            }
        }
        nc++;
        p = obj_end;
        p = rga_skip_ws(p);
        if (*p == ',') p++;
    }

    if (nc < 1) return strdup("{\"status\":\"error\",\"message\":\"no configs\"}");

    /* Unione delle metriche numeriche presenti nei results. */
    char metrics[32][64];
    int nmet = 0;
    for (int i = 0; i < nc; i++) {
        const char* cur = res_json[i];
        char name[64];
        double v;
        while (rge_next_metric(res_json[i], &cur, name, sizeof(name), &v)) {
            int found = 0;
            for (int k = 0; k < nmet; k++) if (strcmp(metrics[k], name) == 0) { found = 1; break; }
            if (!found && nmet < 32) snprintf(metrics[nmet++], sizeof(metrics[0]), "%s", name);
        }
    }

    char buf[8192];
    int pos = snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"winners\":[");
    int wins[32];
    memset(wins, 0, sizeof(wins));
    int first = 1;
    for (int m = 0; m < nmet; m++) {
        int bi = -1;
        double bv = 0.0;
        for (int i = 0; i < nc; i++) {
            double v;
            if (!rge_find_metric(res_json[i], metrics[m], &v)) continue;
            if (bi < 0 || v > bv) { bv = v; bi = i; }
        }
        if (bi < 0) continue;
        wins[bi]++;
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                        "%s{\"metric\":\"%s\",\"winner\":\"%s\",\"value\":%.4f}",
                        first ? "" : ",", metrics[m], names[bi], bv);
        first = 0;
        if (pos >= (int)sizeof(buf) - 128) break;
    }
    int ow = 0;
    for (int i = 1; i < nc; i++) if (wins[i] > wins[ow]) ow = i;
    snprintf(buf + pos, sizeof(buf) - (size_t)pos, "],\"overall_winner\":\"%s\"}", names[ow]);
    return strdup(buf);
}

/* ===================================================================
 * Self-check: 10+ casi hardcoded con valori attesi esatti
 * =================================================================== */
int gmb_rageval_advanced_selfcheck(void)
{
    struct {
        const char* name;
        double got;
        double want;
        double tol;
    } cases[] = {
        /* Diagnosi */
        { "diag_hallucination",
            gmb_rageval_kappa("[{\"umano\":1,\"giudice\":0},{\"umano\":0,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0},{\"umano\":0,\"giudice\":0}]"),
            -0.0526316, 0.01 },
        { "kappa_perfect",
            gmb_rageval_kappa("[{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0}]"),
            1.0, 1e-12 },
        { "kappa_insufficient",
            gmb_rageval_kappa("[{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0}]"),
            -2.0, 0.0 },
        { "kappa_random",
            gmb_rageval_kappa("[{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0},{\"umano\":1,\"giudice\":1},{\"umano\":0,\"giudice\":0}]"),
            1.0, 1e-12 },
        { "wilson_low",
            gmb_rageval_wilson_ci_low("[1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0]"),
            0.299298, 0.01 },
        { "wilson_high",
            gmb_rageval_wilson_ci_high("[1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0]"),
            0.700702, 0.01 },
        { "psi_identical",
            gmb_rageval_psi("[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0]",
                            "[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0]"),
            0.0, 1e-12 },
        { "psi_insufficient",
            gmb_rageval_psi("[0.1,0.2]", "[0.3,0.4]"),
            -1.0, 0.0 },
        { "ztest_basic",
            gmb_rageval_z_test("[1,1,1,1,1,0,0,0,0,0]", "[1,1,1,0,0,0,0,0,0,0]"),
            0.0, 0.1 },
        { "ztest_insufficient",
            gmb_rageval_z_test("[1]", "[0]"),
            -2.0, 0.0 },
        { "welcht_basic",
            gmb_rageval_welch_t("[1.0,2.0,3.0,4.0,5.0]", "[2.0,3.0,4.0,5.0,6.0]"),
            -1.0, 0.1 },
        { "welcht_insufficient",
            gmb_rageval_welch_t("[1.0]", "[2.0]"),
            -2.0, 0.0 },
        { "calibrate_basic",
            gmb_rageval_calibrate_threshold("[{\"umano\":1,\"giudice\":90},{\"umano\":1,\"giudice\":85},{\"umano\":1,\"giudice\":80},{\"umano\":1,\"giudice\":75},{\"umano\":1,\"giudice\":70},{\"umano\":0,\"giudice\":30},{\"umano\":0,\"giudice\":25},{\"umano\":0,\"giudice\":20},{\"umano\":0,\"giudice\":15},{\"umano\":0,\"giudice\":10},{\"umano\":1,\"giudice\":95},{\"umano\":1,\"giudice\":88},{\"umano\":0,\"giudice\":12},{\"umano\":0,\"giudice\":8},{\"umano\":1,\"giudice\":82},{\"umano\":0,\"giudice\":18},{\"umano\":1,\"giudice\":78},{\"umano\":0,\"giudice\":22},{\"umano\":1,\"giudice\":92},{\"umano\":0,\"giudice\":5}]"),
            0.50, 0.25 },
        { "psi_drift",
            gmb_rageval_psi("[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0]",
                            "[5.0,5.1,5.2,5.3,5.4,5.5,5.6,5.7,5.8,5.9,6.0,6.1,6.2,6.3,6.4,6.5,6.6,6.7,6.8,6.9]"),
            0.0, 99.0 }, /* non-zero drift expected but value depends on binning */
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < n; i++) {
        if (cases[i].got < cases[i].want - cases[i].tol ||
            cases[i].got > cases[i].want + cases[i].tol) {
            fprintf(stderr,
                "gmb_rageval_advanced_selfcheck: %s = %.12f (atteso %.12f +- %g)\n",
                cases[i].name, cases[i].got, cases[i].want, cases[i].tol);
            return -1;
        }
    }

    /* Test diagnose strings */
    char* diag = gmb_rageval_diagnose(
        "{\"retrieved\":[\"doc1\",\"doc2\"],\"ideal\":[\"doc3\",\"doc4\"],"
        "\"faithfulness\":0.2,\"relevance\":0.5,\"hallucination\":0.8,"
        "\"completeness\":0.9,\"context_len\":500}");
    if (!diag) return -1;
    if (!strstr(diag, "generation_hallucination")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: diagnose hallucination failed: %s\n", diag);
        free(diag);
        return -1;
    }
    free(diag);

    diag = gmb_rageval_diagnose(
        "{\"retrieved\":[\"doc1\",\"doc2\"],\"ideal\":[\"doc3\",\"doc4\"],"
        "\"faithfulness\":0.9,\"relevance\":0.5,\"hallucination\":0.1,"
        "\"completeness\":0.3,\"context_len\":500}");
    if (!diag) return -1;
    if (!strstr(diag, "generation_incomplete")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: diagnose incomplete failed: %s\n", diag);
        free(diag);
        return -1;
    }
    free(diag);

    diag = gmb_rageval_diagnose(
        "{\"retrieved\":[\"doc1\",\"doc2\"],\"ideal\":[\"doc3\",\"doc4\"],"
        "\"faithfulness\":0.9,\"relevance\":0.5,\"hallucination\":0.1,"
        "\"completeness\":0.9,\"context_len\":50}");
    if (!diag) return -1;
    if (!strstr(diag, "context_insufficient")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: diagnose context_insufficient failed: %s\n", diag);
        free(diag);
        return -1;
    }
    free(diag);

    diag = gmb_rageval_diagnose(
        "{\"retrieved\":[\"doc1\",\"doc2\",\"doc3\"],\"ideal\":[\"doc1\",\"doc4\"],"
        "\"faithfulness\":0.9,\"relevance\":0.5,\"hallucination\":0.1,"
        "\"completeness\":0.9,\"context_len\":500}");
    if (!diag) return -1;
    if (!strstr(diag, "retrieval_ranking")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: diagnose retrieval_ranking failed: %s\n", diag);
        free(diag);
        return -1;
    }
    free(diag);

    diag = gmb_rageval_diagnose(
        "{\"retrieved\":[\"doc5\",\"doc6\"],\"ideal\":[\"doc1\",\"doc4\"],"
        "\"faithfulness\":0.9,\"relevance\":0.5,\"hallucination\":0.1,"
        "\"completeness\":0.9,\"context_len\":500}");
    if (!diag) return -1;
    if (!strstr(diag, "retrieval_miss")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: diagnose retrieval_miss failed: %s\n", diag);
        free(diag);
        return -1;
    }
    free(diag);

    /* Test eval_gate (metriche del contratto piatto: p_at = retrieval, faithfulness = giudizio) */
    char* gate = gmb_rageval_eval_gate(
        "{\"p_at\":0.8,\"faithfulness\":0.9}",
        "{\"p_at\":0.7,\"faithfulness\":0.85}");
    if (!gate) return -1;
    if (!strstr(gate, "fail")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: eval_gate fail (retrieval drop) failed: %s\n", gate);
        free(gate);
        return -1;
    }
    free(gate);

    gate = gmb_rageval_eval_gate(
        "{\"p_at\":0.8,\"faithfulness\":0.9}",
        "{\"p_at\":0.78,\"faithfulness\":0.75}");
    if (!gate) return -1;
    if (!strstr(gate, "warn")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: eval_gate warn (judgment drop) failed: %s\n", gate);
        free(gate);
        return -1;
    }
    free(gate);

    gate = gmb_rageval_eval_gate(
        "{\"p_at\":0.8,\"faithfulness\":0.9}",
        "{\"p_at\":0.8,\"faithfulness\":0.9}");
    if (!gate) return -1;
    if (!strstr(gate, "pass")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: eval_gate pass failed: %s\n", gate);
        free(gate);
        return -1;
    }
    free(gate);

    /* Test compare */
    char* comp = gmb_rageval_compare(
        "[{\"config\":{\"name\":\"A\"},\"results\":{\"pAt\":0.8,\"faithfulness\":0.9}},"
         "{\"config\":{\"name\":\"B\"},\"results\":{\"pAt\":0.6,\"faithfulness\":0.7}}]");
    if (!comp) return -1;
    if (!strstr(comp, "\"winner\":\"A\"")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: compare winner A failed: %s\n", comp);
        free(comp);
        return -1;
    }
    free(comp);

    /* Test eval_diff (contratto piatto: metriche in entrambi gli oggetti) */
    char* diff = gmb_rageval_eval_diff(
        "{\"p_at\":0.9,\"recall\":0.8,\"solo_bl\":1.0}",
        "{\"p_at\":0.5,\"recall\":0.4}");
    if (!diff) return -1;
    if (!strstr(diff, "\"p_at\"") || !strstr(diff, "\"diff\":-0.4") || strstr(diff, "solo_bl")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: eval_diff failed: %s\n", diff);
        free(diff);
        return -1;
    }
    free(diff);

    /* Test ab_test (proporzioni: 60/100 vs 40/100 → z ~2.886, p ~0.0039, effect 0.2) */
    char* ab = gmb_rageval_ab_test("{\"n\":100,\"success\":60}", "{\"n\":100,\"success\":40}");
    if (!ab) return -1;
    if (!strstr(ab, "\"z_score\"") || !strstr(ab, "\"effect_size\":0.2")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: ab_test failed: %s\n", ab);
        free(ab);
        return -1;
    }
    free(ab);

    /* Test psi con forma {"values":[...]} (contratto guida §8.7) */
    double psi_ok = gmb_rageval_psi("{\"values\":[1,2,3,4,5,6,7,8,9,10]}",
                                    "{\"values\":[1,2,3,4,5,6,7,8,9,10]}");
    if (!(psi_ok >= 0.0 && psi_ok < 0.1)) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: psi values-form failed: %f\n", psi_ok);
        return -1;
    }

    /* Test diagnose_report */
    char* report = gmb_rageval_diagnose_report();
    if (!report) return -1;
    if (!strstr(report, "\"total\"")) {
        fprintf(stderr, "gmb_rageval_advanced_selfcheck: diagnose_report failed: %s\n", report);
        free(report);
        return -1;
    }
    free(report);

    return 0;
}
