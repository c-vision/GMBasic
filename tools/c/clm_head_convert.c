/* clm_head_convert.c — checkpoint CLM (`torch.save`, .pt) → safetensors + sidecar .json.
 *
 * Python-free: legge lo zip (voci STORED) e un SOTTOINSIEME RISTRETTO di pickle (protocol 2):
 * dict/OrderedDict, tuple, str, int, bool, float, il globale `_rebuild_tensor_v2` e le storage
 * persistenti. Qualunque altro opcode o globale e' un errore esplicito: il file NON viene mai
 * eseguito (nessun REDUCE arbitrario), quindi un .pt ostile non puo' far girare codice.
 *
 * Uso:   clm_head_convert <in.pt> <out.safetensors>
 * Scrive <out.safetensors> (tensori F32: state_head.*, action_head.*, logit_scale) e
 * <out>.json gemello (activation, residual, layernorm, hidden_size, projection_dim, width, depth).
 * C99, nessuna dipendenza. Build: cc -O2 tools/c/clm_head_convert.c -o clm_head_convert -lm
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIE(...) do { fprintf(stderr, "clm_head_convert: " __VA_ARGS__); fputc('\n', stderr); exit(1); } while (0)

/* ── file in memoria ── */
static uint8_t* g_file; static size_t g_size;
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

typedef struct { char name[256]; uint32_t lho; uint32_t csize, usize; uint16_t method; } zent_t;
static zent_t* g_z; static int g_nz;

static void zip_index(void)
{
    if (g_size < 22) DIE("file troppo piccolo");
    size_t i = g_size - 22;
    for (;; i--) {
        if (rd32(g_file + i) == 0x06054b50u) break;
        if (i == 0 || g_size - i > 65557) DIE("non e' uno zip (EOCD assente)");
    }
    g_nz = rd16(g_file + i + 10);
    size_t cd = rd32(g_file + i + 16);
    g_z = (zent_t*)calloc((size_t)g_nz, sizeof(zent_t));
    for (int k = 0; k < g_nz; k++) {
        if (cd + 46 > g_size || rd32(g_file + cd) != 0x02014b50u) DIE("central directory corrotta");
        zent_t* e = &g_z[k];
        e->method = rd16(g_file + cd + 10);
        e->csize = rd32(g_file + cd + 20); e->usize = rd32(g_file + cd + 24);
        uint16_t nl = rd16(g_file + cd + 28), xl = rd16(g_file + cd + 30), cl = rd16(g_file + cd + 32);
        e->lho = rd32(g_file + cd + 42);
        if (nl >= sizeof e->name) DIE("nome voce troppo lungo");
        memcpy(e->name, g_file + cd + 46, nl); e->name[nl] = 0;
        cd += 46u + nl + xl + cl;
    }
}
/* Dati di una voce STORED. */
static const uint8_t* zip_data(const char* name, size_t* n)
{
    for (int k = 0; k < g_nz; k++) {
        if (strcmp(g_z[k].name, name) != 0) continue;
        zent_t* e = &g_z[k];
        if (e->method != 0) DIE("voce '%s' compressa (solo STORED supportato)", name);
        size_t lo = e->lho;
        if (lo + 30 > g_size || rd32(g_file + lo) != 0x04034b50u) DIE("local header corrotto");
        size_t off = lo + 30u + rd16(g_file + lo + 26) + rd16(g_file + lo + 28);
        if (off + e->usize > g_size) DIE("voce '%s' oltre la fine del file", name);
        *n = e->usize;
        return g_file + off;
    }
    return NULL;
}

/* ── valori pickle ── */
enum { V_NONE, V_INT, V_FLOAT, V_BOOL, V_STR, V_TUPLE, V_LIST, V_DICT, V_GLOBAL, V_STORAGE, V_TENSOR, V_MARK };
typedef struct val {
    int t;
    int64_t i;
    double f;
    char* s;                       /* STR / GLOBAL ("modulo nome") / STORAGE: chiave */
    struct val** it; int n, cap;   /* TUPLE/LIST items; DICT: k0,v0,k1,v1,... */
    int dtype;                     /* STORAGE: 0 F32, 1 F16, 2 BF16 */
    int64_t shape[8]; int ndim; int64_t stride[8]; int64_t off; struct val* st;   /* TENSOR */
} val;

