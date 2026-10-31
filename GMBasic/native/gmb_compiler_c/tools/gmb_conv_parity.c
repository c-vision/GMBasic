/* gmb_conv_parity.c — parity SPOT-CHECK source-anchored di un container generico `GGUF`.
 *
 * Confronta il contenuto del container col SORGENTE safetensors per i tensori indicati (dense MLX):
 *   - il container (dtype F16) deve combaciare BIT-EXACT con f16(dequant_mlx_f32(sorgente));
 *   - il container (dtype GGML+q) conserva la quant del converter: si ri-quantizza la stessa sorgente
 *     con lo STESSO tipo ggml e si confrontano i byte (bit-exact), più l'errore f32 atteso.
 * group/bits: globali dal config + OVERRIDE per-tensore (oggetto `quantization`), come il converter.
 *
 * Uso: gmb_conv_parity <model_dir> <containerGGUF> <tensor.name> [<tensor.name> ...] */
#include "gmb_convert_internal.h"
#include "llmengine_moe_model.h"
#include "gmb_moe_storage.h"
#include "llmcodec_ggml_quants.h"
#include "llmcodec_gguf_read.h"   /* llmcodec_ggml_type_bytes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static char* vslurp(const char* p) {
    FILE* f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* b = (char*)malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f); b[n] = 0; return b;
}
static const char* jf(const char* s, const char* key) {
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(s, pat); if (!p) return NULL; p += strlen(pat);
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') ++p; return (*p==':') ? p+1 : NULL;
}
/* port di quant_override: override per-tensore di group_size/bits dall'oggetto `quantization`. */
static int quant_override(const char* cfg, const char* name, uint32_t* g, uint32_t* b) {
    if (cfg == NULL) return 0;
    const char* q = jf(cfg, "quantization"); if (q == NULL) return 0;
    while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') ++q; if (*q != '{') return 0; ++q;
    size_t nlen = strlen(name), best = 0; uint32_t bg = 0, bb = 0; int found = 0;
    while (*q) {
        while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r'||*q==',') ++q;
        if (*q != '"') break;
        ++q; const char* ks = q; while (*q && *q != '"') ++q; size_t kl = (size_t)(q - ks);
        if (*q == '"') ++q;
        while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') ++q;
        if (*q != ':') break; ++q; while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r') ++q;
        if (*q != '{') { while (*q && *q != ',' && *q != '}') ++q; continue; }
        int d = 0; const char* ob = q; const char* e = q;
        for (; *e; ++e) { if (*e=='{') d++; else if (*e=='}') { d--; if (d==0) { ++e; break; } } }
        if (kl > 0 && kl < nlen && name[kl]=='.' && kl >= best && strncmp(ks, name, kl)==0) {
            char sub[96]; size_t cl = (size_t)(e-ob); if (cl >= sizeof sub) cl = sizeof sub-1;
            memcpy(sub, ob, cl); sub[cl] = 0;
            const char* pg = jf(sub, "group_size"); const char* pb = jf(sub, "bits");
            long gv = pg ? strtol(pg,NULL,10) : -1, bv = pb ? strtol(pb,NULL,10) : -1;
            if (gv > 0 && bv > 0) { bg=(uint32_t)gv; bb=(uint32_t)bv; best=kl; found=1; }
        }
        q = e;
    }
    if (found) { *g = bg; *b = bb; } return found;
}
static void safe_name(const char* name, char* out, size_t cap) {
    size_t k = 0; for (const char* q = name; *q && k < 62; ++q) out[k++] = (*q=='.') ? '_' : *q; out[k]=0;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "uso: %s <model_dir> <containerGGUF> <tensor>...\n", argv[0]); return 2; }
    const char* model = argv[1];
    char ip[1200]; snprintf(ip, sizeof ip, "%s/model.safetensors.index.json", model);
    char* map = vslurp(ip); if (!map) { fprintf(stderr, "index mancante\n"); return 2; }
    char cp[1200]; snprintf(cp, sizeof cp, "%s/config.json", model); char* cj = vslurp(cp);
    uint32_t g0 = (uint32_t)gmb_conv_cfg_num(cj, "group_size", 64);
    uint32_t b0 = (uint32_t)gmb_conv_cfg_num(cj, "bits", 8);
    printf("config globale: group_size=%u bits=%u\n", g0, b0);

    GmbMoeStorage* st = NULL; GmbMoeModel* m = NULL;
    if (gmb_moe_storage_open(&st, argv[2], NULL) != GMB_MOE_OK) { fprintf(stderr,"storage\n"); return 2; }
    if (llmengine_moe_model_open_deprecated(&m, st, NULL) != GMB_MOE_OK) { fprintf(stderr,"model\n"); return 2; }

    int total = 0, ok = 0, bad = 0;
    for (int a = 3; a < argc; ++a) {
        const char* name = argv[a];
        /* sorgente: weight + scales + biases */
        char dt[16]={0}; uint32_t rows=0, cols=0; uint8_t* w=NULL; size_t wn=0;
        if (!read_tensor_full(model, map, name, dt, sizeof dt, &rows, &cols, &w, &wn)) { fprintf(stderr,"MISS src %s\n", name); continue; }
        char base[240]; snprintf(base,sizeof base,"%s",name); char* dot=strrchr(base,'.'); if(dot)*dot=0;
        char scn[260], bin[260]; snprintf(scn,sizeof scn,"%s.scales",base); snprintf(bin,sizeof bin,"%s.biases",base);
        char d1[8]={0},d2[8]={0}; uint32_t r1=0,c1=0,r2=0,c2=0; uint8_t *sc=NULL,*bi=NULL; size_t sn=0,bn=0;
        int have_mlx = (strcmp(dt,"U32")==0) &&
            read_tensor_full(model,map,scn,d1,sizeof d1,&r1,&c1,&sc,&sn) &&
            read_tensor_full(model,map,bin,d2,sizeof d2,&r2,&c2,&bi,&bn);
        uint32_t g = g0, b = b0; quant_override(cj, name, &g, &b);
        uint32_t lcols = (strcmp(dt,"U32")==0 && b>0 && (32%b)==0) ? cols*(32/b) : cols;
        size_t ln = (size_t)(rows?rows:1)*(lcols?lcols:1);
        float* f = (float*)malloc(ln*4);
        if (have_mlx) dequant_mlx_f32(w, sc, bi, rows, lcols, g, b, strcmp(d1,"F16")==0, f);
        else if (!st_to_f32(dt, w, ln, f)) { fprintf(stderr,"dtype src non gestito %s (%s)\n", dt, name); free(f); free(w); free(sc); free(bi); continue; }
        free(sc); free(bi);

        /* container */
        char safe[64]; safe_name(name, safe, sizeof safe); char cn[80]; snprintf(cn,sizeof cn,"g_%s",safe);
        GmbMoeTensorRecord rec;
        if (llmengine_moe_model_tensor_record_deprecated(m, cn, &rec) != GMB_MOE_OK) { fprintf(stderr,"MISS container %s (%s)\n", cn, name); free(f); free(w); continue; }
        uint64_t need = rec.size; uint8_t* got = (uint8_t*)malloc(need?need:1); size_t gn=0;
        if (llmengine_moe_model_read_tensor_deprecated(m, cn, got, need, &gn) != GMB_MOE_OK || gn != need) { fprintf(stderr,"READ fail %s\n", cn); free(got); free(f); free(w); continue; }
        total++;
        if (rec.dtype == GMB_MOE_DT_F16) {
            uint16_t* h = (uint16_t*)malloc(ln*2);
            for (size_t i=0;i<ln;++i) h[i]=llmcodec_f16_from_f32(f[i]);
            int eq = (rec.size == ln*2) && (memcmp(got,h,ln*2)==0);
            printf("%-16s F16 %ux%u : %s\n", have_mlx?"MLX":"src", rows, lcols, eq?"OK (bit-exact)":"MISMATCH");
            if (eq) ok++; else bad++;
            free(h);
        } else if (rec.dtype >= GMB_MOE_DT_GGML) {
            int t = (int)(rec.dtype - GMB_MOE_DT_GGML);
            uint64_t bb=0;
            if (llmcodec_ggml_type_bytes(t, (int64_t)ln, &bb)!=0 || bb!=rec.size) { printf("%s GGML t=%d: SIZE mismatch (rec=%llu atteso=%llu)\n", name, t, (unsigned long long)rec.size, (unsigned long long)bb); bad++; }
            else {
                uint8_t* blk=(uint8_t*)malloc(bb);
                if (llmcodec_ggml_quant_row(t, f, blk, (int64_t)ln, NULL)==0 && memcmp(blk,got,bb)==0) { printf("%-16s GGML t=%d %ux%u : OK (bit-exact col quant del converter)\n", name, t, rows, lcols); ok++; }
                else {
                    /* errore f32 atteso (dequant del quant) */
                    float* d=(float*)malloc(ln*4); llmcodec_ggml_dequant_row(t, got, d, (int64_t)ln);
                    double maxe=0, rms=0; for (size_t i=0;i<ln;++i){ double e=fabs((double)d[i]-f[i]); if(e>maxe)maxe=e; rms+=e*e; } rms=sqrt(rms/(double)ln);
                    printf("%-16s GGML t=%d %ux%u : byte-diff (max_abs=%.4g rms=%.4g)\n", name, t, rows, lcols, maxe, rms);
                    bad++; free(d);
                }
                free(blk);
            }
        } else { printf("%s: dtype container=%u (non F16/GGML) skip\n", name, rec.dtype); }
        free(got); free(f); free(w);
    }
    printf("PARITY: ok=%d bad=%d (su %d tensori)\n", ok, bad, total);
    llmengine_moe_model_close_deprecated(m); gmb_moe_storage_close(st); free(map); free(cj);
    return bad ? 1 : 0;
}
