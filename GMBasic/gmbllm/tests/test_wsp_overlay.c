/* test_wsp_overlay.c — selfcheck dell'overlay WSP (W6). Sintetico (fixture F32) + conversioni dtype
 * + opzionale modello reale. Uso: test_wsp_overlay [dir_modello_bf16 out_dir_base] */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_analyze.h"
#include "llmwsp_fixture.h"
#include "llmwsp_overlay.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned long long rs = 0x1234567ULL;
static double rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return ((double)(rs >> 11) / 9007199254740992.0) * 2 - 1; }

static void conversions(void) {
    double in[6] = {1.5, -2.0, 0.15625, 1.0 + 1.0 / 256, 1.0 + 3.0 / 512, 0}, out[6]; uint16_t raw[6];
    llmwsp_encode_f64("BF16", in, 6, raw); llmwsp_decode_f64("BF16", raw, 6, out);
    CHECK(out[0] == 1.5 && out[1] == -2.0 && out[2] == 0.15625, "bf16 esatti");
    CHECK(out[3] == 1.0, "bf16: meta' ulp -> pari (1.0), ottenuto %.10g", out[3]);
    CHECK(out[4] == 1.0 + 1.0 / 128, "bf16: sopra la meta' -> arrotonda su, ottenuto %.10g", out[4]);
    double h[6] = {1.0 + 1.0 / 2048, 65504.0, 1e6, 5.960464477539063e-08, -0.333251953125, 3.0}, ho[6];
    llmwsp_encode_f64("F16", h, 6, raw); llmwsp_decode_f64("F16", raw, 6, ho);
    CHECK(ho[0] == 1.0 && ho[1] == 65504.0 && isinf(ho[2]) && fabs(ho[3] - 5.960464477539063e-08) < 1e-15 && fabs(ho[4] + 0.333251953125) < 1e-9 && ho[5] == 3.0, "f16: %g %g %g %g %g", ho[0], ho[1], ho[2], ho[3], ho[4]);
    CHECK(llmwsp_encode_f64("U32", in, 1, raw) < 0, "dtype non supportato");
}

static void same_sha(llmwsp_overlay *a, const llmwsp_store *sa, llmwsp_overlay *b, const char *what) {
    for (size_t i = 0; i < llmwsp_store_count(sa); i++) {
        const char *nm = llmwsp_store_at(sa, i)->name; char ha[65], hb[65];
        llmwsp_ov_tensor_sha256(a, nm, ha); llmwsp_ov_tensor_sha256(b, nm, hb);
        if (strcmp(ha, hb) != 0) { CHECK(0, "%s: sha diverso su %s", what, nm); return; }
    }
}

static double colnorm(const double *m, size_t R, size_t C, size_t j) { double a = 0; for (size_t i = 0; i < R; i++) a += m[i * C + j] * m[i * C + j]; return sqrt(a); }

