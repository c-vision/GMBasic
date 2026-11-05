/* llmwsp_store.c — Weight Store (W1 del piano Weight-Space Programming). C99, sola lettura. */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_store.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct llmwsp_store {
    char          *dir;
    int            nfiles;
    char         **files;      /* percorsi completi */
    char         **base;       /* nomi file */
    int           *fds;
    uint64_t      *fsize;
    uint64_t      *data0;      /* inizio zona dati per shard */
    llmwsp_tensor *t;
    size_t         n, cap;
    uint64_t       total;
};

static void seterr(char *err, size_t cap, const char *fmt, const char *a, const char *b) {
    if (err && cap) snprintf(err, cap, fmt, a ? a : "", b ? b : "");
}

/* ---------------------------------------------------------------- dtype */
size_t llmwsp_dtype_size(const char *d) {
    static const struct { const char *n; size_t s; } tab[] = {
        {"BOOL", 1}, {"U8", 1}, {"I8", 1}, {"F8_E4M3", 1}, {"F8_E5M2", 1},
        {"U16", 2}, {"I16", 2}, {"F16", 2}, {"BF16", 2},
        {"U32", 4}, {"I32", 4}, {"F32", 4},
        {"U64", 8}, {"I64", 8}, {"F64", 8}, {NULL, 0}};
    for (int i = 0; tab[i].n; i++) if (strcmp(tab[i].n, d) == 0) return tab[i].s;
    return 0;
}

int64_t llmwsp_tensor_elems(const llmwsp_tensor *t) {
    int64_t n = 1;
    for (int i = 0; i < t->ndim; i++) n *= t->shape[i];
    return n;
}

/* ---------------------------------------------------------------- mini JSON */
typedef struct { const char *p, *e; int bad; } jr;

static void ws(jr *j) { while (j->p < j->e && (*j->p == ' ' || *j->p == '\n' || *j->p == '\t' || *j->p == '\r')) j->p++; }
static int  eat(jr *j, char c) { ws(j); if (j->p < j->e && *j->p == c) { j->p++; return 1; } return 0; }

/* stringa -> buffer allocato (caller libera) */
static char *jstr(jr *j) {
    ws(j);
    if (j->p >= j->e || *j->p != '"') { j->bad = 1; return NULL; }
    j->p++;
    const char *s = j->p;
    size_t cap = 0;
    while (j->p < j->e && *j->p != '"') { if (*j->p == '\\') j->p++; j->p++; }
    if (j->p >= j->e) { j->bad = 1; return NULL; }
    cap = (size_t)(j->p - s);
    char *o = (char *)malloc(cap + 1);
    if (!o) { j->bad = 1; return NULL; }
    size_t k = 0;
    for (const char *q = s; q < j->p; q++) {
        if (*q == '\\' && q + 1 < j->p) q++;
        o[k++] = *q;
    }
    o[k] = 0;
    j->p++;
    return o;
}

static int64_t jint(jr *j) {
    ws(j);
    if (j->p >= j->e || ((*j->p < '0' || *j->p > '9') && *j->p != '-')) { j->bad = 1; return 0; }
    int neg = 0; if (*j->p == '-') { neg = 1; j->p++; }
    int64_t v = 0;
    while (j->p < j->e && *j->p >= '0' && *j->p <= '9') { v = v * 10 + (*j->p - '0'); j->p++; }
    return neg ? -v : v;
}

static void jskip(jr *j) {
    ws(j);
    if (j->p >= j->e) { j->bad = 1; return; }
    if (*j->p == '"') { free(jstr(j)); return; }
    if (*j->p == '{' || *j->p == '[') {
        char open = *j->p, close = open == '{' ? '}' : ']';
        int depth = 0;
        while (j->p < j->e) {
            if (*j->p == '"') { free(jstr(j)); if (j->bad) return; continue; }
            if (*j->p == open) depth++;
            else if (*j->p == close) { depth--; if (depth == 0) { j->p++; return; } }
            j->p++;
        }
        j->bad = 1; return;
    }
    while (j->p < j->e && *j->p != ',' && *j->p != '}' && *j->p != ']') j->p++;
}