static val* nv(int t) { val* v = (val*)calloc(1, sizeof *v); if (!v) DIE("memoria"); v->t = t; return v; }
static void push_item(val* v, val* x)
{
    if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 4; v->it = (val**)realloc(v->it, (size_t)v->cap * sizeof(val*)); if (!v->it) DIE("memoria"); }
    v->it[v->n++] = x;
}
static char* sdup(const uint8_t* p, size_t n) { char* s = (char*)malloc(n + 1); if (!s) DIE("memoria"); memcpy(s, p, n); s[n] = 0; return s; }

#define STK_MAX 4096
static val* g_stk[STK_MAX]; static int g_sp;
static val** g_memo; static int g_memo_n;
static void push(val* v) { if (g_sp >= STK_MAX) DIE("stack pickle pieno"); g_stk[g_sp++] = v; }
static val* pop(void) { if (g_sp <= 0) DIE("stack pickle vuoto"); return g_stk[--g_sp]; }
static int find_mark(void) { for (int i = g_sp - 1; i >= 0; i--) if (g_stk[i]->t == V_MARK) return i; DIE("MARK mancante"); return -1; }
static void memo_put(int idx, val* v)
{
    if (idx < 0 || idx > 1000000) DIE("indice memo non valido");
    if (idx >= g_memo_n) { int nn = idx + 256; g_memo = (val**)realloc(g_memo, (size_t)nn * sizeof(val*)); if (!g_memo) DIE("memoria"); memset(g_memo + g_memo_n, 0, (size_t)(nn - g_memo_n) * sizeof(val*)); g_memo_n = nn; }
    g_memo[idx] = v;
}
static val* memo_get(int idx) { if (idx < 0 || idx >= g_memo_n || !g_memo[idx]) DIE("BINGET su indice vuoto"); return g_memo[idx]; }
static val* dict_get(val* d, const char* k)
{
    for (int i = 0; i + 1 < d->n; i += 2) if (d->it[i]->t == V_STR && strcmp(d->it[i]->s, k) == 0) return d->it[i + 1];
    return NULL;
}

static void do_reduce(void)
{
    val* args = pop(); val* fn = pop();
    if (fn->t != V_GLOBAL || args->t != V_TUPLE) DIE("REDUCE su oggetto non supportato");
    if (strcmp(fn->s, "collections OrderedDict") == 0) { push(nv(V_DICT)); return; }
    if (strcmp(fn->s, "torch._utils _rebuild_tensor_v2") == 0) {
        if (args->n < 4 || args->it[0]->t != V_STORAGE || args->it[2]->t != V_TUPLE || args->it[3]->t != V_TUPLE) DIE("_rebuild_tensor_v2: argomenti inattesi");
        val* t = nv(V_TENSOR);
        t->st = args->it[0]; t->off = args->it[1]->i;
        t->ndim = args->it[2]->n;
        if (t->ndim > 8 || args->it[3]->n != t->ndim) DIE("tensore: ndim non valido");
        for (int k = 0; k < t->ndim; k++) { t->shape[k] = args->it[2]->it[k]->i; t->stride[k] = args->it[3]->it[k]->i; }
        push(t); return;
    }
    DIE("callable non ammesso: %s", fn->s);
}

