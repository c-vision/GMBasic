/* llmwsp_doc.c — WSP Document (W2): sezionamento completo di un modello denso. C99. */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_doc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct llmwsp_doc {
    const llmwsp_store *s;
    char      model_type[48];
    int64_t   H, nh, nkv, hd, I, V;
    int       layers;
    int       tied;           /* tie_word_embeddings dichiarato */
    llmwsp_leaf *l;
    size_t    n;
    size_t    unclassified;
};

static void seterr(char *err, size_t cap, const char *fmt, const char *a) {
    if (err && cap) snprintf(err, cap, fmt, a ? a : "");
}

/* ------------------------------------------------------------ lettura config.json (livello superiore) */
static const char *sk_ws(const char *p) { while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r' || *p == ',') p++; return p; }
static const char *sk_str(const char *p) {   /* p su '"' -> dopo la '"' di chiusura */
    p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    return *p ? p + 1 : p;
}
static const char *sk_val(const char *p) {
    if (*p == '"') return sk_str(p);
    if (*p == '{' || *p == '[') {
        int d = 0;
        while (*p) {
            if (*p == '"') { p = sk_str(p); continue; }
            if (*p == '{' || *p == '[') d++;
            else if (*p == '}' || *p == ']') { d--; if (d == 0) return p + 1; }
            p++;
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}
/* 1 se trovata. num per numeri/bool; str per stringhe (se non NULL). */
static int cfg_get(const char *j, const char *key, double *num, char *str, size_t cap) {
    const char *p = strchr(j, '{');
    if (!p) return 0;
    p++;
    for (;;) {
        p = sk_ws(p);
        if (*p != '"') return 0;
        const char *ks = p + 1, *ke = sk_str(p) - 1;
        p = sk_ws(ke + 1);
        if (*p != ':') return 0;
        p = sk_ws(p + 1);
        int match = (size_t)(ke - ks) == strlen(key) && strncmp(ks, key, (size_t)(ke - ks)) == 0;
        if (match) {
            if (*p == '"') {
                if (str && cap) { const char *e = sk_str(p) - 1; size_t n = (size_t)(e - (p + 1)); if (n >= cap) n = cap - 1; memcpy(str, p + 1, n); str[n] = 0; }
                return 1;
            }
            if (strncmp(p, "true", 4) == 0) { if (num) *num = 1; return 1; }
            if (strncmp(p, "false", 5) == 0) { if (num) *num = 0; return 1; }
            if (strncmp(p, "null", 4) == 0) return 0;
            if (num) *num = atof(p);
            return 1;
        }
        p = sk_val(p);
        if (!*p) return 0;
    }
}

/* ------------------------------------------------------------ classificazione dei nomi */
typedef struct { const char *hf; const char *role; const char *addr; } ltab;
static const ltab LT[] = {
    {"input_layernorm", "attn_norm", "attn_norm"},
    {"post_attention_layernorm", "mlp_norm", "mlp_norm"},
    {"self_attn.q_proj", "attn_q", "attn.q"}, {"self_attn.k_proj", "attn_k", "attn.k"},
    {"self_attn.v_proj", "attn_v", "attn.v"}, {"self_attn.o_proj", "attn_o", "attn.o"},
    {"self_attn.q_norm", "q_norm", "attn.q_norm"}, {"self_attn.k_norm", "k_norm", "attn.k_norm"},
    {"mlp.gate_proj", "mlp_gate", "mlp.gate"}, {"mlp.up_proj", "mlp_up", "mlp.up"},
    {"mlp.down_proj", "mlp_down", "mlp.down"}, {NULL, NULL, NULL}};

/* Ruolo di base (senza suffisso _bias/_scales/_biases) usato per partizioni e controlli. */
static void base_role(const char *role, char *out, size_t cap) {
    snprintf(out, cap, "%s", role);
    char *u = strrchr(out, '_');
    if (u && (strcmp(u, "_bias") == 0 || strcmp(u, "_scales") == 0 || strcmp(u, "_biases") == 0)) *u = 0;
}

static int classify(const char *name, int *layer, char *role, size_t rcap, char *addr, size_t acap) {
    char b[256];
    snprintf(b, sizeof b, "%s", name);
    char *dot = strrchr(b, '.');
    if (!dot) return 0;
    const char *suf = dot + 1;
    if (strcmp(suf, "weight") && strcmp(suf, "bias") && strcmp(suf, "scales") && strcmp(suf, "biases")) return 0;
    *dot = 0;
    const char *base = b, *rb = NULL, *ab = NULL;
    *layer = -1;
    char tmp[64], rtmp[64];
    if (strcmp(b, "lm_head") == 0) { rb = "lm_head"; ab = "lm_head"; }
    else {
        if (strncmp(base, "model.", 6) == 0) base += 6;
        if (strcmp(base, "embed_tokens") == 0) { rb = "embed"; ab = "embed"; }
        else if (strcmp(base, "norm") == 0) { rb = "final_norm"; ab = "final_norm"; }
        else if (strncmp(base, "layers.", 7) == 0) {
            char *e; long L = strtol(base + 7, &e, 10);
            if (e == base + 7 || *e != '.' || L < 0) return 0;
            for (const ltab *t = LT; t->hf; t++)
                if (strcmp(e + 1, t->hf) == 0) {
                    *layer = (int)L;
                    snprintf(tmp, sizeof tmp, "layers.%ld.%s", L, t->addr);
                    ab = tmp; rb = t->role;
                    break;
                }
        }
    }
    if (!rb) return 0;
    if (strcmp(suf, "weight") == 0) { snprintf(role, rcap, "%s", rb); snprintf(addr, acap, "%s", ab); }
    else { snprintf(rtmp, sizeof rtmp, "%s_%s", rb, suf); snprintf(role, rcap, "%s", rtmp); snprintf(addr, acap, "%s_%s", ab, suf); }
    return 1;
}

/* Cosa ci si aspetta da un ruolo di base: partizione (se c'è) e dimensione dell'altro asse. */
typedef struct { const char *pname; int paxis; int64_t pcount, psize; int oaxis; int64_t odim; } expect_t;
static int expect(const llmwsp_doc *d, const char *role, expect_t *x) {
    memset(x, 0, sizeof *x); x->paxis = -1; x->oaxis = -1;
    #define E(pn, pa, pc, ps, oa, od) do { x->pname = pn; x->paxis = pa; x->pcount = pc; x->psize = ps; x->oaxis = oa; x->odim = od; return 1; } while (0)
    if (!strcmp(role, "embed") || !strcmp(role, "lm_head")) E("tokens", 0, d->V, 1, 1, d->H);
    if (!strcmp(role, "attn_q")) E("heads", 0, d->nh, d->hd, 1, d->H);
    if (!strcmp(role, "attn_k") || !strcmp(role, "attn_v")) E("kv_heads", 0, d->nkv, d->hd, 1, d->H);
    if (!strcmp(role, "attn_o")) E("heads", 1, d->nh, d->hd, 0, d->H);
    if (!strcmp(role, "mlp_gate") || !strcmp(role, "mlp_up")) E("neurons", 0, d->I, 1, 1, d->H);
    if (!strcmp(role, "mlp_down")) E("neurons", 1, d->I, 1, 0, d->H);
    #undef E
    return 0;
}

static int cmp_leaf(const void *a, const void *b) {
    const llmwsp_leaf *x = (const llmwsp_leaf *)a, *y = (const llmwsp_leaf *)b;
    #define GRP(l) (!strcmp((l)->role, "unclassified") ? 4 : (l)->layer >= 0 ? 1 : !strncmp((l)->role, "embed", 5) ? 0 : !strncmp((l)->role, "final_norm", 10) ? 2 : 3)
    int gx = GRP(x), gy = GRP(y);
    #undef GRP
    if (gx != gy) return gx - gy;
    if (x->layer != y->layer) return x->layer - y->layer;
    return strcmp(x->addr, y->addr);
}

/* ------------------------------------------------------------ build */
int llmwsp_doc_build(const llmwsp_store *s, const char *cfgpath, llmwsp_doc **out, char *err, size_t errcap) {
    if (err && errcap) err[0] = 0;
    *out = NULL;
    FILE *f = fopen(cfgpath, "rb");
    if (!f) { seterr(err, errcap, "config.json illeggibile: %s", cfgpath); return -1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *j = (char *)malloc((size_t)n + 1);
    if (!j || fread(j, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(j); return -1; }
    fclose(f); j[n] = 0;
    llmwsp_doc *d = (llmwsp_doc *)calloc(1, sizeof *d);
    if (!d) { free(j); return -1; }
    d->s = s;
    double v;
    cfg_get(j, "model_type", NULL, d->model_type, sizeof d->model_type);
    int ok = 1;
    #define NEED(key, dst) do { if (cfg_get(j, key, &v, NULL, 0)) dst = (int64_t)v; else { seterr(err, errcap, "config.json: manca %s", key); ok = 0; } } while (0)
    NEED("hidden_size", d->H); NEED("num_attention_heads", d->nh); NEED("num_hidden_layers", d->layers);
    NEED("intermediate_size", d->I); NEED("vocab_size", d->V);
    #undef NEED
    if (!ok) { free(j); free(d); return -1; }
    d->nkv = cfg_get(j, "num_key_value_heads", &v, NULL, 0) ? (int64_t)v : d->nh;
    d->hd = cfg_get(j, "head_dim", &v, NULL, 0) ? (int64_t)v : (d->nh ? d->H / d->nh : 0);
    d->tied = cfg_get(j, "tie_word_embeddings", &v, NULL, 0) ? (int)v : 0;
    free(j);
    size_t cnt = llmwsp_store_count(s);
    d->l = (llmwsp_leaf *)calloc(cnt ? cnt : 1, sizeof *d->l);
    if (!d->l) { free(d); return -1; }
    for (size_t i = 0; i < cnt; i++) {
        const llmwsp_tensor *t = llmwsp_store_at(s, i);
        llmwsp_leaf *L = &d->l[d->n++];
        char role[32], addr[200];
        L->t = t; L->layer = -1;
        if (classify(t->name, &L->layer, role, sizeof role, addr, sizeof addr)) {
            snprintf(L->role, sizeof L->role, "%s", role);
            L->addr = strdup(addr);
        } else {
            snprintf(L->role, sizeof L->role, "unclassified");
            snprintf(addr, sizeof addr, "unclassified.%s", t->name);
            L->addr = strdup(addr); L->layer = -1; d->unclassified++;
        }
        /* partizioni solo sul peso pieno (non quantizzato): niente companion .scales */
        char br[32]; base_role(L->role, br, sizeof br);
        if (strcmp(br, L->role) == 0 && strcmp(L->role, "unclassified") != 0) {
            char qn[300]; snprintf(qn, sizeof qn, "%.*s.scales", (int)(strlen(t->name) - 7), t->name);
            int quant = strlen(t->name) > 7 && llmwsp_store_find(s, qn) != NULL;
            expect_t x;
            if (!quant && expect(d, br, &x)) {
                L->nparts = 1;
                snprintf(L->parts[0].name, sizeof L->parts[0].name, "%s", x.pname);
                L->parts[0].axis = x.paxis; L->parts[0].count = x.pcount; L->parts[0].size = x.psize;
            }
        }
    }
    qsort(d->l, d->n, sizeof *d->l, cmp_leaf);
    *out = d;
    return 0;
}

void llmwsp_doc_free(llmwsp_doc *d) {
    if (!d) return;
    for (size_t i = 0; i < d->n; i++) free(d->l[i].addr);
    free(d->l); free(d);
}
size_t llmwsp_doc_leaf_count(const llmwsp_doc *d) { return d->n; }
const llmwsp_leaf *llmwsp_doc_leaf(const llmwsp_doc *d, size_t i) { return i < d->n ? &d->l[i] : NULL; }
int llmwsp_doc_layers(const llmwsp_doc *d) { return d->layers; }
size_t llmwsp_doc_unclassified(const llmwsp_doc *d) { return d->unclassified; }
const llmwsp_leaf *llmwsp_doc_find(const llmwsp_doc *d, const char *addr) {
    for (size_t i = 0; i < d->n; i++) if (strcmp(d->l[i].addr, addr) == 0) return &d->l[i];
    return NULL;
}

/* ------------------------------------------------------------ check */
#define VIOL(...) do { if (!viol && err && errcap) snprintf(err, errcap, __VA_ARGS__); viol++; } while (0)

int llmwsp_doc_check(const llmwsp_doc *d, char *err, size_t errcap) {
    int viol = 0;
    if (err && errcap) err[0] = 0;
    size_t cnt = llmwsp_store_count(d->s);
    const llmwsp_tensor *base = llmwsp_store_at(d->s, 0);
    unsigned char *seen = (unsigned char *)calloc(cnt ? cnt : 1, 1);
    if (!seen) return -1;
    uint64_t bytes = 0;
    for (size_t i = 0; i < d->n; i++) {
        const llmwsp_leaf *L = &d->l[i];
        size_t ti = (size_t)(L->t - base);
        if (ti >= cnt) { VIOL("foglia %s: tensore fuori dallo store", L->addr); continue; }
        if (seen[ti]++) VIOL("tensore duplicato in più foglie: %s", L->t->name);
        bytes += L->t->nbytes;
        for (int p = 0; p < L->nparts; p++) {
            const llmwsp_part *P = &L->parts[p];
            if (P->axis >= L->t->ndim || P->count * P->size != L->t->shape[P->axis])
                VIOL("%s: la partizione %s non tassella l'asse %d (%lld*%lld != %lld)", L->addr, P->name, P->axis,
                     (long long)P->count, (long long)P->size, P->axis < L->t->ndim ? (long long)L->t->shape[P->axis] : -1LL);
        }
        char br[32]; base_role(L->role, br, sizeof br);
        expect_t x;
        if (L->nparts && expect(d, br, &x) && x.oaxis < L->t->ndim && L->t->shape[x.oaxis] != x.odim)
            VIOL("%s: l'asse %d vale %lld, attesi %lld (hidden_size/config)", L->addr, x.oaxis, (long long)L->t->shape[x.oaxis], (long long)x.odim);
        if (!strcmp(L->role, "attn_norm") || !strcmp(L->role, "mlp_norm") || !strcmp(L->role, "final_norm")) {
            if (L->t->ndim != 1 || L->t->shape[0] != d->H) VIOL("%s: norma non [hidden_size]", L->addr);
        }
        if ((!strcmp(L->role, "q_norm") || !strcmp(L->role, "k_norm")) && (L->t->ndim != 1 || L->t->shape[0] != d->hd))
            VIOL("%s: norma non [head_dim]", L->addr);
        if (L->layer >= d->layers) VIOL("%s: layer %d oltre num_hidden_layers=%d", L->addr, L->layer, d->layers);
    }
    for (size_t i = 0; i < cnt; i++) if (!seen[i]) VIOL("tensore orfano (in nessuna foglia): %s", llmwsp_store_at(d->s, i)->name);
    free(seen);
    if (bytes != llmwsp_store_total_bytes(d->s)) VIOL("byte foglie %llu != byte store %llu", (unsigned long long)bytes, (unsigned long long)llmwsp_store_total_bytes(d->s));
    static const char *REQ[] = {"attn_norm", "mlp_norm", "attn.q", "attn.k", "attn.v", "attn.o", "mlp.gate", "mlp.up", "mlp.down", NULL};
    for (int L = 0; L < d->layers; L++)
        for (const char **r = REQ; *r; r++) {
            char a[96]; snprintf(a, sizeof a, "layers.%d.%s", L, *r);
            if (!llmwsp_doc_find(d, a)) VIOL("manca %s", a);
        }
    if (!llmwsp_doc_find(d, "embed")) VIOL("manca embed");
    if (!llmwsp_doc_find(d, "final_norm")) VIOL("manca final_norm");
    if (!llmwsp_doc_find(d, "lm_head") && !d->tied) VIOL("manca lm_head e tie_word_embeddings è falso");
    return viol;
}

/* ------------------------------------------------------------ resolve */
int llmwsp_doc_resolve(const llmwsp_doc *d, const char *addr, const llmwsp_tensor **t, int *axis, int64_t *start, int64_t *end) {
    const llmwsp_leaf *L = llmwsp_doc_find(d, addr);
    if (L) {
        *t = L->t; *axis = 0; *start = 0; *end = L->t->ndim ? L->t->shape[0] : 1;
        return 0;
    }
    char b[256];
    snprintf(b, sizeof b, "%s", addr);
    char *i2 = strrchr(b, '.');
    if (!i2) return -1;
    *i2 = 0;
    char *p2 = strrchr(b, '.');
    if (!p2) return -1;
    *p2 = 0;
    char *e; long long idx = strtoll(i2 + 1, &e, 10);
    if (*e || e == i2 + 1 || idx < 0) return -1;
    L = llmwsp_doc_find(d, b);
    if (!L) return -1;
    for (int p = 0; p < L->nparts; p++)
        if (strcmp(L->parts[p].name, p2 + 1) == 0) {
            if (idx >= L->parts[p].count) return -1;
            *t = L->t; *axis = L->parts[p].axis;
            *start = idx * L->parts[p].size; *end = *start + L->parts[p].size;
            return 0;
        }
    return -1;
}

/* ------------------------------------------------------------ JSON */
typedef struct { char *p; size_t n, cap; } sb;
static void sb_put(sb *b, const char *s) {
    size_t k = strlen(s);
    if (b->n + k + 1 > b->cap) { size_t nc = b->cap ? b->cap : 8192; while (nc < b->n + k + 1) nc *= 2; b->p = (char *)realloc(b->p, nc); b->cap = nc; }
    memcpy(b->p + b->n, s, k + 1); b->n += k;
}
static void sb_f(sb *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void sb_f(sb *b, const char *fmt, ...) {
    char t[1024]; va_list ap; va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap); sb_put(b, t);
}
static void sb_q(sb *b, const char *s) {
    sb_put(b, "\"");
    for (; *s; s++) { char c[3] = {0, 0, 0}; if (*s == '"' || *s == '\\') { c[0] = '\\'; c[1] = *s; } else c[0] = *s; sb_put(b, c); }
    sb_put(b, "\"");
}

char *llmwsp_doc_to_json(const llmwsp_doc *d) {
    sb b = {0};
    sb_put(&b, "{\"format\":\"gmb.wsp@1\",\"model_type\":"); sb_q(&b, d->model_type);
    sb_f(&b, ",\"config\":{\"hidden_size\":%lld,\"num_hidden_layers\":%d,\"num_attention_heads\":%lld,"
             "\"num_key_value_heads\":%lld,\"head_dim\":%lld,\"intermediate_size\":%lld,\"vocab_size\":%lld,\"tie_word_embeddings\":%s}",
         (long long)d->H, d->layers, (long long)d->nh, (long long)d->nkv, (long long)d->hd, (long long)d->I, (long long)d->V, d->tied ? "true" : "false");
    sb_put(&b, ",\"ties\":[");
    if (!llmwsp_doc_find(d, "lm_head") && d->tied) sb_put(&b, "{\"alias\":\"lm_head\",\"of\":\"embed\"}");
    sb_f(&b, "],\"coverage\":{\"tensors\":%zu,\"bytes\":%llu,\"unclassified\":%zu},\"leaves\":[", llmwsp_store_count(d->s),
         (unsigned long long)llmwsp_store_total_bytes(d->s), d->unclassified);
    for (size_t i = 0; i < d->n; i++) {
        const llmwsp_leaf *L = &d->l[i];
        if (i) sb_put(&b, ",");
        sb_put(&b, "\n{\"addr\":"); sb_q(&b, L->addr);
        sb_put(&b, ",\"role\":"); sb_q(&b, L->role);
        sb_f(&b, ",\"layer\":%d,\"tensor\":", L->layer); sb_q(&b, L->t->name);
        sb_f(&b, ",\"dtype\":\"%s\",\"shape\":[", L->t->dtype);
        for (int k = 0; k < L->t->ndim; k++) sb_f(&b, "%s%lld", k ? "," : "", (long long)L->t->shape[k]);
        sb_put(&b, "],\"file\":"); sb_q(&b, llmwsp_store_file_name(d->s, L->t->file));
        sb_f(&b, ",\"offset\":%llu,\"nbytes\":%llu,\"partitions\":[", (unsigned long long)L->t->offset, (unsigned long long)L->t->nbytes);
        for (int p = 0; p < L->nparts; p++)
            sb_f(&b, "%s{\"name\":\"%s\",\"axis\":%d,\"count\":%lld,\"size\":%lld}", p ? "," : "", L->parts[p].name, L->parts[p].axis,
                 (long long)L->parts[p].count, (long long)L->parts[p].size);
        sb_put(&b, "]}");
    }
    sb_put(&b, "\n]}\n");
    return b.p;
}