static void synthetic(void) {
    char dir[] = "/tmp/wsp_ov_XXXXXX", out1[] = "/tmp/wsp_ov_o1_XXXXXX", out2[] = "/tmp/wsp_ov_o2_XXXXXX", p[400], err[400];
    if (!mkdtemp(dir) || !mkdtemp(out1) || !mkdtemp(out2)) { fails++; return; }
    llmwsp_fx_truth tr;
    CHECK(llmwsp_fixture_compile("induction", dir, 7, &tr, err, sizeof err) == 0, "fixture: %s", err);
    llmwsp_store *s; llmwsp_doc *d;
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open");
    snprintf(p, sizeof p, "%s/config.json", dir);
    CHECK(llmwsp_doc_build(s, p, &d, err, sizeof err) == 0, "doc");
    llmwsp_overlay *o = llmwsp_ov_new(s, d, dir), *fresh = llmwsp_ov_new(s, d, dir);
    const char *O1 = "layers.0.attn.o.heads.1", *OT = "model.layers.0.self_attn.o_proj.weight";
    char h0[65], h1[65], h2[65];
    llmwsp_ov_tensor_sha256(o, OT, h0);

    /* --- SCALE + report + undo/redo --- */
    llmwsp_ov_report r;
    CHECK(llmwsp_ov_scale(o, O1, 2.0, &r, err, sizeof err) == 0, "scale: %s", err);
    CHECK(r.total == 64 && r.touched > 0 && r.touched <= 64 && fabs(r.rel_norm - 1.0) < 1e-6, "report scale: tot=%lld touched=%lld rel=%g", (long long)r.total, (long long)r.touched, r.rel_norm);
    llmwsp_ov_tensor_sha256(o, OT, h1);
    CHECK(strcmp(h0, h1) != 0, "il tensore deve cambiare");
    const llmwsp_tensor *ot = llmwsp_store_find(s, OT);
    double *orig = malloc(16 * 16 * 8), *cur = malloc(16 * 16 * 8);
    llmwsp_load_f64(s, ot, 0, 0, 16, orig, 256);
    CHECK(llmwsp_ov_undo(o) == 0, "undo");
    llmwsp_ov_tensor_sha256(o, OT, h2);
    CHECK(strcmp(h0, h2) == 0, "undo deve ripristinare i byte");
    CHECK(llmwsp_ov_redo(o) == 0, "redo");
    llmwsp_ov_tensor_sha256(o, OT, h2);
    CHECK(strcmp(h1, h2) == 0, "redo deve riapplicare");
    CHECK(llmwsp_ov_undo(o) == 0 && llmwsp_ov_journal_pos(o) == 0 && llmwsp_ov_journal_len(o) == 1, "pos/len dopo undo");
    CHECK(llmwsp_ov_zero(o, "layers.0.attn.o.heads.3", NULL, err, sizeof err) == 0 && llmwsp_ov_journal_len(o) == 1, "nuova edit tronca il redo");
    CHECK(llmwsp_ov_redo(o) < 0, "redo vuoto");
    llmwsp_ov_undo(o);

    /* --- PROJECTOUT: semplice, norm-preserving, multi-dim, harmless --- */
    double dirs[4 * 16]; for (int i = 0; i < 4 * 16; i++) dirs[i] = rnd();
    const char *PT = "layers.1.attn.o.heads.2";
    const llmwsp_tensor *pt = llmwsp_store_find(s, "model.layers.1.self_attn.o_proj.weight");
    double W0[16 * 4], W1[16 * 4];
    { const llmwsp_tensor *t; int ax; int64_t a, b; llmwsp_doc_resolve(d, PT, &t, &ax, &a, &b); llmwsp_load_f64(s, t, ax, a, b, W0, 64); }
    /* simple */
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 1, 16, 0, 0, &r, err, sizeof err) == 0, "proj: %s", err);
    /* leggo il risultato materializzando in out1 e rileggendo */
    {
        llmwsp_mat_stats st; llmwsp_ov_materialize(o, out1, &st, err, sizeof err);
        llmwsp_store *s1; llmwsp_store_open(out1, &s1, err, sizeof err);
        const llmwsp_tensor *t1 = llmwsp_store_find(s1, pt->name);
        llmwsp_load_f64(s1, t1, 1, 8, 12, W1, 64);   /* testa 2 = colonne 8..11 */
        double nn = 0; for (int i = 0; i < 16; i++) nn += dirs[i] * dirs[i]; nn = sqrt(nn);
        double worst = 0;
        for (int j = 0; j < 4; j++) { double dp = 0; for (int i = 0; i < 16; i++) dp += W1[i * 4 + j] * dirs[i] / nn; worst = fmax(worst, fabs(dp)); }
        CHECK(worst < 1e-6, "proiezione: componente residua %g", worst);
        llmwsp_store_close(s1);
    }
    CHECK(llmwsp_ov_undo(o) == 0, "undo proj");
    /* norm-preserving: norme delle colonne conservate */
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 1, 16, 0, LLMWSP_PROJ_NORMPRESERVING, &r, err, sizeof err) == 0, "proj np");
    {
        char t2[] = "/tmp/wsp_ov_o3_XXXXXX"; mkdtemp(t2); llmwsp_mat_stats st; llmwsp_ov_materialize(o, t2, &st, err, sizeof err);
        llmwsp_store *s1; llmwsp_store_open(t2, &s1, err, sizeof err);
        llmwsp_load_f64(s1, llmwsp_store_find(s1, pt->name), 1, 8, 12, W1, 64);
        double worst = 0; for (int j = 0; j < 4; j++) worst = fmax(worst, fabs(colnorm(W1, 16, 4, (size_t)j) - colnorm(W0, 16, 4, (size_t)j)) / colnorm(W0, 16, 4, (size_t)j));
        CHECK(worst < 1e-6, "norme colonna non conservate: %g", worst);
        llmwsp_store_close(s1);
    }
    llmwsp_ov_undo(o);
    /* multi-dim (2 direzioni) e harmless */
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 2, 16, 0, 0, &r, err, sizeof err) == 0, "proj 2d");
    llmwsp_ov_undo(o);
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 2, 16, 0, LLMWSP_PROJ_HARMLESS, &r, err, sizeof err) == 0, "proj harmless: %s", err);
    llmwsp_ov_undo(o);
    /* rifiuti */
    double dep[32]; memcpy(dep, dirs, 16 * 8); memcpy(dep + 16, dirs, 16 * 8);
    CHECK(llmwsp_ov_projectout(o, PT, dep, 2, 16, 0, 0, NULL, err, sizeof err) < 0, "direzioni dipendenti rifiutate");
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 1, 15, 0, 0, NULL, err, sizeof err) < 0, "lunghezza errata rifiutata");
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 1, 16, 1, 0, NULL, err, sizeof err) < 0, "asse errato rifiutato (le righe hanno 4 elementi)");
    size_t jl = llmwsp_ov_journal_len(o);
    double bad[64]; for (int i = 0; i < 64; i++) bad[i] = i; bad[10] = NAN;
    CHECK(llmwsp_ov_set(o, O1, bad, 64, NULL, err, sizeof err) < 0 && llmwsp_ov_journal_len(o) == jl, "NaN rifiutato senza effetti");
    CHECK(llmwsp_ov_set(o, O1, bad, 10, NULL, err, sizeof err) < 0, "dimensione errata");
    CHECK(llmwsp_ov_scale(o, "layers.9.attn.o.heads.1", 2, NULL, err, sizeof err) < 0, "indirizzo ignoto");
    CHECK(llmwsp_ov_scale(o, O1, INFINITY, NULL, err, sizeof err) < 0, "inf rifiutato");
    CHECK(llmwsp_ov_scale(o, "@0:0:99:model.norm.weight", 2, NULL, err, sizeof err) < 0, "slice fuori limiti");
    /* q_proj: righe come vettori (vec_axis=1) */
    CHECK(llmwsp_ov_projectout(o, "layers.0.attn.q.heads.0", dirs, 1, 16, 1, 0, &r, err, sizeof err) == 0, "proj righe: %s", err);
    llmwsp_ov_undo(o);

    /* --- storia mista, materialize, recipe/replay --- */
    double sv[64]; for (int i = 0; i < 64; i++) sv[i] = 0.01 * (i + 1);
    CHECK(llmwsp_ov_scale(o, O1, 0.5, NULL, err, sizeof err) == 0, "h scale");
    CHECK(llmwsp_ov_zero(o, "layers.0.attn.o.heads.3", NULL, err, sizeof err) == 0, "h zero");
    CHECK(llmwsp_ov_set(o, "layers.1.attn.o.heads.0", sv, 64, NULL, err, sizeof err) == 0, "h set");
    CHECK(llmwsp_ov_adddelta(o, "layers.1.attn.o.heads.0", sv, 64, NULL, err, sizeof err) == 0, "h add");
    CHECK(llmwsp_ov_copy(o, "layers.0.attn.q.heads.2", "layers.0.attn.q.heads.0", NULL, err, sizeof err) == 0, "h copy");
    CHECK(llmwsp_ov_projectout(o, PT, dirs, 2, 16, 0, LLMWSP_PROJ_NORMPRESERVING, NULL, err, sizeof err) == 0, "h proj");
    CHECK(llmwsp_ov_projectout(o, "layers.0.attn.v.kv_heads.1", dirs, 1, 16, 1, 0, NULL, err, sizeof err) == 0, "h proj righe");
    CHECK(llmwsp_ov_scale(o, "layers.0.attn.o.heads.0", 3.0, NULL, err, sizeof err) == 0 && llmwsp_ov_undo(o) == 0, "edit annullata fuori dalla ricetta");
    char rd[] = "/tmp/wsp_ov_rc_XXXXXX"; mkdtemp(rd);
    CHECK(llmwsp_ov_recipe(o, rd, err, sizeof err) == 0, "recipe: %s", err);
    CHECK(llmwsp_ov_replay(fresh, rd, err, sizeof err) == 0, "replay: %s", err);
    same_sha(o, s, fresh, "replay");
    { llmwsp_mat_stats st1; llmwsp_ov_materialize(o, out2, &st1, err, sizeof err);
      CHECK(st1.edited_tensors >= 4, "tensori modificati: %d", st1.edited_tensors); }

    /* --- diff --- */
    llmwsp_ov_diffent dd[16]; size_t nd;
    CHECK(llmwsp_ov_diff(o, dd, 16, &nd) == 0 && nd >= 4, "diff: %zu tensori", nd);
    for (size_t i = 0; i < nd; i++) if (!strcmp(dd[i].tensor, OT)) CHECK(dd[i].changed > 0 && dd[i].changed <= 16 * 8 && dd[i].c0 >= 4 && dd[i].c1 <= 16, "diff bbox o_proj L0: c=[%lld,%lld) n=%lld", (long long)dd[i].c0, (long long)dd[i].c1, (long long)dd[i].changed);

    /* --- patch sparsa: più piccola del tensore intero e riproduce lo stesso stato --- */
    llmwsp_overlay *single = llmwsp_ov_new(s, d, dir), *applied = llmwsp_ov_new(s, d, dir);
    llmwsp_ov_scale(single, "layers.1.attn.o.heads.0", 2.0, NULL, err, sizeof err);
    char pp[400]; snprintf(pp, sizeof pp, "%s/p.wsppatch", rd); size_t pb = 0;
    CHECK(llmwsp_ov_export_patch(single, pp, &pb, err, sizeof err) == 0, "export patch");
    const llmwsp_tensor *tt = llmwsp_store_find(s, "model.layers.1.self_attn.o_proj.weight");
    CHECK(pb > 0 && pb < tt->nbytes, "patch %zu byte vs tensore %llu", pb, (unsigned long long)tt->nbytes);
    CHECK(llmwsp_ov_apply_patch(applied, pp, err, sizeof err) == 0, "apply patch: %s", err);
    same_sha(single, s, applied, "patch");
    /* tutto annullato -> nessuno shard riscritto */
    while (llmwsp_ov_undo(single) == 0) {}
    { llmwsp_mat_stats st; char o4[] = "/tmp/wsp_ov_o4_XXXXXX"; mkdtemp(o4);
      CHECK(llmwsp_ov_materialize(single, o4, &st, err, sizeof err) == 0 && st.shards_rewritten == 0 && st.edited_tensors == 0, "dopo undo totale nulla da riscrivere"); }
    llmwsp_ov_free(single); llmwsp_ov_free(applied);
    llmwsp_ov_free(o); llmwsp_ov_free(fresh); llmwsp_doc_free(d); llmwsp_store_close(s);
    free(orig); free(cur);
}

