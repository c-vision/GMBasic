/* llmwsp_derive.c — chirurgia di architettura (W7). C99 + POSIX. */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_derive.h"
#include "llmwsp_analyze.h"
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void E(char *err, size_t ec, const char *m, const char *a) { if (err && ec) snprintf(err, ec, "%s%s%s", m, a ? ": " : "", a ? a : ""); }
static int dtype_ok(const char *d) { return !strcmp(d, "F32") || !strcmp(d, "BF16") || !strcmp(d, "F16"); }
static int has_sfx(const char *s, const char *x) { size_t a = strlen(s), b = strlen(x); return a >= b && !strcmp(s + a - b, x); }

/* ------------------------------------------------------------ validazione */
int llmwsp_derive_validate(const llmwsp_store *s, const llmwsp_doc *d, const llmwsp_derive_ops *ops, char *err, size_t ec) {
    (void)s;
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    char e2[200];
    if (llmwsp_doc_check(d, e2, sizeof e2) != 0) { E(err, ec, "il documento sorgente non è coerente", e2); return -1; }
    if (llmwsp_doc_unclassified(d) != 0) { E(err, ec, "tensori non classificati: famiglia non supportata", NULL); return -1; }
    size_t nl = ops->layer_map ? ops->nl : (size_t)g.layers;
    if (nl == 0) { E(err, ec, "nessun layer in uscita", NULL); return -1; }
    if (ops->layer_map)
        for (size_t j = 0; j < nl; j++)
            if (ops->layer_map[j] < 0 || ops->layer_map[j] >= g.layers) { E(err, ec, "layer sorgente fuori intervallo nella mappa", NULL); return -1; }
    int prune = ops->kv_keep || ops->neuron_keep;
    if (prune)
        for (size_t i = 0; i < llmwsp_doc_leaf_count(d); i++) {
            const llmwsp_leaf *L = llmwsp_doc_leaf(d, i);
            if (has_sfx(L->role, "_scales") || has_sfx(L->role, "_biases") || !dtype_ok(L->t->dtype)) { E(err, ec, "la potatura richiede pesi non quantizzati (F32/BF16/F16)", L->t->name); return -1; }
        }
    if (ops->kv_keep) {
        if (g.heads % g.kv_heads) { E(err, ec, "heads non multiplo di kv_heads", NULL); return -1; }
        if (ops->kv_m < 1 || (int64_t)ops->kv_m > g.kv_heads) { E(err, ec, "numero di gruppi kv da tenere fuori intervallo", NULL); return -1; }
        for (size_t j = 0; j < nl; j++)
            for (size_t a = 0; a < ops->kv_m; a++) {
                int x = ops->kv_keep[j * ops->kv_m + a];
                if (x < 0 || x >= g.kv_heads) { E(err, ec, "indice di gruppo kv fuori intervallo", NULL); return -1; }
                for (size_t b = 0; b < a; b++) if (ops->kv_keep[j * ops->kv_m + b] == x) { E(err, ec, "gruppo kv duplicato nella selezione", NULL); return -1; }
            }
    }
    if (ops->neuron_keep) {
        if (ops->neu_k < 1 || (int64_t)ops->neu_k > g.inter) { E(err, ec, "numero di neuroni da tenere fuori intervallo", NULL); return -1; }
        unsigned char *seen = (unsigned char *)malloc((size_t)g.inter);
        if (!seen) return -1;
        for (size_t j = 0; j < nl; j++) {
            memset(seen, 0, (size_t)g.inter);
            for (size_t a = 0; a < ops->neu_k; a++) {
                int x = ops->neuron_keep[j * ops->neu_k + a];
                if (x < 0 || x >= g.inter || seen[x]) { free(seen); E(err, ec, "neurone fuori intervallo o duplicato", NULL); return -1; }
                seen[x] = 1;
            }
        }
        free(seen);
    }
    return 0;
}

