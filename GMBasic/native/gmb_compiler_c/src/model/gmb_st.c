/* gmb_st.c — reader/writer minimale safetensors. Vedi include/gmb_st.h. */
#include "gmb_st.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* K7: dimensione in byte di un dtype safetensors (0 = non supportato → rifiuto esplicito). */
static uint64_t st_dtype_size(const char* d) {
    if (d == NULL || d[0] == 0) return 0;
    if (!strcmp(d, "F64") || !strcmp(d, "I64") || !strcmp(d, "U64")) return 8;
    if (!strcmp(d, "F32") || !strcmp(d, "I32") || !strcmp(d, "U32")) return 4;
    if (!strcmp(d, "F16") || !strcmp(d, "BF16") || !strcmp(d, "I16") || !strcmp(d, "U16")) return 2;
    if (!strcmp(d, "I8") || !strcmp(d, "U8") || !strcmp(d, "BOOL")) return 1;
    if (!strcmp(d, "F8_E4M3") || !strcmp(d, "F8_E5M2")) return 1;
    return 0;
}

static int st_cmp_off(const void* a, const void* b) {
    const GstEntry* x = *(const GstEntry* const*)a;
    const GstEntry* y = *(const GstEntry* const*)b;
    if (x->off0 != y->off0) return x->off0 < y->off0 ? -1 : 1;
    if (x->off1 != y->off1) return x->off1 < y->off1 ? -1 : 1;
    return 0;
}

static int st_cmp_name(const void* a, const void* b) {
    const GstEntry* x = *(const GstEntry* const*)a;
    const GstEntry* y = *(const GstEntry* const*)b;
    return strcmp(x->name, y->name);
}

/* K7: mappa tensore -> shard dell'index (verifica per-shard). */
typedef struct { char tensor[256]; char shard[96]; } StMap;
static int st_cmp_map_tensor(const void* a, const void* b) {
    return strcmp(((const StMap*)a)->tensor, ((const StMap*)b)->tensor);
}
static int st_cmp_map_shard(const void* a, const void* b) {
    const StMap* x = (const StMap*)a;
    const StMap* y = (const StMap*)b;
    int c = strcmp(x->shard, y->shard);
    return c != 0 ? c : strcmp(x->tensor, y->tensor);
}

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static int jstr(const char* p, const char* key, char* out, size_t cap) {
    char pat[160]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* v = strstr(p, pat);
    if (!v) return 0;
    v += strlen(pat);
    while (is_ws(*v)) ++v;
    if (*v != ':') return 0;
    ++v; while (is_ws(*v)) ++v;
    if (*v != '"') return 0;
    ++v; size_t i = 0;
    while (*v && *v != '"' && i + 1 < cap) out[i++] = *v++;
    out[i] = 0;
    return 1;
}
static int jshape(const char* p, const char* key, uint64_t* out, uint32_t* ndim) {
    char pat[160]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* v = strstr(p, pat);
    if (!v) return 0;
    v += strlen(pat);
    while (is_ws(*v)) ++v;
    if (*v != ':') return 0;
    ++v; while (is_ws(*v)) ++v;
    if (*v != '[') return 0;
    ++v; uint32_t n = 0;
    while (*v && *v != ']' && n < 8) {
        while (is_ws(*v)) ++v;
        if (*v == ']') break;
        out[n++] = strtoull(v, (char**)&v, 10);
        while (is_ws(*v)) ++v;
        if (*v == ',') ++v;
    }
    *ndim = n;
    return 1;
}

