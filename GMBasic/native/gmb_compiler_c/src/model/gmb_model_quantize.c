/* gmb_model_quantize.c — quantizza un modello HF/MLX (F32/F16/BF16) in formato MLX affine.
 *
 * Per ogni tensore 2D `.weight` quantizzabile (cols multiple di group e di 32/bits) produce
 * `<base>.weight` (U32 packed), `<base>.scales` (F16), `<base>.biases` (F16) via RTN affine
 * (`llmcodec_moe_affine_quantize`). Gli altri tensori (norm 1D, bias, già quantizzati) sono copiati.
 * Output: un singolo `model.safetensors` + config/tokenizer copiati (con `quantization` iniettato).
 * Primo slice: un solo file di output in RAM (modelli piccoli/medi). */
#include "gmb_st.h"
#include "llmcodec_moe_quant.h"
#include "llmengine_moe_quant_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

static float f16_to_f32(uint16_t h) {
    uint32_t s = (uint32_t)(h & 0x8000u) << 16, e = (h >> 10) & 0x1Fu, m = h & 0x3FFu, u;
    if (e == 0) { if (m == 0) u = s; else { int sh = 0; while (!(m & 0x400u)) { m <<= 1; sh++; } m &= 0x3FFu; u = s | ((uint32_t)(127 - 15 - sh + 1) << 23) | (m << 13); } }
    else if (e == 31) { if (m && !(m & 0x200u)) m |= 0x200u; u = s | 0x7F800000u | (m << 13); }
    else u = s | ((e - 15 + 127) << 23) | (m << 13);
    float f; memcpy(&f, &u, 4); return f;
}
static float bf16_to_f32(uint16_t h) { uint32_t u = ((uint32_t)h) << 16; float f; memcpy(&f, &u, 4); return f; }
static uint16_t f32_to_f16(float f) {
    uint32_t u; memcpy(&u, &f, 4);
    uint32_t s = (u >> 16) & 0x8000u;
    int32_t e = (int32_t)((u >> 23) & 0xFFu) - 127 + 15;
    uint32_t m = u & 0x7FFFFFu;
    if (e <= 0) {
        if (e < -10) return (uint16_t)s;
        m = (m | 0x800000u) >> (1 - e);
        return (uint16_t)(s | ((m + 0x1000u) >> 13));
    }
    if (e >= 31) return (uint16_t)(s | 0x7C00u | (m ? 0x200u : 0));
    return (uint16_t)(s | ((uint32_t)e << 10) | ((m + 0x1000u) >> 13));
}

/* ── RE-QUANT (2026-10-24): sorgente GIA' quantizzata (MLX affine U32+scales+biases) ──────────
 * Prima i tensori gia' quantizzati erano COPIATI (n_copy): "4-bit -> 2-bit" restava 4-bit.
 * Ora si dequantizzano in F32 (llmcodec_moe_affine_dequantize) e si ri-quantizzano ai bits
 * richiesti, tensore-per-tensore (RSS bounded). Parametri sorgente da config.json. */
static int read_src_qparams(const char* dir, uint32_t* g, uint32_t* b, int* present) {
    *g = 64; *b = 4; *present = 0;
    char p[1200]; snprintf(p, sizeof p, "%s/config.json", dir);
    FILE* f = fopen(p, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1<<24)) { fclose(f); return -1; }
    char* js = (char*)malloc((size_t)sz + 1); if (!js) { fclose(f); return -1; }
    size_t rd = fread(js, 1, (size_t)sz, f); js[rd] = 0; fclose(f);
    const char* qk = strstr(js, "\"quantization\"");
    if (qk) {
        const char* gs = strstr(qk, "\"group_size\""); const char* bs = strstr(qk, "\"bits\"");
        if (gs) { const char* c = strchr(gs, ':'); if (c) *g = (uint32_t)strtoul(c+1, NULL, 10); }
        if (bs) { const char* c = strchr(bs, ':'); if (c) *b = (uint32_t)strtoul(c+1, NULL, 10); }
        if (*g >= 2 && *b >= 2 && *b <= 8) *present = 1;
    }
    free(js); return 0;
}
static const GstEntry* ent_find(const GstEntry* es, uint32_t n, const char* name) {
    for (uint32_t i = 0; i < n; ++i) if (strcmp(es[i].name, name) == 0) return &es[i];
    return NULL;
}
/* Override di quantizzazione PER-MODULO del config sorgente: i checkpoint MLX reali NON sono
 * uniformi (es. Qwen3.8-Flash-Next: top 4/g32 ma router 8/g64, PLE 4/g32). Usare i bits globali
 * per il dequant produce pesi sbagliati (bug 2026-10-24: router 4-bit dequantizzato come 2-bit). */
typedef struct { char key[192]; uint32_t g, b; } QpItem;
static QpItem g_qp[1024]; static int g_nqp = 0;
/* Moduli con bits DIVERSI dal top-level (router 8-bit): vanno dichiarati nel config di output. */
static char g_ovr[1024][160]; static int g_novr = 0;
static void ovr_add(const char* name) {
    char base[160]; snprintf(base, sizeof base, "%s", name);
    size_t bl = strlen(base); if (bl > 7 && strcmp(base+bl-7, ".weight") == 0) base[bl-7] = 0;
    for (int i = 0; i < g_novr; ++i) if (strcmp(g_ovr[i], base) == 0) return;
    if (g_novr < 1024) snprintf(g_ovr[g_novr++], sizeof g_ovr[0], "%s", base);
}
static uint32_t g_qp_g = 64, g_qp_b = 4;

static int qp_load(const char* dir) {
    char pth[1200]; snprintf(pth, sizeof pth, "%s/config.json", dir);
    FILE* f = fopen(pth, "rb"); if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1<<24)) { fclose(f); return -1; }
    char* js = (char*)malloc((size_t)sz + 1); if (!js) { fclose(f); return -1; }
    size_t rd = fread(js, 1, (size_t)sz, f); js[rd] = 0; fclose(f);
    const char* q = strstr(js, "\"quantization\"");
    if (!q) { free(js); return 0; }
    const char* v = strchr(q, ':'); if (!v) { free(js); return 0; }
    ++v; while (*v==' '||*v=='\t'||*v=='\n'||*v=='\r') ++v;
    if (*v != '{') { free(js); return 0; }
    { const char* x = strstr(v, "\"group_size\""); const char* y = strstr(v, "\"bits\"");
      if (x) { const char* c = strchr(x, ':'); if (c) g_qp_g = (uint32_t)strtoul(c+1, NULL, 10); }
      if (y) { const char* c = strchr(y, ':'); if (c) g_qp_b = (uint32_t)strtoul(c+1, NULL, 10); } }
    const char* p = v + 1;
    while (*p && *p != '}') {
        while (*p && (*p==' '||*p==','||*p=='\t'||*p=='\n'||*p=='\r')) ++p;
        if (*p != '"') { p++; continue; }
        const char* ke = strchr(p+1, '"'); if (!ke) break;
        size_t kl = (size_t)(ke - p - 1);
        const char* c = strchr(ke, ':'); if (!c) break;
        const char* o = c + 1; while (*o==' '||*o=='\t'||*o=='\n'||*o=='\r') ++o;
        if (*o != '{') {
            /* salta il VALORE stringa/numero: fermarsi al close-quote della chiave faceva
             * trattare il valore (es. "affine") come chiave e SALTARE l'override seguente. */
            const char* np = o;
            if (*np == '"') { const char* q2 = strchr(np + 1, '"'); np = q2 ? q2 + 1 : np + 1; }
            p = np; continue;
        }
        const char* oe = o; int dep = 0;
        for (; *oe; ++oe) { if (*oe=='{') dep++; else if (*oe=='}') { dep--; if (!dep) break; } }
        if (g_nqp < 1024 && kl > 0 && kl < sizeof g_qp[0].key) {
            memcpy(g_qp[g_nqp].key, p+1, kl); g_qp[g_nqp].key[kl] = 0;
            g_qp[g_nqp].g = g_qp_g; g_qp[g_nqp].b = g_qp_b;
            { const char* bx = strstr(o, "\"bits\""); const char* gx = strstr(o, "\"group_size\"");
              if (bx && bx < oe) { const char* cc = strchr(bx, ':'); if (cc) g_qp[g_nqp].b = (uint32_t)strtoul(cc+1, NULL, 10); }
              if (gx && gx < oe) { const char* cc = strchr(gx, ':'); if (cc) g_qp[g_nqp].g = (uint32_t)strtoul(cc+1, NULL, 10); } }
            g_nqp++;
        }
        p = oe + 1;
    }
    free(js);
    return 0;
}
/* Parametri sorgente del tensore: override piu' LUNGO che e' prefisso del nome, altrimenti globali. */
static void qp_for(const char* name, uint32_t* g, uint32_t* b) {
    size_t best = 0; *g = g_qp_g; *b = g_qp_b;
    for (int i = 0; i < g_nqp; ++i) {
        size_t kl = strlen(g_qp[i].key);
        if (kl > best && strncmp(name, g_qp[i].key, kl) == 0) { best = kl; *g = g_qp[i].g; *b = g_qp[i].b; }
    }
}
/* Indice GLOBALE di scales/biases: nei checkpoint multi-shard gli ausiliari possono stare in uno
 * shard DIVERSO dal `.weight` (bug 2026-10-24: layer 14 -> aux mancante). */