static int add_tensor(llmwsp_store *s, llmwsp_tensor *t) {
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 256;
        llmwsp_tensor *nt = (llmwsp_tensor *)realloc(s->t, nc * sizeof *nt);
        if (!nt) return -1;
        s->t = nt; s->cap = nc;
    }
    s->t[s->n++] = *t;
    return 0;
}

/* ---------------------------------------------------------------- shard */
static int read_exact(int fd, void *buf, size_t n, uint64_t off) {
    char *p = (char *)buf;
    while (n) {
        ssize_t r = pread(fd, p, n, (off_t)off);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r; off += (uint64_t)r;
    }
    return 0;
}

static int parse_shard(llmwsp_store *s, int fi, char *err, size_t cap) {
    int fd = s->fds[fi];
    unsigned char lb[8];
    if (read_exact(fd, lb, 8, 0) < 0) { seterr(err, cap, "header illeggibile: %s%s", s->base[fi], ""); return -1; }
    uint64_t hl = 0;
    for (int i = 7; i >= 0; i--) hl = (hl << 8) | lb[i];
    if (hl == 0 || hl > (256u << 20) || 8 + hl > s->fsize[fi]) { seterr(err, cap, "header non valido: %s%s", s->base[fi], ""); return -1; }
    char *h = (char *)malloc((size_t)hl + 1);
    if (!h) return -1;
    if (read_exact(fd, h, (size_t)hl, 8) < 0) { free(h); seterr(err, cap, "header troncato: %s%s", s->base[fi], ""); return -1; }
    s->data0[fi] = 8 + hl;
    jr j = {h, h + hl, 0};
    int rc = -1;
    if (!eat(&j, '{')) goto done;
    ws(&j);
    if (j.p < j.e && *j.p == '}') { rc = 0; goto done; }
    for (;;) {
        char *key = jstr(&j);
        if (!key || !eat(&j, ':')) { free(key); goto done; }
        if (strcmp(key, "__metadata__") == 0) { free(key); jskip(&j); if (j.bad) goto done; }
        else {
            llmwsp_tensor t; memset(&t, 0, sizeof t);
            t.name = key; t.file = fi;
            int64_t o0 = -1, o1 = -1; int have_dt = 0, have_shape = 0;
            if (!eat(&j, '{')) { free(key); goto done; }
            for (;;) {
                char *k2 = jstr(&j);
                if (!k2 || !eat(&j, ':')) { free(k2); free(key); goto done; }
                if (strcmp(k2, "dtype") == 0) {
                    char *d = jstr(&j);
                    if (!d) { free(k2); free(key); goto done; }
                    snprintf(t.dtype, sizeof t.dtype, "%s", d); free(d); have_dt = 1;
                } else if (strcmp(k2, "shape") == 0) {
                    if (!eat(&j, '[')) { free(k2); free(key); goto done; }
                    ws(&j);
                    if (!eat(&j, ']')) {
                        for (;;) {
                            int64_t v = jint(&j);
                            if (t.ndim >= LLMWSP_MAX_DIMS) { free(k2); free(key); goto done; }
                            t.shape[t.ndim++] = v;
                            if (eat(&j, ',')) continue;
                            if (eat(&j, ']')) break;
                            free(k2); free(key); goto done;
                        }
                    }
                    have_shape = 1;
                } else if (strcmp(k2, "data_offsets") == 0) {
                    if (!eat(&j, '[')) { free(k2); free(key); goto done; }
                    o0 = jint(&j);
                    if (!eat(&j, ',')) { free(k2); free(key); goto done; }
                    o1 = jint(&j);
                    if (!eat(&j, ']')) { free(k2); free(key); goto done; }
                } else jskip(&j);
                free(k2);
                if (j.bad) { free(key); goto done; }
                if (eat(&j, ',')) continue;
                if (eat(&j, '}')) break;
                free(key); goto done;
            }
            if (!have_dt || !have_shape || o0 < 0 || o1 < o0) { free(key); seterr(err, cap, "tensore malformato in %s%s", s->base[fi], ""); goto done; }
            t.offset = s->data0[fi] + (uint64_t)o0;
            t.nbytes = (uint64_t)(o1 - o0);
            if (t.offset + t.nbytes > s->fsize[fi]) { free(key); seterr(err, cap, "offset oltre la fine in %s%s", s->base[fi], ""); goto done; }
            size_t es = llmwsp_dtype_size(t.dtype);
            if (es && (uint64_t)llmwsp_tensor_elems(&t) * es != t.nbytes) {
                free(key); seterr(err, cap, "byte != forma*dtype in %s%s", s->base[fi], ""); goto done;
            }
            if (add_tensor(s, &t) < 0) { free(key); goto done; }
            s->total += t.nbytes;
        }
        if (eat(&j, ',')) continue;
        if (eat(&j, '}')) { rc = 0; break; }
        goto done;
    }
done:
    free(h);
    if (rc < 0 && err && cap && !err[0]) seterr(err, cap, "JSON header non valido: %s%s", s->base[fi], "");
    return rc;
}