/* ------------------------------------------------------------ importanza (baseline senza dati) */
int llmwsp_mlp_importance(const llmwsp_store *s, const llmwsp_doc *d, int layer, double *score) {
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    char a[96];
    const llmwsp_leaf *Lg, *Lu, *Ld;
    snprintf(a, sizeof a, "layers.%d.mlp.gate", layer); Lg = llmwsp_doc_find(d, a);
    snprintf(a, sizeof a, "layers.%d.mlp.up", layer); Lu = llmwsp_doc_find(d, a);
    snprintf(a, sizeof a, "layers.%d.mlp.down", layer); Ld = llmwsp_doc_find(d, a);
    if (!Lg || !Lu || !Ld) return -1;
    size_t I = (size_t)g.inter, H = (size_t)g.hidden;
    double *ng = (double *)calloc(I, sizeof(double)), *nu = (double *)calloc(I, sizeof(double)), *nd = (double *)calloc(I, sizeof(double));
    enum { RB = 64 };
    double *blk = (double *)malloc((size_t)RB * (I > H ? I : H) * sizeof(double));
    int rc = -1;
    if (!ng || !nu || !nd || !blk) goto out;
    for (int w = 0; w < 2; w++) {                                   /* gate, up: [I×H], norma per riga */
        const llmwsp_leaf *L = w ? Lu : Lg; double *dst = w ? nu : ng;
        for (size_t r0 = 0; r0 < I; r0 += RB) {
            size_t r1 = r0 + RB < I ? r0 + RB : I;
            if (llmwsp_load_f64(s, L->t, 0, (int64_t)r0, (int64_t)r1, blk, (r1 - r0) * H) < 0) goto out;
            for (size_t r = r0; r < r1; r++) { double q = 0; for (size_t c = 0; c < H; c++) q += blk[(r - r0) * H + c] * blk[(r - r0) * H + c]; dst[r] = sqrt(q); }
        }
    }
    for (size_t r0 = 0; r0 < H; r0 += RB) {                         /* down: [H×I], norma per colonna */
        size_t r1 = r0 + RB < H ? r0 + RB : H;
        if (llmwsp_load_f64(s, Ld->t, 0, (int64_t)r0, (int64_t)r1, blk, (r1 - r0) * I) < 0) goto out;
        for (size_t r = r0; r < r1; r++) for (size_t c = 0; c < I; c++) nd[c] += blk[(r - r0) * I + c] * blk[(r - r0) * I + c];
    }
    for (size_t i = 0; i < I; i++) score[i] = ng[i] * nu[i] * sqrt(nd[i]);
    rc = 0;
out:
    free(ng); free(nu); free(nd); free(blk);
    return rc;
}

int llmwsp_kvgroup_importance(const llmwsp_store *s, const llmwsp_doc *d, int layer, double *score) {
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    if (g.kv_heads <= 0 || g.heads % g.kv_heads) return -1;
    double *vn = (double *)malloc((size_t)g.kv_heads * sizeof(double)), *on = (double *)malloc((size_t)g.heads * sizeof(double));
    int rc = -1;
    if (vn && on && llmwsp_an_head_norms(s, d, layer, NULL, NULL, vn, on) == 0) {
        int64_t gr = g.heads / g.kv_heads;
        for (int64_t k = 0; k < g.kv_heads; k++) { double q = 0; for (int64_t x = 0; x < gr; x++) q += on[k * gr + x] * on[k * gr + x]; score[k] = vn[k] * sqrt(q); }
        rc = 0;
    }
    free(vn); free(on);
    return rc;
}

void llmwsp_topk_sorted(const double *score, size_t n, size_t k, int *out) {
    unsigned char *pick = (unsigned char *)calloc(n ? n : 1, 1);
    for (size_t t = 0; t < k && pick; t++) {
        long best = -1;
        for (size_t i = 0; i < n; i++) if (!pick[i] && (best < 0 || score[i] > score[best])) best = (long)i;
        if (best >= 0) pick[best] = 1;
    }
    size_t c = 0;
    for (size_t i = 0; i < n && c < k; i++) if (pick && pick[i]) out[c++] = (int)i;
    free(pick);
}

/* ------------------------------------------------------------ testo JSON: sostituzioni mirate */
static int json_set_int(char **buf, const char *key, long long v) {
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key);
    char *p = strstr(*buf, pat);
    if (!p) return 0;
    char *q = p + strlen(pat);
    while (*q == ' ' || *q == '\n' || *q == '\t' || *q == '\r') q++;
    if (*q != ':') return 0;
    q++;
    while (*q == ' ' || *q == '\n' || *q == '\t' || *q == '\r') q++;
    char *e = q; if (*e == '-') e++;
    while (*e >= '0' && *e <= '9') e++;
    if (e == q) return 0;
    char num[32]; snprintf(num, sizeof num, "%lld", v);
    size_t head = (size_t)(q - *buf), tail = strlen(e), nn = strlen(num);
    char *nb = (char *)malloc(head + nn + tail + 1);
    if (!nb) return 0;
    memcpy(nb, *buf, head); memcpy(nb + head, num, nn); memcpy(nb + head + nn, e, tail + 1);
    free(*buf); *buf = nb;
    return 1;
}