typedef struct { char name[300]; char shard[96]; char dtype[8]; } AuxLoc;
static AuxLoc* g_aux = NULL; static int g_naux = 0, g_auxcap = 0;
static void aux_index_build(const char* dir) {
    if (g_aux) return;
    char shards[512][96]; int ns = 0;
    if (gst_collect_shards(dir, shards, 512, &ns) <= 0) return;
    for (int si = 0; si < ns; ++si) {
        char sp[1200]; snprintf(sp, sizeof sp, "%s/%s", dir, shards[si]);
        GstEntry* es = NULL; uint32_t ne = 0;
        if (gst_read_header(sp, &es, &ne, NULL) != 0) { free(es); continue; }
        for (uint32_t i = 0; i < ne; ++i) {
            size_t nl = strlen(es[i].name);
            if (!((nl > 7 && strcmp(es[i].name + nl - 7, ".scales") == 0) ||
                  (nl > 7 && strcmp(es[i].name + nl - 7, ".biases") == 0))) continue;
            if (g_naux == g_auxcap) { int nc = g_auxcap ? g_auxcap * 2 : 64;
                AuxLoc* na = (AuxLoc*)realloc(g_aux, (size_t)nc * sizeof(AuxLoc)); if (!na) return;
                g_aux = na; g_auxcap = nc; }
            snprintf(g_aux[g_naux].name, sizeof g_aux[g_naux].name, "%s", es[i].name);
            snprintf(g_aux[g_naux].shard, sizeof g_aux[g_naux].shard, "%s", shards[si]);
            snprintf(g_aux[g_naux].dtype, sizeof g_aux[g_naux].dtype, "%s", es[i].dtype);
            g_naux++;
        }
        free(es);
    }
}
static int aux_read(const char* dir, const char* name, uint8_t** raw, size_t* n, char* dt, size_t dtl) {
    for (int i = 0; i < g_naux; ++i) if (strcmp(g_aux[i].name, name) == 0) {
        if (dt && dtl) snprintf(dt, dtl, "%s", g_aux[i].dtype);
        char sp[1200]; snprintf(sp, sizeof sp, "%s/%s", dir, g_aux[i].shard);
        GstEntry* es = NULL; uint32_t ne = 0;
        if (gst_read_header(sp, &es, &ne, NULL) != 0) { free(es); return -1; }
        const GstEntry* e = ent_find(es, ne, name);
        int rc = e ? gst_read_tensor(sp, e, raw, n) : -1;
        if (rc == 0 && dt && dtl) snprintf(dt, dtl, "%s", e->dtype);
        free(es); return rc;
    }
    return -1;
}

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

/* Config JSON: inietta `"quantization":{"group_size":G,"bits":B}` se assente. */
static void copy_config_quant(const char* src_dir, const char* out_dir, uint32_t bits, uint32_t group) {
    char sp[1200], dp[1200];
    snprintf(sp, sizeof(sp), "%s/config.json", src_dir);
    snprintf(dp, sizeof(dp), "%s/config.json", out_dir);
    FILE* f = fopen(sp, "rb");
    if (!f) { copy_file(sp, dp); return; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)n + 1);
    if (!js) { fclose(f); return; }
    if (fread(js, 1, (size_t)n, f) != (size_t)n) { free(js); fclose(f); return; }
    js[n] = 0; fclose(f);
    /* `"quantization"` presente: se il valore è un OGGETTO -> copia; se `null` (o assente) -> inietta.
     * (Prima bastava la presenza della chiave: un `"quantization": null` di default faceva saltare
     * l'iniezione -> il modello quantizzato usciva con quantization null = non caricabile.) */
    { const char* qk = strstr(js, "\"quantization\"");
      if (qk) {
          const char* c = strchr(qk + 14, ':');
          const char* v = c ? c + 1 : NULL;
          while (v && (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')) v++;
          if (v && *v == '{') {
              /* RE-QUANT: il blocco `quantization` della SORGENTE descrive i bits VECCHI. Va
               * RISCRITTO con group/bits di destinazione, altrimenti il loader dequantizza con i
               * bits sbagliati (reshape error). Per-tensor overrides non applicabili -> si
               * sostituisce l'oggetto con la forma uniforme. */
              const char* qe = v; int dep = 0;
              for (; *qe; ++qe) { if (*qe == '{') dep++; else if (*qe == '}') { dep--; if (dep == 0) break; } }
              long pre = (long)(v - js);
              FILE* oo = fopen(dp, "wb");
              if (oo) {
                  fwrite(js, 1, (size_t)pre, oo);
                  fprintf(oo, "{\"group_size\":%u,\"bits\":%u,\"mode\":\"affine\"}", group, bits);
                  fwrite(qe + 1, 1, strlen(qe + 1), oo);
                  fclose(oo);
              }
              free(js); return;
          }
          if (v && strncmp(v, "null", 4) == 0) {
              long off = (long)(v - js);
              FILE* oo = fopen(dp, "wb");
              if (oo) { fwrite(js, 1, (size_t)off, oo);
                        fprintf(oo, "{\"group_size\":%u,\"bits\":%u}", group, bits);
                        fwrite(v + 4, 1, strlen(v + 4), oo); fclose(oo); }
              free(js); return;
          }
          copy_file(sp, dp); free(js); return;
      } }
    const char* brace = strchr(js, '{');
    FILE* o = fopen(dp, "wb");
    if (!o || !brace) { free(js); if (o) fclose(o); return; }
    fwrite(js, 1, (size_t)(brace - js) + 1, o);
    fprintf(o, "\"quantization\":{\"group_size\":%u,\"bits\":%u},", group, bits);
    fwrite(brace + 1, 1, strlen(brace + 1), o);
    fclose(o); free(js);
}

/* ── Report imatrix (DS4-Q1.b): righe `<layer> <expert> <count> <H energy...> <M energy...>`.
 * H = hidden (colonne gate/up), M = moe_intermediate (colonne down). Il tool divide le energie
 * quando conosce H (dalla shape del tensore). Vedi llm_ai_mlx_model_imatrix_write. ── */
typedef struct { int layer, expert, count, n; float* vals; } ImRow;
typedef struct { ImRow* rows; int n; } ImReport;

/* Fallback ggml LEGACY `.dat` (formato BINARIO di llama.cpp `save_imatrix_legacy`): `int32
 * n_entries`, poi per entry `int32 name_len` + `name` + `int32 ncall` + `int32 nval` + `nval float`.
 * Il nome `blk.N.*` -> (layer=N, expert=0). Serve a leggere gli imatrix prodotti da `llama-imatrix
 * --output-format dat`. */
static int im_load_legacy(const char* path, ImReport* r) {
    FILE* f = fopen(path, "rb"); if (!f) return -1;
    int32_t ne = 0;
    if (fread(&ne, 4, 1, f) != 1 || ne <= 0 || ne > (1 << 20)) { fclose(f); return -1; }
    for (int e = 0; e < ne; e++) {
        int32_t nlen = 0; if (fread(&nlen, 4, 1, f) != 1 || nlen <= 0 || nlen > 1024) break;
        char name[1025]; if (fread(name, 1, (size_t)nlen, f) != (size_t)nlen) break; name[nlen] = 0;
        int32_t ncall = 0, nval = 0;
        if (fread(&ncall, 4, 1, f) != 1 || fread(&nval, 4, 1, f) != 1) break;
        if (nval <= 0 || nval > (1 << 26)) break;
        float* vals = (float*)malloc((size_t)nval * 4);
        if (vals == NULL || fread(vals, 4, (size_t)nval, f) != (size_t)nval) { free(vals); break; }
        int layer = 0; { const char* b = strstr(name, "blk."); if (b) layer = atoi(b + 4); }
        r->rows = (ImRow*)realloc(r->rows, (size_t)(r->n + 1) * sizeof(ImRow));
        r->rows[r->n].layer = layer; r->rows[r->n].expert = 0; r->rows[r->n].count = ncall;
        r->rows[r->n].n = (int)nval; r->rows[r->n].vals = vals; r->n++;
    }
    fclose(f);
    return r->n > 0 ? 0 : -1;
}

static int im_load(const char* path, ImReport* r) {
    r->rows = NULL; r->n = 0;
    FILE* f = fopen(path, "r"); if (!f) return -1;
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == 0) continue;
        int layer, expert, count; int off = 0, adv = 0;
        if (sscanf(line, "%d %d %d%n", &layer, &expert, &count, &adv) < 3) continue;
        off = adv;
        int cap = 64, n = 0; float* vals = (float*)malloc((size_t)cap * sizeof(float));
        char* q = line + off;
        while (*q) {
            char* e = NULL; float v = strtof(q, &e);
            if (e == q) break;
            if (n == cap) { cap *= 2; vals = (float*)realloc(vals, (size_t)cap * sizeof(float)); }
            vals[n++] = v; q = e;
        }
        r->rows = (ImRow*)realloc(r->rows, (size_t)(r->n + 1) * sizeof(ImRow));
        r->rows[r->n].layer = layer; r->rows[r->n].expert = expert; r->rows[r->n].count = count;
        r->rows[r->n].n = n; r->rows[r->n].vals = vals;
        r->n++;
    }
    fclose(f);
    if (r->n == 0) return im_load_legacy(path, r);   /* fallback: formato binario ggml legacy `.dat` */
    return 0;
}
static const ImRow* im_find(const ImReport* r, int layer, int expert) {
    for (int i = 0; i < r->n; i++) if (r->rows[i].layer == layer && r->rows[i].expert == expert) return &r->rows[i];
    return NULL;
}
static void im_free(ImReport* r) { for (int i = 0; i < r->n; i++) free(r->rows[i].vals); free(r->rows); r->rows = NULL; r->n = 0; }