static int cmp_t(const void *a, const void *b) {
    return strcmp(((const llmwsp_tensor *)a)->name, ((const llmwsp_tensor *)b)->name);
}
static int cmp_s(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int add_file(llmwsp_store *s, const char *dir, const char *name) {
    int n = s->nfiles + 1;
    char **f = (char **)realloc(s->files, (size_t)n * sizeof *f);
    if (!f) return -1; s->files = f;
    char **b = (char **)realloc(s->base, (size_t)n * sizeof *b);
    if (!b) return -1; s->base = b;
    size_t L = strlen(dir) + strlen(name) + 2;
    s->files[n - 1] = (char *)malloc(L);
    s->base[n - 1] = strdup(name);
    if (!s->files[n - 1] || !s->base[n - 1]) return -1;
    if (dir[0]) snprintf(s->files[n - 1], L, "%s/%s", dir, name); else snprintf(s->files[n - 1], L, "%s", name);
    s->nfiles = n;
    return 0;
}

int llmwsp_store_open(const char *path, llmwsp_store **out, char *err, size_t errcap) {
    if (err && errcap) err[0] = 0;
    *out = NULL;
    llmwsp_store *s = (llmwsp_store *)calloc(1, sizeof *s);
    if (!s) return -1;
    struct stat st;
    if (stat(path, &st) < 0) { seterr(err, errcap, "percorso inesistente: %s%s", path, ""); free(s); return -1; }
    if (S_ISDIR(st.st_mode)) {
        s->dir = strdup(path);
        DIR *d = opendir(path);
        if (!d) { seterr(err, errcap, "directory illeggibile: %s%s", path, ""); llmwsp_store_close(s); return -1; }
        struct dirent *e;
        while ((e = readdir(d))) {
            size_t L = strlen(e->d_name);
            if (L > 12 && strcmp(e->d_name + L - 12, ".safetensors") == 0 && e->d_name[0] != '.')
                if (add_file(s, path, e->d_name) < 0) { closedir(d); llmwsp_store_close(s); return -1; }
        }
        closedir(d);
        /* ordine deterministico degli shard (coppia files/base ordinata insieme) */
        for (int i = 1; i < s->nfiles; i++)
            for (int k = i; k > 0 && strcmp(s->base[k - 1], s->base[k]) > 0; k--) {
                char *x = s->files[k]; s->files[k] = s->files[k - 1]; s->files[k - 1] = x;
                x = s->base[k]; s->base[k] = s->base[k - 1]; s->base[k - 1] = x;
            }
        (void)cmp_s;
    } else {
        s->dir = strdup(path);
        const char *sl = strrchr(path, '/');
        if (add_file(s, "", path) < 0) { llmwsp_store_close(s); return -1; }
        free(s->base[0]); s->base[0] = strdup(sl ? sl + 1 : path);
    }
    if (s->nfiles == 0) { seterr(err, errcap, "nessun .safetensors in %s%s", path, ""); llmwsp_store_close(s); return -1; }
    s->fds = (int *)malloc((size_t)s->nfiles * sizeof(int));
    s->fsize = (uint64_t *)calloc((size_t)s->nfiles, sizeof(uint64_t));
    s->data0 = (uint64_t *)calloc((size_t)s->nfiles, sizeof(uint64_t));
    if (!s->fds || !s->fsize || !s->data0) { llmwsp_store_close(s); return -1; }
    for (int i = 0; i < s->nfiles; i++) s->fds[i] = -1;
    for (int i = 0; i < s->nfiles; i++) {
        s->fds[i] = open(s->files[i], O_RDONLY);
        struct stat fs;
        if (s->fds[i] < 0 || fstat(s->fds[i], &fs) < 0) { seterr(err, errcap, "impossibile aprire %s%s", s->files[i], ""); llmwsp_store_close(s); return -1; }
        s->fsize[i] = (uint64_t)fs.st_size;
        if (parse_shard(s, i, err, errcap) < 0) { llmwsp_store_close(s); return -1; }
    }
    qsort(s->t, s->n, sizeof *s->t, cmp_t);
    for (size_t i = 1; i < s->n; i++)
        if (strcmp(s->t[i - 1].name, s->t[i].name) == 0) {
            seterr(err, errcap, "tensore duplicato fra shard: %s%s", s->t[i].name, "");
            llmwsp_store_close(s); return -1;
        }
    *out = s;
    return 0;
}

void llmwsp_store_close(llmwsp_store *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) free(s->t[i].name);
    free(s->t);
    for (int i = 0; i < s->nfiles; i++) {
        if (s->fds && s->fds[i] >= 0) close(s->fds[i]);
        if (s->files) free(s->files[i]);
        if (s->base) free(s->base[i]);
    }
    free(s->files); free(s->base); free(s->fds); free(s->fsize); free(s->data0); free(s->dir);
    free(s);
}

