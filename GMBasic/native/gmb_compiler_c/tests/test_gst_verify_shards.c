/* test_gst_verify_shards.c — K7 (residuo PUL): verifica PER-SHARD del model compiler.
 *
 * Costruisce un modello 2-shard + `model.safetensors.index.json` in una dir temp e verifica
 * che ogni corruzione sia RIFIUTATA ESPLICITAMENTE (mai silenziosa):
 *   valido | shard mancante | shard troncato | nome duplicato | shape/dtype incoerenti |
 *   index con tensore extra | shard con tensore non mappato | tensore mappato a due shard.
 * Offline, deterministico, senza modello reale. */
#include "gmb_st.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++;                     \
    printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void wfile(const char* path, const char* s) {
    FILE* f = fopen(path, "wb");
    if (f) { fwrite(s, 1, strlen(s), f); fclose(f); }
}

/* Scrive uno shard con 1 o 2 tensori F32 [4] (writer canonico). */
static int write_shard(const char* dir, const char* name, const char* t0, const char* t1) {
    char path[1200]; snprintf(path, sizeof(path), "%s/%s", dir, name);
    GstWriter* w = NULL;
    if (gst_writer_create(&w, path) != 0) return -1;
    uint64_t shape[1] = { 4 }; float v[4] = { 1, 2, 3, 4 };
    if (gst_writer_add(w, t0, "F32", 1, shape, v, sizeof v) != 0) return -1;
    if (t1 != NULL && gst_writer_add(w, t1, "F32", 1, shape, v, sizeof v) != 0) return -1;
    return gst_writer_finish(w);
}

/* Scrive un safetensors "grezzo" (header JSON arbitrario + data_len byte di dati). */
static void write_raw_st(const char* path, const char* json, int data_len) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) return;
    size_t hlen = strlen(json);
    size_t pad = (8 - ((8 + hlen) % 8)) % 8;
    uint64_t h = (uint64_t)(hlen + pad);
    unsigned char lb[8];
    for (int i = 0; i < 8; ++i) lb[i] = (unsigned char)((h >> (8 * i)) & 0xFF);
    fwrite(lb, 1, 8, f);
    fwrite(json, 1, hlen, f);
    for (size_t i = 0; i < pad; ++i) fputc(' ', f);
    for (int i = 0; i < data_len; ++i) fputc(0, f);
    fclose(f);
}

#define SH1 "model-00001-of-00002.safetensors"
#define SH2 "model-00002-of-00002.safetensors"

static void reset_valid(const char* dir) {
    write_shard(dir, SH1, "w0", NULL);
    write_shard(dir, SH2, "w1", NULL);
    char p[1200]; snprintf(p, sizeof(p), "%s/model.safetensors.index.json", dir);
    wfile(p, "{\"metadata\":{},\"weight_map\":{"
             "\"w0\":\"model-00001-of-00002.safetensors\","
             "\"w1\":\"model-00002-of-00002.safetensors\"}}");
}

int main(void) {
    char tmpl[] = "/tmp/gst-shards-XXXXXX";
    char* dir = mkdtemp(tmpl);
    if (dir == NULL) { printf("test_gst_verify_shards: mkdtemp FAIL\n"); return 1; }
    char sp1[1200], sp2[1200], ip[1200];
    snprintf(sp1, sizeof(sp1), "%s/%s", dir, SH1);
    snprintf(sp2, sizeof(sp2), "%s/%s", dir, SH2);
    snprintf(ip, sizeof(ip), "%s/model.safetensors.index.json", dir);
    char shards[512][96]; int ns = 0;

    /* 1. valido */
    reset_valid(dir);
    CHECK(gst_verify_shards(dir) == 0, "valido: atteso OK");
    CHECK(gst_collect_shards(dir, shards, 512, &ns) == 1 && ns == 2, "valido: collect 2 shard");

    /* 2. shard mancante */
    reset_valid(dir); remove(sp2);
    CHECK(gst_verify_shards(dir) != 0, "shard mancante: atteso rifiuto");

    /* 3. shard troncato (header oltre il file) */
    reset_valid(dir); truncate(sp2, 8);
    CHECK(gst_verify_shards(dir) != 0, "shard troncato: atteso rifiuto");

    /* 4. nome duplicato nello shard */
    reset_valid(dir); write_shard(dir, SH2, "w1", "w1");
    CHECK(gst_verify_shards(dir) != 0, "nome duplicato: atteso rifiuto");

    /* 5. shape/dtype incoerenti con data_offsets (F32 [4] = 16 byte, dichiarati 3) */
    reset_valid(dir);
    write_raw_st(sp2, "{\"w1\":{\"dtype\":\"F32\",\"shape\":[4],\"data_offsets\":[0,3]}}", 16);
    CHECK(gst_verify_shards(dir) != 0, "shape/dtype incoerenti: atteso rifiuto");

    /* 6. index con un tensore extra (non presente negli shard) */
    reset_valid(dir);
    wfile(ip, "{\"metadata\":{},\"weight_map\":{"
              "\"w0\":\"model-00001-of-00002.safetensors\","
              "\"w1\":\"model-00002-of-00002.safetensors\","
              "\"w2\":\"model-00002-of-00002.safetensors\"}}");
    CHECK(gst_verify_shards(dir) != 0, "index extra: atteso rifiuto");

    /* 7. shard con un tensore NON mappato nell'index */
    reset_valid(dir); write_shard(dir, SH2, "w1", "extra");
    CHECK(gst_verify_shards(dir) != 0, "tensore non mappato: atteso rifiuto");

    /* 8. tensore mappato a DUE shard (stessa chiave ripetuta) */
    reset_valid(dir);
    wfile(ip, "{\"metadata\":{},\"weight_map\":{"
              "\"w0\":\"model-00001-of-00002.safetensors\","
              "\"w0\":\"model-00002-of-00002.safetensors\"}}");
    CHECK(gst_verify_shards(dir) != 0, "tensore mappato due volte: atteso rifiuto");

    /* 9. senza index: valida il singolo model.safetensors */
    reset_valid(dir); remove(ip);
    remove(sp1); remove(sp2);
    char ssp[1200]; snprintf(ssp, sizeof(ssp), "%s/model.safetensors", dir);
    write_shard(dir, "model.safetensors", "w0", NULL);
    CHECK(gst_verify_shards(dir) == 0, "singolo file: atteso OK");
    write_raw_st(ssp, "{\"w0\":{\"dtype\":\"F32\",\"shape\":[4],\"data_offsets\":[0,64]}}", 16);
    CHECK(gst_verify_shards(dir) != 0, "singolo file corrotto: atteso rifiuto");

    /* cleanup */
    remove(sp1); remove(sp2); remove(ip); remove(ssp); rmdir(dir);
    if (fails == 0) { printf("test_gst_verify_shards: OK\n"); return 0; }
    printf("test_gst_verify_shards: %d FAIL\n", fails);
    return 1;
}