static void parse_pickle(const uint8_t* p, size_t n)
{
    size_t i = 0;
    while (i < n) {
        uint8_t op = p[i++];
        switch (op) {
            case 0x80: i++; break;                                           /* PROTO */
            case '}': push(nv(V_DICT)); break;
            case ']': push(nv(V_LIST)); break;
            case ')': push(nv(V_TUPLE)); break;
            case '(': push(nv(V_MARK)); break;
            case 'N': push(nv(V_NONE)); break;
            case 0x88: { val* v = nv(V_BOOL); v->i = 1; push(v); break; }
            case 0x89: { val* v = nv(V_BOOL); v->i = 0; push(v); break; }
            case 'K': { val* v = nv(V_INT); v->i = p[i]; i += 1; push(v); break; }
            case 'M': { val* v = nv(V_INT); v->i = rd16(p + i); i += 2; push(v); break; }
            case 'J': { val* v = nv(V_INT); v->i = (int32_t)rd32(p + i); i += 4; push(v); break; }
            case 0x8a: { int l = p[i++]; int64_t x = 0; for (int k = 0; k < l && k < 8; k++) x |= (int64_t)p[i + k] << (8 * k); i += (size_t)l; val* v = nv(V_INT); v->i = x; push(v); break; }
            case 'G': { uint64_t b = 0; for (int k = 0; k < 8; k++) b = b << 8 | p[i + k]; i += 8; val* v = nv(V_FLOAT); memcpy(&v->f, &b, 8); push(v); break; }
            case 'X': { uint32_t l = rd32(p + i); i += 4; if (i + l > n) DIE("stringa oltre il pickle"); val* v = nv(V_STR); v->s = sdup(p + i, l); i += l; push(v); break; }
            case 0x8c: { uint8_t l = p[i++]; val* v = nv(V_STR); v->s = sdup(p + i, l); i += l; push(v); break; }
            case 'q': memo_put(p[i], g_stk[g_sp - 1]); i += 1; break;
            case 'r': memo_put((int)rd32(p + i), g_stk[g_sp - 1]); i += 4; break;
            case 'h': push(memo_get(p[i])); i += 1; break;
            case 'j': push(memo_get((int)rd32(p + i))); i += 4; break;
            case 'c': {                                                      /* GLOBAL "mod\nname\n" */
                const uint8_t* a = p + i; size_t l1 = 0; while (i + l1 < n && a[l1] != '\n') l1++;
                size_t j = i + l1 + 1; const uint8_t* b = p + j; size_t l2 = 0; while (j + l2 < n && b[l2] != '\n') l2++;
                val* v = nv(V_GLOBAL); v->s = (char*)malloc(l1 + l2 + 2); memcpy(v->s, a, l1); v->s[l1] = ' '; memcpy(v->s + l1 + 1, b, l2); v->s[l1 + 1 + l2] = 0;
                i = j + l2 + 1; push(v); break;
            }
            case 't': { int m = find_mark(); val* v = nv(V_TUPLE); for (int k = m + 1; k < g_sp; k++) push_item(v, g_stk[k]); g_sp = m; push(v); break; }
            case 0x85: { val* v = nv(V_TUPLE); val* a = pop(); push_item(v, a); push(v); break; }
            case 0x86: { val* v = nv(V_TUPLE); val* b = pop(); val* a = pop(); push_item(v, a); push_item(v, b); push(v); break; }
            case 0x87: { val* v = nv(V_TUPLE); val* c = pop(); val* b = pop(); val* a = pop(); push_item(v, a); push_item(v, b); push_item(v, c); push(v); break; }
            case 's': { val* v = pop(); val* k = pop(); val* d = g_stk[g_sp - 1]; if (d->t != V_DICT) DIE("SETITEM su non-dict"); push_item(d, k); push_item(d, v); break; }
            case 'u': { int m = find_mark(); val* d = g_stk[m - 1]; if (d->t != V_DICT) DIE("SETITEMS su non-dict"); for (int k = m + 1; k < g_sp; k++) push_item(d, g_stk[k]); g_sp = m; break; }
            case 'a': { val* v = pop(); push_item(g_stk[g_sp - 1], v); break; }
            case 'e': { int m = find_mark(); val* d = g_stk[m - 1]; for (int k = m + 1; k < g_sp; k++) push_item(d, g_stk[k]); g_sp = m; break; }
            case 'R': do_reduce(); break;
            case 'b': { (void)pop(); break; }                               /* BUILD: stato (metadata) ignorato */
            case 'Q': {                                                      /* BINPERSID: ('storage', cls, key, loc, numel) */
                val* pid = pop();
                if (pid->t != V_TUPLE || pid->n < 5 || pid->it[0]->t != V_STR || strcmp(pid->it[0]->s, "storage") != 0) DIE("persistent id non supportato");
                val* s = nv(V_STORAGE); s->s = pid->it[2]->s; s->i = pid->it[4]->i;
                const char* cls = pid->it[1]->s;
                if (strcmp(cls, "torch FloatStorage") == 0) s->dtype = 0;
                else if (strcmp(cls, "torch HalfStorage") == 0) s->dtype = 1;
                else if (strcmp(cls, "torch BFloat16Storage") == 0) s->dtype = 2;
                else DIE("tipo di storage non supportato: %s", cls);
                push(s); break;
            }
            case '.': return;
            default: DIE("opcode pickle non ammesso: 0x%02x (offset %zu)", op, i - 1);
        }
    }
    DIE("pickle senza STOP");
}

