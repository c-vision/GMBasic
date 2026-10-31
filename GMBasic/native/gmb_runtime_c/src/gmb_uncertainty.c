/* ============================================================================
 * gmb_uncertainty.c — Self-Consistency / Semantic Entropy (Spanda R_sc)
 * Implementation: deterministic string normalization + exact-match clustering +
 *                 Shannon entropy (log2) → risk_score 0..1.
 * Cost: ~1.3 µs on CPU, zero GPU. No external dependencies.
 * Reference: github.com/Adarshent/Spnda (Spanda, DOI 10.5281/zenodo.22233648)
 * ============================================================================ */

#include "gmb_uncertainty.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------------
 * Internal: String Normalization Helpers
 * ---------------------------------------------------------------------------- */

/* Remove co-markers: "####", "\boxed{...}" (extract content from boxed) */
static void strip_co_markers(char* s)
{
    if (!s) return;
    char* p = s;
    while (*p)
    {
        /* "####" */
        if (p[0] == '#' && p[1] == '#' && p[2] == '#' && p[3] == '#')
        {
            char* q = p + 4;
            while (*q) *p++ = *q++;
            *p = '\0';
            continue;
        }
        /* "\boxed{...}" — EXTRACT content inside, don't remove entirely */
        if (p[0] == '\\' && p[1] == 'b' && p[2] == 'o' && p[3] == 'x' &&
            p[4] == 'e' && p[5] == 'd' && p[6] == '{')
        {
            char* q = p + 7; /* after \boxed{ */
            char* start = q;
            int depth = 1;
            while (*q && depth > 0)
            {
                if (*q == '{') depth++;
                else if (*q == '}') depth--;
                q++;
            }
            /* q points after closing }. Extract content [start, q-1) */
            int content_len = (int)(q - start - 1); /* exclude final } */
            if (content_len > 0)
            {
                memmove(p, start, content_len);
                p += content_len;
            }
            /* Copy rest after the boxed expression */
            while (*q) *p++ = *q++;
            *p = '\0';
            continue;
        }
        p++;
    }
}

/* Trim filler prefixes: "The answer is ", "Answer: ", "Result: " */
static void trim_filler_prefix(char* s)
{
    if (!s) return;
    /* Trim leading whitespace first */
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return;

    const char* prefixes[] = {
        "the answer is ",
        "answer: ",
        "result: ",
        "the result is ",
        "final answer: ",
        "final answer is ",
    };
    int n = sizeof(prefixes) / sizeof(prefixes[0]);

    for (int i = 0; i < n; i++)
    {
        const char* pref = prefixes[i];
        int len = (int)strlen(pref);
        if (strncasecmp(s, pref, len) == 0)
        {
            char* q = s + len;
            while (*q && isspace((unsigned char)*q)) q++;
            memmove(s, q, strlen(q) + 1);
            trim_filler_prefix(s);
            return;
        }
    }
}

/* Normalize numbers: "42.0" → "42", "$42" → "42", basic word numbers */
static void normalize_numbers(char* s)
{
    if (!s) return;
    /* This is a simplified version — full word-number parsing is complex.
     * For MVP: strip trailing ".0" from floats, strip "$", ",". */
    char* p = s;
    while (*p)
    {
        if (*p == '$' || *p == ',')
        {
            char* q = p + 1;
            while (*q) *p++ = *q++;
            *p = '\0';
            continue;
        }
        p++;
    }

    /* Strip trailing ".0" from integers */
    int len = (int)strlen(s);
    if (len >= 2 && s[len - 2] == '.' && s[len - 1] == '0')
    {
        s[len - 2] = '\0';
    }

    /* Basic word-to-number mapping for common test cases */
    struct { const char* word; const char* num; } word_nums[] = {
        {"zero", "0"}, {"one", "1"}, {"two", "2"}, {"three", "3"}, {"four", "4"},
        {"five", "5"}, {"six", "6"}, {"seven", "7"}, {"eight", "8"}, {"nine", "9"},
        {"ten", "10"}, {"eleven", "11"}, {"twelve", "12"}, {"thirteen", "13"},
        {"fourteen", "14"}, {"fifteen", "15"}, {"sixteen", "16"}, {"seventeen", "17"},
        {"eighteen", "18"}, {"nineteen", "19"}, {"twenty", "20"},
        {"thirty", "30"}, {"forty", "40"}, {"fifty", "50"}, {"sixty", "60"},
        {"seventy", "70"}, {"eighty", "80"}, {"ninety", "90"}, {"hundred", "100"},
        {"thousand", "1000"}, {"million", "1000000"}
    };
    int n_words = sizeof(word_nums) / sizeof(word_nums[0]);

    for (int i = 0; i < n_words; i++)
    {
        if (strcmp(s, word_nums[i].word) == 0)
        {
            strcpy(s, word_nums[i].num);
            break;
        }
    }
}

