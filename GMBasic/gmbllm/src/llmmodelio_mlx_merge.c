/* llmmodelio_mlx_merge.c — MERGE LoRA su un modello MLX (dir: config.json + *.safetensors).
 * Sibling di llmmodelio_model_adapter (che è GGUF→GGUF e sola-FP). Qui:
 *   - base = una DIR MLX/HF (config.json + shard safetensors)
 *   - adapter = un .safetensors (LoRA A/B, naming HF: <prefix>.lora_{A,B}.weight)
 *   - per ogni tensore 2D base con coppia LoRA: base += (alpha/r) * B@A
 *       · se il tensore è FP  (F16/BF16/F32) → fondi e riscrivi FP
 *       · se è affine quantizzato (weight U32 + scales/biases F16) → dequant → fondi → requant
 *         con lo STESSO bits/group (dal config `quantization`) → resta caricabile.
 *   - gli altri tensori sono copiati identici.
 * Scrive <out_dir>/config.json (verbatim) + <out_dir>/model.safetensors.
 * COMPILATORE C AUTONOMO del model compiler (PUL). */
#include "llmmodelio_model.h"
#include "llmcodec_safetensors.h"
#include "llmcodec_moe_quant.h"
#include "llmcodec_fp16.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

static char* mlxm_read(const char* p) {
    FILE* f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    b[n] = 0; fclose(f); return b;
}
static int mlxm_json_int(const char* js, const char* key, int dflt) {
    const char* p = js ? strstr(js, key) : NULL; if (!p) return dflt;
    p = strchr(p, ':'); if (!p) return dflt;
    while (*p && (*p == ':' || *p == ' ' || *p == '\t')) p++;
    if (*p < '0' || *p > '9') return dflt;
    return atoi(p);
}
static int mlxm_st_find(const LlmCodecStTensorInfo* st, int n, const char* name) {
    for (int i = 0; i < n; i++) if (strcmp(st[i].name, name) == 0) return i;
    return -1;
}
static float* mlxm_load_f32(FILE* fp, uint64_t base, const LlmCodecStTensorInfo* t, uint64_t* out_n) {
    uint64_t n = 1; for (int d = 0; d < t->ndim; d++) n *= t->shape[d];
    if (out_n) *out_n = n;
    uint64_t b = t->end - t->begin;
    char* raw = (char*)malloc((size_t)b); if (!raw) return NULL;
    if (fseek(fp, (long)(base + t->begin), SEEK_SET) != 0 || fread(raw, 1, (size_t)b, fp) != (size_t)b) { free(raw); return NULL; }
    float* o = (float*)malloc((size_t)n * 4); if (!o) { free(raw); return NULL; }
    if (strcmp(t->dtype, "F32") == 0) { memcpy(o, raw, (size_t)n * 4); }
    else if (strcmp(t->dtype, "F16") == 0) { uint16_t* h = (uint16_t*)raw; for (uint64_t i = 0; i < n; i++) o[i] = llmcodec_f16_to_f32(h[i]); }
    else if (strcmp(t->dtype, "BF16") == 0) { uint16_t* h = (uint16_t*)raw; for (uint64_t i = 0; i < n; i++) o[i] = llmcodec_bf16_to_f32(h[i]); }
    else { free(raw); free(o); return NULL; }
    free(raw); return o;
}
/* Coppia LoRA: prova il prefisso pieno, poi la variante senza self_attn./mlp. (nome SAVE attuale). */
static int mlxm_lora_pair(const LlmCodecStTensorInfo* st, int n, const char* base_name, int* ia, int* ib) {
    const char* suf = ".weight"; size_t bl = strlen(base_name), sl = strlen(suf);
    char prefix[256];
    if (bl > sl && strcmp(base_name + bl - sl, suf) == 0) { size_t pl = bl - sl; if (pl > 255) pl = 255; memcpy(prefix, base_name, pl); prefix[pl] = 0; }
    else { if (bl > 255) return -1; strcpy(prefix, base_name); }
    const char* cand[3];
    char stripped[256]; stripped[0] = 0;
    const char* stripkeys[2] = { "self_attn.", "mlp." };
    for (int s = 0; s < 2; s++) {
        const char* q = strstr(prefix, stripkeys[s]);
        if (q) { size_t head = (size_t)(q - prefix); snprintf(stripped, sizeof stripped, "%.*s%s", (int)head, prefix, q + strlen(stripkeys[s])); break; }
    }
    cand[0] = prefix; cand[1] = stripped[0] ? stripped : prefix;
    char bm[600]; snprintf(bm, sizeof bm, "base_model.model.%s", prefix); cand[2] = bm;
    for (int c = 0; c < 3; c++) {
        char a[700], b[700];
        snprintf(a, sizeof a, "%s.lora_A.weight", cand[c]);
        snprintf(b, sizeof b, "%s.lora_B.weight", cand[c]);
        int x = mlxm_st_find(st, n, a), y = mlxm_st_find(st, n, b);
        if (x >= 0 && y >= 0) { *ia = x; *ib = y; return 0; }
    }
    return -1;
}