/* Indice del layer dal nome (`...layers.<N>.`), o -1. */
static int layer_of(const char* name) {
    const char* p = strstr(name, "layers.");
    if (!p) return -1;
    return atoi(p + 7);
}

/* Vero se il vettore di importanza e' tutto <= 0 (o NaN): in quel caso NON va usato come peso
 * (denominatore nullo -> errore 0 -> il tensore sembrerebbe esente da errore e non verrebbe mai
 * promosso). Succede con sezioni non accumulate (es. o_proj in un blocco attention senza wiring). */
static int im_all_zero(const float* v, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) if (v[i] > 0.0f) return 0;
    return 1;
}

/* Q2-b (§E4a): importanza per-colonna (imatrix) del tensore 2D, o NULL se non disponibile.
 * La riga del report e' `<layer> <expert> <count> <H energie...> <M energie...>`; la riga
 * dedicata all'attention ha `expert = -1` e layout `<qkv (hidden)> <o (n_heads*head_dim)>`.
 * Convenzione (identica a `gmb_model_quantize_ex`):
 *   - gate_proj / up_proj  -> prime `cols` energie della riga expert 0 (input del FFN, H = hidden);
 *   - down_proj            -> ULTIME `cols` (uscita SwiGLU, M = intermedio);
 *   - q/k/v_proj           -> prime `cols` della riga expert=-1 (input normalizzato condiviso, H);
 *   - o_proj / out_proj    -> ULTIME `cols` della riga expert=-1 (uscita attention).
 * Non coperti (-> NULL, non pesati): gli altri tensori e le sezioni assenti. La riga usata per il
 * FFN e' l'esperto 0 (densi ed esperti condivisi: stesso input normalizzato). Il puntatore e' dentro
 * il report: NON va liberato dal chiamante. */
static const float* im_imp_for(const ImReport* im, const char* tname, uint32_t cols) {
    if (im == NULL || im->n <= 0 || tname == NULL || cols == 0) return NULL;
    int layer = layer_of(tname);
    if (layer < 0) return NULL;
    int is_gateup = (strstr(tname, "gate_proj") != NULL || strstr(tname, "up_proj") != NULL);
    int is_down = (strstr(tname, "down_proj") != NULL);
    int is_qkv = (strstr(tname, "q_proj") != NULL || strstr(tname, "k_proj") != NULL ||
                  strstr(tname, "v_proj") != NULL);
    int is_o = (strstr(tname, "o_proj") != NULL || strstr(tname, "out_proj") != NULL);
    int attn = is_qkv || is_o;
    if (!is_gateup && !is_down && !attn) return NULL;
    const ImRow* row = im_find(im, layer, attn ? -1 : 0);
    if (row == NULL || row->vals == NULL || row->n < (int)cols) return NULL;
    const float* v = (is_down || is_o) ? row->vals + (row->n - (int)cols) : row->vals;
    return im_all_zero(v, cols) ? NULL : v;
}

