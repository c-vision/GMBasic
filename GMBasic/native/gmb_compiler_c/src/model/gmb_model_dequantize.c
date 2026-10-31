/* gmb_model_dequantize.c — converte una dir MLX affine (weight U32 + scales/biases) in f16.
 * Mirror del quantizzatore: riusa gmb_st (safetensors) + llmcodec_moe_affine_dequantize. bits/group
 * letti da `config.json` (`quantization`). Serve alla chiusura §E1 (fp16 → int4 → eseguito). */
#include "gmb_st.h"
#include "llmcodec_moe_quant.h"
#include "llmcodec_fp16.h"   /* f32->f16 UNA SORGENTE (codec) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct { char name[256]; uint8_t* data; size_t n; GstEntry e; } DqItem;

static void mkdir_p(const char* p) {
    char buf[1200]; snprintf(buf, sizeof(buf), "%s", p);
    for (char* q = buf + 1; *q; ++q) if (*q == '/') { *q = 0; mkdir(buf, 0777); *q = '/'; }
    mkdir(buf, 0777);
}
static int copy_file(const char* src, const char* dst) {
    FILE* a = fopen(src, "rb"); if (!a) return 0;
    FILE* b = fopen(dst, "wb"); if (!b) { fclose(a); return 0; }
    char buf[65536]; size_t r;
    while ((r = fread(buf, 1, sizeof(buf), a)) > 0) if (fwrite(buf, 1, r, b) != r) break;
    fclose(a); fclose(b); return 1;
}

static int read_qparams(const char* src_dir, uint32_t* group, uint32_t* bits) {
    char p[1200]; snprintf(p, sizeof(p), "%s/config.json", src_dir);
    FILE* f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)n + 1);
    if (!js || fread(js, 1, (size_t)n, f) != (size_t)n) { free(js); fclose(f); return 0; }
    js[n] = 0; fclose(f);
    const char* qs = strstr(js, "\"group_size\"");
    const char* bs = strstr(js, "\"bits\"");
    *group = qs ? (uint32_t)strtoul(strchr(qs, ':') + 1, NULL, 10) : 64;
    *bits  = bs ? (uint32_t)strtoul(strchr(bs, ':') + 1, NULL, 10) : 4;
    free(js);
    return 1;
}

/* Rimuove in-place la coppia `"key":{...}` (oggetto) da un JSON testuale, gestendo la virgola
 * separatrice (prima o dopo). Ritorna 1 se rimosso. Usato per togliere `quantization` E
 * `quantization_config` (il mirror HF) dal config del modello dequantizzato: mlx_lm rifiuta un
 * modello che dichiara `quantization_config` con "quant_method" assente. */
static int json_remove_obj(char* js, const char* key) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    char* q = strstr(js, pat);
    if (!q) return 0;
    char* ob = strchr(q, '{');
    if (!ob) return 0;
    int depth = 0; char* cb = NULL;
    for (char* p = ob; *p; ++p) {
        if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) { cb = p; break; } }
    }
    if (!cb) return 0;
    char* from = q;
    char* after = cb + 1;
    if (*after == ',') { ++after; while (*after == ' ') ++after; }
    else {   /* ultimo membro: mangia la virgola PRECEDENTE */
        char* b = q; while (b > js && (b[-1] == ' ' || b[-1] == '\n' || b[-1] == '\t')) --b;
        if (b > js && b[-1] == ',') from = b - 1;
    }
    memmove(from, after, strlen(after) + 1);
    return 1;
}

