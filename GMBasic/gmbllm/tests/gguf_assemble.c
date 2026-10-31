/* gguf_assemble.c — unisce N GGUF (slice di layer) in un unico GGUF, in streaming.
 * Uso: gmbllm_gguf_assemble <out.gguf> <in1.gguf> [in2...] | --info <file> [n]
 * I tensori devono essere disgiunti per nome; i globali ripetuti restano del primo file.
 * I dati sono copiati con pread/fwrite a buffer piccolo: RSS basso, nessun mmap da 100 GB. */
#include "llmcodec_gguf_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static uint64_t align_up(uint64_t x, uint64_t a) { return a > 1 ? ((x + a - 1) / a) * a : x; }
static int wr(FILE* f, const void* p, size_t n) { return fwrite(p, 1, n, f) == n ? 0 : -1; }
static int wr_u32(FILE* f, uint32_t v) { return wr(f, &v, 4); }
static int wr_u64(FILE* f, uint64_t v) { return wr(f, &v, 8); }
static int wr_i32(FILE* f, int32_t v) { return wr(f, &v, 4); }
static int wr_f32(FILE* f, float v) { return wr(f, &v, 4); }
static int wr_str(FILE* f, const char* s) {
    uint64_t n = s ? (uint64_t)strlen(s) : 0;
    if (wr_u64(f, n)) return -1;
    return n ? wr(f, s, (size_t)n) : 0;
}