int llmmodelio_mlx_adapter(const char* base_dir, const char* adapter_path, const char* out_dir, const char* alpha_str) {
    if (!base_dir || !adapter_path || !out_dir) return -1;
    double alpha = alpha_str ? atof(alpha_str) : 1.0;

    char cp[2048]; snprintf(cp, sizeof cp, "%s/config.json", base_dir);
    char* cfg = mlxm_read(cp);
    if (!cfg) { fprintf(stderr, "merge-mlx: config.json assente in %s\n", base_dir); return -1; }
    int group = mlxm_json_int(cfg, "\"group_size\"", 64);
    int bits  = mlxm_json_int(cfg, "\"bits\"", 4);

    /* shard base */
    char** shards = NULL; int nsh = 0;
    DIR* dp = opendir(base_dir); if (!dp) { free(cfg); return -1; }
    struct dirent* de;
    while ((de = readdir(dp)) != NULL) {
        size_t L = strlen(de->d_name);
        if (L > 12 && strcmp(de->d_name + L - 12, ".safetensors") == 0) { char sp[2048]; snprintf(sp, sizeof sp, "%s/%s", base_dir, de->d_name); shards = (char**)realloc(shards, sizeof(char*) * (nsh + 1)); shards[nsh++] = strdup(sp); }
    }
    closedir(dp);
    if (nsh == 0) { free(cfg); return -1; }

    /* adapter */
    LlmCodecStTensorInfo* ad = NULL; int nad = 0; uint64_t adbase = 0;
    FILE* afp = fopen(adapter_path, "rb");
    if (!afp || llmcodec_safetensors_read(adapter_path, &ad, &nad, &adbase) != 0) { if (afp) fclose(afp); for (int i = 0; i < nsh; i++) free(shards[i]); free(shards); free(cfg); return -1; }

    /* output: accumula tensori (nomi/dtype/shape puntano a buffer che possediamo) */
    int cap = 0, nout = 0;
    LlmCodecStTensor* out = NULL;
    char** keep = NULL; size_t* keepn = NULL;   /* buffer da liberare a fine scrittura */
    int rc = 0; uint64_t nmerged = 0;

    for (int s = 0; s < nsh && rc == 0; s++) {
        LlmCodecStTensorInfo* st = NULL; int nst = 0; uint64_t dbase = 0;
        if (llmcodec_safetensors_read(shards[s], &st, &nst, &dbase) != 0) { rc = -1; break; }
        FILE* bfp = fopen(shards[s], "rb");
        if (!bfp) { llmcodec_safetensors_info_free(st); rc = -1; break; }
        for (int i = 0; i < nst && rc == 0; i++) {
            LlmCodecStTensorInfo* t = &st[i];
            int ia = -1, ib = -1;
            int is_weight = (t->ndim == 2 && strcmp(t->dtype, "U32") == 0);
            int has_pair = (t->ndim == 2) && mlxm_lora_pair(ad, nad, t->name, &ia, &ib) == 0;
            if (!has_pair) {
                /* copia identica */
                uint64_t b = t->end - t->begin; char* raw = (char*)malloc((size_t)b ? (size_t)b : 1);
                if (!raw || fseek(bfp, (long)(dbase + t->begin), SEEK_SET) != 0 || fread(raw, 1, (size_t)b, bfp) != (size_t)b) { free(raw); rc = -1; break; }
                if (nout + 1 > cap) { cap = cap * 2 + 16; out = (LlmCodecStTensor*)realloc(out, sizeof(LlmCodecStTensor) * (size_t)cap); keep = (char**)realloc(keep, sizeof(char*) * (size_t)cap); keepn = (size_t*)realloc(keepn, sizeof(size_t) * (size_t)cap); }
                int* shp = (int*)malloc(sizeof(int) * (t->ndim ? t->ndim : 1)); for (int d = 0; d < t->ndim; d++) shp[d] = (int)t->shape[d];
                out[nout].name = strdup(t->name); out[nout].dtype = strdup(t->dtype); out[nout].shape = shp; out[nout].ndim = t->ndim; out[nout].data = raw;
                keep[nout] = raw; keepn[nout] = b; nout++;
                continue;
            }
            if (!is_weight) { /* FP tensor con coppia: fondi e riscrivi FP */
                uint64_t bn = 0; float* B0 = mlxm_load_f32(bfp, dbase, t, &bn);
                if (!B0) { rc = -1; break; }
                uint64_t An = 0, Bn = 0;
                float* A = mlxm_load_f32(afp, adbase, &ad[ia], &An);
                float* Bm = mlxm_load_f32(afp, adbase, &ad[ib], &Bn);
                if (!A || !Bm) { free(A); free(Bm); free(B0); rc = -1; break; }
                uint64_t in = t->shape[0], outp = t->shape[1]; uint64_t r = ad[ia].shape[0];
                if (ad[ia].shape[1] != in || ad[ib].shape[0] != outp) { free(A); free(Bm); free(B0); rc = -1; break; }
                double scale = alpha / (double)(r ? r : 1);
                for (uint64_t o = 0; o < outp; o++) for (uint64_t ii = 0; ii < in; ii++) {
                    double acc = 0; for (uint64_t k = 0; k < r; k++) acc += (double)Bm[o * r + k] * (double)A[k * in + ii];
                    B0[o * in + ii] = (float)((double)B0[o * in + ii] + scale * acc);
                }
                /* back to F16 */
                uint16_t* h = (uint16_t*)malloc((size_t)bn * 2);
                for (uint64_t x = 0; x < bn; x++) h[x] = llmcodec_f16_from_f32(B0[x]);
                if (nout + 1 > cap) { cap = cap * 2 + 16; out = realloc(out, sizeof(LlmCodecStTensor) * cap); keep = realloc(keep, sizeof(char*) * cap); keepn = realloc(keepn, sizeof(size_t) * cap); }
                int* shp = (int*)malloc(sizeof(int) * t->ndim); for (int d = 0; d < t->ndim; d++) shp[d] = (int)t->shape[d];
                out[nout].name = strdup(t->name); out[nout].dtype = strdup("F16"); out[nout].shape = shp; out[nout].ndim = t->ndim; out[nout].data = h;
                keep[nout] = (char*)h; keepn[nout] = (size_t)(bn * 2); nout++;
                free(A); free(Bm); free(B0); nmerged++;
                continue;
            }
            /* affine quantizzato: dequant (weight+scales+biases) -> fondi -> requant */
            char sn[300], bn2[300], wpre[256];
            { const char* dot = strrchr(t->name, '.'); size_t pl = dot ? (size_t)(dot - t->name) : strlen(t->name); if (pl > 255) pl = 255; memcpy(wpre, t->name, pl); wpre[pl] = 0; }
            snprintf(sn, sizeof sn, "%s.scales", wpre);
            snprintf(bn2, sizeof bn2, "%s.biases", wpre);
            int isc = mlxm_st_find(st, nst, sn), ibi = mlxm_st_find(st, nst, bn2);
            if (isc < 0 || ibi < 0) { rc = -1; break; }   /* weight U32 senza scales/biases: inatteso */
            uint64_t rows = t->shape[0]; uint64_t cols = (uint64_t)t->shape[1] * 8; /* 4-bit packed */
            uint64_t wb = t->end - t->begin;
            uint32_t* w = (uint32_t*)malloc((size_t)wb); if (!w) { rc = -1; break; }
            if (fseek(bfp, (long)(dbase + t->begin), SEEK_SET) != 0 || fread(w, 1, (size_t)wb, bfp) != (size_t)wb) { free(w); rc = -1; break; }
            uint64_t scb = st[isc].end - st[isc].begin, bib = st[ibi].end - st[ibi].begin;
            uint16_t* sc = (uint16_t*)malloc((size_t)scb), *bi = (uint16_t*)malloc((size_t)bib);
            if (fseek(bfp, (long)(dbase + st[isc].begin), SEEK_SET) != 0 || fread(sc, 1, (size_t)scb, bfp) != (size_t)scb
                || fseek(bfp, (long)(dbase + st[ibi].begin), SEEK_SET) != 0 || fread(bi, 1, (size_t)bib, bfp) != (size_t)bib) { free(w); free(sc); free(bi); rc = -1; break; }
            float* dq = (float*)malloc((size_t)(rows * cols) * 4);
            llmcodec_moe_affine_dequantize(w, sc, bi, (uint32_t)rows, (uint32_t)cols, (uint32_t)group, (uint32_t)bits, 1, dq);
            uint64_t An = 0, Bn = 0;
            float* A = mlxm_load_f32(afp, adbase, &ad[ia], &An);
            float* Bm = mlxm_load_f32(afp, adbase, &ad[ib], &Bn);
            if (!A || !Bm) { free(A); free(Bm); free(w); free(sc); free(bi); free(dq); rc = -1; break; }
            uint64_t r = ad[ia].shape[0];
            if (ad[ia].shape[1] != cols || ad[ib].shape[0] != rows) { free(A); free(Bm); free(w); free(sc); free(bi); free(dq); rc = -1; break; }
            double scale = alpha / (double)(r ? r : 1);
            for (uint64_t o = 0; o < rows; o++) for (uint64_t ii = 0; ii < cols; ii++) {
                double acc = 0; for (uint64_t k = 0; k < r; k++) acc += (double)Bm[o * r + k] * (double)A[k * cols + ii];
                dq[o * cols + ii] = (float)((double)dq[o * cols + ii] + scale * acc);
            }
            uint32_t* w2 = (uint32_t*)malloc((size_t)wb);
            uint16_t* sc2 = (uint16_t*)malloc((size_t)scb), *bi2 = (uint16_t*)malloc((size_t)bib);
            if (llmcodec_moe_affine_quantize(dq, (uint32_t)rows, (uint32_t)cols, (uint32_t)group, (uint32_t)bits, w2, sc2, bi2) != 0) { free(A); free(Bm); free(w); free(sc); free(bi); free(dq); free(w2); free(sc2); free(bi2); rc = -1; break; }
            /* emetti la tripla (weight U32 + scales F16 + biases F16) con gli stessi nomi */
            if (nout + 3 > cap) { cap = cap * 2 + 16; out = realloc(out, sizeof(LlmCodecStTensor) * cap); keep = realloc(keep, sizeof(char*) * cap); keepn = realloc(keepn, sizeof(size_t) * cap); }
            int* shp = (int*)malloc(sizeof(int) * t->ndim); for (int d = 0; d < t->ndim; d++) shp[d] = (int)t->shape[d];
            out[nout].name = strdup(t->name); out[nout].dtype = strdup("U32"); out[nout].shape = shp; out[nout].ndim = t->ndim; out[nout].data = w2; keep[nout] = (char*)w2; keepn[nout] = (size_t)wb; nout++;
            int* shp_s = (int*)malloc(sizeof(int) * st[isc].ndim); for (int d = 0; d < st[isc].ndim; d++) shp_s[d] = (int)st[isc].shape[d];
            out[nout].name = strdup(st[isc].name); out[nout].dtype = strdup("F16"); out[nout].shape = shp_s; out[nout].ndim = st[isc].ndim; out[nout].data = sc2; keep[nout] = (char*)sc2; keepn[nout] = (size_t)scb; nout++;
            int* shp_b = (int*)malloc(sizeof(int) * st[ibi].ndim); for (int d = 0; d < st[ibi].ndim; d++) shp_b[d] = (int)st[ibi].shape[d];
            out[nout].name = strdup(st[ibi].name); out[nout].dtype = strdup("F16"); out[nout].shape = shp_b; out[nout].ndim = st[ibi].ndim; out[nout].data = bi2; keep[nout] = (char*)bi2; keepn[nout] = (size_t)bib; nout++;
            free(A); free(Bm); free(w); free(sc); free(bi); free(dq); nmerged++;
        }
        fclose(bfp); llmcodec_safetensors_info_free(st);
    }

    if (rc == 0 && nmerged == 0) { fprintf(stderr, "merge-mlx: nessuna coppia LoRA abbinata\n"); rc = -1; }

    if (rc == 0) {
        for (char* q = (char*)out_dir + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(out_dir, 0755); *q = '/'; }
        mkdir(out_dir, 0755);
        /* copia i file NON-.safetensors della base (config.json, tokenizer*, *.index.json, ...) */
        { DIR* bp = opendir(base_dir); struct dirent* be;
          while (bp && (be = readdir(bp)) != NULL) {
            size_t L = strlen(be->d_name);
            if (be->d_name[0] == '.' || (L > 12 && strcmp(be->d_name + L - 12, ".safetensors") == 0)) continue;
            char sp[2048], op2[2048]; snprintf(sp, sizeof sp, "%s/%s", base_dir, be->d_name); snprintf(op2, sizeof op2, "%s/%s", out_dir, be->d_name);
            char* dat = mlxm_read(sp); if (dat) { FILE* of = fopen(op2, "wb"); if (of) { fwrite(dat, 1, strlen(dat), of); fclose(of); } free(dat); }
          }
          if (bp) closedir(bp);
        }
        char op[2048]; snprintf(op, sizeof op, "%s/model.safetensors", out_dir);
        if (llmcodec_safetensors_write(op, out, nout) != 0) rc = -1;

    }
    for (int i = 0; i < nout; i++) { free((void*)out[i].name); free((void*)out[i].dtype); free((void*)out[i].shape); }
    free(out); free(keep); free(keepn);
    fclose(afp); llmcodec_safetensors_info_free(ad);
    for (int i = 0; i < nsh; i++) free(shards[i]); free(shards); free(cfg);
    return rc == 0 ? 0 : -1;
}

