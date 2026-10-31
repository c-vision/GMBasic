/* ============================================================================
 * gmb_uncertainty.h — Self-Consistency / Semantic Entropy (Spanda R_sc)
 * Public API for the C runtime (§0bis: single source, 4 backends call it).
 * Implements: deterministic string normalization + exact-match clustering +
 *             Shannon entropy (log2) → risk_score 0..1.
 * Cost: ~1.3 µs on CPU, zero GPU. No external dependencies.
 * Reference: github.com/Adarshent/Spnda (Spanda, DOI 10.5281/zenodo.22233648)
 *            liquidngas.substack.com/p/i-tried-to-make-semantic-entropy (2026-09-10)
 * ============================================================================ */

#ifndef GMB_UNCERTAINTY_H
#define GMB_UNCERTAINTY_H

#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------------
 * Configuration constants
 * ---------------------------------------------------------------------------- */

#define GMB_UNCERTAINTY_MAX_RESPONSES  32   /* max K responses supported */
#define GMB_UNCERTAINTY_MAX_RESP_LEN   8192 /* max length per response string */
#define GMB_UNCERTAINTY_MAX_BUCKETS    32   /* max unique buckets (≤ K) */

/* ----------------------------------------------------------------------------
 * String Normalization (deterministic, pure C)
 * ---------------------------------------------------------------------------- */

/* Normalizes a response string for clustering:
 *   - Removes co-markers: "####", "\boxed{}"
 *   - Normalizes numbers: "42.0" → "42", "$42" → "42", "forty-two" → "42" (basic)
 *   - Trims filler prefixes: "The answer is ", "Answer: ", "Result: "
 *   - Lowercases, collapses whitespace, trims punctuation
 *   - Returns normalized length (≤ out_len), or -1 on buffer too small.
 * Thread-safe (no global state). */
int gmb_text_normalize(const char* input, char* output, int out_len);

/* ----------------------------------------------------------------------------
 * Exact-Match Clustering
 * ---------------------------------------------------------------------------- */

/* Clusters K normalized responses into exact-match buckets.
 * Inputs:
 *   responses[]   - array of K normalized strings (caller-owned)
 *   k             - number of responses (1..GMB_UNCERTAINTY_MAX_RESPONSES)
 *   buckets_out   - array of GMB_UNCERTAINTY_MAX_BUCKETS char* (filled with
 *                   pointer to first member of each bucket, or NULL)
 *   counts_out    - array of GMB_UNCERTAINTY_MAX_BUCKETS int (bucket sizes)
 * Returns: number of unique buckets (1..k), or -1 on error (k out of range). */
int gmb_bucket_exact_match(const char* responses[], int k,
                           const char* buckets_out[],
                           int counts_out[]);

/* ----------------------------------------------------------------------------
 * Shannon Entropy (log2)
 * ---------------------------------------------------------------------------- */

/* Computes Shannon entropy H = -Σ (p_i * log2(p_i)) over bucket counts.
 * Inputs:
 *   counts[] - array of bucket sizes (positive integers)
 *   k        - number of buckets (1..GMB_UNCERTAINTY_MAX_BUCKETS)
 * Output:
 *   entropy_out - filled with entropy value (≥0, max = log2(k))
 * Returns: 0 on success, -1 on error (k out of range or zero total). */
int gmb_shannon_entropy(const int counts[], int k, double* entropy_out);

/* ----------------------------------------------------------------------------
 * Risk Score (normalized 0..1)
 * ---------------------------------------------------------------------------- */

/* Computes normalized risk score from entropy.
 * Formula: risk = entropy / log2(k)  [for k > 1; if k==1, risk=0]
 *   - 0 = unanimous agreement (all K responses identical)
 *   - 1 = maximum divergence (all K responses unique)
 * Inputs:
 *   entropy - Shannon entropy (from gmb_shannon_entropy)
 *   k       - number of original responses (≥1)
 * Output:
 *   risk_out - filled with risk score 0..1
 * Returns: 0 on success, -1 on error (k < 1). */
int gmb_risk_score(double entropy, int k, double* risk_out);

/* ----------------------------------------------------------------------------
 * High-Level API: Full Pipeline (sampling → normalization → clustering → entropy → risk)
 * ---------------------------------------------------------------------------- */

/* Computes self-consistency risk from K raw response strings.
 * This is the main entry point for backends.
 * Inputs:
 *   responses[] - array of K raw response strings
 *   k           - number of responses (1..GMB_UNCERTAINTY_MAX_RESPONSES)
 * Outputs:
 *   risk_out        - risk score 0..1
 *   entropy_out     - Shannon entropy (optional, can be NULL)
 *   n_buckets_out   - number of unique clusters (optional, can be NULL)
 * Returns: 0 on success, -1 on error. */
int gmb_consistency_compute(const char* responses[], int k,
                            double* risk_out,
                            double* entropy_out,
                            int* n_buckets_out);

/* ----------------------------------------------------------------------------
 * Structured Detail Report (for AI.CONSISTENCYDETAIL$)
 * ---------------------------------------------------------------------------- */

typedef struct {
    int k;                      /* original K */
    int n_buckets;              /* number of unique clusters */
    double entropy;             /* Shannon entropy (bits) */
    double risk;                /* normalized risk 0..1 */
    int bucket_sizes[GMB_UNCERTAINTY_MAX_BUCKETS]; /* sizes of each bucket */
    const char* bucket_reps[GMB_UNCERTAINTY_MAX_BUCKETS]; /* representative (first) string of each bucket */
    bool collapse_warning;      /* true if risk ≈ 0 AND domain suggests factoid (heuristic) */
} gmb_consistency_detail_t;

/* Computes full detail report from K raw responses.
 * Inputs:
 *   responses[] - array of K raw response strings
 *   k           - number of responses
 *   detail_out  - filled struct (caller allocates)
 * Returns: 0 on success, -1 on error. */
int gmb_consistency_detail(const char* responses[], int k,
                           gmb_consistency_detail_t* detail_out);

/* ----------------------------------------------------------------------------
 * JSON Output (for BASIC string return)
 * ---------------------------------------------------------------------------- */

/* Serializes a detail struct to compact JSON string.
 * Caller must free returned string with free().
 * Returns: malloc'd JSON string, or NULL on error. */
char* gmb_consistency_detail_to_json(const gmb_consistency_detail_t* detail);

#ifdef __cplusplus
}
#endif

/* ----------------------------------------------------------------------------
 * Entry point BASIC (chiamati dai codegen C: gmb_c99_emit_uncertainty.c)
 * ---------------------------------------------------------------------------- */

/* AI.LEXICALENTROPY(responses$): entropia di Shannon (bit) sulle risposte separate da
 * ';' o newline (pipeline: normalizza → cluster → entropia). Deterministico, nessun
 * provider. Aggiorna il dettaglio per gmb_consistency_detail_json(). */
double gmb_lexical_entropy(const char* responses);

/* AI.CONSISTENCY(prompt, opts): risk 0..1 da K campionamenti del provider.
 * Nel runtime C non c'è un provider in-process (vive nell'host): ritorna NaN con lo
 * stesso contratto del backend gestito senza provider configurato, e invalida il
 * dettaglio (nessun valore inventato). */
double gmb_consistency(const char* prompt, const char* options_json);

/* AI.CONSISTENCYDETAIL$(): JSON dell'ULTIMO calcolo (lexical o consistency).
 * "{}" se nessun calcolo valido è stato registrato. Caller free(). */
char* gmb_consistency_detail_json(void);

#ifdef __cplusplus
}
#endif

#endif /* GMB_UNCERTAINTY_H */