/* Lowercase, collapse whitespace, trim */
static void canonicalize_whitespace(char* s)
{
    if (!s) return;
    char* p = s;
    char* q = s;
    bool in_space = false;

    while (*p)
    {
        unsigned char c = (unsigned char)*p;
        if (isspace(c))
        {
            if (!in_space)
            {
                *q++ = ' ';
                in_space = true;
            }
        }
        else
        {
            *q++ = tolower(c);
            in_space = false;
        }
        p++;
    }
    *q = '\0';

    /* Trim leading/trailing spaces */
    p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    q = s + strlen(s) - 1;
    while (q >= s && isspace((unsigned char)*q)) q--;
    q[1] = '\0';
}

/* ----------------------------------------------------------------------------
 * Public: gmb_text_normalize
 * ---------------------------------------------------------------------------- */

int gmb_text_normalize(const char* input, char* output, int out_len)
{
    if (!input || !output || out_len <= 0) return -1;

    int len = (int)strlen(input);
    if (len >= out_len) return -1; /* buffer too small */

    strcpy(output, input);

    /* Pipeline: co-markers → filler prefixes → numbers → canonicalize */
    strip_co_markers(output);
    trim_filler_prefix(output);
    normalize_numbers(output);
    canonicalize_whitespace(output);

    return (int)strlen(output);
}

/* ----------------------------------------------------------------------------
 * Public: gmb_bucket_exact_match
 * ---------------------------------------------------------------------------- */

int gmb_bucket_exact_match(const char* responses[], int k,
                           const char* buckets_out[],
                           int counts_out[])
{
    if (k < 1 || k > GMB_UNCERTAINTY_MAX_RESPONSES) return -1;
    if (!responses || !buckets_out || !counts_out) return -1;

    int n_buckets = 0;

    for (int i = 0; i < k; i++)
    {
        const char* r = responses[i];
        if (!r) continue;

        /* Find existing bucket */
        int found = -1;
        for (int b = 0; b < n_buckets; b++)
        {
            if (strcmp(r, buckets_out[b]) == 0)
            {
                found = b;
                break;
            }
        }

        if (found >= 0)
        {
            counts_out[found]++;
        }
        else
        {
            if (n_buckets >= GMB_UNCERTAINTY_MAX_BUCKETS) return -1;
            buckets_out[n_buckets] = r;
            counts_out[n_buckets] = 1;
            n_buckets++;
        }
    }

    return n_buckets;
}

/* ----------------------------------------------------------------------------
 * Public: gmb_shannon_entropy
 * ---------------------------------------------------------------------------- */

int gmb_shannon_entropy(const int counts[], int k, double* entropy_out)
{
    if (k < 1 || k > GMB_UNCERTAINTY_MAX_BUCKETS) return -1;
    if (!counts || !entropy_out) return -1;

    int total = 0;
    for (int i = 0; i < k; i++)
    {
        if (counts[i] <= 0) return -1;
        total += counts[i];
    }
    if (total == 0) return -1;

    double entropy = 0.0;
    for (int i = 0; i < k; i++)
    {
        double p = (double)counts[i] / total;
        if (p > 0)
        {
            entropy -= p * log2(p);
        }
    }

    *entropy_out = entropy;
    return 0;
}

/* ----------------------------------------------------------------------------
 * Public: gmb_risk_score
 * ---------------------------------------------------------------------------- */

int gmb_risk_score(double entropy, int k, double* risk_out)
{
    if (k < 1) return -1;
    if (!risk_out) return -1;

    if (k == 1)
    {
        *risk_out = 0.0;
        return 0;
    }

    double max_entropy = log2((double)k);
    if (max_entropy <= 0)
    {
        *risk_out = 0.0;
        return 0;
    }

    double risk = entropy / max_entropy;
    if (risk < 0.0) risk = 0.0;
    if (risk > 1.0) risk = 1.0;

    *risk_out = risk;
    return 0;
}

/* ----------------------------------------------------------------------------
 * Public: gmb_consistency_compute
 * ---------------------------------------------------------------------------- */