int gmb_model_quantize_ex(const char* src_dir, const char* out_dir, uint32_t bits, uint32_t group,
                          const char* imatrix_path, uint32_t expert_bits, int verbose) {
    if (src_dir == NULL || out_dir == NULL) return 1;
    /* Quantizzatore affine 1..8 bit; il packing e' un flusso di bit contiguo -> larghezza
     * impacchettata = cols*bits/32 (per 2/4/8 coincide col vecchio schema). */
    if (bits < 1 || bits > 8 || group == 0) { fprintf(stderr, "bits/group invalidi\n"); return 1; }
    uint32_t src_group = 64, src_bits = 4; int src_q = 0;
    (void)read_src_qparams(src_dir, &src_group, &src_bits, &src_q);
    if (src_q) (void)qp_load(src_dir);
    ImReport im; im.rows = NULL; im.n = 0;
    if (imatrix_path != NULL && imatrix_path[0] != 0) {
        if (im_load(imatrix_path, &im) != 0) { fprintf(stderr, "imatrix non leggibile: %s\n", imatrix_path); return 1; }
        if (verbose) fprintf(stderr, "[imatrix] %d righe (layer/expert) da %s\n", im.n, imatrix_path);
    }
    mkdir_p(out_dir);
    aux_index_build(src_dir);
    char shards[512][96]; int ns = 0;
    int cs = gst_collect_shards(src_dir, shards, 512, &ns);
    if (cs <= 0) { if (cs == 0) fprintf(stderr, "nessun safetensors in %s\n", src_dir); return 1; }
    if (gst_verify_shards(src_dir) != 0) return 1;   /* K7: verifica PER-SHARD (index + header) */

    char op[1200]; snprintf(op, sizeof(op), "%s/model.safetensors", out_dir);
    GstWriter* w = NULL;
    if (gst_writer_create(&w, op) != 0) { fprintf(stderr, "writer fallito %s\n", op); return 1; }

    uint32_t n_q = 0, n_copy = 0;
    for (int si = 0; si < ns; ++si) {
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/%s", src_dir, shards[si]);
        GstEntry* ents = NULL; uint32_t ne = 0;
        if (gst_read_header(sp, &ents, &ne, NULL) != 0) { fprintf(stderr, "header non leggibile: %s\n", sp); free(ents); return 1; }
        for (uint32_t i = 0; i < ne; ++i) {
            GstEntry* e = &ents[i];
            uint8_t* raw = NULL; size_t rn = 0;
            if (gst_read_tensor(sp, e, &raw, &rn) != 0) { fprintf(stderr, "tensore non leggibile: %s\n", e->name); free(ents); return 1; }
            int ends_weight = (strlen(e->name) >= 7 && strcmp(e->name + strlen(e->name) - 7, ".weight") == 0);
            int is_fp = (strcmp(e->dtype, "F32") == 0 || strcmp(e->dtype, "F16") == 0 || strcmp(e->dtype, "BF16") == 0);
            /* RE-QUANT: sorgente U32 packed (gia' quantizzata) -> dequant in F32 e ri-quant. */
            int is_packed = src_q && (strcmp(e->dtype, "U32") == 0);
            uint32_t qg = src_group, qb = src_bits;
            if (is_packed) qp_for(e->name, &qg, &qb);
            int is_router = (strstr(e->name, ".mlp.gate.") != NULL || strstr(e->name, "shared_expert_gate") != NULL);
            const uint32_t obits = bits;
            const uint32_t ogroup = group;
            /* PASSTHRU calcolato sulla BASE (weight/scales/biases condividono i qparams): per gli
             * ausiliari is_packed e' false, ma vanno copiati se il loro peso passa intatto. */
            char pbase[300]; snprintf(pbase, sizeof pbase, "%s", e->name);
            { size_t bl = strlen(pbase);
              if (bl > 7 && strcmp(pbase + bl - 7, ".weight") == 0) pbase[bl - 7] = 0;
              else if (bl > 7 && strcmp(pbase + bl - 7, ".scales") == 0) pbase[bl - 7] = 0;
              else if (bl > 7 && strcmp(pbase + bl - 7, ".biases") == 0) pbase[bl - 7] = 0; }
            uint32_t pg = src_group, pb = src_bits; qp_for(pbase, &pg, &pb);
            int passthru = src_q && (obits == pb) && (ogroup == pg);
            /* Sorgente GIA' quantizzata: `.scales`/`.biases` (anche BF16) sono gli ausiliari del
             * `.weight` packed e vengono CONSUMATI dalla ri-quantizzazione. Copiarli produceva
             * duplicati (bf16 + f16) e gonfiava l'output (bug 2026-10-24: 81GB invece di ~50). */
            if (src_q && !passthru) {
                size_t nl = strlen(e->name);
                if ((nl > 7 && strcmp(e->name + nl - 7, ".scales") == 0) ||
                    (nl > 7 && strcmp(e->name + nl - 7, ".biases") == 0)) { free(raw); continue; }
            }
            uint64_t rows = e->ndim >= 1 ? e->shape[0] : 1, cols = e->ndim >= 2 ? e->shape[1] : 1;
            if (is_packed && qb && e->ndim >= 2) cols = (uint64_t)e->shape[1] * 32u / qb;
            /* Sensibilita': il ROUTER (`mlp.gate`, `shared_expert_gate`) decide quali esperti
             * eseguire; a 2-bit il routing cambia e l'output diverge (misurato 2026-10-24:
             * IDS 77 vs 198). Resta a 8-bit/g64 (minuscolo in byte). */
            int can_q = (is_fp || (is_packed && e->ndim == 2)) && e->ndim == 2 && ends_weight &&
                        (cols % ogroup) == 0 && (((uint64_t)cols * obits) % 32u) == 0 && rows > 0;
            /* §E2 step 2: esperti 3D (pattern `switch_mlp.*`/`experts.*`): quantizza PER-ESPERTO,
             * layout MLX identico al 2D con un asse esperto in testa -> qualunque MoE rientra. */
            /* §E2 step 3: esperti 3D quantizzati PER-ESPERTO, con bits dedicato (`expert_bits`) e,
             * se disponibile un report imatrix, errore PESATO per-colonna (gate/up = prime H
             * energie, down = ultime M). Senza imatrix -> RTN invariato. */
            uint32_t ebits = (expert_bits >= 1 && expert_bits <= 8) ? expert_bits : obits;
            /* ROUTER in PLAIN F16: e' minuscolo (<=~130MB totali) e 2-bit cambierebbe il routing.
             * L'engine sceglie il linear plain quando mancano scales/biases -> nessun supporto
             * per-modulo necessario. */
            if (!passthru && is_router && is_packed && e->ndim == 2 && ends_weight) {
                float* xr = (float*)malloc((size_t)rows * cols * sizeof(float));
                uint8_t* sraw = NULL; uint8_t* braw = NULL; char sdt[8] = ""; size_t srn = 0, brn = 0;
                char b2n[300]; snprintf(b2n, sizeof b2n, "%s", e->name); b2n[strlen(b2n) - 7] = 0;
                char sn2[340], bn2[340];
                snprintf(sn2, sizeof sn2, "%s.scales", b2n); snprintf(bn2, sizeof bn2, "%s.biases", b2n);
                const GstEntry* se = ent_find(ents, ne, sn2); const GstEntry* be = ent_find(ents, ne, bn2);
                int rok = 0;
                if (se && be) { rok = (gst_read_tensor(sp, (GstEntry*)se, &sraw, &srn) == 0 &&
                                      gst_read_tensor(sp, (GstEntry*)be, &braw, &brn) == 0);
                                if (rok) snprintf(sdt, sizeof sdt, "%s", se->dtype); }
                if (!rok) rok = (aux_read(src_dir, sn2, &sraw, &srn, sdt, sizeof sdt) == 0 &&
                                 aux_read(src_dir, bn2, &braw, &brn, NULL, 0) == 0);
                if (xr && rok) {
                    const int scf = (strcmp(sdt, "F16") == 0) ? 1 : 0;
                    llmcodec_moe_affine_dequantize((const uint32_t*)raw, (const uint16_t*)sraw,
                        (const uint16_t*)braw, (uint32_t)rows, (uint32_t)cols, qg, qb, scf, xr);
                    size_t nb = (size_t)rows * (size_t)cols * 2;
                    uint16_t* hf = (uint16_t*)malloc(nb);
                    if (hf) {
                        for (size_t k2 = 0; k2 < (size_t)rows * (size_t)cols; ++k2) hf[k2] = f32_to_f16(xr[k2]);
                        uint64_t shp2[2] = { rows, cols };
                        gst_writer_add(w, e->name, "F16", 2, shp2, hf, nb);
                        free(hf);
                    }
                }
                free(xr); free(sraw); free(braw); free(raw);
                continue;
            }
            if (passthru && is_packed && ends_weight && (e->ndim == 2 || e->ndim == 3)) {
                gst_writer_add(w, e->name, "U32", e->ndim, e->shape, raw, rn);
                n_copy++; free(raw); continue;
            }
            uint64_t ec_q = e->ndim == 3 ? e->shape[2] : 0;
            if (is_packed && qb && e->ndim == 3) ec_q = (uint64_t)e->shape[2] * 32u / qb;
            int can_q3 = (is_fp || (is_packed && e->ndim == 3)) && e->ndim == 3 && ends_weight &&
                         ec_q > 0 && (ec_q % ogroup) == 0 && (((uint64_t)ec_q * ebits) % 32u) == 0 &&
                         e->shape[0] > 0 && e->shape[1] > 0;
            int is_dn = (strstr(e->name, "down_proj") != NULL);
            int e_layer = layer_of(e->name);
            if (can_q3) {
                uint64_t nE = e->shape[0], er = e->shape[1], ec = e->shape[2];
                if (is_packed) ec = (uint64_t)e->shape[2] * 32u / qb;
                size_t xrow = (size_t)er * ec;
                size_t wb = llmcodec_moe_affine_w_bytes((uint32_t)er, (uint32_t)ec, ebits);
                size_t sb = llmcodec_moe_affine_sc_bytes((uint32_t)er, (uint32_t)ec, ogroup);
                float* x = (float*)malloc((size_t)nE * xrow * sizeof(float));
                uint32_t* wo = (uint32_t*)malloc((size_t)nE * wb);
                uint16_t* so = (uint16_t*)malloc((size_t)nE * sb);
                uint16_t* bo = (uint16_t*)malloc((size_t)nE * sb);
                if (!x || !wo || !so || !bo) { free(x); free(wo); free(so); free(bo); free(raw); free(ents); return 1; }
#if defined(__APPLE__)
                __block uint32_t n_w = 0;   /* __block = estensione Blocks (Apple): su Linux è un uint32 normale */
#else
                uint32_t n_w = 0;
#endif
                uint8_t* sraw = NULL; uint8_t* braw = NULL; char sdt[8] = "";
                const uint32_t* wpk = NULL; const uint16_t* sck = NULL; const uint16_t* bik = NULL;
                if (is_packed) {
                    char b3n[300]; snprintf(b3n, sizeof b3n, "%s", e->name); b3n[strlen(b3n) - 7] = 0;
                    char sn3[340], bn3[340];
                    snprintf(sn3, sizeof sn3, "%s.scales", b3n); snprintf(bn3, sizeof bn3, "%s.biases", b3n);
                    const GstEntry* se = ent_find(ents, ne, sn3); const GstEntry* be = ent_find(ents, ne, bn3);
                    size_t srn = 0, brn = 0;
                    int rok = 0;
                    if (se && be) { rok = (gst_read_tensor(sp, (GstEntry*)se, &sraw, &srn) == 0 &&
                                           gst_read_tensor(sp, (GstEntry*)be, &braw, &brn) == 0);
                                    if (rok) snprintf(sdt, sizeof sdt, "%s", se->dtype); }
                    if (!rok) rok = (aux_read(src_dir, sn3, &sraw, &srn, sdt, sizeof sdt) == 0 &&
                                     aux_read(src_dir, bn3, &braw, &brn, NULL, 0) == 0);
                    if (!rok) {
                        fprintf(stderr, "aux (scales/biases) mancante per %s\n", e->name);
                        free(x); free(wo); free(so); free(bo); free(raw); free(ents); return 1;
                    }
                    wpk = (const uint32_t*)raw; sck = (const uint16_t*)sraw; bik = (const uint16_t*)braw;
                }
                const uint32_t ng = (uint32_t)(ec / qg);
                const int scf = (strcmp(sdt, "F16") == 0) ? 1 : 0;
#if defined(__APPLE__)
                __block int qerr2 = 0;
                dispatch_apply((size_t)nE, DISPATCH_APPLY_AUTO, ^(size_t exi) {
                    uint64_t ex = (uint64_t)exi;
                    if (qerr2) return;
#else
                for (uint64_t ex = 0; ex < nE; ++ex) {
#endif
                    if (is_packed) {
                        llmcodec_moe_affine_dequantize(wpk + ex * (size_t)er * (size_t)e->shape[2],
                            sck + ex * (size_t)er * ng,
                            bik + ex * (size_t)er * ng, (uint32_t)er, (uint32_t)ec, qg, qb, scf,
                            x + ex * xrow);
                    } else if (strcmp(e->dtype, "F32") == 0) memcpy(x + ex * xrow, raw + ex * xrow * 4, xrow * 4);
                    else for (size_t k = 0; k < xrow; ++k) {
                        uint16_t h; memcpy(&h, raw + (ex * xrow + k) * 2, 2);
                        x[ex * xrow + k] = (strcmp(e->dtype, "F16") == 0) ? f16_to_f32(h) : bf16_to_f32(h);
                    }
                    const float* imp = NULL;
                    const ImRow* row = (im.n > 0 && e_layer >= 0) ? im_find(&im, e_layer, (int)ex) : NULL;
                    if (row != NULL) {
                        int H = row->n - (int)ec;
                        if (is_dn) imp = (H >= 0) ? row->vals + H : NULL;
                        else       imp = (row->n >= (int)ec) ? row->vals : NULL;
                    }
                    int qr;
                    if (imp != NULL)
                        qr = llmcodec_moe_affine_quantize_weighted(x + ex * xrow, imp, (uint32_t)er, (uint32_t)ec,
                                ogroup, ebits, wo + (size_t)ex * (wb / 4), so + (size_t)ex * (sb / 2), bo + (size_t)ex * (sb / 2));
                    else
                        qr = llmcodec_moe_affine_quantize(x + ex * xrow, (uint32_t)er, (uint32_t)ec, ogroup, ebits,
                                wo + (size_t)ex * (wb / 4), so + (size_t)ex * (sb / 2), bo + (size_t)ex * (sb / 2));
                    if (qr != 0) {
#if defined(__APPLE__)
                        qerr2 = 1; return;
#else
                        fprintf(stderr, "quantize 3D fallita: %s (expert %llu)\n", e->name, (unsigned long long)ex);
                        free(x); free(wo); free(so); free(bo); free(raw); free(ents); im_free(&im); return 1;
#endif
                    }
                    if (imp) n_w++;
#if defined(__APPLE__)
                });
                if (qerr2) {
                    fprintf(stderr, "quantize 3D fallita: %s\n", e->name);
                    free(x); free(wo); free(so); free(bo); free(raw); free(ents); im_free(&im); return 1;
                }
#else
                }
#endif
                char base3[256]; snprintf(base3, sizeof(base3), "%s", e->name); base3[strlen(base3) - 7] = 0;
                char nm3[300]; uint64_t shp3[3];
                snprintf(nm3, sizeof(nm3), "%s.weight", base3); shp3[0] = nE; shp3[1] = er; shp3[2] = (int)(((uint64_t)ec * ebits) / 32u);
                gst_writer_add(w, nm3, "U32", 3, shp3, wo, (size_t)nE * wb);
                snprintf(nm3, sizeof(nm3), "%s.scales", base3); shp3[2] = ec / ogroup;
                gst_writer_add(w, nm3, "F16", 3, shp3, so, (size_t)nE * sb);
                snprintf(nm3, sizeof(nm3), "%s.biases", base3);
                gst_writer_add(w, nm3, "F16", 3, shp3, bo, (size_t)nE * sb);
                if (verbose) fprintf(stderr, "[quant3] %-52s %s %llux%llux%llu -> Q%u/g%u%s\n", e->name, e->dtype,
                                     (unsigned long long)nE, (unsigned long long)er, (unsigned long long)ec, ebits, ogroup,
                                     n_w ? " (imatrix)" : "");
                n_q++;
                free(x); free(wo); free(so); free(bo); free(sraw); free(braw);
            } else if (can_q) {
                float* x = (float*)malloc((size_t)rows * cols * sizeof(float));
                uint32_t* wo = (uint32_t*)malloc(llmcodec_moe_affine_w_bytes((uint32_t)rows, (uint32_t)cols, obits));
                uint16_t* so = (uint16_t*)malloc(llmcodec_moe_affine_sc_bytes((uint32_t)rows, (uint32_t)cols, ogroup));
                uint16_t* bo = (uint16_t*)malloc(llmcodec_moe_affine_sc_bytes((uint32_t)rows, (uint32_t)cols, ogroup));
                if (!x || !wo || !so || !bo) { free(x); free(wo); free(so); free(bo); free(raw); free(ents); return 1; }
                uint8_t* sraw = NULL; uint8_t* braw = NULL;
                if (is_packed) {
                    char b2n[300]; snprintf(b2n, sizeof b2n, "%s", e->name); b2n[strlen(b2n) - 7] = 0;
                    char sn2[340], bn2[340];
                    snprintf(sn2, sizeof sn2, "%s.scales", b2n); snprintf(bn2, sizeof bn2, "%s.biases", b2n);
                    const GstEntry* se = ent_find(ents, ne, sn2); const GstEntry* be = ent_find(ents, ne, bn2);
                    size_t srn = 0, brn = 0;
                    int rok = 0; char sdt[8] = "";
                    if (se && be) { rok = (gst_read_tensor(sp, (GstEntry*)se, &sraw, &srn) == 0 &&
                                           gst_read_tensor(sp, (GstEntry*)be, &braw, &brn) == 0);
                                    if (rok) snprintf(sdt, sizeof sdt, "%s", se->dtype); }
                    if (!rok) rok = (aux_read(src_dir, sn2, &sraw, &srn, sdt, sizeof sdt) == 0 &&
                                     aux_read(src_dir, bn2, &braw, &brn, NULL, 0) == 0);
                    const int scf = (strcmp(sdt, "F16") == 0) ? 1 : 0;
                    if (!rok) {
                        fprintf(stderr, "aux (scales/biases) mancante per %s\n", e->name);
                        free(x); free(wo); free(so); free(bo); free(raw); free(ents); return 1;
                    }
                    llmcodec_moe_affine_dequantize((const uint32_t*)raw, (const uint16_t*)sraw,
                        (const uint16_t*)braw, (uint32_t)rows, (uint32_t)cols, qg, qb, scf, x);
                } else if (strcmp(e->dtype, "F32") == 0) memcpy(x, raw, (size_t)rows * cols * 4);
                else for (size_t k = 0; k < (size_t)rows * cols; ++k) {
                    uint16_t h; memcpy(&h, raw + k * 2, 2);
                    x[k] = (strcmp(e->dtype, "F16") == 0) ? f16_to_f32(h) : bf16_to_f32(h);
                }
                if (llmcodec_moe_affine_quantize(x, (uint32_t)rows, (uint32_t)cols, ogroup, obits, wo, so, bo) != 0) {
                    fprintf(stderr, "quantize fallita: %s\n", e->name); free(x); free(wo); free(so); free(bo); free(raw); free(ents); return 1;
                }
                char base[256]; snprintf(base, sizeof(base), "%s", e->name); base[strlen(base) - 7] = 0;
                char nm[300]; uint64_t shp[2];
                uint32_t packed_cols = (uint32_t)(((uint64_t)cols * obits) / 32u);
                snprintf(nm, sizeof(nm), "%s.weight", base); shp[0] = rows; shp[1] = (int)packed_cols;
                gst_writer_add(w, nm, "U32", 2, shp, wo, (size_t)rows * packed_cols * 4);
                snprintf(nm, sizeof(nm), "%s.scales", base); shp[0] = rows; shp[1] = cols / ogroup;
                gst_writer_add(w, nm, "F16", 2, shp, so, (size_t)rows * (cols / ogroup) * 2);
                snprintf(nm, sizeof(nm), "%s.biases", base);
                gst_writer_add(w, nm, "F16", 2, shp, bo, (size_t)rows * (cols / ogroup) * 2);
                if (verbose) fprintf(stderr, "[quant] %-52s %s %llux%llu -> Q%u/g%u\n", e->name, e->dtype,
                                     (unsigned long long)rows, (unsigned long long)cols, obits, ogroup);
                n_q++;
                free(x); free(wo); free(so); free(bo); free(sraw); free(braw);
            } else {
                gst_writer_add(w, e->name, e->dtype, e->ndim, e->shape, raw, rn);
                n_copy++;
            }
            free(raw);
        }
        free(ents);
    }
    if (gst_writer_finish(w) != 0) { fprintf(stderr, "scrittura fallita %s\n", op); return 1; }
    copy_config_quant(src_dir, out_dir, bits, group);
    const char* extra[] = { "tokenizer.json", "tokenizer_config.json", "merges.txt",
                            "vocab.json", "special_tokens_map.json", "generation_config.json" };
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i) {
        char a[1200], b[1200];
        snprintf(a, sizeof(a), "%s/%s", src_dir, extra[i]);
        snprintf(b, sizeof(b), "%s/%s", out_dir, extra[i]);
        copy_file(a, b);
    }
    if (verbose) printf("gmb_model_quantize: ok  %u quantizzati, %u copiati, Q%u/g%u%s -> %s\n", n_q, n_copy, bits, group,
           expert_bits >= 2 ? " (expert-bits)" : "", op);
    im_free(&im);
    free(g_aux); g_aux = NULL; g_naux = g_auxcap = 0;
    return 0;
}