/* Rimuove la coppia top-level `"key": <value/object>` da un JSON (per togliere la quantizzazione). */
static char* mlxm_strip_kv(const char* js, const char* keyq) {
    const char* p = strstr(js, keyq);
    if (!p) return strdup(js);
    const char* ks = p;
    const char* a = p + strlen(keyq);
    while (*a == ' ' || *a == '\t') a++;
    if (*a != ':') return strdup(js);
    a++; while (*a == ' ' || *a == '\t') a++;
    const char* ve = a;
    if (*ve == '{') { int d = 0; for (; *ve; ve++) { if (*ve == '{') d++; else if (*ve == '}') { d--; if (!d) { ve++; break; } } } }
    else if (*ve == '[') { int d = 0; for (; *ve; ve++) { if (*ve == '[') d++; else if (*ve == ']') { d--; if (!d) { ve++; break; } } } }
    else while (*ve && *ve != ',' && *ve != '}' && *ve != '\n') ve++;
    const char* cut_a = ks, *cut_b = ve;
    const char* t = ve; while (*t == ' ' || *t == '\t') t++;
    if (*t == ',') cut_b = t + 1;
    else { const char* u = ks; while (u > js && (u[-1] == ' ' || u[-1] == '\t')) u--; if (u > js && u[-1] == ',') cut_a = u - 1; }
    size_t pre = (size_t)(cut_a - js);
    char* o = (char*)malloc(strlen(js) + 1);
    memcpy(o, js, pre); strcpy(o + pre, cut_b);
    return o;
}

