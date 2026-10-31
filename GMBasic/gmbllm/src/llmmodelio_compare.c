/* llmmodelio_compare.c — `LLM TENSOR COMPARE <a>,<b>[,<glob>]` (B6).
 *
 * Confronto numerico per-tensore tra due GGUF (analisi, sola lettura): dequant + metriche
 * (rel_l2, avg_diff% normalizzato stile antirez). Serve a lineage/frozen-layer e al breakdown
 * per-ruolo dopo quant/surgery. Il CORE vive in LLMEnv/LLMCodec (`llmcodec_gguf_compare`, Apache)
 * — UNA SOLA SORGENTE, consumata anche dalla CLI FastLLM. Qui solo: argomenti + formattazione. */
#include "llmmodelio_model.h"
#include "llmcodec_gguf_compare.h"
#include "llmcodec_ggml_quants.h"
#include <stdio.h>
#include <string.h>

static int cmp_print_cb(const char* name, const LlmCodecGgufCompareStat* st, void* ud)
{
    FILE* fo = (FILE*)ud;
    char ta[32] = "?", tb[32] = "?";
    llmcodec_ggml_type_name(st->ggml_type_a, ta, sizeof ta);
    llmcodec_ggml_type_name(st->ggml_type_b, tb, sizeof tb);
    fprintf(fo, "  [%s] %s/%s n=%llu rel_l2=%.6g avg_diff=%.4f%% max_abs=%.6g\n",
            name, ta, tb, (unsigned long long)st->numel,
            st->rel_l2, st->avg_diff_pct, st->max_abs);
    return 0;
}

int llmmodelio_compare(const char* a, const char* b, const char* glob, FILE* out, int* handled)
{
    if (handled) *handled = 0;
    if (a == NULL || !*a || b == NULL || !*b) {
        fprintf(stderr, "LLM TENSOR COMPARE: attesi <modello_a>, <modello_b>[, <glob>]\n");
        return -1;
    }
    FILE* fo = out ? out : stdout;
    if (handled) *handled = 1;

    LlmCodecGgufCompareSummary s;
    if (llmcodec_gguf_compare(a, b, (glob && *glob) ? glob : NULL, cmp_print_cb, fo, &s) != 0) {
        fprintf(stderr, "LLM TENSOR COMPARE: impossibile leggere '%s' o '%s' come GGUF\n", a, b);
        return -1;
    }
    fprintf(fo, "LLM TENSOR COMPARE '%s' vs '%s': n_a=%llu n_b=%llu matched=%llu only_a=%llu "
                "only_b=%llu mismatch=%llu skipped=%llu rel_l2=%.6g avg_diff=%.4f%% max_diff=%.4f%% @%s\n",
            a, b, (unsigned long long)s.n_a, (unsigned long long)s.n_b,
            (unsigned long long)s.matched, (unsigned long long)s.only_a, (unsigned long long)s.only_b,
            (unsigned long long)s.mismatch, (unsigned long long)s.skipped,
            s.rel_l2_agg, s.avg_diff_pct, s.max_diff_pct,
            s.max_diff_name[0] ? s.max_diff_name : "-");
    return 0;
}