/* ── scrittura safetensors ── */
typedef struct { char name[160]; int ndim; int64_t shape[8]; float* data; size_t n; } out_t;
static out_t g_out[64]; static int g_nout;

static float half_to_float(uint16_t h)
{
    uint32_t sg = (uint32_t)(h >> 15) & 1, ex = (h >> 10) & 31, mn = h & 1023; float v;
    if (ex == 0) v = ldexpf((float)mn, -24); else if (ex == 31) v = mn ? NAN : INFINITY; else v = ldexpf((float)(mn | 1024), (int)ex - 25);
    return sg ? -v : v;
}

static void add_tensor(const char* prefix, const char* key, val* t, const char* root)
{
    if (t->t != V_TENSOR) DIE("'%s%s' non e' un tensore", prefix, key);
    if (g_nout >= 64) DIE("troppi tensori");
    size_t cnt = 1; for (int k = 0; k < t->ndim; k++) cnt *= (size_t)t->shape[k];
    int64_t exp = 1;                                           /* contiguo row-major? */
    for (int k = t->ndim - 1; k >= 0; k--) { if (t->shape[k] != 1 && t->stride[k] != exp) DIE("tensore '%s%s' non contiguo", prefix, key); exp *= t->shape[k]; }
    char zn[512]; snprintf(zn, sizeof zn, "%s/data/%s", root, t->st->s);
    size_t zl = 0; const uint8_t* d = zip_data(zn, &zl);
    if (!d) DIE("storage '%s' assente nello zip", zn);
    size_t bytes = t->st->dtype == 0 ? 4 : 2;
    if (((size_t)t->off + cnt) * bytes > zl) DIE("storage '%s' troppo corta", zn);
    out_t* o = &g_out[g_nout++];
    snprintf(o->name, sizeof o->name, "%s%s", prefix, key);
    o->ndim = t->ndim; for (int k = 0; k < t->ndim; k++) o->shape[k] = t->shape[k];
    if (o->ndim == 0) { o->ndim = 1; o->shape[0] = 1; }                      /* scalare → [1] */
    o->n = cnt; o->data = (float*)malloc(cnt * sizeof(float)); if (!o->data) DIE("memoria");
    for (size_t k = 0; k < cnt; k++) {
        const uint8_t* q = d + ((size_t)t->off + k) * bytes;
        if (t->st->dtype == 0) { uint32_t u = rd32(q); memcpy(&o->data[k], &u, 4); }
        else if (t->st->dtype == 1) o->data[k] = half_to_float(rd16(q));
        else { uint32_t u = (uint32_t)rd16(q) << 16; memcpy(&o->data[k], &u, 4); }
    }
}