static void real(const char *dir, const char *outbase) {
    char p[1024], err[400], tmp[1100];
    llmwsp_store *s; llmwsp_doc *d;
    if (llmwsp_store_open(dir, &s, err, sizeof err) < 0) { CHECK(0, "open %s", err); return; }
    snprintf(p, sizeof p, "%s/config.json", dir);
    if (llmwsp_doc_build(s, p, &d, err, sizeof err) < 0) { CHECK(0, "doc"); return; }
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    llmwsp_overlay *o = llmwsp_ov_new(s, d, dir), *fresh = llmwsp_ov_new(s, d, dir);
    llmwsp_ov_report r; clock_t t0 = clock();
    CHECK(llmwsp_ov_scale(o, "layers.5.attn.o.heads.7", 0.5, &r, err, sizeof err) == 0, "scale: %s", err);
    printf("SCALE 0.5 layers.5.attn.o.heads.7: toccati %lld/%lld (%.4f%%) rel_norm=%.4f\n", (long long)r.touched, (long long)r.total, 100.0 * r.touched / r.total, r.rel_norm);
    double *dir16 = malloc((size_t)g.hidden * 8); for (int64_t i = 0; i < g.hidden; i++) dir16[i] = rnd();
    CHECK(llmwsp_ov_projectout(o, "layers.5.mlp.down", dir16, 1, (size_t)g.hidden, 0, LLMWSP_PROJ_NORMPRESERVING, &r, err, sizeof err) == 0, "proj: %s", err);
    printf("PROJECTOUT (norm-preserving) layers.5.mlp.down: toccati %lld/%lld (%.2f%%) rel_norm=%.4f max_abs=%.3g\n", (long long)r.touched, (long long)r.total, 100.0 * r.touched / r.total, r.rel_norm, r.max_abs);
    snprintf(tmp, sizeof tmp, "%s_recipe", outbase);
    CHECK(llmwsp_ov_recipe(o, tmp, err, sizeof err) == 0 && llmwsp_ov_replay(fresh, tmp, err, sizeof err) == 0, "recipe/replay: %s", err);
    const char *tn[2] = {"model.layers.5.self_attn.o_proj.weight", "model.layers.5.mlp.down_proj.weight"};
    for (int i = 0; i < 2; i++) { char a[65], b[65]; llmwsp_ov_tensor_sha256(o, tn[i], a); llmwsp_ov_tensor_sha256(fresh, tn[i], b); CHECK(!strcmp(a, b), "REPLAY != overlay su %s", tn[i]); }
    snprintf(tmp, sizeof tmp, "%s_mat", outbase);
    llmwsp_mat_stats st;
    CHECK(llmwsp_ov_materialize(o, tmp, &st, err, sizeof err) == 0, "materialize: %s", err);
    printf("materialize: cloni=%d riscritti=%d tensori modificati=%d\n", st.shards_cloned, st.shards_rewritten, st.edited_tensors);
    llmwsp_store *so;
    CHECK(llmwsp_store_open(tmp, &so, err, sizeof err) == 0, "riapertura");
    if (so) {
        for (int i = 0; i < 2; i++) { char a[65], b[65]; llmwsp_ov_tensor_sha256(o, tn[i], a); llmwsp_tensor_sha256(so, llmwsp_store_find(so, tn[i]), b); CHECK(!strcmp(a, b), "materializzato != overlay su %s", tn[i]); }
        CHECK(llmwsp_store_count(so) == llmwsp_store_count(s), "numero tensori");
        llmwsp_store_close(so);
    }
    printf("tempo: %.1f s\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    free(dir16); llmwsp_ov_free(o); llmwsp_ov_free(fresh); llmwsp_doc_free(d); llmwsp_store_close(s);
}

int main(int argc, char **argv) {
    conversions();
    synthetic();
    if (argc > 2) real(argv[1], argv[2]);
    printf(fails ? "WSP OVERLAY: %d FALLITI\n" : "WSP OVERLAY: OK\n", fails);
    return fails != 0;
}
