/* gmb_c99_emit_stats.c — emitter C99 nativo dei test d'ipotesi STATS (§0quater).
 * UN handler (emit_bf_stats_dict) per il sotto-dominio: TTEST/TTESTPAIRED/TTESTONESAMPLE,
 * ANOVA, CHI2/CHI2CON, FISHER, ZTEST, KS, SHAPIRO, NORMALTEST, PEARSONR/SPEARMANR/LINREGRESS,
 * MANNWHITNEY/WILCOXON, SKEWTEST/KURTOSISTEST/JARQUEBERA/DESCRIBE.
 * Ogni funzione emette la chiamata al runtime C condiviso gmb_* (gmb_stats.c, §0bis):
 * MAI logica inline. Parità con emitter C# TryEmitStatsDictCall (C99CodeEmitter.Stats.cs).
 * La tabella SD_TBL è token-first (regola d'oro) e l'audit la conta. */
#include "gmb_c99_emit_internal.h"

/* argomento = variabile array numerica (non matrice/classe, non jagged). */
static int sd_var(ctx_t* c, gmb_expr_t* e, var_t** out) {
    if (!e || e->kind != GMB_EXPR_VAR || !e->var) return 0;
    var_t* v = lookup_var(c, e->var);
    if (!v || !v->is_array || v->matrix || v->vclass[0] || v->jagged) return 0;
    *out = v;
    return 1;
}

/* array nativo double[N] → gmb_val_t (lista boxata), statement-expression. */
static void sd_vallist(ctx_t* c, var_t* v) {
    sb_put(&c->out, "({ gmb_list_t _gvl = gmb_list_new(); int _gn = (int)(");
    emit_arr_len_of(c, v);
    sb_put(&c->out, "); for (int _gi = 0; _gi < _gn; _gi++) gmb_list_add(&_gvl, gmb_make_val_double((double)");
    emit_var_ref(c, v);
    sb_put(&c->out, "[_gi])); gmb_make_val_list(_gvl); })");
}

/* array nativo double[N] → (const double* ptr, int n) per i runtime *_flat. */
static void sd_flat(ctx_t* c, var_t* v) {
    sb_put(&c->out, "(const double*)");
    emit_var_ref(c, v);
    sb_put(&c->out, ", (int)(");
    emit_arr_len_of(c, v);
    sb_put(&c->out, ")");
}

enum { SD_TWO_VAL, SD_ONE_VAL, SD_ONE_VAL_MU, SD_FOUR_INT, SD_VAL_MU_SIG,
       SD_TWO_FLAT, SD_TWO_FLAT_LONG, SD_ONE_FLAT, SD_ONE_FLAT_DOUBLE, SD_CHI2CON };
typedef struct { gmb_tokenkind_t tk; int kind; const char* fn; } sd_row;

static const sd_row SD_TBL[] = {
    { GMB_TOK_TTest, SD_TWO_VAL, "gmb_ttest" },
    { GMB_TOK_TTestPaired, SD_TWO_VAL, "gmb_ttestpaired" },
    { GMB_TOK_Chi2Test, SD_TWO_VAL, "gmb_chi2" },
    { GMB_TOK_KolmogorovSmirnov, SD_TWO_VAL, "gmb_ks" },
    { GMB_TOK_TTestOneSample, SD_ONE_VAL_MU, "gmb_ttestonesample" },
    { GMB_TOK_StatsAnova, SD_ONE_VAL, "gmb_anova" },
    { GMB_TOK_FisherExact, SD_FOUR_INT, "gmb_fisher" },
    { GMB_TOK_ZTest, SD_VAL_MU_SIG, "gmb_ztest" },
    { GMB_TOK_ShapiroWilk, SD_ONE_VAL, "gmb_shapiro" },
    { GMB_TOK_NormalTest, SD_ONE_VAL, "gmb_normaltest" },
    { GMB_TOK_StatsPearsonR, SD_TWO_FLAT, "gmb_pearsonr_flat" },
    { GMB_TOK_StatsSpearmanR, SD_TWO_FLAT, "gmb_spearmanr_flat" },
    { GMB_TOK_StatsKendallTau, SD_TWO_FLAT, "gmb_kendalltau_flat" },
    { GMB_TOK_StatsFTest, SD_TWO_FLAT, "gmb_ftest_flat" },
    { GMB_TOK_StatsKruskal, SD_ONE_VAL, "gmb_kruskal" },
    { GMB_TOK_TsGranger, SD_TWO_FLAT_LONG, "gmb_ts_granger_flat" },
    { GMB_TOK_TsGarch11, SD_ONE_FLAT, "gmb_ts_garch11_flat" },
    { GMB_TOK_TsOuFit, SD_ONE_FLAT_DOUBLE, "gmb_ts_ou_fit_flat" },
    { GMB_TOK_StatsLinRegress, SD_TWO_FLAT, "gmb_linregress_flat" },
    { GMB_TOK_StatsMannWhitney, SD_TWO_FLAT, "gmb_mannwhitney_flat" },
    { GMB_TOK_StatsWilcoxon, SD_TWO_FLAT, "gmb_wilcoxon_flat" },
    { GMB_TOK_StatsSkewTest, SD_ONE_FLAT, "gmb_skewtest_flat" },
    { GMB_TOK_StatsKurtosisTest, SD_ONE_FLAT, "gmb_kurtosistest_flat" },
    { GMB_TOK_StatsJarqueBera, SD_ONE_FLAT, "gmb_jarquebera_flat" },
    { GMB_TOK_StatsDescribe, SD_ONE_FLAT, "gmb_describe_flat" },
    { GMB_TOK_StatsChi2Contingency, SD_CHI2CON, "gmb_chi2con_jagged" },
};

