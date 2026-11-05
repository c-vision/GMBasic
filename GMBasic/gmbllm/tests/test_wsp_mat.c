/* test_wsp_mat.c — selfcheck del Materializer (W3). Sintetico + opzionale modello reale.
 * Uso: test_wsp_mat [dir_modello nome_tensore_da_modificare dir_out_base] */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_mat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void mkshard(const char *path, const llmwsp_wspec *sp, int n, unsigned char seed) {
    llmwsp_writer *w; char err[200];
    uint64_t tot = 0;
    for (int i = 0; i < n; i++) { uint64_t e = 1; for (int d = 0; d < sp[i].ndim; d++) e *= (uint64_t)sp[i].shape[d]; tot += e * llmwsp_dtype_size(sp[i].dtype); }
    unsigned char *b = (unsigned char *)malloc(tot ? tot : 1);
    for (uint64_t i = 0; i < tot; i++) b[i] = (unsigned char)(seed + i * 7);
    if (llmwsp_writer_open(path, sp, (size_t)n, &w, err, sizeof err) < 0) { fails++; free(b); return; }
    llmwsp_writer_append(w, b, tot); llmwsp_writer_close(w, err, sizeof err); free(b);
}

static void synthetic(void) {
    char src[] = "/tmp/wsp_mat_src_XXXXXX", out1[] = "/tmp/wsp_mat_o1_XXXXXX", out2[] = "/tmp/wsp_mat_o2_XXXXXX", p[300], err[300];
    if (!mkdtemp(src) || !mkdtemp(out1) || !mkdtemp(out2)) { fails++; return; }
    llmwsp_wspec a[2] = {{"w1", "BF16", 2, {4, 4}}, {"w2", "F32", 1, {5}}};
    llmwsp_wspec b[1] = {{"w3", "U8", 2, {3, 7}}};
    snprintf(p, sizeof p, "%s/model-00001-of-00002.safetensors", src); mkshard(p, a, 2, 1);
    snprintf(p, sizeof p, "%s/model-00002-of-00002.safetensors", src); mkshard(p, b, 1, 9);
    snprintf(p, sizeof p, "%s/config.json", src); FILE *f = fopen(p, "wb"); fputs("{\"x\":1}", f); fclose(f);
    llmwsp_store *s; CHECK(llmwsp_store_open(src, &s, err, sizeof err) == 0, "open: %s", err);
    if (!s) return;
    llmwsp_mat_stats st;
    /* 1) nessuna modifica: tutto clonato, tensor-identico (outdir è vuota ma esiste) */
    CHECK(llmwsp_materialize(s, src, NULL, 0, out1, &st, err, sizeof err) == 0, "mat0: %s", err);
    CHECK(st.shards_cloned == 2 && st.shards_rewritten == 0 && st.files_copied == 1, "stats0 %d %d %d", st.shards_cloned, st.shards_rewritten, st.files_copied);
    CHECK(llmwsp_materialize_verify(s, NULL, 0, out1, err, sizeof err) == 0, "verify0: %s", err);
    /* 2) una modifica su w2 (shard 1): si riscrive solo quello */
    float nv[5] = {1, 2, 3, 4, 5};
    llmwsp_edit e = {"w2", nv, sizeof nv};
    CHECK(llmwsp_materialize(s, src, &e, 1, out2, &st, err, sizeof err) == 0, "mat1: %s", err);
    CHECK(st.shards_cloned == 1 && st.shards_rewritten == 1 && st.edited_tensors == 1, "stats1 %d %d", st.shards_cloned, st.shards_rewritten);
    CHECK(llmwsp_materialize_verify(s, &e, 1, out2, err, sizeof err) == 0, "verify1: %s", err);
    CHECK(llmwsp_materialize_verify(s, NULL, 0, out2, err, sizeof err) > 0, "senza edit il risultato deve divergere");
    snprintf(p, sizeof p, "%s/provenance.json", out2);
    FILE *pf = fopen(p, "rb"); char pb[1024] = {0}; if (pf) { fread(pb, 1, sizeof pb - 1, pf); fclose(pf); }
    CHECK(strstr(pb, "\"tensor\":\"w2\"") && strstr(pb, "sha256_before") && strstr(pb, "\"shards_rewritten\":1"), "provenance");
    /* 3) rifiuti */
    CHECK(llmwsp_materialize(s, src, &e, 1, out2, &st, err, sizeof err) < 0, "outdir non vuota rifiutata");
    llmwsp_edit bad1 = {"nope", nv, 4}, bad2 = {"w2", nv, 8}, dup[2] = {e, e};
    char o3[] = "/tmp/wsp_mat_o3_XXXXXX"; mkdtemp(o3);
    CHECK(llmwsp_materialize(s, src, &bad1, 1, o3, &st, err, sizeof err) < 0, "tensore inesistente");
    CHECK(llmwsp_materialize(s, src, &bad2, 1, o3, &st, err, sizeof err) < 0, "dimensione errata");
    CHECK(llmwsp_materialize(s, src, dup, 2, o3, &st, err, sizeof err) < 0, "edit duplicata");
    llmwsp_store_close(s);
}

static void real(const char *dir, const char *tname, const char *outbase) {
    char err[300], o1[1024], o2[1024];
    llmwsp_store *s;
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open: %s", err);
    if (!s) return;
    snprintf(o1, sizeof o1, "%s_noedit", outbase); snprintf(o2, sizeof o2, "%s_edit", outbase);
    llmwsp_mat_stats st;
    CHECK(llmwsp_materialize(s, dir, NULL, 0, o1, &st, err, sizeof err) == 0, "mat0: %s", err);
    printf("senza edit: cloni=%d riscritti=%d file=%d\n", st.shards_cloned, st.shards_rewritten, st.files_copied);
    int v = llmwsp_materialize_verify(s, NULL, 0, o1, err, sizeof err);
    CHECK(v == 0, "verify0: %d %s", v, err);
    const llmwsp_tensor *t = llmwsp_store_find(s, tname);
    CHECK(t != NULL, "tensore %s", tname);
    if (t) {
        unsigned char *buf = (unsigned char *)malloc((size_t)t->nbytes);
        llmwsp_read_bytes(s, t, 0, t->nbytes, buf);
        for (uint64_t i = 0; i < t->nbytes; i += 2) buf[i] ^= 0x01;     /* perturbazione deterministica */
        llmwsp_edit e = {tname, buf, t->nbytes};
        CHECK(llmwsp_materialize(s, dir, &e, 1, o2, &st, err, sizeof err) == 0, "mat1: %s", err);
        printf("con 1 edit (%s): cloni=%d riscritti=%d\n", tname, st.shards_cloned, st.shards_rewritten);
        CHECK(st.shards_rewritten == 1, "deve riscrivere un solo shard");
        v = llmwsp_materialize_verify(s, &e, 1, o2, err, sizeof err);
        CHECK(v == 0, "verify1: %d %s", v, err);
        free(buf);
    }
    llmwsp_store_close(s);
}

int main(int argc, char **argv) {
    synthetic();
    if (argc > 3) real(argv[1], argv[2], argv[3]);
    printf(fails ? "WSP MAT: %d FALLITI\n" : "WSP MAT: OK\n", fails);
    return fails != 0;
}