typedef struct { int fd; uint64_t data_start; LlmCodecGgufTensorInfo* ti; uint64_t n; } Src;
typedef struct { const char* name; int type; uint64_t dims[8]; int ndim; int src; uint64_t off; uint64_t bytes; } MT;

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "uso: %s <out.gguf> <in1.gguf> [in2...] | --info <file> [n]\n", argv[0]); return 2; }
    if (strcmp(argv[1], "--check") == 0) {
        LlmCodecGgufMetaKVOwned* kv = NULL; int n = 0;
        LlmCodecGgufStatus st = llmcodec_gguf_read_meta_status(argv[2], &kv, &n, NULL);
        printf("status=%d kv=%d\n", (int)st, n);
        return st == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "--info") == 0) {
        LlmCodecGgufFileInfo fi; if (llmcodec_gguf_read_info(argv[2], &fi) != 0) return 1;
        int lim = argc > 3 ? atoi(argv[3]) : 40;
        printf("ntensors=%llu data_start=%llu\n", (unsigned long long)fi.n_tensors, (unsigned long long)fi.data_start);
        for (uint64_t t = 0; t < fi.n_tensors && (int)t < lim; t++)
            printf("%-44s type=%2d ndim=%d dims=[%llu,%llu,%llu] bytes=%llu\n", fi.tensors[t].name, fi.tensors[t].ggml_type,
                   fi.tensors[t].ndim, (unsigned long long)fi.tensors[t].dims[0], (unsigned long long)fi.tensors[t].dims[1],
                   (unsigned long long)fi.tensors[t].dims[2], (unsigned long long)llmcodec_gguf_tensor_bytes(&fi.tensors[t]));
        return 0;
    }

    const int ns = argc - 2;
    Src* s = calloc((size_t)ns, sizeof *s);
    LlmCodecGgufMetaKVOwned* kv0 = NULL; int nkv0 = 0;
    uint64_t ntot = 0; int max_layer = -1;
    for (int i = 0; i < ns; i++) {
        LlmCodecGgufFileInfo fi;
        if (i == 0) { if (llmcodec_gguf_read_meta(argv[2], &kv0, &nkv0, &fi) != 0) { fprintf(stderr, "read_meta %s\n", argv[2]); return 1; } }
        else { if (llmcodec_gguf_read_info(argv[2 + i], &fi) != 0) { fprintf(stderr, "read_info %s\n", argv[2+i]); return 1; } }
        s[i].fd = open(argv[2 + i], O_RDONLY);
        if (s[i].fd < 0) { perror(argv[2+i]); return 1; }
        s[i].data_start = fi.data_start; s[i].ti = fi.tensors; s[i].n = fi.n_tensors;
        ntot += fi.n_tensors;
        for (uint64_t t = 0; t < fi.n_tensors; t++) { const char* b = strstr(fi.tensors[t].name, "blk."); if (b) { int L = atoi(b+4); if (L > max_layer) max_layer = L; } }
    }

    MT* mt = calloc((size_t)ntot, sizeof *mt);
    uint64_t nm = 0;
    for (int i = 0; i < ns; i++) for (uint64_t t = 0; t < s[i].n; t++) {
        const char* nmn = s[i].ti[t].name;
        int dup = 0; for (uint64_t j = 0; j < nm; j++) if (strcmp(mt[j].name, nmn) == 0) { dup = 1; break; }
        if (dup) continue;
        mt[nm].name = nmn; mt[nm].type = s[i].ti[t].ggml_type; mt[nm].ndim = s[i].ti[t].ndim;
        for (int d = 0; d < 8; d++) mt[nm].dims[d] = s[i].ti[t].dims[d];
        mt[nm].src = i; mt[nm].off = s[i].data_start + s[i].ti[t].offset;
        mt[nm].bytes = llmcodec_gguf_tensor_bytes(&s[i].ti[t]);
        nm++;
    }
    uint64_t off = 0;
    for (uint64_t j = 0; j < nm; j++) { off = align_up(off, 32); mt[j].bytes = mt[j].bytes; mt[j].off = mt[j].off; mt[j].ndim = mt[j].ndim; off += mt[j].bytes; }
    /* ricalcola gli offset GGUF in un array separato */
    uint64_t* goff = calloc((size_t)nm, sizeof(uint64_t));
    off = 0;
    for (uint64_t j = 0; j < nm; j++) { off = align_up(off, 32); goff[j] = off; off += mt[j].bytes; }

    FILE* f = fopen(argv[1], "wb");
    if (!f) { perror(argv[1]); return 1; }
    int rc = 0;
    int nkv = nkv0;
    rc |= wr_u32(f, 0x46554747u); rc |= wr_u32(f, 3);
    rc |= wr_u64(f, nm); rc |= wr_u64(f, (uint64_t)nkv);
    for (int i = 0; i < nkv0 && rc == 0; i++) {
        LlmCodecGgufMetaKVOwned* k = &kv0[i];
        rc |= wr_str(f, k->key); rc |= wr_u32(f, (uint32_t)k->type);
        switch (k->type) {
            case 0: case 1: { uint8_t v = (uint8_t)k->u32; rc |= wr(f, &v, 1); } break;
            case 2: case 3: { uint16_t v = (uint16_t)k->u32; rc |= wr(f, &v, 2); } break;
            case 4: rc |= wr_u32(f, k->u32); break;
            case 5: rc |= wr_i32(f, k->i32); break;
            case 6: rc |= wr_f32(f, k->f32); break;
            case 10: rc |= wr_u64(f, k->u64); break;
            case 11: case 12: rc |= wr_u64(f, k->u64); break;
            case 7: { uint8_t v = k->b ? 1 : 0; rc |= wr(f, &v, 1); } break;
            case 8: rc |= wr_str(f, k->str); break;
            case 9:
                if (k->arr_raw) { rc |= wr_u32(f, (uint32_t)k->arr_type); rc |= wr_u64(f, k->arr_count); if (k->arr_raw_bytes) rc |= wr(f, k->arr_raw, (size_t)k->arr_raw_bytes); }
                else { rc |= wr_u32(f, 8); rc |= wr_u64(f, (uint64_t)k->arr_n); for (int a = 0; a < k->arr_n && rc == 0; a++) rc |= wr_str(f, k->arr[a]); }
                break;
            default: rc = -1; break;
        }
        if (rc == 0 && strstr(k->key, "block_count") && max_layer >= 0) {
            if (k->type == 4) { long p = ftell(f) - 4; fseek(f, p, SEEK_SET); rc |= wr_u32(f, (uint32_t)(max_layer + 1)); fseek(f, 0, SEEK_END); }
        }
    }
    for (uint64_t j = 0; j < nm && rc == 0; j++) {
        rc |= wr_str(f, mt[j].name); rc |= wr_u32(f, (uint32_t)mt[j].ndim);
        for (int d = 0; d < mt[j].ndim && rc == 0; d++) rc |= wr_u64(f, mt[j].dims[d]);
        rc |= wr_u32(f, (uint32_t)mt[j].type); rc |= wr_u64(f, goff[j]);
    }
    { long pos = ftell(f); uint64_t pad = align_up((uint64_t)pos, 32) - (uint64_t)pos; for (uint64_t i = 0; i < pad && rc == 0; i++) { char z = 0; rc |= wr(f, &z, 1); } }
    fprintf(stderr, "[assemble] tensori=%llu block_count=%d\n", (unsigned long long)nm, max_layer + 1);
    uint8_t* buf = malloc(4u << 20);
    uint64_t written = 0;
    uint64_t data_start = (uint64_t)ftell(f);
    for (uint64_t j = 0; j < nm && rc == 0; j++) {
        uint64_t want_pos = data_start + goff[j], cur = (uint64_t)ftell(f);
        while (cur < want_pos && rc == 0) { char z = 0; rc |= wr(f, &z, 1); cur++; }
        uint64_t done = 0;
        while (done < mt[j].bytes) {
            size_t want = (size_t)(mt[j].bytes - done); if (want > (4u << 20)) want = 4u << 20;
            ssize_t r = pread(s[mt[j].src].fd, buf, want, (off_t)(mt[j].off + done));
            if (r <= 0) { fprintf(stderr, "pread fail %s\n", mt[j].name); rc = -1; break; }
            if (wr(f, buf, (size_t)r)) { rc = -1; break; }
            done += (uint64_t)r; written += (uint64_t)r;
        }
        if (written > (1ull << 31)) { fflush(f); fsync(fileno(f)); written = 0; }
    }
    free(buf); fclose(f);
    fprintf(stderr, "[assemble] rc=%d -> %s\n", rc, argv[1]);
    return rc == 0 ? 0 : 1;
}