int emit_bf_stats_dict(ctx_t* c, const char* name, const char* ns, gmb_tokenkind_t tk,
                       gmb_expr_t** a, int n, int line) {
    (void)name; (void)ns;
    const sd_row* row = NULL;
    for (size_t i = 0; i < sizeof(SD_TBL) / sizeof(SD_TBL[0]); i++)
        if (SD_TBL[i].tk == tk) { row = &SD_TBL[i]; break; }
    if (!row) return 0;
    var_t *x, *y;
    switch (row->kind) {
    case SD_TWO_VAL:
        if (n != 2 || !sd_var(c, a[0], &x) || !sd_var(c, a[1], &y)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        sd_vallist(c, x); sb_put(&c->out, ", "); sd_vallist(c, y); sb_put(&c->out, ")");
        return 1;
    case SD_ONE_VAL:
        if (n != 1) return 0;
        /* ANOVA su matrice jagged (DIM groups(k) AS ARRAY OF DOUBLE): righe uniformi →
         * sizes tutte uguali a groups__cols (stesso schema del ramo STATS.Anova). */
        if ((tk == GMB_TOK_StatsAnova || tk == GMB_TOK_StatsKruskal) && a[0] && a[0]->kind == GMB_EXPR_VAR && a[0]->var) {
            var_t* jv = lookup_var(c, a[0]->var);
            if (jv && jv->jagged) {
                sb_put(&c->out, "({ int __asz[256]; int __an = (int)(");
                sb_bound_var(&c->out, jv, 0);
                sb_put(&c->out, " + 1); for (int __ai = 0; __ai < __an && __ai < 256; __ai++) __asz[__ai] = (int)(");
                emit_ident(c, jv->name); sb_put(&c->out, "__cols); "); sb_put(&c->out, tk == GMB_TOK_StatsKruskal ? "gmb_kruskal_jagged" : "gmb_anova_jagged"); sb_put(&c->out, "((const double* const*)");
                emit_var_ref(c, jv);
                sb_put(&c->out, ", __asz, __an); })");
                return 1;
            }
        }
        if (!sd_var(c, a[0], &x)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "("); sd_vallist(c, x); sb_put(&c->out, ")");
        return 1;
    case SD_ONE_VAL_MU:
        if (n != 2 || !sd_var(c, a[0], &x)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "("); sd_vallist(c, x);
        sb_put(&c->out, ", gmb_make_val_double((double)("); emit_expr(c, a[1]); sb_put(&c->out, ")))");
        return 1;
    case SD_FOUR_INT:
        if (n != 4) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "((gmb_int_t)("); emit_expr(c, a[0]);
        for (int i = 1; i < 4; i++) { sb_put(&c->out, "), (gmb_int_t)("); emit_expr(c, a[i]); }
        sb_put(&c->out, "))");
        return 1;
    case SD_VAL_MU_SIG:
        if (n != 3 || !sd_var(c, a[0], &x)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "("); sd_vallist(c, x);
        sb_put(&c->out, ", gmb_make_val_double((double)("); emit_expr(c, a[1]);
        sb_put(&c->out, ")), gmb_make_val_double((double)("); emit_expr(c, a[2]); sb_put(&c->out, ")))");
        return 1;
    case SD_TWO_FLAT:
        if (n != 2 || !sd_var(c, a[0], &x) || !sd_var(c, a[1], &y)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        sd_flat(c, x); sb_put(&c->out, ", "); sd_flat(c, y); sb_put(&c->out, ")");
        return 1;
    case SD_TWO_FLAT_LONG:
        if (n != 3 || !sd_var(c, a[0], &x) || !sd_var(c, a[1], &y)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "(");
        sd_flat(c, x); sb_put(&c->out, ", "); sd_flat(c, y);
        sb_put(&c->out, ", (int)("); emit_expr(c, a[2]); sb_put(&c->out, "))");
        return 1;
    case SD_ONE_FLAT:
        if (n != 1 || !sd_var(c, a[0], &x)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "("); sd_flat(c, x); sb_put(&c->out, ")");
        return 1;
    case SD_ONE_FLAT_DOUBLE:
        if (n != 2 || !sd_var(c, a[0], &x)) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "("); sd_flat(c, x);
        sb_put(&c->out, ", "); emit_expr(c, a[1]); sb_put(&c->out, ")");
        return 1;
    case SD_CHI2CON:
        if (n != 1 || !a[0] || a[0]->kind != GMB_EXPR_VAR || !a[0]->var) return 0;
        x = lookup_var(c, a[0]->var);
        if (!x || !x->is_array || x->matrix || x->vclass[0]) return 0;
        sb_put(&c->out, row->fn); sb_put(&c->out, "((const double**)");
        emit_var_ref(c, x);
        sb_put(&c->out, ", (int)(");
        if (x->jagged) { sb_bound_var(&c->out, x, 0); sb_put(&c->out, " + 1"); }
        else emit_arr_len_of(c, x);
        sb_put(&c->out, "), 2)");
        return 1;
    }
    return 0;
}