int gst_read_header(const char* path, GstEntry** ents, uint32_t* n_out, uint64_t* header_len_out) {
    if (ents) *ents = NULL;
    if (n_out) *n_out = 0;
    /* K7: legge SOLO l'header (non l'intero file: per-shard verification su shard da GB). */
    FILE* f = fopen(path, "rb");
    if (f == NULL) { fprintf(stderr, "safetensors: apri fallita: %s\n", path ? path : "(null)"); return -1; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long total = ftell(f);
    if (total < 8) { fclose(f); fprintf(stderr, "safetensors: file troppo corto: %s\n", path); return -1; }
    unsigned char lb[8];
    if (fseek(f, 0, SEEK_SET) != 0 || fread(lb, 1, 8, f) != 8) { fclose(f); return -1; }
    uint64_t hlen = 0;
    for (int i = 7; i >= 0; --i) hlen = (hlen << 8) | lb[i];
    if ((uint64_t)total < 8 + hlen) {
        fclose(f);
        fprintf(stderr, "safetensors: header oltre il file (%llu byte > %ld): %s\n",
                (unsigned long long)hlen, total, path);
        return -1;
    }
    char* hdr = (char*)malloc((size_t)hlen + 1);
    if (!hdr) { fclose(f); return -1; }
    if (hlen > 0 && fread(hdr, 1, (size_t)hlen, f) != (size_t)hlen) { free(hdr); fclose(f); return -1; }
    hdr[hlen] = 0;
    fclose(f);
    uint64_t data_cap = (uint64_t)total - 8 - hlen;
    if (header_len_out) *header_len_out = hlen;

    GstEntry* arr = NULL; uint32_t n = 0;
    const char* p = hdr;
    if (*p == '{') ++p;
    while (*p) {
        while (is_ws(*p) || *p == ',') ++p;
        if (*p == '}') break;
        if (*p != '"') { ++p; continue; }
        char key[256]; size_t i = 0; ++p;
        while (*p && *p != '"' && i + 1 < sizeof(key)) key[i++] = *p++;
        key[i] = 0;
        if (*p == '"') ++p;
        while (is_ws(*p)) ++p;
        if (*p != ':') continue;
        ++p; while (is_ws(*p)) ++p;
        if (*p != '{') continue;
        const char* obj = p;
        const char* end = strchr(obj, '}');
        if (!end) break;
        char sub[1024]; size_t sn = (size_t)(end - obj) + 1;
        if (sn >= sizeof(sub)) sn = sizeof(sub) - 1;
        memcpy(sub, obj, sn); sub[sn] = 0;
        if (strcmp(key, "__metadata__") != 0) {
            GstEntry e; memset(&e, 0, sizeof(e));
            snprintf(e.name, sizeof(e.name), "%s", key);
            jstr(sub, "dtype", e.dtype, sizeof(e.dtype));
            jshape(sub, "shape", e.shape, &e.ndim);
            uint64_t offs[2] = {0, 0}; uint32_t no = 0;
            jshape(sub, "data_offsets", offs, &no);
            e.off0 = offs[0]; e.off1 = offs[1];
            if (e.dtype[0]) {
                GstEntry* na = (GstEntry*)realloc(arr, (size_t)(n + 1) * sizeof(GstEntry));
                if (!na) { free(arr); free(hdr); return -1; }
                arr = na; arr[n++] = e;
            }
        }
        p = end + 1;
    }
    free(hdr);

    /* K7: validazione PER-SHARD — bounds, dtype/shape vs byte, nomi duplicati, overlap.
     * Un totale può nascondere due errori opposti: qui ogni tensore è verificato nel SUO file. */
    for (uint32_t i = 0; i < n; ++i) {
        const GstEntry* e = &arr[i];
        if (e->off1 < e->off0 || e->off1 > data_cap) {
            fprintf(stderr, "safetensors: data_offsets fuori dal file [%llu,%llu] > %llu (%s): %s\n",
                    (unsigned long long)e->off0, (unsigned long long)e->off1,
                    (unsigned long long)data_cap, e->name, path);
            free(arr); return -1;
        }
        uint64_t ds = st_dtype_size(e->dtype);
        if (ds == 0) {
            fprintf(stderr, "safetensors: dtype non supportato \"%s\" (%s): %s\n", e->dtype, e->name, path);
            free(arr); return -1;
        }
        uint64_t nelem = 1;
        for (uint32_t d = 0; d < e->ndim; ++d) {
            if (e->shape[d] != 0 && nelem > (uint64_t)-1 / e->shape[d]) {
                fprintf(stderr, "safetensors: shape in overflow (%s): %s\n", e->name, path);
                free(arr); return -1;
            }
            nelem *= e->shape[d];
        }
        if (nelem != 0 && nelem > (uint64_t)-1 / ds) {
            fprintf(stderr, "safetensors: shape in overflow (%s): %s\n", e->name, path);
            free(arr); return -1;
        }
        if (nelem * ds != e->off1 - e->off0) {
            fprintf(stderr, "safetensors: shape/dtype non combaciano con data_offsets (%s: %llu byte dichiarati, %llu attesi): %s\n",
                    e->name, (unsigned long long)(e->off1 - e->off0),
                    (unsigned long long)(nelem * ds), path);
            free(arr); return -1;
        }
    }
    if (n > 1) {
        GstEntry** ix = (GstEntry**)malloc((size_t)n * sizeof(*ix));
        if (ix == NULL) { free(arr); return -1; }
        for (uint32_t i = 0; i < n; ++i) ix[i] = &arr[i];
        qsort(ix, n, sizeof(*ix), st_cmp_off);
        for (uint32_t i = 1; i < n; ++i) {
            if (ix[i]->off0 < ix[i - 1]->off1) {
                fprintf(stderr, "safetensors: tensori sovrapposti (%s / %s): %s\n",
                        ix[i - 1]->name, ix[i]->name, path);
                free(ix); free(arr); return -1;
            }
        }
        qsort(ix, n, sizeof(*ix), st_cmp_name);
        for (uint32_t i = 1; i < n; ++i) {
            if (strcmp(ix[i]->name, ix[i - 1]->name) == 0) {
                fprintf(stderr, "safetensors: nome duplicato \"%s\": %s\n", ix[i]->name, path);
                free(ix); free(arr); return -1;
            }
        }
        free(ix);
    }
    if (ents) *ents = arr; else free(arr);
    if (n_out) *n_out = n;
    return 0;
}

/* K7: raccolta shard (index o singolo file) con controlli ESPLICITI:
 * 1 = ok, 0 = nessun safetensors, -1 = errore (shard mancante / troppi shard). */
int gst_collect_shards(const char* src_dir, char shards[][96], int max, int* n_out) {
    if (src_dir == NULL || shards == NULL || n_out == NULL || max <= 0) return -1;
    *n_out = 0;
    char ip[1200]; snprintf(ip, sizeof(ip), "%s/model.safetensors.index.json", src_dir);
    FILE* f = fopen(ip, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char* js = (char*)malloc((size_t)(n > 0 ? n : 0) + 1);
        if (js != NULL && (n <= 0 || fread(js, 1, (size_t)n, f) == (size_t)n)) {
            js[n > 0 ? n : 0] = 0;
            for (const char* p = js; (p = strstr(p, ".safetensors")) != NULL; p += 12) {
                const char* q = p;
                while (q > js && (q[-1] == '.' || q[-1] == '-' || q[-1] == '_' || (q[-1] >= '0' && q[-1] <= '9') ||
                       (q[-1] >= 'a' && q[-1] <= 'z') || (q[-1] >= 'A' && q[-1] <= 'Z'))) --q;
                char name[96]; size_t len = (size_t)(p + 12 - q);
                if (len >= sizeof(name)) len = sizeof(name) - 1;
                memcpy(name, q, len); name[len] = 0;
                int dup = 0; for (int i = 0; i < *n_out; ++i) if (strcmp(shards[i], name) == 0) { dup = 1; break; }
                if (dup) continue;
                if (*n_out >= max) {
                    fprintf(stderr, "safetensors: troppi shard (>%d) in %s\n", max, ip);
                    free(js); fclose(f); return -1;
                }
                snprintf(shards[(*n_out)++], 96, "%s", name);
            }
        }
        free(js); fclose(f);
    }
    if (*n_out == 0) {
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/model.safetensors", src_dir);
        FILE* g = fopen(sp, "rb");
        if (g) { fclose(g); snprintf(shards[(*n_out)++], 96, "model.safetensors"); }
    }
    /* Esistenza di OGNI shard elencato: un totale può nascondere uno shard mancante. */
    for (int i = 0; i < *n_out; ++i) {
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/%s", src_dir, shards[i]);
        FILE* g = fopen(sp, "rb");
        if (g == NULL) { fprintf(stderr, "safetensors: shard mancante: %s\n", sp); return -1; }
        fclose(g);
    }
    return *n_out > 0 ? 1 : 0;
}

/* K7: verifica per-shard dell'index `weight_map` (tensor -> shard):
 * ogni shard esiste e il suo header ha ESATTAMENTE i tensori mappati, una sola volta.
 * Senza index: valida il singolo `model.safetensors`. 0 = ok, -1 = errore esplicito. */
int gst_verify_shards(const char* src_dir) {
    if (src_dir == NULL) return -1;
    char ip[1200]; snprintf(ip, sizeof(ip), "%s/model.safetensors.index.json", src_dir);
    FILE* f = fopen(ip, "rb");
    if (f == NULL) {
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/model.safetensors", src_dir);
        GstEntry* e = NULL; uint32_t n = 0;
        if (gst_read_header(sp, &e, &n, NULL) != 0) return -1;
        free(e);
        return 0;
    }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* js = (char*)malloc((size_t)(sz > 0 ? sz : 0) + 1);
    if (js == NULL || (sz > 0 && fread(js, 1, (size_t)sz, f) != (size_t)sz)) { free(js); fclose(f); return -1; }
    js[sz > 0 ? sz : 0] = 0;
    fclose(f);

    const char* wm = strstr(js, "\"weight_map\"");
    if (wm == NULL) { fprintf(stderr, "safetensors: index senza weight_map: %s\n", ip); free(js); return -1; }
    const char* p = strchr(wm, '{');
    if (p == NULL) { fprintf(stderr, "safetensors: weight_map malformato: %s\n", ip); free(js); return -1; }
    ++p;

    StMap* map = NULL; int nm = 0, capm = 0;
    while (*p) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) ++p;
        if (*p == '}' || *p == 0) break;
        if (*p != '"') { fprintf(stderr, "safetensors: weight_map malformato: %s\n", ip); free(map); free(js); return -1; }
        ++p; char key[256]; size_t ki = 0;
        while (*p && *p != '"' && ki + 1 < sizeof(key)) key[ki++] = *p++;
        key[ki] = 0;
        if (*p != '"') { fprintf(stderr, "safetensors: weight_map malformato: %s\n", ip); free(map); free(js); return -1; }
        ++p;
        while (*p && *p != ':') ++p;
        if (*p != ':') { fprintf(stderr, "safetensors: weight_map malformato: %s\n", ip); free(map); free(js); return -1; }
        ++p; while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (*p != '"') { fprintf(stderr, "safetensors: weight_map malformato: %s\n", ip); free(map); free(js); return -1; }
        ++p; char val[96]; size_t vi = 0;
        while (*p && *p != '"' && vi + 1 < sizeof(val)) val[vi++] = *p++;
        val[vi] = 0;
        if (*p != '"') { fprintf(stderr, "safetensors: weight_map malformato: %s\n", ip); free(map); free(js); return -1; }
        ++p;
        size_t vl = strlen(val);
        if (vl < 12 || strcmp(val + vl - 12, ".safetensors") != 0) continue;   /* non-shard */
        if (nm == capm) {
            int nc = capm ? capm * 2 : 256;
            StMap* na = (StMap*)realloc(map, (size_t)nc * sizeof(*na));
            if (na == NULL) { free(map); free(js); return -1; }
            map = na; capm = nc;
        }
        snprintf(map[nm].tensor, sizeof(map[nm].tensor), "%s", key);
        snprintf(map[nm].shard, sizeof(map[nm].shard), "%s", val);
        nm++;
    }
    free(js);
    if (nm == 0) { fprintf(stderr, "safetensors: weight_map vuoto: %s\n", ip); free(map); return -1; }

    /* Ordina per tensore: un tensore mappato a più shard → duplicato adiacente. */
    qsort(map, (size_t)nm, sizeof(*map), st_cmp_map_tensor);
    for (int i = 1; i < nm; ++i)
        if (strcmp(map[i].tensor, map[i - 1].tensor) == 0) {
            fprintf(stderr, "safetensors: tensore mappato più volte (%s): %s\n", map[i].tensor, ip);
            free(map); return -1;
        }
    /* Ordina per (shard, tensore): gruppi per shard + bsearch di membership. */
    qsort(map, (size_t)nm, sizeof(*map), st_cmp_map_shard);
    for (int i = 0; i < nm; ) {
        int j = i;
        while (j < nm && strcmp(map[j].shard, map[i].shard) == 0) ++j;
        const char* sh = map[i].shard;
        char sp[1200]; snprintf(sp, sizeof(sp), "%s/%s", src_dir, sh);
        GstEntry* ents = NULL; uint32_t ne = 0;
        if (gst_read_header(sp, &ents, &ne, NULL) != 0) { free(ents); free(map); return -1; }
        if ((int)ne != j - i) {
            fprintf(stderr, "safetensors: shard %s: %u tensori nell'header, %d mappati nell'index\n",
                    sh, ne, j - i);
            free(ents); free(map); return -1;
        }
        for (uint32_t k = 0; k < ne; ++k) {
            int lo = i, hi = j - 1, found = 0;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                int c = strcmp(map[mid].tensor, ents[k].name);
                if (c == 0) { found = 1; break; }
                if (c < 0) lo = mid + 1; else hi = mid - 1;
            }
            if (!found) {
                fprintf(stderr, "safetensors: tensore %s non mappato allo shard %s nell'index\n", ents[k].name, sh);
                free(ents); free(map); return -1;
            }
        }
        free(ents);
        i = j;
    }
    free(map);
    return 0;
}

