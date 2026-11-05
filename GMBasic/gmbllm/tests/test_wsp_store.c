/* test_wsp_store.c — selfcheck del Weight Store (W1 WSP). Oracolo sintetico + opzionale modello reale.
 * Uso:  test_wsp_store [dir_modello [n_tensori_attesi [byte_totali_attesi]]]  */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void write_shard(const char *path, const char *hdr, const unsigned char *data, size_t n) {
    FILE *f = fopen(path, "wb");
    uint64_t hl = strlen(hdr);
    unsigned char lb[8];
    for (int i = 0; i < 8; i++) lb[i] = (unsigned char)(hl >> (8 * i));
    fwrite(lb, 1, 8, f); fwrite(hdr, 1, hl, f); fwrite(data, 1, n, f);
    fclose(f);
}

static long rss_kb(void) {
    struct rusage u; getrusage(RUSAGE_SELF, &u);
#ifdef __APPLE__
    return (long)(u.ru_maxrss / 1024);
#else
    return (long)u.ru_maxrss;
#endif
}

static void synthetic(void) {
    char dir[] = "/tmp/wsp_store_XXXXXX";
    if (!mkdtemp(dir)) { fails++; return; }
    /* shard 1: A (BF16 4x3 = 24B), msg (U8 3B "abc") ; shard 2: B (F32 2x2 = 16B), s (F32 scalare 4B) */
    unsigned char d1[27], d2[20];
    for (int i = 0; i < 24; i++) d1[i] = (unsigned char)(i + 1);
    memcpy(d1 + 24, "abc", 3);
    for (int i = 0; i < 20; i++) d2[i] = (unsigned char)(100 + i);
    const char *h1 = "{\"__metadata__\":{\"format\":\"pt\"},\"A\":{\"dtype\":\"BF16\",\"shape\":[4,3],\"data_offsets\":[0,24]},"
                     "\"msg\":{\"dtype\":\"U8\",\"shape\":[3],\"data_offsets\":[24,27]}}";
    const char *h2 = "{\"B\":{\"dtype\":\"F32\",\"shape\":[2,2],\"data_offsets\":[0,16]},"
                     "\"s\":{\"dtype\":\"F32\",\"shape\":[],\"data_offsets\":[16,20]}}";
    char p[256];
    snprintf(p, sizeof p, "%s/model-00001-of-00002.safetensors", dir); write_shard(p, h1, d1, 27);
    snprintf(p, sizeof p, "%s/model-00002-of-00002.safetensors", dir); write_shard(p, h2, d2, 20);

    llmwsp_store *s; char err[256];
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open: %s", err);
    if (!s) return;
    CHECK(llmwsp_store_count(s) == 4, "count %zu", llmwsp_store_count(s));
    CHECK(llmwsp_store_total_bytes(s) == 47, "total");
    CHECK(llmwsp_store_file_count(s) == 2, "files");
    CHECK(llmwsp_store_check_coverage(s, err, sizeof err) == 0, "coverage: %s", err);
    const llmwsp_tensor *A = llmwsp_store_find(s, "A"), *B = llmwsp_store_find(s, "B"), *m = llmwsp_store_find(s, "msg");
    CHECK(A && B && m && !llmwsp_store_find(s, "zz"), "find");
    if (A && B && m) {
        CHECK(A->ndim == 2 && A->shape[0] == 4 && A->shape[1] == 3 && A->file == 0, "A meta");
        CHECK(B->file == 1 && B->nbytes == 16, "B meta");
        unsigned char buf[32];
        /* righe 1..3 di A: byte 6..17 */
        CHECK(llmwsp_read_rows(s, A, 1, 3, buf, sizeof buf) == 12, "rows n");
        CHECK(memcmp(buf, d1 + 6, 12) == 0, "rows content");
        /* colonna 1 di A (BF16, 2B): per ogni riga r, byte (r*3+1)*2 */
        CHECK(llmwsp_read_cols(s, A, 1, 2, buf, sizeof buf) == 8, "cols n");
        for (int r = 0; r < 4; r++) CHECK(memcmp(buf + 2 * r, d1 + (r * 3 + 1) * 2, 2) == 0, "col row %d", r);
        CHECK(llmwsp_read_rows(s, A, 0, 5, buf, sizeof buf) < 0, "rows oltre");
        CHECK(llmwsp_read_rows(s, A, 0, 4, buf, 4) < 0, "buffer piccolo");
        CHECK(llmwsp_read_cols(s, B, 0, 3, buf, sizeof buf) < 0, "cols oltre");
        char hex[65];
        CHECK(llmwsp_tensor_sha256(s, m, hex) == 0 &&
              strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "sha256(abc)=%s", hex);
    }
    llmwsp_store_close(s);

    /* shard corrotto: data_offsets oltre la fine */
    const char *bad = "{\"X\":{\"dtype\":\"U8\",\"shape\":[9],\"data_offsets\":[0,9]}}";
    snprintf(p, sizeof p, "%s/bad.safetensors", dir); write_shard(p, bad, d2, 4);
    llmwsp_store *s2 = NULL;
    CHECK(llmwsp_store_open(p, &s2, err, sizeof err) < 0 && s2 == NULL && err[0], "bad deve fallire (%s)", err);
    /* forma incoerente col dtype */
    const char *bad2 = "{\"X\":{\"dtype\":\"F32\",\"shape\":[3],\"data_offsets\":[0,4]}}";
    write_shard(p, bad2, d2, 4);
    CHECK(llmwsp_store_open(p, &s2, err, sizeof err) < 0, "forma incoerente deve fallire");
    /* buco: due tensori con 1 byte non coperto */
    const char *gap = "{\"X\":{\"dtype\":\"U8\",\"shape\":[1],\"data_offsets\":[0,1]},\"Y\":{\"dtype\":\"U8\",\"shape\":[1],\"data_offsets\":[2,3]}}";
    write_shard(p, gap, d2, 3);
    CHECK(llmwsp_store_open(p, &s2, err, sizeof err) == 0, "gap open");
    if (s2) { CHECK(llmwsp_store_check_coverage(s2, err, sizeof err) == 1, "gap violazioni"); llmwsp_store_close(s2); }
}