int gmb_consistency_compute(const char* responses[], int k,
                            double* risk_out,
                            double* entropy_out,
                            int* n_buckets_out)
{
    if (k < 1 || k > GMB_UNCERTAINTY_MAX_RESPONSES) return -1;
    if (!responses || !risk_out) return -1;

    /* Step 1: Normalize all responses (in-place on stack) */
    char normalized[GMB_UNCERTAINTY_MAX_RESPONSES][GMB_UNCERTAINTY_MAX_RESP_LEN];
    const char* norm_ptrs[GMB_UNCERTAINTY_MAX_RESPONSES];

    for (int i = 0; i < k; i++)
    {
        if (!responses[i])
        {
            norm_ptrs[i] = "";
            continue;
        }
        int r = gmb_text_normalize(responses[i], normalized[i], GMB_UNCERTAINTY_MAX_RESP_LEN);
        if (r < 0) return -1;
        norm_ptrs[i] = normalized[i];
    }

    /* Step 2: Cluster */
    const char* buckets[GMB_UNCERTAINTY_MAX_BUCKETS];
    int counts[GMB_UNCERTAINTY_MAX_BUCKETS];
    int n_buckets = gmb_bucket_exact_match(norm_ptrs, k, buckets, counts);
    if (n_buckets < 0) return -1;

    /* Step 3: Entropy */
    double entropy = 0.0;
    int r = gmb_shannon_entropy(counts, n_buckets, &entropy);
    if (r < 0) return -1;

    /* Step 4: Risk */
    r = gmb_risk_score(entropy, k, risk_out);
    if (r < 0) return -1;

    if (entropy_out) *entropy_out = entropy;
    if (n_buckets_out) *n_buckets_out = n_buckets;

    return 0;
}

/* ----------------------------------------------------------------------------
 * Public: gmb_consistency_detail
 * ---------------------------------------------------------------------------- */

int gmb_consistency_detail(const char* responses[], int k,
                           gmb_consistency_detail_t* detail_out)
{
    if (k < 1 || k > GMB_UNCERTAINTY_MAX_RESPONSES) return -1;
    if (!responses || !detail_out) return -1;

    char normalized[GMB_UNCERTAINTY_MAX_RESPONSES][GMB_UNCERTAINTY_MAX_RESP_LEN];
    const char* norm_ptrs[GMB_UNCERTAINTY_MAX_RESPONSES];

    for (int i = 0; i < k; i++)
    {
        if (!responses[i])
        {
            norm_ptrs[i] = "";
            continue;
        }
        int r = gmb_text_normalize(responses[i], normalized[i], GMB_UNCERTAINTY_MAX_RESP_LEN);
        if (r < 0) return -1;
        norm_ptrs[i] = normalized[i];
    }

    const char* buckets[GMB_UNCERTAINTY_MAX_BUCKETS];
    int counts[GMB_UNCERTAINTY_MAX_BUCKETS];
    int n_buckets = gmb_bucket_exact_match(norm_ptrs, k, buckets, counts);
    if (n_buckets < 0) return -1;

    double entropy = 0.0;
    int r = gmb_shannon_entropy(counts, n_buckets, &entropy);
    if (r < 0) return -1;

    double risk = 0.0;
    r = gmb_risk_score(entropy, k, &risk);
    if (r < 0) return -1;

    detail_out->k = k;
    detail_out->n_buckets = n_buckets;
    detail_out->entropy = entropy;
    detail_out->risk = risk;
    detail_out->collapse_warning = false;

    for (int i = 0; i < GMB_UNCERTAINTY_MAX_BUCKETS; i++)
    {
        if (i < n_buckets)
        {
            detail_out->bucket_sizes[i] = counts[i];
            detail_out->bucket_reps[i] = buckets[i];
        }
        else
        {
            detail_out->bucket_sizes[i] = 0;
            detail_out->bucket_reps[i] = NULL;
        }
    }

    /* Heuristic collapse warning: risk ≈ 0 on factoid domain.
     * We can't know the domain here, so flag if risk < 0.05 and K >= 3. */
    if (risk < 0.05 && k >= 3)
    {
        detail_out->collapse_warning = true;
    }

    return 0;
}

/* ----------------------------------------------------------------------------
 * Public: gmb_consistency_detail_to_json
 * ---------------------------------------------------------------------------- */