int main(int argc, char** argv)
{
    if (argc != 3) { fprintf(stderr, "uso: clm_head_convert <in.pt> <out.safetensors>\n"); return 2; }
    FILE* f = fopen(argv[1], "rb"); if (!f) DIE("impossibile aprire %s", argv[1]);
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) DIE("file vuoto");
    g_size = (size_t)sz; g_file = (uint8_t*)malloc(g_size); if (!g_file || fread(g_file, 1, g_size, f) != g_size) DIE("lettura fallita");
    fclose(f);
    zip_index();
    char root[256] = ""; size_t pl = 0; const uint8_t* pk = NULL;
    for (int k = 0; k < g_nz; k++) {
        size_t L = strlen(g_z[k].name);
        if (L > 9 && strcmp(g_z[k].name + L - 9, "/data.pkl") == 0) { snprintf(root, sizeof root, "%.*s", (int)(L - 9), g_z[k].name); pk = zip_data(g_z[k].name, &pl); break; }
    }
    if (!pk) DIE("data.pkl non trovato");
    parse_pickle(pk, pl);
    if (g_sp != 1 || g_stk[0]->t != V_DICT) DIE("radice del checkpoint non e' un dict");
    val* ck = g_stk[0];
    const char* heads[2] = { "state_head", "action_head" };
    for (int h = 0; h < 2; h++) {
        val* d = dict_get(ck, heads[h]);
        if (!d || d->t != V_DICT) DIE("manca '%s'", heads[h]);
        char pre[64]; snprintf(pre, sizeof pre, "%s.", heads[h]);
        for (int k = 0; k + 1 < d->n; k += 2) {
            if (d->it[k]->t != V_STR || d->it[k + 1]->t != V_TENSOR) continue;      /* salta _metadata */
            add_tensor(pre, d->it[k]->s, d->it[k + 1], root);
        }
    }
    val* ls = dict_get(ck, "logit_scale");
    if (!ls) DIE("manca 'logit_scale'");
    if (ls->t == V_TENSOR) add_tensor("", "logit_scale", ls, root);
    else if (ls->t == V_FLOAT) { out_t* o = &g_out[g_nout++]; strcpy(o->name, "logit_scale"); o->ndim = 1; o->shape[0] = 1; o->n = 1; o->data = (float*)malloc(sizeof(float)); o->data[0] = (float)ls->f; }
    else DIE("logit_scale di tipo inatteso");

    /* header safetensors */
    size_t cap = 1 << 16; char* hdr = (char*)malloc(cap); size_t hl = 0; uint64_t off = 0;
    hl += (size_t)snprintf(hdr + hl, cap - hl, "{");
    for (int k = 0; k < g_nout; k++) {
        out_t* o = &g_out[k];
        hl += (size_t)snprintf(hdr + hl, cap - hl, "%s\"%s\":{\"dtype\":\"F32\",\"shape\":[", k ? "," : "", o->name);
        for (int d = 0; d < o->ndim; d++) hl += (size_t)snprintf(hdr + hl, cap - hl, "%s%lld", d ? "," : "", (long long)o->shape[d]);
        hl += (size_t)snprintf(hdr + hl, cap - hl, "],\"data_offsets\":[%llu,%llu]}", (unsigned long long)off, (unsigned long long)(off + o->n * 4));
        off += o->n * 4;
    }
    hl += (size_t)snprintf(hdr + hl, cap - hl, "}");
    while (hl % 8) hdr[hl++] = ' ';
    FILE* w = fopen(argv[2], "wb"); if (!w) DIE("impossibile scrivere %s", argv[2]);
    uint8_t lb[8]; for (int k = 0; k < 8; k++) lb[k] = (uint8_t)((uint64_t)hl >> (8 * k));
    fwrite(lb, 1, 8, w); fwrite(hdr, 1, hl, w);
    for (int k = 0; k < g_nout; k++) fwrite(g_out[k].data, 4, g_out[k].n, w);
    fclose(w);

    /* sidecar */
    val* cfg = dict_get(ck, "cfg");
    char side[1024]; size_t L = strlen(argv[2]);
    if (L > 12 && strcmp(argv[2] + L - 12, ".safetensors") == 0) snprintf(side, sizeof side, "%.*s.json", (int)(L - 12), argv[2]); else snprintf(side, sizeof side, "%s.json", argv[2]);
    FILE* sj = fopen(side, "wb"); if (!sj) DIE("impossibile scrivere %s", side);
    val* act = cfg ? dict_get(cfg, "activation") : NULL; val* res = cfg ? dict_get(cfg, "residual") : NULL; val* lnv = cfg ? dict_get(cfg, "layernorm") : NULL;
    val* hs = cfg ? dict_get(cfg, "hidden_size") : NULL; val* pd = cfg ? dict_get(cfg, "projection_dim") : NULL;
    val* wd = cfg ? dict_get(cfg, "width") : NULL; val* dp = cfg ? dict_get(cfg, "depth") : NULL;
    fprintf(sj, "{\"activation\":\"%s\",\"residual\":%s,\"layernorm\":%s,\"hidden_size\":%lld,\"projection_dim\":%lld,\"width\":%lld,\"depth\":%lld}\n",
            act && act->t == V_STR ? act->s : "gelu", res && res->i ? "true" : "false", lnv && lnv->i ? "true" : "false",
            hs ? (long long)hs->i : 0, pd ? (long long)pd->i : 0, wd ? (long long)wd->i : 0, dp ? (long long)dp->i : 0);
    fclose(sj);
    printf("clm_head_convert: %d tensori F32 -> %s (+ %s)\n", g_nout, argv[2], side);
    return 0;
}