static void real_model(const char *dir, long want_n, long long want_bytes) {
    llmwsp_store *s; char err[256];
    long r0 = rss_kb();
    CHECK(llmwsp_store_open(dir, &s, err, sizeof err) == 0, "open reale: %s", err);
    if (!s) return;
    printf("modello: %zu tensori, %d shard, %llu byte\n", llmwsp_store_count(s), llmwsp_store_file_count(s),
           (unsigned long long)llmwsp_store_total_bytes(s));
    if (want_n) CHECK((long)llmwsp_store_count(s) == want_n, "tensori attesi %ld", want_n);
    if (want_bytes) CHECK((long long)llmwsp_store_total_bytes(s) == want_bytes, "byte attesi %lld", want_bytes);
    int viol = llmwsp_store_check_coverage(s, err, sizeof err);
    CHECK(viol == 0, "copertura: %d violazioni (%s)", viol, err);
    const llmwsp_tensor *lm = llmwsp_store_find(s, "lm_head.weight");
    if (!lm) lm = llmwsp_store_find(s, "model.embed_tokens.weight");
    if (lm && lm->ndim == 2) {
        printf("slice su %s [%lld x %lld] %s\n", lm->name, (long long)lm->shape[0], (long long)lm->shape[1], lm->dtype);
        size_t es = llmwsp_dtype_size(lm->dtype);
        int64_t a = lm->shape[0] / 3, b = a + 64;
        size_t rb = (size_t)lm->shape[1] * es, n = 64 * rb;
        unsigned char *x = (unsigned char *)malloc(n), *y = (unsigned char *)malloc(n);
        CHECK(llmwsp_read_rows(s, lm, a, b, x, n) == (int64_t)n, "read_rows reale");
        /* oracolo indipendente: fopen/fseek sul file shard */
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, llmwsp_store_file_name(s, lm->file));
        FILE *f = fopen(path, "rb");
        if (f) {
            fseeko(f, (off_t)(lm->offset + (uint64_t)a * rb), SEEK_SET);
            size_t got = fread(y, 1, n, f); fclose(f);
            CHECK(got == n && memcmp(x, y, n) == 0, "slice != oracolo fseek");
        }
        /* colonne: 8 colonne di tutte le righe */
        size_t cn = (size_t)lm->shape[0] * 8 * es;
        unsigned char *c = (unsigned char *)malloc(cn);
        CHECK(llmwsp_read_cols(s, lm, 100, 108, c, cn) == (int64_t)cn, "read_cols reale");
        if (c && f == NULL) {}
        FILE *g = fopen(path, "rb");
        if (g) {
            unsigned char tmp[64];
            for (int64_t r = 0; r < lm->shape[0]; r += lm->shape[0] / 7 + 1) {
                fseeko(g, (off_t)(lm->offset + ((uint64_t)r * (uint64_t)lm->shape[1] + 100) * es), SEEK_SET);
                size_t got = fread(tmp, 1, 8 * es, g);
                CHECK(got == 8 * es && memcmp(tmp, c + (size_t)r * 8 * es, 8 * es) == 0, "cols riga %lld", (long long)r);
            }
            fclose(g);
        }
        free(x); free(y); free(c);
    }
    long r1 = rss_kb();
    printf("RSS massimo: %ld MB (prima: %ld MB)\n", r1 / 1024, r0 / 1024);
    CHECK(r1 < 200 * 1024, "RSS troppo alta");
    llmwsp_store_close(s);
}

int main(int argc, char **argv) {
    synthetic();
    if (argc > 1) real_model(argv[1], argc > 2 ? atol(argv[2]) : 0, argc > 3 ? atoll(argv[3]) : 0);
    printf(fails ? "WSP STORE: %d FALLITI\n" : "WSP STORE: OK\n", fails);
    return fails != 0;
}
