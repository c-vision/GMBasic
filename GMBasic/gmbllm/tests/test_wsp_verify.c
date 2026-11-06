/* test_wsp_verify.c — selfcheck di W8: verifica, gate, catena di provenienza, checkpoint/rollback, riepilogo. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_fixture.h"
#include "llmwsp_overlay.h"
#include "llmwsp_verify.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char *mk(const char *tag) { static char b[8][64]; static int k; char *p = b[k++ % 8]; snprintf(p, 64, "/tmp/wsp_vf_%s_XXXXXX", tag); return mkdtemp(p); }

int main(void) {
    char *A = mk("a"), *B = mk("b"), *C = mk("c"), *D = mk("d"), *E = mk("e"), err[400], p[500];
    llmwsp_fx_truth tr;
    CHECK(llmwsp_fixture_compile("induction", A, 3, &tr, err, sizeof err) == 0, "fixture A");
    llmwsp_store *sa; llmwsp_doc *da;
    CHECK(llmwsp_store_open(A, &sa, err, sizeof err) == 0, "open A");
    snprintf(p, sizeof p, "%s/config.json", A);
    CHECK(llmwsp_doc_build(sa, p, &da, err, sizeof err) == 0, "doc A");

    /* 1) base valida */
    llmwsp_verify_opts o; memset(&o, 0, sizeof o); o.check_doc = 1; o.check_finite = 1;
    llmwsp_verify_report r;
    CHECK(llmwsp_verify_dir(A, &o, &r) == 0 && r.ok && r.tensors == llmwsp_store_count(sa), "verify base: %s", r.first_error);

    /* 2) edit -> materialize -> verify contro la base; riepilogo scritto */
    llmwsp_overlay *ov = llmwsp_ov_new(sa, da, A);
    CHECK(llmwsp_ov_scale(ov, "layers.0.attn.o.heads.0", 3.0, NULL, err, sizeof err) == 0, "scale");
    llmwsp_mat_stats st;
    CHECK(llmwsp_ov_materialize(ov, B, &st, err, sizeof err) == 0, "materialize: %s", err);
    snprintf(p, sizeof p, "%s/wsp_summary.txt", B);
    { FILE *f = fopen(p, "rb"); char b[2000] = {0}; if (f) { fread(b, 1, sizeof b - 1, f); fclose(f); }
      CHECK(strstr(b, "model.layers.0.self_attn.o_proj.weight") && strstr(b, "TOTALE: 1 tensori"), "riepilogo mancante o errato: %s", b); }
    const char *allowed[1] = {"model.layers.0.self_attn.o_proj.weight"};
    o.base = sa; o.allowed_changed = allowed; o.n_allowed = 1;
    CHECK(llmwsp_verify_dir(B, &o, &r) == 0 && r.ok && r.changed == 1 && r.unexpected_changed == 0, "verify B ammesso: changed=%zu unexp=%zu %s", r.changed, r.unexpected_changed, r.first_error);
    o.n_allowed = 0;
    CHECK(llmwsp_verify_dir(B, &o, &r) == 0 && !r.ok && r.unexpected_changed == 1 && strstr(r.first_error, "non ammesso"), "verify B non ammesso: %s", r.first_error);
    o.n_allowed = 1;

    /* 3) NaN iniettato: la finitezza deve scattare */
    { const llmwsp_tensor *t = llmwsp_store_find(sa, "model.layers.1.self_attn.v_proj.weight");
      float *buf = (float *)malloc((size_t)t->nbytes); llmwsp_read_bytes(sa, t, 0, t->nbytes, buf); buf[5] = NAN; buf[9] = INFINITY;
      llmwsp_edit ed = {t->name, buf, t->nbytes};
      CHECK(llmwsp_materialize(sa, A, &ed, 1, C, &st, err, sizeof err) == 0, "materialize NaN");
      CHECK(llmwsp_verify_dir(C, &o, &r) == 0 && !r.ok && r.nonfinite_tensors == 1 && r.nonfinite_values == 2, "NaN: tensori=%zu valori=%lld", r.nonfinite_tensors, (long long)r.nonfinite_values);
      free(buf); }

    /* 4) forma diversa dalla base (altro fixture) */
    CHECK(llmwsp_fixture_compile("copy", D, 3, &tr, err, sizeof err) == 0, "fixture copy");
    CHECK(llmwsp_verify_dir(D, &o, &r) == 0 && !r.ok && r.shape_mismatch > 0, "forma diversa: %zu", r.shape_mismatch);

    /* 5) gate */
    llmwsp_gate g[3] = {{"KL", 0.02, 0.05, 1}, {"top1", 0.91, 0.90, 0}, {"PPL_ratio", 1.4, 1.2, 1}};
    char *js = NULL; int nf = llmwsp_gates_eval(g, 3, &js);
    CHECK(nf == 1 && js && strstr(js, "\"accepted\":false") && strstr(js, "\"name\":\"PPL_ratio\"") && strstr(strstr(js, "PPL_ratio"), "FAIL"), "gate: %s", js ? js : "(null)");
    free(js);
    llmwsp_gate g2[2] = {{"KL", 0.02, 0.05, 1}, {"top1", 0.95, 0.90, 0}};
    CHECK(llmwsp_gates_eval(g2, 2, &js) == 0 && strstr(js, "\"accepted\":true"), "gate ok"); free(js);
    llmwsp_gate g3[1] = {{"KL", NAN, 0.05, 1}};
    CHECK(llmwsp_gates_eval(g3, 1, NULL) == 1, "NaN deve fallire");

    /* 6) hash di modello e catena */
    llmwsp_store *sb; CHECK(llmwsp_store_open(B, &sb, err, sizeof err) == 0, "open B");
    char h1[65], h2[65], h3[65];
    CHECK(llmwsp_model_hash(sa, h1) == 0 && llmwsp_model_hash(sa, h2) == 0 && !strcmp(h1, h2), "hash non stabile");
    CHECK(llmwsp_model_hash(sb, h3) == 0 && strcmp(h1, h3) != 0, "hash deve cambiare con l'edit");
    CHECK(llmwsp_ov_materialize(llmwsp_ov_new(sa, da, A), E, &st, err, sizeof err) == 0, "clone");   /* overlay vuoto: tutto clonato */
    { llmwsp_store *se; llmwsp_store_open(E, &se, err, sizeof err); char h4[65]; llmwsp_model_hash(se, h4); CHECK(!strcmp(h1, h4), "il clone senza edit deve avere lo stesso hash"); llmwsp_store_close(se); }
    snprintf(p, sizeof p, "%s/chain.json", B);
    const char *recipe = "SCALE layers.0.attn.o.heads.0 0x1.8p+1\n";
    CHECK(llmwsp_provenance_write(p, sa, recipe, sb, "{\"accepted\":true}", err, sizeof err) == 0, "provenance");
    { FILE *f = fopen(p, "rb"); char b[3000] = {0}; if (f) { fread(b, 1, sizeof b - 1, f); fclose(f); }
      char cat[64 + 100], hops[65]; memcpy(cat, h1, 64); memcpy(cat + 64, recipe, strlen(recipe)); llmwsp_sha256_buf(cat, 64 + strlen(recipe), hops);
      CHECK(strstr(b, h1) && strstr(b, h3) && strstr(b, hops) && strstr(b, "\"stage\":\"ops\"") && strstr(b, "\\n"), "catena: %s", b); }

    /* 7) checkpoint/rollback */
    llmwsp_overlay *o2 = llmwsp_ov_new(sa, da, A); char ha[65], hb[65];
    const char *T = "model.layers.1.self_attn.o_proj.weight";
    llmwsp_ov_tensor_sha256(o2, T, ha);
    llmwsp_ov_scale(o2, "layers.1.attn.o.heads.1", 2.0, NULL, err, sizeof err);
    CHECK(llmwsp_ov_checkpoint(o2, "dopo-uno") == 0, "checkpoint");
    llmwsp_ov_scale(o2, "layers.1.attn.o.heads.2", 2.0, NULL, err, sizeof err);
    llmwsp_ov_zero(o2, "layers.1.attn.o.heads.3", NULL, err, sizeof err);
    CHECK(llmwsp_ov_journal_pos(o2) == 3, "pos");
    CHECK(llmwsp_ov_rollback(o2, "dopo-uno") == 0 && llmwsp_ov_journal_pos(o2) == 1, "rollback a dopo-uno");
    CHECK(llmwsp_ov_rollback(o2, "inesistente") < 0, "nome ignoto");
    llmwsp_ov_checkpoint(o2, "inizio-fittizio"); llmwsp_ov_undo(o2);
    CHECK(llmwsp_ov_rollback(o2, "inizio-fittizio") < 0, "rollback verso un punto oltre la posizione corrente");
    llmwsp_ov_checkpoint(o2, "zero"); /* pos 0 */
    llmwsp_ov_scale(o2, "layers.1.attn.o.heads.0", 5.0, NULL, err, sizeof err);
    CHECK(llmwsp_ov_rollback(o2, "zero") == 0, "rollback a zero");
    llmwsp_ov_tensor_sha256(o2, T, hb);
    CHECK(!strcmp(ha, hb), "rollback a zero deve ripristinare i byte");

    llmwsp_store_close(sb); llmwsp_doc_free(da); llmwsp_store_close(sa); llmwsp_ov_free(ov); llmwsp_ov_free(o2);
    printf(fails ? "WSP VERIFY: %d FALLITI\n" : "WSP VERIFY: OK\n", fails);
    return fails != 0;
}