int gmb_model_quantize(const char* src_dir, const char* out_dir, uint32_t bits, uint32_t group, int verbose) {
    return gmb_model_quantize_ex(src_dir, out_dir, bits, group, NULL, 0, verbose);
}

/* ── §E3: mixed-precision per-layer guidata dalla sensibilità (due pass: misura → alloca → quantizza). ── */

typedef struct { char* name; uint64_t nparams; double rel[2]; } AutoTensor;

static void auto_free(AutoTensor* t, int n) { for (int i = 0; i < n; ++i) free(t[i].name); free(t); }
static int auto_find(const AutoTensor* t, int n, const char* mod) {
    for (int i = 0; i < n; ++i) if (strcmp(t[i].name, mod) == 0) return i;
    return -1;
}

/* Converte un tensore F32/F16/BF16 in float (malloc). 0 = ok. */
static int tensor_to_f32(const GstEntry* e, const uint8_t* raw, float** out, size_t* nw) {
    size_t n = 1; for (int d = 0; d < (int)e->ndim; ++d) n *= (size_t)e->shape[d];
    float* x = (float*)malloc(n * sizeof(float));
    if (!x) return -1;
    if (strcmp(e->dtype, "F32") == 0) memcpy(x, raw, n * 4);
    else for (size_t k = 0; k < n; ++k) {
        uint16_t h; memcpy(&h, raw + k * 2, 2);
        x[k] = (strcmp(e->dtype, "F16") == 0) ? f16_to_f32(h) : bf16_to_f32(h);
    }
    *out = x; *nw = n; return 0;
}

