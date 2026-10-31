/* gmb_c99_emit_uncertainty.c — emitter C99 nativo per Self-Consistency / Semantic Entropy
 * (todo AI-SelfConsistency-Uncertainty-Spanda-Gap-Analysis, P2).
 * AI.CONSISTENCY(prompt, opts?) → risk 0..1 (sampling K× via AskMulti, GPM park-based).
 * AI.LEXICALENTROPY(responses$) → entropy 0..1 (deterministic, ~µs, pure C runtime).
 * AI.CONSISTENCYDETAIL$() → JSON detail report (k, clusters, entropy, risk, collapse_warning).
 * Ogni voce emette la chiamata al runtime C condiviso gmb_uncertainty_* (gmb_uncertainty.c, §0bis) — MAI logica inline.
 * Parità con emitter C# NsKindRow (C99CodeEmitter.Builtin.Core.cs). Tabella token-first (regola d'oro) e l'audit la conta. */
#include "gmb_c99_emit_internal.h"

enum { UC_CONSISTENCY, UC_ENTROPY, UC_DETAIL };

typedef struct { gmb_tokenkind_t tk; int kind; const char* fn; } uc_row;

static const uc_row UC_TBL[] = {
    { GMB_TOK_AiSelfConsistency, UC_CONSISTENCY, "gmb_consistency" },
    { GMB_TOK_AiLexicalEntropy, UC_ENTROPY, "gmb_lexical_entropy" },
    { GMB_TOK_AiConsistencyDetail, UC_DETAIL, "gmb_consistency_detail_json" },
};

int emit_bf_uncertainty(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                        gmb_expr_t** a, int n, int line) {
    (void)name; (void)ns;
    const uc_row* row = NULL;
    for (size_t i = 0; i < sizeof(UC_TBL) / sizeof(UC_TBL[0]); i++)
        if (UC_TBL[i].tk == tk) { row = &UC_TBL[i]; break; }
    if (!row) return 0;

    switch (row->kind) {
    case UC_CONSISTENCY:
        if (n < 1 || n > 2) { c99_fail(c, line, "AI.CONSISTENCY: attesi (prompt, options?)"); return 1; }
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        emit_str_operand(c, a[0]);
        sb_put(&c->out, ".data");   /* il runtime C vuole const char*, non gmb_string_t */
        if (n >= 2) {
            /* options: dict literal → il runtime C vuole una stringa JSON (parità emitter C#
             * e IL/PCode dove options è IDictionary). gmb_val_to_json(...).data. */
            sb_put(&c->out, ", gmb_val_to_json(");
            /* gmb_val_to_json vuole un gmb_val_t: BOXA l'argomento (prima passava un
             * gmb_string_t nudo → "passing gmb_string_t to gmb_val_t", bug reale). */
            if (etype(c, a[1]) == TY_STR) {
                sb_put(&c->out, "gmb_make_val_string((");
                emit_str_operand(c, a[1]);
                sb_put(&c->out, ").data)");
            } else {
                sb_put(&c->out, "gmb_make_val_double((gmb_double_t)(");
                emit_expr(c, a[1]);
                sb_put(&c->out, "))");
            }
            sb_put(&c->out, ").data");
        } else {
            sb_put(&c->out, ", NULL");
        }
        sb_put(&c->out, ")");
        return 1;
    case UC_ENTROPY:
        if (n != 1) { c99_fail(c, line, "AI.LEXICALENTROPY: atteso (responses$)"); return 1; }
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        emit_str_operand(c, a[0]);
        sb_put(&c->out, ".data");   /* const char* (vedi sopra) */
        sb_put(&c->out, ")");
        return 1;
    case UC_DETAIL:
        if (n != 0) { c99_fail(c, line, "AI.CONSISTENCYDETAIL: nessun argomento"); return 1; }
        /* Ritorna una STRINGA (JSON): va incapsulata in gmb_string_alloc come gli altri
         * builtin string-returning, altrimenti l'assegnazione a una variabile STRING
         * fallisce con "type mismatch (gmb_string_t vs gmb_double_t)". */
        sb_put(&c->out, "gmb_string_alloc(");
        sb_put(&c->out, row->fn); sb_put(&c->out, "())");
        return 1;
    }
    return 0;
}