static int json_has(const char *buf, const char *key) { char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key); return strstr(buf, pat) != NULL; }

static int json_insert_first(char **buf, const char *kv) {      /* inserisce `kv,` subito dopo la prima '{' */
    char *p = strchr(*buf, '{');
    if (!p) return 0;
    size_t head = (size_t)(p - *buf) + 1, tail = strlen(p + 1), k = strlen(kv);
    char *nb = (char *)malloc(head + k + 3 + tail + 1);
    if (!nb) return 0;
    memcpy(nb, *buf, head); nb[head] = '\n'; memcpy(nb + head + 1, kv, k); nb[head + 1 + k] = ','; memcpy(nb + head + 2 + k, p + 1, tail + 1);
    free(*buf); *buf = nb;
    return 1;
}

/* rimappa un array JSON di token per layer: "layer_types": [ ... ] */
static void json_remap_array(char **buf, const char *key, const int *map, size_t nl, size_t nsrc) {
    char pat[96]; snprintf(pat, sizeof pat, "\"%s\"", key);
    char *p = strstr(*buf, pat);
    if (!p) return;
    char *lb = strchr(p, '[');
    if (!lb) return;
    char *q = lb + 1;
    char **tok = (char **)calloc(nsrc ? nsrc : 1, sizeof(char *)); size_t nt = 0;
    while (*q && *q != ']') {
        while (*q == ' ' || *q == '\n' || *q == '\t' || *q == ',') q++;
        if (*q == ']' || !*q) break;
        char *s0 = q;
        if (*q == '"') { q++; while (*q && *q != '"') { if (*q == '\\') q++; q++; } if (*q) q++; }
        else while (*q && *q != ',' && *q != ']' && *q != ' ' && *q != '\n') q++;
        if (nt < nsrc) { tok[nt] = (char *)malloc((size_t)(q - s0) + 1); memcpy(tok[nt], s0, (size_t)(q - s0)); tok[nt][q - s0] = 0; }
        nt++;
    }
    if (nt == nsrc && *q == ']') {
        size_t cap = 16; for (size_t i = 0; i < nl; i++) cap += strlen(tok[map ? (size_t)map[i] : i]) + 2;
        char *arr = (char *)malloc(cap); size_t n = 0;
        for (size_t i = 0; i < nl; i++) n += (size_t)snprintf(arr + n, cap - n, "%s%s", i ? ", " : "", tok[map ? (size_t)map[i] : i]);
        size_t head = (size_t)(lb - *buf) + 1, tail = strlen(q);
        char *nb = (char *)malloc(head + n + tail + 1);
        memcpy(nb, *buf, head); memcpy(nb + head, arr, n); memcpy(nb + head + n, q, tail + 1);
        free(*buf); *buf = nb; free(arr);
    }
    for (size_t i = 0; i < nsrc; i++) free(tok[i]);
    free(tok);
}

static char *slurp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f); b[n] = 0;
    return b;
}
static int spit(const char *path, const char *s) { FILE *f = fopen(path, "wb"); if (!f) return -1; fputs(s, f); return fclose(f); }

static int copy_file(const char *from, const char *to) {
    FILE *a = fopen(from, "rb"); if (!a) return -1;
    FILE *b = fopen(to, "wb"); if (!b) { fclose(a); return -1; }
    char *buf = (char *)malloc(1 << 20); int rc = buf ? 0 : -1; size_t n;
    while (rc == 0 && (n = fread(buf, 1, 1 << 20, a)) > 0) if (fwrite(buf, 1, n, b) != n) rc = -1;
    free(buf); fclose(a);
    return fclose(b) != 0 ? -1 : rc;
}

/* ------------------------------------------------------------ piano dei tensori */
typedef struct {
    char *outname; const llmwsp_leaf *L;
    int64_t *rk; size_t nrk;          /* righe tenute (NULL = tutte) */
    int64_t *ck; size_t nck;          /* colonne tenute (NULL = tutte) */
    int64_t shape[2]; int nd;
} item;

static int cmp_item(const void *a, const void *b) { return strcmp(((const item *)a)->outname, ((const item *)b)->outname); }