/* Scrive il dict di quantizzazione (globale + override per-path dei tensori a `cand[1]`). */
static void write_quant_dict(FILE* o, uint32_t group, uint32_t def_bits,
                             const AutoTensor* t, const int* oc, uint32_t bits_hi, int n) {
    fprintf(o, "{\"group_size\":%u,\"bits\":%u,\"mode\":\"affine\"", group, def_bits);
    for (int i = 0; i < n; ++i)
        if (oc[i] == 1)
            fprintf(o, ",\"%s\":{\"group_size\":%u,\"bits\":%u,\"mode\":\"affine\"}", t[i].name, group, bits_hi);
    fprintf(o, "}");
}

/* Config.json con `quantization` per-path + `quantization_config` (formato mlx_lm quantize_model). */
static void write_config_mixed(const char* src_dir, const char* out_dir, uint32_t group, uint32_t def_bits,
                               const AutoTensor* t, const int* oc, uint32_t bits_hi, int n, int verbose) {
    char sp[1200], dp[1200];
    snprintf(sp, sizeof(sp), "%s/config.json", src_dir);
    snprintf(dp, sizeof(dp), "%s/config.json", out_dir);
    FILE* f = fopen(sp, "rb");
    if (!f) { copy_file(sp, dp); return; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)sz + 1);
    if (!js || fread(js, 1, (size_t)sz, f) != (size_t)sz) { free(js); fclose(f); return; }
    js[sz] = 0; fclose(f);
    if (strstr(js, "\"quantization\"") != NULL) { copy_file(sp, dp); free(js); if (verbose) fprintf(stderr, "[auto] config gia' con quantization: copiato\n"); return; }
    const char* brace = strchr(js, '{');
    FILE* o = fopen(dp, "wb");
    if (!o || !brace) { free(js); if (o) fclose(o); return; }
    fwrite(js, 1, (size_t)(brace - js) + 1, o);
    fprintf(o, "\"quantization\":");
    write_quant_dict(o, group, def_bits, t, oc, bits_hi, n);
    fprintf(o, ",\"quantization_config\":");
    write_quant_dict(o, group, def_bits, t, oc, bits_hi, n);
    fprintf(o, ",");
    fwrite(brace + 1, 1, strlen(brace + 1), o);
    fclose(o); free(js);
}