/* G6 — DEQUANT: modello MLX affine-quantizzato → **F16 PLAIN** (pesi F16, niente scales/biases,
 * niente `quantization` nel config). Prerequisito del full-parameter (MLX QuantizedMatmul::vjp
 * NON ha gradiente). FP restano copiati. Scrive <out_dir>/config.json + model.safetensors + gli altri file. */
int llmmodelio_mlx_dequant(const char* base_dir, const char* out_dir) {
    if (!base_dir || !out_dir) return -1;
    char cp[2048]; snprintf(cp, sizeof cp, "%s/config.json", base_dir);
    char* cfg = mlxm_read(cp);
    if (!cfg) { fprintf(stderr, "dequant-mlx: config.json assente in %s\n", base_dir); return -1; }
    int group = mlxm_json_int(cfg, "\"group_size\"", 64);
    int bits  = mlxm_json_int(cfg, "\"bits\"", 4);
    if (bits < 1 || bits > 8) bits = 4;
    char* cfg2 = mlxm_strip_kv(cfg, "\"quantization\"");          /* rimuovi la quantizzazione */
    char* cfg3 = mlxm_strip_kv(cfg2, "\"quantization_config\"");
    free(cfg2);
    char** shards = NULL; int nsh = 0;
    DIR* dp = opendir(base_dir); struct dirent* de;
    while ((de = readdir(dp)) != NULL) { size_t L = strlen(de->d_name); if (L > 12 && strcmp(de->d_name + L - 12, ".safetensors") == 0) { char sp[2048]; snprintf(sp, sizeof sp, "%s/%s", base_dir, de->d_name); shards = (char**)realloc(shards, sizeof(char*) * (nsh + 1)); shards[nsh++] = strdup(sp); } }
    closedir(dp);
    if (nsh == 0) { free(cfg); free(cfg3); return -1; }
    int cap = 0, nout = 0; LlmCodecStTensor* out = NULL; char** keep = NULL; size_t* keepn = NULL;
    int rc = 0, ndeq = 0;
    for (int s = 0; s < nsh && rc == 0; s++) {
        LlmCodecStTensorInfo* st = NULL; int nst = 0; uint64_t dbase = 0;
        if (llmcodec_safetensors_read(shards[s], &st, &nst, &dbase) != 0) { rc = -1; break; }
        FILE* bfp = fopen(shards[s], "rb"); if (!bfp) { llmcodec_safetensors_info_free(st); rc = -1; break; }
        for (int i = 0; i < nst && rc == 0; i++) {
            LlmCodecStTensorInfo* t = &st[i];
            size_t L = strlen(t->name);
            if ((L > 7 && strcmp(t->name + L - 7, ".scales") == 0) || (L > 7 && strcmp(t->name + L - 7, ".biases") == 0)) continue;  /* gestiti col .weight */
            void* dat = NULL; size_t nb = 0; const char* dt = t->dtype;
            int is_affine = (t->ndim == 2 && strcmp(t->dtype, "U32") == 0);
            uint64_t aff_rows = 0, aff_cols = 0;
            if (is_affine) {   /* affine -> dequant -> F16 plain */
                char sn[300], bn[300], wpre[256];
                const char* dot = strrchr(t->name, '.'); size_t pl = dot ? (size_t)(dot - t->name) : strlen(t->name); if (pl > 255) pl = 255; memcpy(wpre, t->name, pl); wpre[pl] = 0;
                snprintf(sn, sizeof sn, "%s.scales", wpre); snprintf(bn, sizeof bn, "%s.biases", wpre);
                int isc = mlxm_st_find(st, nst, sn), ibi = mlxm_st_find(st, nst, bn);
                if (isc < 0 || ibi < 0) { rc = -1; break; }
                uint64_t rows = t->shape[0]; uint64_t cols = (uint64_t)t->shape[1] * 32ull / (uint64_t)bits;
                uint64_t wb = t->end - t->begin;
                uint32_t* w = (uint32_t*)malloc((size_t)wb);
                uint64_t scb = st[isc].end - st[isc].begin, bib = st[ibi].end - st[ibi].begin;
                uint16_t* sc = (uint16_t*)malloc((size_t)scb), *bi = (uint16_t*)malloc((size_t)bib);
                float* dq = (float*)malloc((size_t)(rows * cols) * 4);
                uint16_t* h = (uint16_t*)malloc((size_t)(rows * cols) * 2);
                if (!w || !sc || !bi || !dq || !h ||
                    fseek(bfp, (long)(dbase + t->begin), SEEK_SET) != 0 || fread(w, 1, (size_t)wb, bfp) != (size_t)wb ||
                    fseek(bfp, (long)(dbase + st[isc].begin), SEEK_SET) != 0 || fread(sc, 1, (size_t)scb, bfp) != (size_t)scb ||
                    fseek(bfp, (long)(dbase + st[ibi].begin), SEEK_SET) != 0 || fread(bi, 1, (size_t)bib, bfp) != (size_t)bib) { rc = -1; }
                if (rc == 0) { llmcodec_moe_affine_dequantize(w, sc, bi, (uint32_t)rows, (uint32_t)cols, (uint32_t)group, (uint32_t)bits, 1, dq);
                               for (uint64_t x = 0; x < rows * cols; x++) h[x] = llmcodec_f16_from_f32(dq[x]); dat = h; nb = (size_t)(rows * cols) * 2; dt = "F16"; ndeq++;
                               aff_rows = rows; aff_cols = cols; }
                free(w); free(sc); free(bi); free(dq); if (rc != 0) free(h);
                (void)wpre;
            } else {   /* FP: copia identica */
                uint64_t b = t->end - t->begin; char* raw = (char*)malloc((size_t)b ? (size_t)b : 1);
                if (!raw || fseek(bfp, (long)(dbase + t->begin), SEEK_SET) != 0 || fread(raw, 1, (size_t)b, bfp) != (size_t)b) { free(raw); rc = -1; break; }
                dat = raw; nb = (size_t)b;
            }
            if (rc != 0) break;
            if (nout + 1 > cap) { cap = cap * 2 + 16; out = (LlmCodecStTensor*)realloc(out, sizeof(LlmCodecStTensor) * (size_t)cap); keep = (char**)realloc(keep, sizeof(char*) * (size_t)cap); keepn = (size_t*)realloc(keepn, sizeof(size_t) * (size_t)cap); }
            int* shp = (int*)malloc(sizeof(int) * (t->ndim ? t->ndim : 1));
            int ond = t->ndim;
            if (is_affine) { shp[0] = (int)aff_rows; shp[1] = (int)aff_cols; ond = 2; }   /* [rows, cols] dequantizzati */
            else for (int d = 0; d < t->ndim; d++) shp[d] = (int)t->shape[d];
            out[nout].name = strdup(t->name); out[nout].dtype = strdup(dt); out[nout].shape = shp; out[nout].ndim = ond; out[nout].data = dat;
            keep[nout] = (char*)dat; keepn[nout] = nb; nout++;
        }
        fclose(bfp); llmcodec_safetensors_info_free(st);
    }
    if (rc == 0 && ndeq == 0) { fprintf(stderr, "dequant-mlx: nessun tensore affine (modello gia' FP?)\n"); rc = -1; }
    if (rc == 0) {
        for (char* q = (char*)out_dir + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(out_dir, 0755); *q = '/'; }
        mkdir(out_dir, 0755);
        { DIR* bp = opendir(base_dir); struct dirent* be;
          while (bp && (be = readdir(bp)) != NULL) {
            size_t L = strlen(be->d_name);
            if (be->d_name[0] == '.' || (L > 12 && strcmp(be->d_name + L - 12, ".safetensors") == 0)) continue;
            char sp[2048], op2[2048]; snprintf(sp, sizeof sp, "%s/%s", base_dir, be->d_name); snprintf(op2, sizeof op2, "%s/%s", out_dir, be->d_name);
            if (strcmp(be->d_name, "config.json") == 0) { FILE* of = fopen(op2, "wb"); if (of) { fwrite(cfg3, 1, strlen(cfg3), of); fclose(of); } continue; }
            char* dd = mlxm_read(sp); if (dd) { FILE* of = fopen(op2, "wb"); if (of) { fwrite(dd, 1, strlen(dd), of); fclose(of); } free(dd); }
          }
          if (bp) closedir(bp);
        }
        char op[2048]; snprintf(op, sizeof op, "%s/model.safetensors", out_dir);
        if (llmcodec_safetensors_write(op, out, nout) != 0) rc = -1;
    }
    for (int i = 0; i < nout; i++) { free((void*)out[i].name); free((void*)out[i].dtype); free((void*)out[i].shape); }
    free(out); free(keep); free(keepn);
    free(cfg); free(cfg3);
    for (int i = 0; i < nsh; i++) free(shards[i]); free(shards);
    return rc == 0 ? 0 : -1;
}