int gst_read_tensor(const char* path, const GstEntry* e, uint8_t** data, size_t* n_out) {
    if (!e || !data) return -1;
    uint64_t hlen = 0;
    if (gst_read_header(path, NULL, NULL, &hlen) != 0) return -1;
    size_t n = (size_t)(e->off1 - e->off0);
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, (long)(8 + hlen + e->off0), SEEK_SET) != 0) { fclose(f); return -1; }
    uint8_t* b = (uint8_t*)malloc(n ? n : 1);
    if (!b) { fclose(f); return -1; }
    if (n > 0 && fread(b, 1, n, f) != n) { free(b); fclose(f); return -1; }
    fclose(f);
    *data = b; if (n_out) *n_out = n;
    return 0;
}

/* ── writer ─────────────────────────────────────────────────────────── */
typedef struct { char name[256]; char dtype[8]; uint32_t ndim; uint64_t shape[8]; uint64_t off; size_t n; } GstOut;

struct GstWriter {
    char path[1200];
    char dpath[1220];
    FILE* data;          /* STREAMING: i payload vanno subito su file, non in RAM */
    uint64_t dlen;
    GstOut* out;
    uint32_t n, cap;
};

int gst_writer_create(GstWriter** outw, const char* path) {
    GstWriter* w = (GstWriter*)calloc(1, sizeof(GstWriter));
    if (!w) return -1;
    snprintf(w->path, sizeof(w->path), "%s", path);
    snprintf(w->dpath, sizeof(w->dpath), "%s.data", path);
    w->data = fopen(w->dpath, "wb");
    if (!w->data) { free(w); return -1; }
    *outw = w;
    return 0;
}