/* Implementazione comune (§E3 + §E4a/Q2-b). `im` != NULL = sensibilità PESATA dall'importanza
 * per-colonna del report imatrix (solo tensori del FFN, vedi `im_imp_for`). */
static int auto_impl(const char* src_dir, const char* out_dir, uint32_t group,
                     uint32_t bits_lo, uint32_t bits_hi, double target_bpw, ImReport* im, int verbose) {
    if (src_dir == NULL || out_dir == NULL) return 1;
    if (bits_lo < 2 || bits_hi > 8 || bits_hi <= bits_lo || bits_lo == 7 || bits_hi == 7 || group == 0) {
        fprintf(stderr, "auto: bits_lo/bits_hi/group invalidi\n"); return 1;
    }

    char shards[512][96]; int ns = 0;
    int cs = gst_collect_shards(src_dir, shards, 512, &ns);
    if (cs <= 0) { if (cs == 0) fprintf(stderr, "nessun safetensors in %s\n", src_dir); return 1; }
    if (gst_verify_shards(src_dir) != 0) return 1;   /* K7: verifica PER-SHARD (index + header) */
    mkdir_p(out_dir);

    /* Pass 1: misura l'errore relativo (sensibilità statica sui pesi, stile mlx-optiq; PESATO
     * per-colonna quando `im` fornisce l'importanza del tensore, §E4a). */
    AutoTensor* ts = NULL; int n = 0, cap = 0, n_im = 0;
    const int cand[2] = { (int)bits_lo, (int)bits_hi };
    for (int si = 0; si < ns; ++si) {
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/%s", src_dir, shards[si]);
        GstEntry* ents = NULL; uint32_t ne = 0;
        if (gst_read_header(sp, &ents, &ne, NULL) != 0) { free(ents); auto_free(ts, n); return 1; }
        for (uint32_t i = 0; i < ne; ++i) {
            GstEntry* e = &ents[i];
            int ends_weight = (strlen(e->name) >= 7 && strcmp(e->name + strlen(e->name) - 7, ".weight") == 0);
            int is_fp = (strcmp(e->dtype, "F32") == 0 || strcmp(e->dtype, "F16") == 0 || strcmp(e->dtype, "BF16") == 0);
            uint64_t rows = e->ndim >= 1 ? e->shape[0] : 1, cols = e->ndim >= 2 ? e->shape[1] : 1;
            int can_q = is_fp && e->ndim == 2 && ends_weight && rows > 0 &&
                        (cols % group) == 0 && (((uint64_t)cols * bits_lo) % 32u) == 0 && (((uint64_t)cols * bits_hi) % 32u) == 0;
            if (!can_q) continue;
            uint8_t* raw = NULL; size_t rn = 0;
            if (gst_read_tensor(sp, e, &raw, &rn) != 0) { free(raw); free(ents); auto_free(ts, n); return 1; }
            float* x = NULL; size_t nw = 0;
            if (tensor_to_f32(e, raw, &x, &nw) != 0) { free(raw); free(ents); auto_free(ts, n); return 1; }
            char mod[256]; snprintf(mod, sizeof(mod), "%s", e->name); mod[strlen(mod) - 7] = 0;
            if (n == cap) { cap = cap ? cap * 2 : 64; AutoTensor* nt = (AutoTensor*)realloc(ts, (size_t)cap * sizeof(AutoTensor)); if (!nt) { free(x); free(raw); free(ents); auto_free(ts, n); return 1; } ts = nt; }
            ts[n].name = strdup(mod);
            ts[n].nparams = rows * cols;
            const float* imp = im_imp_for(im, e->name, (uint32_t)cols);
            if (imp != NULL) n_im++;
            double r0 = 0, r1 = 0;
            if (llmengine_moe_quant_rel_error(x, imp, (uint32_t)rows, (uint32_t)cols, group, bits_lo, &r0) != 0 ||
                llmengine_moe_quant_rel_error(x, imp, (uint32_t)rows, (uint32_t)cols, group, bits_hi, &r1) != 0) {
                fprintf(stderr, "auto: misura fallita su %s\n", e->name); free(x); free(raw); free(ents); auto_free(ts, n); return 1;
            }
            ts[n].rel[0] = r0; ts[n].rel[1] = r1; n++;
            free(x); free(raw);
        }
        free(ents);
    }
    if (n == 0) { fprintf(stderr, "auto: nessun tensore 2D quantizzabile\n"); auto_free(ts, n); return 1; }

    uint64_t* np = (uint64_t*)malloc((size_t)n * sizeof(uint64_t));
    double* rel = (double*)malloc((size_t)n * 2 * sizeof(double));
    int* oc = (int*)malloc((size_t)n * sizeof(int));
    if (!np || !rel || !oc) { free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
    for (int i = 0; i < n; ++i) { np[i] = ts[i].nparams; rel[i * 2] = ts[i].rel[0]; rel[i * 2 + 1] = ts[i].rel[1]; }
    if (llmengine_moe_quant_alloc_bits(np, rel, n, 2, cand, target_bpw, oc) != 0) { free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
    int n_hi = 0; double bits_sum = 0.0, par_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        if (oc[i] == 1) n_hi++;
        bits_sum += (double)ts[i].nparams * cand[oc[i]];
        par_sum += (double)ts[i].nparams;
    }
    if (verbose) fprintf(stderr, "[auto] %d tensori (%d pesati da imatrix): %d a Q%u, %d a Q%u, bpw medio %.3f (target %.3f)\n",
                         n, n_im, n_hi, bits_hi, n - n_hi, bits_lo, par_sum > 0 ? bits_sum / par_sum : 0.0, target_bpw);

    /* Pass 2: quantizza ogni tensore al bit assegnato (2D); 3D esperti al bit basso; altri copiati. */
    char op[1200]; snprintf(op, sizeof(op), "%s/model.safetensors", out_dir);
    GstWriter* w = NULL;
    if (gst_writer_create(&w, op) != 0) { free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
    uint32_t n_q = 0, n_copy = 0;
    for (int si = 0; si < ns; ++si) {
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/%s", src_dir, shards[si]);
        GstEntry* ents = NULL; uint32_t ne = 0;
        if (gst_read_header(sp, &ents, &ne, NULL) != 0) { free(ents); free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
        for (uint32_t i = 0; i < ne; ++i) {
            GstEntry* e = &ents[i];
            uint8_t* raw = NULL; size_t rn = 0;
            if (gst_read_tensor(sp, e, &raw, &rn) != 0) { free(raw); free(ents); free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
            int ends_weight = (strlen(e->name) >= 7 && strcmp(e->name + strlen(e->name) - 7, ".weight") == 0);
            int is_fp = (strcmp(e->dtype, "F32") == 0 || strcmp(e->dtype, "F16") == 0 || strcmp(e->dtype, "BF16") == 0);
            uint64_t rows = e->ndim >= 1 ? e->shape[0] : 1, cols = e->ndim >= 2 ? e->shape[1] : 1;
            char mod[256]; snprintf(mod, sizeof(mod), "%s", ends_weight ? e->name : ""); if (ends_weight) mod[strlen(mod) - 7] = 0;
            int idx = (is_fp && e->ndim == 2 && ends_weight) ? auto_find(ts, n, mod) : -1;
            int can_q3 = is_fp && e->ndim == 3 && ends_weight && e->shape[2] > 0 &&
                         (e->shape[2] % group) == 0 && (((uint64_t)e->shape[2] * bits_lo) % 32u) == 0 && e->shape[0] > 0 && e->shape[1] > 0;
            if (idx >= 0) {
                uint32_t qb = (uint32_t)cand[oc[idx]];
                float* x = NULL; size_t nw = 0;
                if (tensor_to_f32(e, raw, &x, &nw) != 0) { free(raw); free(ents); free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
                uint32_t* wo = (uint32_t*)malloc(llmcodec_moe_affine_w_bytes((uint32_t)rows, (uint32_t)cols, qb));
                uint16_t* so = (uint16_t*)malloc(llmcodec_moe_affine_sc_bytes((uint32_t)rows, (uint32_t)cols, group));
                uint16_t* bo = (uint16_t*)malloc(llmcodec_moe_affine_sc_bytes((uint32_t)rows, (uint32_t)cols, group));
                if (!wo || !so || !bo || llmcodec_moe_affine_quantize(x, (uint32_t)rows, (uint32_t)cols, group, qb, wo, so, bo) != 0) {
                    fprintf(stderr, "auto: quantize fallita su %s\n", e->name);
                    free(x); free(wo); free(so); free(bo); free(raw); free(ents); free(np); free(rel); free(oc); auto_free(ts, n); return 1;
                }
                char base[256]; snprintf(base, sizeof(base), "%s", e->name); base[strlen(base) - 7] = 0;
                char nm[300]; uint64_t shp[2];
                uint32_t packed_cols = (uint32_t)(((uint64_t)cols * qb) / 32u);
                snprintf(nm, sizeof(nm), "%s.weight", base); shp[0] = rows; shp[1] = (int)packed_cols;
                gst_writer_add(w, nm, "U32", 2, shp, wo, (size_t)rows * packed_cols * 4);
                snprintf(nm, sizeof(nm), "%s.scales", base); shp[0] = rows; shp[1] = cols / group;
                gst_writer_add(w, nm, "F16", 2, shp, so, (size_t)rows * (cols / group) * 2);
                snprintf(nm, sizeof(nm), "%s.biases", base);
                gst_writer_add(w, nm, "F16", 2, shp, bo, (size_t)rows * (cols / group) * 2);
                if (verbose) fprintf(stderr, "[auto] %-54s Q%u (rel %.3e -> %.3e)\n", e->name, qb, ts[idx].rel[0], ts[idx].rel[1]);
                n_q++;
                free(x); free(wo); free(so); free(bo);
            } else if (can_q3) {
                uint64_t nE = e->shape[0], er = e->shape[1], ec = e->shape[2];
                size_t xrow = (size_t)er * ec;
                size_t wb = llmcodec_moe_affine_w_bytes((uint32_t)er, (uint32_t)ec, bits_lo);
                size_t sb = llmcodec_moe_affine_sc_bytes((uint32_t)er, (uint32_t)ec, group);
                float* x = (float*)malloc((size_t)nE * xrow * sizeof(float));
                uint32_t* wo = (uint32_t*)malloc((size_t)nE * wb);
                uint16_t* so = (uint16_t*)malloc((size_t)nE * sb);
                uint16_t* bo = (uint16_t*)malloc((size_t)nE * sb);
                if (!x || !wo || !so || !bo) { free(x); free(wo); free(so); free(bo); free(raw); free(ents); free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
                for (uint64_t ex = 0; ex < nE; ++ex) {
                    if (strcmp(e->dtype, "F32") == 0) memcpy(x + ex * xrow, raw + ex * xrow * 4, xrow * 4);
                    else for (size_t k = 0; k < xrow; ++k) { uint16_t h; memcpy(&h, raw + (ex * xrow + k) * 2, 2); x[ex * xrow + k] = (strcmp(e->dtype, "F16") == 0) ? f16_to_f32(h) : bf16_to_f32(h); }
                    if (llmcodec_moe_affine_quantize(x + ex * xrow, (uint32_t)er, (uint32_t)ec, group, bits_lo, wo + (size_t)ex * (wb / 4), so + (size_t)ex * (sb / 2), bo + (size_t)ex * (sb / 2)) != 0) {
                        fprintf(stderr, "auto: quantize 3D fallita su %s\n", e->name);
                        free(x); free(wo); free(so); free(bo); free(raw); free(ents); free(np); free(rel); free(oc); auto_free(ts, n); return 1;
                    }
                }
                char base3[256]; snprintf(base3, sizeof(base3), "%s", e->name); base3[strlen(base3) - 7] = 0;
                char nm3[300]; uint64_t shp3[3];
                snprintf(nm3, sizeof(nm3), "%s.weight", base3); shp3[0] = nE; shp3[1] = er; shp3[2] = ec / (32 / bits_lo);
                gst_writer_add(w, nm3, "U32", 3, shp3, wo, (size_t)nE * wb);
                snprintf(nm3, sizeof(nm3), "%s.scales", base3); shp3[2] = ec / group;
                gst_writer_add(w, nm3, "F16", 3, shp3, so, (size_t)nE * sb);
                snprintf(nm3, sizeof(nm3), "%s.biases", base3);
                gst_writer_add(w, nm3, "F16", 3, shp3, bo, (size_t)nE * sb);
                n_q++;
                free(x); free(wo); free(so); free(bo);
            } else {
                gst_writer_add(w, e->name, e->dtype, e->ndim, e->shape, raw, rn);
                n_copy++;
            }
            free(raw);
        }
        free(ents);
    }
    if (gst_writer_finish(w) != 0) { fprintf(stderr, "auto: scrittura fallita %s\n", op); free(np); free(rel); free(oc); auto_free(ts, n); return 1; }
    write_config_mixed(src_dir, out_dir, group, bits_lo, ts, oc, bits_hi, n, verbose);
    const char* extra[] = { "tokenizer.json", "tokenizer_config.json", "merges.txt",
                            "vocab.json", "special_tokens_map.json", "generation_config.json" };
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); ++i) {
        char a[1200], b[1200];
        snprintf(a, sizeof(a), "%s/%s", src_dir, extra[i]);
        snprintf(b, sizeof(b), "%s/%s", out_dir, extra[i]);
        copy_file(a, b);
    }
    if (verbose) printf("gmb_model_quantize: ok  %u quantizzati (%d a Q%u, %d a Q%u), %u copiati, mixed auto bpw %.3f -> %s\n",
           n_q, n_hi, bits_hi, n - n_hi, bits_lo, n_copy, par_sum > 0 ? bits_sum / par_sum : 0.0, op);
    free(np); free(rel); free(oc); auto_free(ts, n);
    return 0;
}

int gmb_model_quantize_auto(const char* src_dir, const char* out_dir, uint32_t group,
                            uint32_t bits_lo, uint32_t bits_hi, double target_bpw, int verbose) {
    return auto_impl(src_dir, out_dir, group, bits_lo, bits_hi, target_bpw, NULL, verbose);
}

int gmb_model_quantize_auto_ex(const char* src_dir, const char* out_dir, uint32_t group,
                               uint32_t bits_lo, uint32_t bits_hi, double target_bpw,
                               const char* imatrix_path, int verbose) {
    if (imatrix_path == NULL || imatrix_path[0] == 0)
        return auto_impl(src_dir, out_dir, group, bits_lo, bits_hi, target_bpw, NULL, verbose);
    ImReport im;
    if (im_load(imatrix_path, &im) != 0) {
        fprintf(stderr, "auto: imatrix non leggibile: %s\n", imatrix_path);
        return 1;
    }
    if (verbose) fprintf(stderr, "[auto] imatrix: %d righe (layer/expert) da %s\n", im.n, imatrix_path);
    int rc = auto_impl(src_dir, out_dir, group, bits_lo, bits_hi, target_bpw, &im, verbose);
    im_free(&im);
    return rc;
}