char* gmb_consistency_detail_to_json(const gmb_consistency_detail_t* detail)
{
    if (!detail) return NULL;

    /* Estimate buffer size */
    size_t est = 512;
    for (int i = 0; i < detail->n_buckets; i++)
    {
        if (detail->bucket_reps[i])
        {
            est += strlen(detail->bucket_reps[i]) + 32;
        }
    }

    char* json = (char*)malloc(est);
    if (!json) return NULL;

    char* p = json;
    int written = snprintf(p, est,
        "{\"k\":%d,\"n_buckets\":%d,\"entropy\":%.6f,\"risk\":%.6f,"
        "\"collapse_warning\":%s,\"buckets\":[",
        detail->k, detail->n_buckets, detail->entropy, detail->risk,
        detail->collapse_warning ? "true" : "false");
    if (written < 0 || written >= (int)est)
    {
        free(json);
        return NULL;
    }
    p += written;
    est -= written;

    for (int i = 0; i < detail->n_buckets; i++)
    {
        if (i > 0)
        {
            if (est < 2) { free(json); return NULL; }
            *p++ = ','; est--;
        }
        const char* rep = detail->bucket_reps[i] ? detail->bucket_reps[i] : "";
        written = snprintf(p, est,
            "{\"size\":%d,\"repr\":\"%s\"}",
            detail->bucket_sizes[i], rep);
        if (written < 0 || written >= (int)est)
        {
            free(json);
            return NULL;
        }
        p += written;
        est -= written;
    }

    if (est < 2) { free(json); return NULL; }
    *p++ = ']'; *p++ = '}'; *p = '\0';

    return json;
}
/* ============================================================================
 * Entry point BASIC (§0bis: runtime condiviso, chiamati dai codegen C)
 * ----------------------------------------------------------------------------
 * AI.CONSISTENCY(prompt, opts)      → risk 0..1   (richiede campionamento LLM)
 * AI.LEXICALENTROPY(responses$)     → entropia di Shannon (bit) sulle risposte
 * AI.CONSISTENCYDETAIL$()           → JSON dell'ULTIMO calcolo
 *
 * Coerenza: gli emitter C des C (gmb_c99_emit_uncertainty.c / C99CodeEmitter) chiamano
 * ESATTAMENTE questi nomi; il backend gestito ha lo stesso contratto implementato in C#
 * (common/AI/Uncertainty/SemanticEntropy.cs — C# puro, MAI P/Invoke sul runtime C).
 * ============================================================================ */

/* Ultimo dettaglio calcolato (per AI.CONSISTENCYDETAIL$). */
static gmb_consistency_detail_t g_unc_last_detail;
static int g_unc_last_detail_valid = 0;

/* Divide `responses` su ';' o newline in al più `max` stringhe, scrivendo in `buf`
 * (caller-owned). Ritorna il numero di risposte (>=1) o -1 su errore/overflow. */
static int uncertainty_split(const char* responses, char* buf, size_t cap,
                             const char* out[], int max)
{
    if (!responses || !buf || !out || max < 1) return -1;
    size_t n = strlen(responses);
    if (n + 1 > cap) return -1;
    memcpy(buf, responses, n + 1);

    int k = 0;
    char* p = buf;
    while (p && k < max)
    {
        char* sep = p;
        while (*sep && *sep != ';' && *sep != '\n' && *sep != '\r') sep++;
        char saved = *sep;
        *sep = '\0';
        /* trim degli spazi ai bordi */
        char* s = p;
        while (*s == ' ' || *s == '\t') s++;
        char* e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
        out[k++] = s;
        if (saved == '\0') break;
        p = sep + 1;
    }
    return k > 0 ? k : -1;
}

double gmb_lexical_entropy(const char* responses)
{
    char* buf = (char*)malloc(GMB_UNCERTAINTY_MAX_RESPONSES * GMB_UNCERTAINTY_MAX_RESP_LEN);
    if (!buf) { g_unc_last_detail_valid = 0; return 0.0; }
    const char* out[GMB_UNCERTAINTY_MAX_RESPONSES];
    int k = uncertainty_split(responses, buf, GMB_UNCERTAINTY_MAX_RESPONSES * GMB_UNCERTAINTY_MAX_RESP_LEN,
                              out, GMB_UNCERTAINTY_MAX_RESPONSES);
    double entropy = 0.0;
    if (k < 1 || gmb_consistency_detail(out, k, &g_unc_last_detail) != 0)
    {
        g_unc_last_detail_valid = 0;
        free(buf);
        return 0.0;
    }
    g_unc_last_detail_valid = 1;
    entropy = g_unc_last_detail.entropy;
    free(buf);
    return entropy;
}

double gmb_consistency(const char* prompt, const char* options_json)
{
    /* Il campionamento K× (AskMulti) richiede il PROVIDER LLM, che nel mondo C vive nel
     * processo host (native/host_*): il runtime condiviso non ha un completer in-process.
     * Senza campionamento la self-consistency non è calcolabile → NaN, ESATTAMENTE come il
     * backend gestito quando nessun provider è configurato (nessun numero inventato).
     * Il dettaglio resta non valido: AI.CONSISTENCYDETAIL$() → "{}". */
    (void)prompt; (void)options_json;
    g_unc_last_detail_valid = 0;
    return NAN;
}

char* gmb_consistency_detail_json(void)
{
    if (!g_unc_last_detail_valid) return strdup("{}");
    return gmb_consistency_detail_to_json(&g_unc_last_detail);
}