size_t llmwsp_store_count(const llmwsp_store *s) { return s->n; }
const llmwsp_tensor *llmwsp_store_at(const llmwsp_store *s, size_t i) { return i < s->n ? &s->t[i] : NULL; }
uint64_t llmwsp_store_total_bytes(const llmwsp_store *s) { return s->total; }
int llmwsp_store_file_count(const llmwsp_store *s) { return s->nfiles; }
const char *llmwsp_store_file_name(const llmwsp_store *s, int i) { return (i >= 0 && i < s->nfiles) ? s->base[i] : NULL; }

const llmwsp_tensor *llmwsp_store_find(const llmwsp_store *s, const char *name) {
    size_t lo = 0, hi = s->n;
    while (lo < hi) {
        size_t m = (lo + hi) / 2;
        int c = strcmp(s->t[m].name, name);
        if (c == 0) return &s->t[m];
        if (c < 0) lo = m + 1; else hi = m;
    }
    return NULL;
}

/* ---------------------------------------------------------------- letture */
int64_t llmwsp_read_bytes(const llmwsp_store *s, const llmwsp_tensor *t, uint64_t off, uint64_t n, void *buf) {
    if (off + n > t->nbytes) return -1;
    if (n == 0) return 0;
    if (read_exact(s->fds[t->file], buf, (size_t)n, t->offset + off) < 0) return -1;
    return (int64_t)n;
}

static int64_t row_bytes(const llmwsp_tensor *t) {
    size_t es = llmwsp_dtype_size(t->dtype);
    if (!es || t->ndim < 1) return -1;
    int64_t r = (int64_t)es;
    for (int i = 1; i < t->ndim; i++) r *= t->shape[i];
    return r;
}

int64_t llmwsp_read_rows(const llmwsp_store *s, const llmwsp_tensor *t, int64_t r0, int64_t r1, void *buf, size_t cap) {
    int64_t rb = row_bytes(t);
    if (rb < 0 || r0 < 0 || r1 < r0 || r1 > t->shape[0]) return -1;
    uint64_t n = (uint64_t)(r1 - r0) * (uint64_t)rb;
    if (n > cap) return -1;
    return llmwsp_read_bytes(s, t, (uint64_t)r0 * (uint64_t)rb, n, buf);
}

