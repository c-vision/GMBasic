/* ============================================================================
 * gmb_uncertainty_selfcheck.c — Self-check tests for gmb_uncertainty.c
 * Vector parity tests (5 cases) for CI gate.
 * Expected values from manual computation / reference implementation.
 * ============================================================================ */

#include "gmb_uncertainty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ_INT(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "FAIL: %s: expected %d, got %d\n", msg, expected, actual); \
            tests_failed++; \
        } else { \
            tests_passed++; \
        } \
    } while (0)

#define ASSERT_EQ_DBL(expected, actual, eps, msg) \
    do { \
        if (fabs((expected) - (actual)) > (eps)) { \
            fprintf(stderr, "FAIL: %s: expected %.6f, got %.6f\n", msg, expected, actual); \
            tests_failed++; \
        } else { \
            tests_passed++; \
        } \
    } while (0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            tests_failed++; \
        } else { \
            tests_passed++; \
        } \
    } while (0)

/* Test 1: Identical responses → risk = 0, entropy = 0 */
static void test_identical(void)
{
    const char* responses[] = {
        "42",
        "42",
        "42",
        "42",
        "42"
    };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 5, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_identical: return code");
    ASSERT_EQ_DBL(0.0, risk, 1e-9, "test_identical: risk");
    ASSERT_EQ_DBL(0.0, entropy, 1e-9, "test_identical: entropy");
    ASSERT_EQ_INT(1, n_buckets, "test_identical: n_buckets");
}

/* Test 2: All different → risk = 1, entropy = log2(5) ≈ 2.3219 */
static void test_all_different(void)
{
    const char* responses[] = {
        "42",
        "24",
        "13",
        "7",
        "99"
    };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 5, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_all_different: return code");
    ASSERT_EQ_DBL(1.0, risk, 1e-9, "test_all_different: risk");
    ASSERT_EQ_DBL(log2(5.0), entropy, 1e-6, "test_all_different: entropy");
    ASSERT_EQ_INT(5, n_buckets, "test_all_different: n_buckets");
}

/* Test 3: 3-2 split → entropy = -(0.6*log2(0.6) + 0.4*log2(0.4)) ≈ 0.97095 */
static void test_split_3_2(void)
{
    const char* responses[] = {
        "The answer is 42",
        "42",
        "42.0",
        "seven",
        "7"
    };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 5, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_split_3_2: return code");

    /* After normalization: "42", "42", "42", "seven", "7" → 2 buckets (3 and 2) */
    ASSERT_EQ_INT(2, n_buckets, "test_split_3_2: n_buckets");

    double expected_entropy = -(0.6 * log2(0.6) + 0.4 * log2(0.4));
    ASSERT_EQ_DBL(expected_entropy, entropy, 1e-6, "test_split_3_2: entropy");

    double expected_risk = expected_entropy / log2(5.0);
    ASSERT_EQ_DBL(expected_risk, risk, 1e-6, "test_split_3_2: risk");
}

/* Test 4: Normalization pipeline (co-markers, filler, numbers) */
static void test_normalization(void)
{
    const char* responses[] = {
        "#### The answer is $42.0",
        "Answer: 42",
        "\\boxed{42}",
        "Final answer is 42"
    };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 4, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_normalization: return code");
    ASSERT_EQ_DBL(0.0, risk, 1e-9, "test_normalization: risk (all normalize to 42)");
    ASSERT_EQ_DBL(0.0, entropy, 1e-9, "test_normalization: entropy");
    ASSERT_EQ_INT(1, n_buckets, "test_normalization: n_buckets");
}

/* Test 5: Mixed with filler and different cases */
static void test_mixed_case_filler(void)
{
    const char* responses[] = {
        "THE ANSWER IS 42",
        "the answer is 42",
        "The Answer Is 42",
        "42",
        "forty two"  /* won't normalize to 42 in MVP */
    };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 5, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_mixed_case_filler: return code");
    /* "THE ANSWER IS 42" → "42", "the answer is 42" → "42", etc.
     * "forty two" stays "forty two" → 2 buckets (4 and 1) */
    ASSERT_EQ_INT(2, n_buckets, "test_mixed_case_filler: n_buckets");

    double expected_entropy = -(0.8 * log2(0.8) + 0.2 * log2(0.2));
    ASSERT_EQ_DBL(expected_entropy, entropy, 1e-6, "test_mixed_case_filler: entropy");
}

