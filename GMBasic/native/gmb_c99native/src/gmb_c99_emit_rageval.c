/* gmb_c99_emit_rageval.c — emitter C99 nativo del RAG eval (todo Rag-Eval-Gap-Analysis,
 * Fase A: R1+R2) — §0quater, file di dominio.
 * Quartetto retrieval (PRECISIONAT/RECALLAT/MRR/NDCGAT, CSV di id) + giudizi generazione
 * tier-1 (FAITHFULNESS/RELEVANCE/HALLUCINATION/COMPLETENESS). Ogni voce emette la chiamata
 * al runtime C condiviso gmb_rageval_* (gmb_rag_eval.c, §0bis) — MAI logica inline.
 * Parità con emitter C# NsKindRow (C99CodeEmitter.Builtin.Core.cs). Tabella RE_TBL
 * token-first (regola d'oro) e l'audit la conta. */
#include "gmb_c99_emit_internal.h"

enum { RE_K, RE_PAIR };
typedef struct { gmb_tokenkind_t tk; int kind; const char* fn; } re_row;

static const re_row RE_TBL[] = {
    /* R1 — quartetto retrieval */
    { GMB_TOK_VectorDbPrecisionAt, RE_K, "gmb_rageval_precision_at" },
    { GMB_TOK_VectorDbRecallAt, RE_K, "gmb_rageval_recall_at" },
    { GMB_TOK_VectorDbMrr, RE_PAIR, "gmb_rageval_mrr" },
    { GMB_TOK_VectorDbNdcgAt, RE_K, "gmb_rageval_ndcg_at" },
    /* R2 — giudizi generazione tier-1 */
    { GMB_TOK_VectorDbFaithfulness, RE_PAIR, "gmb_rageval_faithfulness" },
    { GMB_TOK_VectorDbRelevance, RE_PAIR, "gmb_rageval_relevance" },
    { GMB_TOK_VectorDbHallucination, RE_PAIR, "gmb_rageval_hallucination" },
    { GMB_TOK_VectorDbCompleteness, RE_PAIR, "gmb_rageval_completeness" },
};

int emit_bf_rageval(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                    gmb_expr_t** a, int n, int line) {
    (void)name; (void)ns;
    const re_row* row = NULL;
    for (size_t i = 0; i < sizeof(RE_TBL) / sizeof(RE_TBL[0]); i++)
        if (RE_TBL[i].tk == tk) { row = &RE_TBL[i]; break; }
    if (!row) return 0;
    switch (row->kind) {
    case RE_K:
        if (n != 3) { c99_fail(c, line, "VECTORDB.<METRICA>: attesi (retrieved$, ideal$, k)"); return 1; }
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data, (int)(");
        emit_expr(c, a[2]); sb_put(&c->out, "))");
        return 1;
    case RE_PAIR:
        if (n != 2) { c99_fail(c, line, "VECTORDB.<GIUDIZIO>: attesi (a$, b$)"); return 1; }
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        emit_str_operand(c, a[0]); sb_put(&c->out, ".data, ");
        emit_str_operand(c, a[1]); sb_put(&c->out, ".data)");
        return 1;
    }
    return 0;
}