int64_t llmwsp_read_cols(const llmwsp_store *s, const llmwsp_tensor *t, int64_t c0, int64_t c1, void *buf, size_t cap) {
    size_t es = llmwsp_dtype_size(t->dtype);
    if (!es || t->ndim != 2 || c0 < 0 || c1 < c0 || c1 > t->shape[1]) return -1;
    uint64_t seg = (uint64_t)(c1 - c0) * es;
    uint64_t total = seg * (uint64_t)t->shape[0];
    if (total > cap) return -1;
    char *o = (char *)buf;
    for (int64_t r = 0; r < t->shape[0]; r++) {
        uint64_t off = ((uint64_t)r * (uint64_t)t->shape[1] + (uint64_t)c0) * es;
        if (llmwsp_read_bytes(s, t, off, seg, o) < 0) return -1;
        o += seg;
    }
    return (int64_t)total;
}

/* ---------------------------------------------------------------- sha256 */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char b[64]; size_t bl; } sha;
static const uint32_t K[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void sha_blk(sha *c, const unsigned char *p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3]; e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25), ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22), mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void sha_init(sha *c) {
    static const uint32_t i0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(c->h, i0, sizeof i0); c->len = 0; c->bl = 0;
}
static void sha_upd(sha *c, const unsigned char *p, size_t n) {
    c->len += n;
    while (n) {
        size_t k = 64 - c->bl; if (k > n) k = n;
        memcpy(c->b + c->bl, p, k); c->bl += k; p += k; n -= k;
        if (c->bl == 64) { sha_blk(c, c->b); c->bl = 0; }
    }
}
static void sha_fin(sha *c, char hex[65]) {
    uint64_t bits = c->len * 8;
    unsigned char pad = 0x80;
    sha_upd(c, &pad, 1);
    unsigned char z = 0;
    while (c->bl != 56) sha_upd(c, &z, 1);
    unsigned char L[8];
    for (int i = 0; i < 8; i++) L[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha_upd(c, L, 8);
    for (int i = 0; i < 8; i++) snprintf(hex + 8 * i, 9, "%08x", c->h[i]);
}

int llmwsp_tensor_sha256(const llmwsp_store *s, const llmwsp_tensor *t, char hex[65]) {
    enum { CH = 1 << 20 };
    unsigned char *buf = (unsigned char *)malloc(CH);
    if (!buf) return -1;
    sha c; sha_init(&c);
    for (uint64_t off = 0; off < t->nbytes;) {
        uint64_t n = t->nbytes - off; if (n > CH) n = CH;
        if (llmwsp_read_bytes(s, t, off, n, buf) < 0) { free(buf); return -1; }
        sha_upd(&c, buf, (size_t)n); off += n;
    }
    sha_fin(&c, hex);
    free(buf);
    return 0;
}

/* ---------------------------------------------------------------- copertura */
typedef struct { uint64_t off, n; } iv;
static int cmp_iv(const void *a, const void *b) {
    uint64_t x = ((const iv *)a)->off, y = ((const iv *)b)->off;
    return x < y ? -1 : x > y;
}

int llmwsp_store_check_coverage(const llmwsp_store *s, char *err, size_t errcap) {
    int viol = 0;
    if (err && errcap) err[0] = 0;
    for (int f = 0; f < s->nfiles; f++) {
        size_t c = 0;
        iv *v = (iv *)malloc((s->n ? s->n : 1) * sizeof *v);
        if (!v) return -1;
        for (size_t i = 0; i < s->n; i++)
            if (s->t[i].file == f) { v[c].off = s->t[i].offset; v[c].n = s->t[i].nbytes; c++; }
        qsort(v, c, sizeof *v, cmp_iv);
        uint64_t pos = s->data0[f];
        for (size_t i = 0; i < c; i++) {
            if (v[i].off != pos) {
                if (!viol && err && errcap)
                    snprintf(err, errcap, "%s: %s a %llu", s->base[f], v[i].off > pos ? "buco" : "sovrapposizione", (unsigned long long)pos);
                viol++;
            }
            pos = v[i].off + v[i].n;
        }
        if (pos != s->fsize[f]) {
            if (!viol && err && errcap) snprintf(err, errcap, "%s: byte in coda non coperti", s->base[f]);
            viol++;
        }
        free(v);
    }
    return viol;
}