int gst_writer_add(GstWriter* w, const char* name, const char* dtype,
                   uint32_t ndim, const uint64_t* shape, const void* data, size_t nbytes) {
    if (!w || !name || !dtype || !shape || (!data && nbytes)) return -1;
    if (w->n == w->cap) {
        uint32_t nc = w->cap ? w->cap * 2 : 16;
        GstOut* na = (GstOut*)realloc(w->out, (size_t)nc * sizeof(GstOut));
        if (!na) return -1;
        w->out = na; w->cap = nc;
    }
    GstOut* o = &w->out[w->n];
    memset(o, 0, sizeof(*o));
    snprintf(o->name, sizeof(o->name), "%s", name);
    snprintf(o->dtype, sizeof(o->dtype), "%s", dtype);
    o->ndim = ndim;
    for (uint32_t i = 0; i < ndim && i < 8; ++i) o->shape[i] = shape[i];
    o->n = nbytes;
    o->off = w->dlen;
    if (nbytes && fwrite(data, 1, nbytes, w->data) != nbytes) return -1;
    w->dlen += nbytes;
    w->n++;
    return 0;
}

int gst_writer_finish(GstWriter* w) {
    if (!w) return -1;
    /* header JSON */
    size_t cap = 1024 + (size_t)w->n * 256;
    char* hdr = (char*)malloc(cap);
    if (!hdr) return -1;
    size_t off = 0;
    off += (size_t)snprintf(hdr + off, cap - off, "{");
    for (uint32_t i = 0; i < w->n; ++i) {
        GstOut* o = &w->out[i];
        off += (size_t)snprintf(hdr + off, cap - off, "%s\"%s\":{\"dtype\":\"%s\",\"shape\":[",
                                i ? "," : "", o->name, o->dtype);
        for (uint32_t d = 0; d < o->ndim; ++d)
            off += (size_t)snprintf(hdr + off, cap - off, "%s%llu", d ? "," : "", (unsigned long long)o->shape[d]);
        off += (size_t)snprintf(hdr + off, cap - off, "],\"data_offsets\":[%llu,%llu]}",
                                (unsigned long long)o->off, (unsigned long long)(o->off + o->n));
    }
    off += (size_t)snprintf(hdr + off, cap - off, "}");
    size_t pad = (8 - ((8 + off) % 8)) % 8;   /* header totale (8+N) multiplo di 8 */
    FILE* f = fopen(w->path, "wb");
    if (!f) { free(hdr); return -1; }
    uint64_t hlen = (uint64_t)(off + pad);
    unsigned char lb[8];
    for (int i = 0; i < 8; ++i) lb[i] = (unsigned char)((hlen >> (8 * i)) & 0xFF);
    fwrite(lb, 1, 8, f);
    fwrite(hdr, 1, off, f);
    for (size_t i = 0; i < pad; ++i) fputc(' ', f);
    /* copia i payload dal file temporaneo (bounded RAM) */
    fflush(w->data); fclose(w->data); w->data = NULL;
    FILE* df = fopen(w->dpath, "rb");
    if (df) {
        char* buf = (char*)malloc(1u << 20);
        size_t r;
        if (buf) { while ((r = fread(buf, 1, 1u << 20, df)) > 0) fwrite(buf, 1, r, f); free(buf); }
        fclose(df);
    }
    fclose(f);
    remove(w->dpath);
    /* INDICE `model.safetensors.index.json`: il loader MLX lo usa per scoprire gli shard
     * inline della tabella PLE/N-gram (`qwen4_exp_ngram_table_open_shards`); senza indice il
     * modello quantizzato carica ma rifiuta i layer PLE (2026-10-24). */
    if (w->n > 0) {
        const char* bn = strrchr(w->path, '/');
        const char* fname = bn ? bn + 1 : w->path;
        char ip[1300]; snprintf(ip, sizeof ip, "%s", w->path);
        size_t dl = bn ? (size_t)(bn - w->path) + 1 : 0;
        snprintf(ip + dl, sizeof ip - dl, "model.safetensors.index.json");
        FILE* oi = fopen(ip, "wb");
        if (oi) {
            uint64_t tot = 0; for (uint32_t i = 0; i < w->n; ++i) tot += w->out[i].n;
            fprintf(oi, "{\"metadata\":{\"total_size\":%llu},\"weight_map\":{", (unsigned long long)tot);
            for (uint32_t i = 0; i < w->n; ++i)
                fprintf(oi, "%s\"%s\":\"%s\"", i ? "," : "", w->out[i].name, fname);
            fprintf(oi, "}}");
            fclose(oi);
        }
    }
    free(w->out); free(hdr); free(w);
    return 0;
}