static int emit(llmwsp_writer *w, const llmwsp_store *s, const llmwsp_tensor *t, const item *it) {
    size_t es = llmwsp_dtype_size(t->dtype);
    int64_t R = t->shape[0], C = t->ndim == 2 ? t->shape[1] : 1;
    int64_t nrows = it->rk ? (int64_t)it->nrk : R;
    size_t rowb = (size_t)C * es;
    unsigned char *row = (unsigned char *)malloc(rowb ? rowb : 1), *gat = it->ck ? (unsigned char *)malloc(it->nck * es + 1) : NULL;
    if (!row || (it->ck && !gat)) { free(row); free(gat); return -1; }
    int rc = 0;
    for (int64_t i = 0; i < nrows && rc == 0; i++) {
        int64_t r = it->rk ? it->rk[i] : i;
        if (llmwsp_read_bytes(s, t, (uint64_t)r * rowb, rowb, row) < 0) { rc = -1; break; }
        if (it->ck) {
            for (size_t k = 0; k < it->nck; k++) memcpy(gat + k * es, row + (size_t)it->ck[k] * es, es);
            rc = llmwsp_writer_append(w, gat, it->nck * es);
        } else rc = llmwsp_writer_append(w, row, rowb);
    }
    free(row); free(gat);
    return rc;
}

static void free_items(item *it, size_t n) { for (size_t i = 0; i < n; i++) { free(it[i].outname); free(it[i].rk); free(it[i].ck); } free(it); }