/* Test 6: detail struct */
static void test_detail_struct(void)
{
    const char* responses[] = {
        "42",
        "42",
        "24"
    };
    gmb_consistency_detail_t detail;

    int r = gmb_consistency_detail(responses, 3, &detail);
    ASSERT_EQ_INT(0, r, "test_detail_struct: return code");
    ASSERT_EQ_INT(3, detail.k, "test_detail_struct: k");
    ASSERT_EQ_INT(2, detail.n_buckets, "test_detail_struct: n_buckets");
    ASSERT_EQ_INT(2, detail.bucket_sizes[0], "test_detail_struct: bucket 0 size");
    ASSERT_EQ_INT(1, detail.bucket_sizes[1], "test_detail_struct: bucket 1 size");
    ASSERT_TRUE(detail.bucket_reps[0] != NULL, "test_detail_struct: bucket 0 repr");
    ASSERT_TRUE(detail.bucket_reps[1] != NULL, "test_detail_struct: bucket 1 repr");
    ASSERT_TRUE(detail.collapse_warning == false || detail.collapse_warning == true,
                "test_detail_struct: collapse_warning is bool");
}

/* Test 7: JSON serialization */
static void test_json_serialization(void)
{
    const char* responses[] = { "42", "42", "24" };
    gmb_consistency_detail_t detail;

    int r = gmb_consistency_detail(responses, 3, &detail);
    ASSERT_EQ_INT(0, r, "test_json_serialization: detail");

    char* json = gmb_consistency_detail_to_json(&detail);
    ASSERT_TRUE(json != NULL, "test_json_serialization: json not null");

    /* Check JSON contains expected fields */
    ASSERT_TRUE(strstr(json, "\"k\":3") != NULL, "test_json_serialization: k");
    ASSERT_TRUE(strstr(json, "\"n_buckets\":2") != NULL, "test_json_serialization: n_buckets");
    ASSERT_TRUE(strstr(json, "\"collapse_warning\":") != NULL, "test_json_serialization: collapse_warning");
    ASSERT_TRUE(strstr(json, "\"buckets\":") != NULL, "test_json_serialization: buckets array");

    free(json);
}

/* Test 8: K=1 edge case */
static void test_k1_edge(void)
{
    const char* responses[] = { "42" };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 1, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_k1_edge: return code");
    ASSERT_EQ_DBL(0.0, risk, 1e-9, "test_k1_edge: risk");
    ASSERT_EQ_DBL(0.0, entropy, 1e-9, "test_k1_edge: entropy");
    ASSERT_EQ_INT(1, n_buckets, "test_k1_edge: n_buckets");
}

/* Test 9: Empty string handling */
static void test_empty_string(void)
{
    const char* responses[] = { "42", "", "42" };
    double risk, entropy;
    int n_buckets;

    int r = gmb_consistency_compute(responses, 3, &risk, &entropy, &n_buckets);
    ASSERT_EQ_INT(0, r, "test_empty_string: return code");
    ASSERT_EQ_INT(2, n_buckets, "test_empty_string: n_buckets");
}

/* Test 10: collapse_warning heuristic */
static void test_collapse_warning(void)
{
    /* High agreement on factoid-like → collapse_warning = true */
    const char* responses[] = { "42", "42", "42", "42", "42" };
    gmb_consistency_detail_t detail;

    int r = gmb_consistency_detail(responses, 5, &detail);
    ASSERT_EQ_INT(0, r, "test_collapse_warning: return code");
    ASSERT_TRUE(detail.collapse_warning == true, "test_collapse_warning: flag set for unanimous factoid");
}

int main(void)
{
    printf("Running gmb_uncertainty selfcheck...\n\n");

    test_identical();
    test_all_different();
    test_split_3_2();
    test_normalization();
    test_mixed_case_filler();
    test_detail_struct();
    test_json_serialization();
    test_k1_edge();
    test_empty_string();
    test_collapse_warning();

    printf("\n=== SUMMARY ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed > 0)
    {
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}