int gmb_model_dequantize(const char* src_dir, const char* out_dir, int verbose) {
    if (!src_dir || !out_dir) return 1;
    uint32_t group = 64, bits = 4;
    read_qparams(src_dir, &group, &bits);
    if (group == 0 || bits < 2 || bits > 8 || bits == 7) { fprintf(stderr, "quant params invalidi\n"); return 1; }
    mkdir_p(out_dir);

    /* K7: raccolta con verifica per-shard; il dequantize carica in RAM → multi-shard RIFIUTATO
     * esplicitamente (prima leggeva solo il primo shard, in silenzio). */
    char shards[512][96]; int ns = 0;
    int cs = gst_collect_shards(src_dir, shards, 512, &ns);
    if (cs <= 0) { if (cs == 0) fprintf(stderr, "nessun model.safetensors in %s\n", src_dir); return 1; }
    if (gst_verify_shards(src_dir) != 0) return 1;
    if (ns > 1) { fprintf(stderr, "dequantize: modello multi-shard (%d shard) non supportato\n", ns); return 1; }

    /* carica tutto in memoria (modelli piccoli/medi) */
    char sp[1200]; snprintf(sp, sizeof(sp), "%s/%s", src_dir, shards[0]);
    GstEntry* ents = NULL; uint32_t ne = 0;
    if (gst_read_header(sp, &ents, &ne, NULL) != 0) { fprintf(stderr, "header non leggibile\n"); return 1; }
    DqItem* it = (DqItem*)calloc(ne, sizeof(DqItem));
    if (!it) { free(ents); return 1; }
    for (uint32_t i = 0; i < ne; ++i) {
        it[i].e = ents[i];
        if (gst_read_tensor(sp, &ents[i], &it[i].data, &it[i].n) != 0) { fprintf(stderr, "tensore non leggibile: %s\n", ents[i].name); return 1; }
        snprintf(it[i].name, sizeof(it[i].name), "%s", ents[i].name);
    }

    char op[1200]; snprintf(op, sizeof(op), "%s/model.safetensors", out_dir);
    GstWriter* w = NULL;
    if (gst_writer_create(&w, op) != 0) { fprintf(stderr, "writer fallito\n"); return 1; }

    uint32_t n_dq = 0, n_copy = 0;
    for (uint32_t i = 0; i < ne; ++i) {
        GstEntry* e = &it[i].e;
        size_t nl = strlen(e->name);
        if ((nl > 7 && strcmp(e->name + nl - 7, ".scales") == 0) ||
            (nl > 7 && strcmp(e->name + nl - 7, ".biases") == 0)) continue;   /* consumati dal weight */
        int ends_weight = (nl > 7 && strcmp(e->name + nl - 7, ".weight") == 0);
        if (strcmp(e->dtype, "U32") == 0 && ends_weight && (e->ndim == 2 || e->ndim == 3)) {
            char base[256]; snprintf(base, sizeof(base), "%s", e->name); base[nl - 7] = 0;
            char sn[300], bn[300];
            snprintf(sn, sizeof(sn), "%s.scales", base);
            snprintf(bn, sizeof(bn), "%s.biases", base);
            DqItem* si = NULL; DqItem* bi = NULL;
            for (uint32_t j = 0; j < ne; ++j) {
                if (strcmp(it[j].name, sn) == 0) si = &it[j];
                if (strcmp(it[j].name, bn) == 0) bi = &it[j];
            }
            if (si && bi) {
            if (e->ndim == 2) {   /* 2D */
                uint64_t rows = e->shape[0], packed = e->shape[1];
                uint64_t cols = (uint64_t)packed * 32u / bits;
                if (cols % group != 0) { fprintf(stderr, "cols%%group != 0 su %s\n", e->name); return 1; }
                float* x = (float*)malloc((size_t)rows * cols * sizeof(float));
                if (!x) { fprintf(stderr, "OOM\n"); return 1; }
                int sc_f16 = (strcmp(si->e.dtype, "F16") == 0);
                llmcodec_moe_affine_dequantize((const uint32_t*)it[i].data, (const uint16_t*)si->data,
                                          (const uint16_t*)bi->data, (uint32_t)rows, (uint32_t)cols,
                                          group, bits, sc_f16, x);
                /* scrivi f16 convertendo da f32 */
                uint16_t* h = (uint16_t*)malloc((size_t)rows * cols * 2);
                for (size_t k = 0; k < (size_t)rows * cols; ++k) h[k] = llmcodec_f16_from_f32(x[k]);   /* atomo del CODEC */
                uint64_t shp[2] = { rows, cols };
                gst_writer_add(w, e->name, "F16", 2, shp, h, (size_t)rows * cols * 2);
                if (verbose) fprintf(stderr, "[dequant] %-52s U32 -> F16 %llux%llu\n", e->name, (unsigned long long)rows, (unsigned long long)cols);
                free(x); free(h);
            } else {   /* §E2 step 2: esperti 3D (per-esperto) */
                uint64_t nE = e->shape[0], er = e->shape[1], p3 = e->shape[2];
                uint64_t ec = (uint64_t)p3 * 32u / bits;
                if (ec % group != 0) { fprintf(stderr, "cols%%group != 0 su %s (3D)\n", e->name); return 1; }
                size_t xrow = (size_t)er * ec;
                size_t wb = llmcodec_moe_affine_w_bytes((uint32_t)er, (uint32_t)ec, bits);
                size_t sbb = llmcodec_moe_affine_sc_bytes((uint32_t)er, (uint32_t)ec, group);
                float* x = (float*)malloc((size_t)nE * xrow * sizeof(float));
                uint16_t* h = (uint16_t*)malloc((size_t)nE * xrow * 2);
                if (!x || !h) { free(x); free(h); fprintf(stderr, "OOM\n"); return 1; }
                int sc_f16b = (strcmp(si->e.dtype, "F16") == 0);
                for (uint64_t ex = 0; ex < nE; ++ex) {
                    llmcodec_moe_affine_dequantize((const uint32_t*)(it[i].data + ex * wb),
                                              (const uint16_t*)(si->data + ex * sbb),
                                              (const uint16_t*)(bi->data + ex * sbb),
                                              (uint32_t)er, (uint32_t)ec, group, bits, sc_f16b, x + ex * xrow);
                    for (size_t k = 0; k < xrow; ++k) h[ex * xrow + k] = llmcodec_f16_from_f32(x[ex * xrow + k]);   /* atomo CODEC */
                }
                uint64_t shp3[3] = { nE, er, ec };
                gst_writer_add(w, e->name, "F16", 3, shp3, h, (size_t)nE * xrow * 2);
                if (verbose) fprintf(stderr, "[dequant3] %-50s U32 -> F16 %llux%llux%llu\n", e->name, (unsigned long long)nE, (unsigned long long)er, (unsigned long long)ec);
                free(x); free(h);
            }
            n_dq++;
            continue;
            }
        }
        gst_writer_add(w, e->name, e->dtype, e->ndim, e->shape, it[i].data, it[i].n);
        n_copy++;
    }
    if (gst_writer_finish(w) != 0) { fprintf(stderr, "scrittura fallita\n"); return 1; }
    /* config senza quantization (non più quantizzato) + tokenizer */
    { char a[1200], b[1200];
      snprintf(a, sizeof(a), "%s/config.json", src_dir); snprintf(b, sizeof(b), "%s/config.json", out_dir);
      FILE* f = fopen(a, "rb");
      if (f) {
          fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
          char* js = (char*)malloc((size_t)n + 1);
          if (js && fread(js, 1, (size_t)n, f) == (size_t)n) {
              js[n] = 0;
              /* rimuovi `quantization` E `quantization_config` (mirror HF): un modello dequantizzato
               * non deve dichiarare alcuna quantizzazione (mlx_lm altrimenti cerca quant_method). */
              json_remove_obj(js, "quantization");
              json_remove_obj(js, "quantization_config");
              FILE* g = fopen(b, "wb"); if (g) { fwrite(js, 1, strlen(js), g); fclose(g); }
          }
          free(js); fclose(f);
      }
    }
    const char* extra[] = { "tokenizer.json", "tokenizer_config.json", "merges.txt", "vocab.json", "special_tokens_map.json", "generation_config.json" };
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i) {
        char a[1200], b[1200];
        snprintf(a, sizeof(a), "%s/%s", src_dir, extra[i]);
        snprintf(b, sizeof(b), "%s/%s", out_dir, extra[i]);
        copy_file(a, b);
    }
    for (uint32_t i = 0; i < ne; ++i) free(it[i].data);
    free(it); free(ents);
    printf("gmb_model_dequantize: ok  %u dequantizzati, %u copiati -> %s\n", n_dq, n_copy, op);
    return 0;
}