int llmwsp_derive(const llmwsp_store *s, const llmwsp_doc *d, const char *srcdir, const llmwsp_derive_ops *ops, const char *outdir,
                  llmwsp_derive_report *rep, char *err, size_t ec) {
    if (err && ec) err[0] = 0;
    if (llmwsp_derive_validate(s, d, ops, err, ec) < 0) return -1;
    llmwsp_dims g; llmwsp_doc_dims(d, &g);
    size_t nl = ops->layer_map ? ops->nl : (size_t)g.layers;
    int64_t gr = g.heads / (g.kv_heads ? g.kv_heads : 1);
    size_t kvm = ops->kv_keep ? ops->kv_m : (size_t)g.kv_heads;
    size_t neuk = ops->neuron_keep ? ops->neu_k : (size_t)g.inter;
    /* outdir nuova o vuota */
    struct stat st;
    if (stat(outdir, &st) == 0) {
        DIR *dp = opendir(outdir); int busy = 0; struct dirent *e;
        if (!dp) { E(err, ec, "outdir non utilizzabile", outdir); return -1; }
        while ((e = readdir(dp))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) busy = 1;
        closedir(dp);
        if (busy) { E(err, ec, "outdir non vuota", outdir); return -1; }
    } else {
#if defined(_WIN32)
        mkdir(outdir);
#else
        mkdir(outdir, 0755);
#endif
    }

    size_t cap = llmwsp_doc_leaf_count(d) * (nl ? nl : 1) / (size_t)(g.layers ? g.layers : 1) + 8, n = 0;
    item *it = (item *)calloc(cap + llmwsp_doc_leaf_count(d), sizeof *it);
    if (!it) return -1;
    uint64_t before = llmwsp_store_total_bytes(s);
    for (size_t i = 0; i < llmwsp_doc_leaf_count(d); i++) {            /* tensori globali */
        const llmwsp_leaf *L = llmwsp_doc_leaf(d, i);
        if (L->layer >= 0) continue;
        item *x = &it[n++]; x->outname = strdup(L->t->name); x->L = L; x->nd = L->t->ndim;
        for (int k = 0; k < L->t->ndim; k++) x->shape[k] = L->t->shape[k];
    }
    for (size_t j = 0; j < nl; j++) {
        int src = ops->layer_map ? ops->layer_map[j] : (int)j;
        char pre[48]; snprintf(pre, sizeof pre, "model.layers.%d.", src);
        for (size_t i = 0; i < llmwsp_doc_leaf_count(d); i++) {
            const llmwsp_leaf *L = llmwsp_doc_leaf(d, i);
            if (L->layer != src) continue;
            if (strncmp(L->t->name, pre, strlen(pre)) != 0) { free_items(it, n); E(err, ec, "convenzione di nomi non supportata", L->t->name); return -1; }
            item *x = &it[n++];
            char nm[300]; snprintf(nm, sizeof nm, "model.layers.%zu.%s", j, L->t->name + strlen(pre));
            x->outname = strdup(nm); x->L = L; x->nd = L->t->ndim;
            for (int k = 0; k < L->t->ndim; k++) x->shape[k] = L->t->shape[k];
            char br[40]; snprintf(br, sizeof br, "%s", L->role);
            int bias = has_sfx(br, "_bias"); if (bias) br[strlen(br) - 5] = 0;
            if (ops->kv_keep && (!strcmp(br, "attn_q") || !strcmp(br, "attn_k") || !strcmp(br, "attn_v") || !strcmp(br, "attn_o"))) {
                int isq = !strcmp(br, "attn_q") || !strcmp(br, "attn_o");
                size_t per = isq ? (size_t)gr : 1, cnt = kvm * per * (size_t)g.head_dim;
                int64_t *idx = (int64_t *)malloc(cnt * sizeof(int64_t)); size_t c = 0;
                for (size_t a = 0; a < kvm; a++) {
                    int64_t grp = ops->kv_keep[j * kvm + a];
                    for (size_t h = 0; h < per; h++) for (int64_t q = 0; q < g.head_dim; q++) idx[c++] = (grp * (int64_t)per + (int64_t)h) * g.head_dim + q;
                }
                if (!strcmp(br, "attn_o") && !bias) { x->ck = idx; x->nck = cnt; x->shape[1] = (int64_t)cnt; }
                else { x->rk = idx; x->nrk = cnt; x->shape[0] = (int64_t)cnt; }
            }
            if (ops->neuron_keep && (!strcmp(br, "mlp_gate") || !strcmp(br, "mlp_up") || !strcmp(br, "mlp_down"))) {
                int64_t *idx = (int64_t *)malloc(neuk * sizeof(int64_t));
                for (size_t a = 0; a < neuk; a++) idx[a] = ops->neuron_keep[j * neuk + a];
                if (!strcmp(br, "mlp_down") && !bias) { x->ck = idx; x->nck = neuk; x->shape[1] = (int64_t)neuk; }
                else { x->rk = idx; x->nrk = neuk; x->shape[0] = (int64_t)neuk; }
            }
        }
    }
    qsort(it, n, sizeof *it, cmp_item);
    llmwsp_wspec *sp = (llmwsp_wspec *)calloc(n, sizeof *sp);
    uint64_t after = 0;
    for (size_t i = 0; i < n; i++) {
        sp[i].name = it[i].outname; sp[i].dtype = it[i].L->t->dtype; sp[i].ndim = it[i].nd;
        uint64_t e = 1; for (int k = 0; k < it[i].nd; k++) { sp[i].shape[k] = it[i].shape[k]; e *= (uint64_t)it[i].shape[k]; }
        after += e * llmwsp_dtype_size(it[i].L->t->dtype);
    }
    char p[1100];
    snprintf(p, sizeof p, "%s/model.safetensors", outdir);
    llmwsp_writer *w = NULL;
    int rc = llmwsp_writer_open(p, sp, n, &w, err, ec);
    for (size_t i = 0; rc == 0 && i < n; i++) if (emit(w, s, it[i].L->t, &it[i]) < 0) { rc = -1; E(err, ec, "scrittura fallita", it[i].outname); }
    if (w) { char e2[200]; if (llmwsp_writer_close(w, e2, sizeof e2) < 0 && rc == 0) { rc = -1; E(err, ec, "chiusura writer", e2); } }
    free(sp); free_items(it, n);
    if (rc < 0) return -1;

    /* config.json rigenerato */
    long long nh2 = (long long)(kvm * (size_t)gr), nkv2 = (long long)kvm;
    char cp[1100]; snprintf(cp, sizeof cp, "%s/config.json", srcdir);
    char *cfg = slurp(cp);
    if (!cfg) { E(err, ec, "config.json sorgente illeggibile", cp); return -1; }
    json_set_int(&cfg, "num_hidden_layers", (long long)nl);
    if (ops->kv_keep) {
        if (!json_has(cfg, "head_dim")) { char kv[64]; snprintf(kv, sizeof kv, "\"head_dim\": %lld", (long long)g.head_dim); json_insert_first(&cfg, kv); }
        json_set_int(&cfg, "num_attention_heads", nh2); json_set_int(&cfg, "num_key_value_heads", nkv2);
    }
    if (ops->neuron_keep) json_set_int(&cfg, "intermediate_size", (long long)neuk);
    if (ops->layer_map) json_remap_array(&cfg, "layer_types", ops->layer_map, nl, (size_t)g.layers);
    snprintf(p, sizeof p, "%s/config.json", outdir);
    int w1 = spit(p, cfg);
    free(cfg);
    if (w1 < 0) { E(err, ec, "config.json non scrivibile", p); return -1; }

    /* file accessori + .gmbarch rigenerati */
    DIR *dp = opendir(srcdir);
    struct dirent *e;
    while (dp && (e = readdir(dp))) {
        char sp2[1100], dp2[1100]; struct stat fs;
        if (e->d_name[0] == '.') continue;
        snprintf(sp2, sizeof sp2, "%s/%s", srcdir, e->d_name); snprintf(dp2, sizeof dp2, "%s/%s", outdir, e->d_name);
        if (stat(sp2, &fs) < 0) continue;
        if (S_ISDIR(fs.st_mode) && !strcmp(e->d_name, "arch")) {
#if defined(_WIN32)
            mkdir(dp2);
#else
            mkdir(dp2, 0755);
#endif
            DIR *ad = opendir(sp2); struct dirent *ae;
            while (ad && (ae = readdir(ad))) {
                if (!has_sfx(ae->d_name, ".gmbarch")) continue;
                char a1[1200], a2[1200]; snprintf(a1, sizeof a1, "%s/%s", sp2, ae->d_name); snprintf(a2, sizeof a2, "%s/%s", dp2, ae->d_name);
                char *ar = slurp(a1);
                if (!ar) continue;
                json_set_int(&ar, "n_layers", (long long)nl);
                if (ops->kv_keep) { json_set_int(&ar, "n_heads", nh2); json_set_int(&ar, "n_kv_heads", nkv2); }
                if (ops->neuron_keep) json_set_int(&ar, "inter", (long long)neuk);
                spit(a2, ar); free(ar);
            }
            if (ad) closedir(ad);
            continue;
        }
        if (!S_ISREG(fs.st_mode) || has_sfx(e->d_name, ".safetensors") || !strcmp(e->d_name, "model.safetensors.index.json") ||
            !strcmp(e->d_name, "config.json") || !strcmp(e->d_name, "provenance.json")) continue;
        if (copy_file(sp2, dp2) < 0) { closedir(dp); E(err, ec, "copia file accessorio fallita", e->d_name); return -1; }
    }
    if (dp) closedir(dp);

    /* provenance */
    snprintf(p, sizeof p, "%s/provenance.json", outdir);
    FILE *pf = fopen(p, "wb");
    if (pf) {
        fprintf(pf, "{\"format\":\"gmb.wsp.derive@1\",\"source\":\"%s\",\"layers_in\":%d,\"layers_out\":%zu,\"layer_map\":[", srcdir, g.layers, nl);
        for (size_t j = 0; j < nl; j++) fprintf(pf, "%s%d", j ? "," : "", ops->layer_map ? ops->layer_map[j] : (int)j);
        fprintf(pf, "],\"kv_groups_kept\":%zu,\"kv_groups_in\":%lld,\"neurons_kept\":%zu,\"inter_in\":%lld,\"bytes_before\":%llu,\"bytes_after\":%llu}\n",
                kvm, (long long)g.kv_heads, neuk, (long long)g.inter, (unsigned long long)before, (unsigned long long)after);
        fclose(pf);
    }
    /* rivalidazione del risultato */
    llmwsp_store *so = NULL; llmwsp_doc *dd = NULL; char e2[300];
    if (llmwsp_store_open(outdir, &so, e2, sizeof e2) < 0) { E(err, ec, "risultato non riapribile", e2); return -1; }
    snprintf(cp, sizeof cp, "%s/config.json", outdir);
    if (llmwsp_doc_build(so, cp, &dd, e2, sizeof e2) < 0) { llmwsp_store_close(so); E(err, ec, "risultato: documento non costruibile", e2); return -1; }
    int v = llmwsp_doc_check(dd, e2, sizeof e2);
    if (v == 0 && llmwsp_store_check_coverage(so, e2, sizeof e2) != 0) v = 1;
    if (rep) { rep->out_layers = nl; rep->tensors = llmwsp_store_count(so); rep->bytes_before = before; rep->bytes_after = llmwsp_store_total_bytes(so); rep->heads = nh2; rep->kv_heads = nkv2; rep->inter = (long long)neuk; }
    llmwsp_doc_free(dd); llmwsp_store_close(so);
    if (v != 0) { E(err, ec, "il derivato non supera la validazione", e2); return -1; }
    return 0;
}
