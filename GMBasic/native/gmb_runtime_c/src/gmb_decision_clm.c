/* gmb_decision_clm.c — provider `clm` (JEV/SystemOne). Vedi gmb_decision_clm.h.
 * C99 puro: nessuna dipendenza da MLX, rete o runtime BASIC. */
#include "gmb_decision_clm.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "gmb_st.h"

/* ── buffer dinamico ─────────────────────────────────────────────────────────────────────── */
typedef struct { char* p; size_t n, cap; int oom; } sb_t;

static void sb_put(sb_t* b, const char* s, size_t n)
{
    if (b->oom) return;
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 128;
        while (nc < b->n + n + 1) nc *= 2;
        char* q = (char*)realloc(b->p, nc);
        if (!q) { b->oom = 1; return; }
        b->p = q; b->cap = nc;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}
static void sb_str(sb_t* b, const char* s) { sb_put(b, s, strlen(s)); }
static char* sb_take(sb_t* b)
{
    if (b->oom) { free(b->p); return NULL; }
    if (!b->p) return (char*)calloc(1, 1);
    return b->p;
}
static void sb_json_str(sb_t* b, const char* s)
{
    sb_put(b, "\"", 1);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        char t[8];
        switch (c) {
            case '"':  sb_str(b, "\\\""); break;
            case '\\': sb_str(b, "\\\\"); break;
            case '\n': sb_str(b, "\\n"); break;
            case '\r': sb_str(b, "\\r"); break;
            case '\t': sb_str(b, "\\t"); break;
            default:
                if (c < 0x20) { snprintf(t, sizeof t, "\\u%04x", c); sb_str(b, t); }
                else sb_put(b, (const char*)&c, 1);
        }
    }
    sb_put(b, "\"", 1);
}

/* ── mini scanner JSON ───────────────────────────────────────────────────────────────────── */
static const char* skip_ws(const char* p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}
static const char* skip_string(const char* p)
{
    p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    return *p == '"' ? p + 1 : NULL;
}
static const char* skip_value(const char* p)
{
    p = skip_ws(p);
    if (*p == '"') return skip_string(p);
    if (*p == '{' || *p == '[') {
        char open = *p, close = open == '{' ? '}' : ']';
        p = skip_ws(p + 1);
        if (*p == close) return p + 1;
        for (;;) {
            if (open == '{') {
                p = skip_ws(p);
                if (*p != '"') return NULL;
                p = skip_string(p);
                if (!p) return NULL;
                p = skip_ws(p);
                if (*p != ':') return NULL;
                p++;
            }
            p = skip_value(p);
            if (!p) return NULL;
            p = skip_ws(p);
            if (*p == ',') { p++; continue; }
            return *p == close ? p + 1 : NULL;
        }
    }
    const char* q = p;
    while (*q && !strchr(",}] \t\r\n", *q)) q++;
    return q > p ? q : NULL;
}

/* Decodifica la stringa JSON che inizia a `p` (sul '"'). Heap. */
static char* str_dup(const char* p)
{
    const char* e = skip_string(p);
    if (!e) return NULL;
    sb_t b = {0};
    for (const char* q = p + 1; q < e - 1; q++) {
        if (*q != '\\') { sb_put(&b, q, 1); continue; }
        q++;
        switch (*q) {
            case 'n': sb_str(&b, "\n"); break;
            case 't': sb_str(&b, "\t"); break;
            case 'r': sb_str(&b, "\r"); break;
            case 'b': sb_str(&b, "\b"); break;
            case 'f': sb_str(&b, "\f"); break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 1; i <= 4 && q[i]; i++) {
                    char h = q[i];
                    cp = cp * 16 + (unsigned)(h >= 'a' ? h - 'a' + 10 : h >= 'A' ? h - 'A' + 10 : h - '0');
                }
                q += 4;
                char u[4]; size_t n;
                if (cp < 0x80) { u[0] = (char)cp; n = 1; }
                else if (cp < 0x800) { u[0] = (char)(0xC0 | (cp >> 6)); u[1] = (char)(0x80 | (cp & 63)); n = 2; }
                else { u[0] = (char)(0xE0 | (cp >> 12)); u[1] = (char)(0x80 | ((cp >> 6) & 63)); u[2] = (char)(0x80 | (cp & 63)); n = 3; }
                sb_put(&b, u, n);
                break;
            }
            default: sb_put(&b, q, 1);
        }
    }
    return sb_take(&b);
}

typedef int (*member_cb)(const char* key, size_t klen, const char* val, void* u);

/* Itera i membri di un oggetto (`obj` sul '{') o gli elementi di un array (key=NULL). 0 ok. */
static int each(const char* p, member_cb cb, void* u)
{
    p = skip_ws(p);
    int is_obj = *p == '{';
    if (!is_obj && *p != '[') return -1;
    char close = is_obj ? '}' : ']';
    p = skip_ws(p + 1);
    if (*p == close) return 0;
    for (;;) {
        p = skip_ws(p);
        const char* key = NULL; size_t klen = 0;
        if (is_obj) {
            if (*p != '"') return -1;
            const char* e = skip_string(p);
            if (!e) return -1;
            key = p + 1; klen = (size_t)(e - 1 - key);
            p = skip_ws(e);
            if (*p != ':') return -1;
            p = skip_ws(p + 1);
        }
        const char* vend = skip_value(p);
        if (!vend) return -1;
        if (cb(key, klen, p, u)) return 0;
        p = skip_ws(vend);
        if (*p == ',') { p++; continue; }
        return *p == close ? 0 : -1;
    }
}

typedef struct { const char* key; const char* val; } find_t;
static int find_cb(const char* k, size_t kl, const char* v, void* u)
{
    find_t* f = (find_t*)u;
    if (k && strlen(f->key) == kl && strncmp(k, f->key, kl) == 0) { f->val = v; return 1; }
    return 0;
}
/* Valore del membro `key` di un oggetto, o NULL. */
static const char* obj_get(const char* obj, const char* key)
{
    find_t f = { key, NULL };
    if (!obj || each(obj, find_cb, &f) != 0) return NULL;
    return f.val;
}
static double num_at(const char* v, int* ok)
{
    char* e = NULL;
    double d = strtod(v, &e);
    *ok = e != v;
    return d;
}

/* ── Prosa ───────────────────────────────────────────────────────────────────────────────── */
typedef struct { sb_t* b; int indent; } render_t;
static void render(const char* v, int indent, sb_t* b);

static int is_container(const char* v) { v = skip_ws(v); return *v == '{' || *v == '['; }

static void put_indent(sb_t* b, int n) { for (int i = 0; i < n; i++) sb_str(b, " "); }

static void put_scalar(sb_t* b, const char* v)
{
    v = skip_ws(v);
    if (*v == '"') { char* s = str_dup(v); if (s) { sb_str(b, s); free(s); } return; }
    const char* e = skip_value(v);
    if (e) sb_put(b, v, (size_t)(e - v));
}

static int render_member(const char* k, size_t kl, const char* v, void* u)
{
    render_t* r = (render_t*)u;
    put_indent(r->b, r->indent);
    if (k) {                                    /* oggetto: `chiave: valore` */
        char* key = (char*)malloc(kl + 3);
        if (!key) { r->b->oom = 1; return 1; }
        key[0] = '"'; memcpy(key + 1, k, kl); key[kl + 1] = '"'; key[kl + 2] = 0;
        char* dk = str_dup(key);
        sb_str(r->b, dk ? dk : "");
        free(dk); free(key);
        if (is_container(v)) { sb_str(r->b, ":\n"); render(v, r->indent + 2, r->b); return 0; }
        sb_str(r->b, ": ");
    } else {                                    /* array: `- elemento` */
        if (is_container(v)) { sb_str(r->b, "-\n"); render(v, r->indent + 2, r->b); return 0; }
        sb_str(r->b, "- ");
    }
    put_scalar(r->b, v);
    sb_str(r->b, "\n");
    return 0;
}

static void render(const char* v, int indent, sb_t* b)
{
    render_t r = { b, indent };
    each(v, render_member, &r);
}

char* gmb_clm_to_text(const char* state)
{
    if (!state) return (char*)calloc(1, 1);
    const char* p = skip_ws(state);
    if (*p == '{' || *p == '[') {
        const char* e = skip_value(p);
        if (e && *skip_ws(e) == 0) {
            sb_t b = {0};
            render(p, 0, &b);
            if (b.n && b.p[b.n - 1] == '\n') b.p[--b.n] = 0;     /* niente newline finale */
            return sb_take(&b);
        }
    }
    size_t n = strlen(state);
    char* c = (char*)malloc(n + 1);
    if (c) memcpy(c, state, n + 1);
    return c;
}

char* gmb_clm_state_text(const char* state, const char* instructions)
{
    char* s = gmb_clm_to_text(state);
    if (!s) return NULL;
    const char* i = instructions ? instructions : "";
    if (!*s) { free(s); char* c = strdup(i); return c; }
    if (!*i) return s;
    sb_t b = {0};
    sb_str(&b, s); sb_str(&b, "\n\n"); sb_str(&b, i);
    free(s);
    return sb_take(&b);
}

/* ── Wire ────────────────────────────────────────────────────────────────────────────────── */
static const char* opt_text(const gmb_decision_option_t* o)
{
    return (o->label && *o->label) ? o->label : (o->id ? o->id : "");
}

char* gmb_clm_build_request(const char* state, const char* kind,
                            const gmb_decision_option_t* options, int n_options,
                            const char* instructions)
{
    int k = (kind && strncasecmp(kind, "CHOICE", 6) == 0) ? 1
          : (kind && strncasecmp(kind, "SCORE", 5) == 0) ? 2 : 0;
    sb_t b = {0};
    sb_str(&b, "{\"state\":");
    const char* sp = skip_ws(state ? state : "");
    const char* se = (*sp == '{' || *sp == '[') ? skip_value(sp) : NULL;
    if (se && *skip_ws(se) == 0) sb_put(&b, sp, (size_t)(se - sp));
    else sb_json_str(&b, state ? state : "");
    sb_str(&b, ",\"questions\":{\"q\":{\"type\":\"");
    sb_str(&b, k == 1 ? "choice" : k == 2 ? "score" : "noul");
    sb_str(&b, "\",\"instructions\":");
    sb_json_str(&b, instructions ? instructions : "");
    if (k == 1) {
        sb_str(&b, ",\"criteria\":{");
        for (int i = 0; i < n_options; i++) {
            const char* id = options[i].id ? options[i].id : "";
            const char* lab = options[i].label ? options[i].label : "";
            if (i) sb_str(&b, ",");
            sb_json_str(&b, id);
            sb_str(&b, ":");
            sb_json_str(&b, strcmp(id, lab) == 0 ? "" : lab);   /* descrizione vuota → il server usa la chiave */
        }
        sb_str(&b, "}");
    } else if (k == 2) {
        sb_str(&b, ",\"criteria\":[");
        for (int i = 0; i < n_options; i++) {
            if (i) sb_str(&b, ",");
            sb_json_str(&b, opt_text(&options[i]));
        }
        sb_str(&b, "]");
    }
    sb_str(&b, "}}}");
    return sb_take(&b);
}

static void set_err(char* err, size_t cap, const char* fmt, const char* a)
{
    if (err && cap) snprintf(err, cap, fmt, a ? a : "");
}

typedef struct { const char* want; const char* val; } type_t;
static int has_type_cb(const char* k, size_t kl, const char* v, void* u)
{
    (void)kl;
    if (k && is_container(v) && *skip_ws(v) == '{' && obj_get(v, "type")) { *(const char**)u = v; return 1; }
    return 0;
}

/* Trova l'oggetto-risposta: il corpo stesso (ha "type") o il primo membro-oggetto che lo ha. */
static const char* find_answer(const char* body)
{
    const char* p = skip_ws(body);
    if (*p != '{') return NULL;
    if (obj_get(p, "type")) return p;
    const char* hit = NULL;
    each(p, has_type_cb, &hit);
    return hit;
}

static double prob_of(const char* probs, const char* id)
{
    const char* v = probs ? obj_get(probs, id) : NULL;
    int ok = 0;
    if (!v) return -1.0;
    double d = num_at(v, &ok);
    return ok ? d : -1.0;
}

double gmb_clm_confidence(const float* p, int n)
{
    if (n < 2) return 1.0;
    int top = 0;
    for (int i = 1; i < n; i++) if (p[i] > p[top]) top = i;
    double rest = 0.0;
    for (int i = 0; i < n; i++) if (i != top) rest += p[i];
    double c = (double)p[top] - rest / (double)(n - 1);
    return c < 0.0 ? 0.0 : c > 1.0 ? 1.0 : c;
}

static void fill(gmb_decision_result_t* out, gmb_decision_kind_t kind,
                 const gmb_decision_option_t* o, double prob, double conf)
{
    memset(out, 0, sizeof *out);
    out->kind = kind;
    out->option_id = o->id;
    out->value = (o->label && *o->label) ? o->label : o->id;
    out->probability = prob;
    out->confidence = conf;
    out->has_confidence = 1;
}

int gmb_clm_parse_response(const char* body, gmb_decision_kind_t kind,
                           const gmb_decision_option_t* options, int n_options,
                           gmb_decision_result_t* out, char* err, size_t errcap)
{
    if (!body || !*body) { set_err(err, errcap, "clm: %s", "risposta vuota"); return -1; }
    const char* ans = find_answer(body);
    if (!ans) { set_err(err, errcap, "clm: %s", "risposta senza oggetto 'type'"); return -1; }
    const char* ty = obj_get(ans, "type");
    char* type = ty && *ty == '"' ? str_dup(ty) : NULL;
    if (!type) { set_err(err, errcap, "clm: %s", "campo 'type' non valido"); return -1; }
    const char* want = kind == GMB_DECISION_NOUL ? "noul" : kind == GMB_DECISION_SCORE ? "score" : "choice";
    if (strcasecmp(type, want) != 0) {
        char m[96]; snprintf(m, sizeof m, "type '%s' incoerente (atteso %s)", type, want);
        free(type); set_err(err, errcap, "clm: %s", m); return -1;
    }
    free(type);
    int ok = 0;
    if (kind == GMB_DECISION_NOUL) {
        const char* v = obj_get(ans, "noul");
        double pt = v ? num_at(v, &ok) : 0.0;
        if (!v || !ok || pt < 0.0 || pt > 1.0) { set_err(err, errcap, "clm: %s", "noul assente o fuori [0,1]"); return -1; }
        int it = -1, iff = -1;
        for (int i = 0; i < n_options; i++) {
            if (options[i].id && strcasecmp(options[i].id, "true") == 0) it = i;
            if (options[i].id && strcasecmp(options[i].id, "false") == 0) iff = i;
        }
        if (it < 0 || iff < 0) { set_err(err, errcap, "clm: %s", "dominio NOUL senza true/false"); return -1; }
        float pr[2] = { (float)(1.0 - pt), (float)pt };
        int win = pt >= 0.5 ? it : iff;
        fill(out, kind, &options[win], pt >= 0.5 ? pt : 1.0 - pt, gmb_clm_confidence(pr, 2));
        return 1;
    }
    const char* probs = obj_get(ans, "probabilities");
    if (!probs) { set_err(err, errcap, "clm: %s", "risposta senza 'probabilities'"); return -1; }
    int best = -1; double bp = -1.0;
    if (kind == GMB_DECISION_CHOICE) {
        const char* c = obj_get(ans, "choice");
        char* ck = c && *c == '"' ? str_dup(c) : NULL;
        if (!ck) { set_err(err, errcap, "clm: %s", "risposta senza 'choice'"); return -1; }
        for (int i = 0; i < n_options && best < 0; i++)
            if (options[i].id && strcasecmp(options[i].id, ck) == 0) best = i;
        for (int i = 0; i < n_options && best < 0; i++)
            if (options[i].label && strcasecmp(options[i].label, ck) == 0) best = i;
        if (best < 0) { set_err(err, errcap, "clm: option non nel dominio: %s", ck); free(ck); return -1; }
        free(ck);
        bp = prob_of(probs, options[best].id);
        if (bp < 0.0) bp = prob_of(probs, opt_text(&options[best]));
    } else {
        for (int i = 0; i < n_options; i++) {
            double p = prob_of(probs, options[i].id);
            if (p < 0.0) p = prob_of(probs, opt_text(&options[i]));
            if (p > bp) { bp = p; best = i; }
        }
    }
    if (best < 0 || bp < 0.0 || bp > 1.0) { set_err(err, errcap, "clm: %s", "probabilities incomplete o fuori [0,1]"); return -1; }
    double conf = -1.0;
    const char* cv = obj_get(ans, "confidence");
    if (cv) { conf = num_at(cv, &ok); if (!ok) conf = -1.0; }
    if (conf < 0.0) {
        float pf[GMB_DECISION_MAX_OPTIONS]; int n = n_options > GMB_DECISION_MAX_OPTIONS ? GMB_DECISION_MAX_OPTIONS : n_options;
        for (int i = 0; i < n; i++) { double p = prob_of(probs, options[i].id); pf[i] = (float)(p < 0 ? 0 : p); }
        conf = gmb_clm_confidence(pf, n);
    }
    fill(out, kind, &options[best], bp, conf);
    return 1;
}

/* ── Head ────────────────────────────────────────────────────────────────────────────────── */
typedef struct { int in, out; float* w; float* b; } lin_t;
typedef struct {
    int hidden, width, proj, nh;
    lin_t inp, outl, *hid;
    float *ln_w, *ln_b;              /* nh*width ciascuno, NULL senza LayerNorm */
} sub_t;
struct gmb_clm_head {
    sub_t s, a;
    int act;                         /* 0 gelu, 1 relu, 2 silu */
    int residual;
    float scale;
};

static float half_to_float(uint16_t h)
{
    uint32_t sign = (uint32_t)(h >> 15) & 1, exp = (h >> 10) & 31, man = h & 1023;
    float v;
    if (exp == 0) v = ldexpf((float)man, -24);
    else if (exp == 31) v = man ? NAN : INFINITY;
    else v = ldexpf((float)(man | 1024), (int)exp - 25);
    return sign ? -v : v;
}

static const GstEntry* ent_find(const GstEntry* e, uint32_t n, const char* name)
{
    for (uint32_t i = 0; i < n; i++) if (strcmp(e[i].name, name) == 0) return &e[i];
    return NULL;
}

/* Legge il tensore `name` come float. */
static float* load_t(const char* path, const GstEntry* ents, uint32_t n, const char* name,
                     uint64_t* d0, uint64_t* d1)
{
    const GstEntry* e = ent_find(ents, n, name);
    if (!e) return NULL;
    uint64_t cnt = 1;
    for (uint32_t i = 0; i < e->ndim; i++) cnt *= e->shape[i];
    if (d0) *d0 = e->ndim > 0 ? e->shape[0] : 1;
    if (d1) *d1 = e->ndim > 1 ? e->shape[1] : 1;
    uint8_t* raw = NULL; size_t rn = 0;
    if (gst_read_tensor(path, e, &raw, &rn) != 0 || !raw) return NULL;
    float* f = (float*)malloc((size_t)cnt * sizeof(float));
    if (!f) { free(raw); return NULL; }
    int bytes = strcmp(e->dtype, "F32") == 0 ? 4 : (strcmp(e->dtype, "F16") == 0 || strcmp(e->dtype, "BF16") == 0) ? 2 : 0;
    if (!bytes || rn < (size_t)cnt * (size_t)bytes) { free(raw); free(f); return NULL; }
    for (uint64_t i = 0; i < cnt; i++) {
        if (bytes == 4) memcpy(&f[i], raw + i * 4, 4);
        else {
            uint16_t h; memcpy(&h, raw + i * 2, 2);
            if (e->dtype[0] == 'B') { uint32_t u = (uint32_t)h << 16; memcpy(&f[i], &u, 4); }
            else f[i] = half_to_float(h);
        }
    }
    free(raw);
    return f;
}

static void sub_free(sub_t* s)
{
    free(s->inp.w); free(s->inp.b); free(s->outl.w); free(s->outl.b);
    for (int i = 0; i < s->nh; i++) { free(s->hid[i].w); free(s->hid[i].b); }
    free(s->hid); free(s->ln_w); free(s->ln_b);
    memset(s, 0, sizeof *s);
}

static int sub_load(sub_t* s, const char* path, const GstEntry* ents, uint32_t n, const char* pre,
                    char* err, size_t cap)
{
    char nm[160];
    uint64_t d0 = 0, d1 = 0;
#define NAME(fmt, ...) (snprintf(nm, sizeof nm, "%s" fmt, pre, ##__VA_ARGS__), nm)
    s->inp.w = load_t(path, ents, n, NAME("inp.weight"), &d0, &d1);
    s->inp.b = load_t(path, ents, n, NAME("inp.bias"), NULL, NULL);
    if (!s->inp.w || !s->inp.b) { set_err(err, cap, "clm: tensori %s: inp.* mancanti", pre); return -1; }
    s->width = (int)d0; s->hidden = (int)d1; s->inp.out = s->width; s->inp.in = s->hidden;
    s->outl.w = load_t(path, ents, n, NAME("out.weight"), &d0, &d1);
    s->outl.b = load_t(path, ents, n, NAME("out.bias"), NULL, NULL);
    if (!s->outl.w || !s->outl.b || (int)d1 != s->width) { set_err(err, cap, "clm: tensori %s: out.* mancanti o incoerenti", pre); return -1; }
    s->proj = (int)d0; s->outl.out = s->proj; s->outl.in = s->width;
    int nh = 0;
    while (ent_find(ents, n, NAME("hidden.%d.weight", nh))) nh++;
    s->nh = nh;
    if (nh) {
        s->hid = (lin_t*)calloc((size_t)nh, sizeof(lin_t));
        if (!s->hid) return -1;
        int ln = ent_find(ents, n, NAME("norms.0.weight")) != NULL;
        if (ln) {
            s->ln_w = (float*)malloc((size_t)nh * (size_t)s->width * sizeof(float));
            s->ln_b = (float*)malloc((size_t)nh * (size_t)s->width * sizeof(float));
            if (!s->ln_w || !s->ln_b) return -1;
        }
        for (int i = 0; i < nh; i++) {
            s->hid[i].w = load_t(path, ents, n, NAME("hidden.%d.weight", i), &d0, &d1);
            s->hid[i].b = load_t(path, ents, n, NAME("hidden.%d.bias", i), NULL, NULL);
            if (!s->hid[i].w || !s->hid[i].b || (int)d0 != s->width || (int)d1 != s->width) {
                set_err(err, cap, "clm: tensori %s: hidden.* incoerenti", pre); return -1;
            }
            s->hid[i].in = s->hid[i].out = s->width;
            if (ln) {
                float* w = load_t(path, ents, n, NAME("norms.%d.weight", i), NULL, NULL);
                float* b = load_t(path, ents, n, NAME("norms.%d.bias", i), NULL, NULL);
                if (!w || !b) { free(w); free(b); set_err(err, cap, "clm: tensori %s: norms.* mancanti", pre); return -1; }
                memcpy(s->ln_w + (size_t)i * (size_t)s->width, w, (size_t)s->width * sizeof(float));
                memcpy(s->ln_b + (size_t)i * (size_t)s->width, b, (size_t)s->width * sizeof(float));
                free(w); free(b);
            }
        }
    }
#undef NAME
    return 0;
}

void gmb_clm_head_free(gmb_clm_head_t* h)
{
    if (!h) return;
    sub_free(&h->s); sub_free(&h->a);
    free(h);
}
int gmb_clm_head_hidden_size(const gmb_clm_head_t* h) { return h ? h->s.hidden : 0; }
int gmb_clm_head_projection_dim(const gmb_clm_head_t* h) { return h ? h->s.proj : 0; }

gmb_clm_head_t* gmb_clm_head_load(const char* path, char* err, size_t cap)
{
    if (!path || !*path) { set_err(err, cap, "clm: %s", "percorso head mancante"); return NULL; }
    GstEntry* ents = NULL; uint32_t n = 0; uint64_t hl = 0;
    if (gst_read_header(path, &ents, &n, &hl) != 0) { set_err(err, cap, "clm: head illeggibile: %s", path); return NULL; }
    gmb_clm_head_t* h = (gmb_clm_head_t*)calloc(1, sizeof *h);
    if (!h) { free(ents); return NULL; }
    if (sub_load(&h->s, path, ents, n, "state_head.", err, cap) != 0
        || sub_load(&h->a, path, ents, n, "action_head.", err, cap) != 0) {
        free(ents); gmb_clm_head_free(h); return NULL;
    }
    if (h->s.hidden != h->a.hidden || h->s.proj != h->a.proj) {
        set_err(err, cap, "clm: %s", "state_head e action_head hanno dimensioni diverse");
        free(ents); gmb_clm_head_free(h); return NULL;
    }
    float* ls = load_t(path, ents, n, "logit_scale", NULL, NULL);
    free(ents);
    if (!ls) { set_err(err, cap, "clm: %s", "tensore logit_scale mancante"); gmb_clm_head_free(h); return NULL; }
    double sc = exp((double)ls[0]);
    h->scale = (float)(sc > 100.0 ? 100.0 : sc);
    free(ls);
    /* Sidecar `.json` (activation/residual): default gelu, no residual. */
    size_t L = strlen(path);
    char side[2048];
    if (L > 12 && strcmp(path + L - 12, ".safetensors") == 0) snprintf(side, sizeof side, "%.*s.json", (int)(L - 12), path);
    else snprintf(side, sizeof side, "%s.json", path);
    FILE* f = fopen(side, "rb");
    if (f) {
        char buf[1024]; size_t r = fread(buf, 1, sizeof buf - 1, f); buf[r] = 0; fclose(f);
        const char* a = obj_get(buf, "activation");
        char* as = a && *a == '"' ? str_dup(a) : NULL;
        if (as) { h->act = strcasecmp(as, "relu") == 0 ? 1 : strcasecmp(as, "silu") == 0 ? 2 : 0; free(as); }
        const char* rv = obj_get(buf, "residual");
        if (rv && strncmp(rv, "true", 4) == 0) h->residual = 1;
    }
    return h;
}

static float activate(int act, float x)
{
    if (act == 1) return x > 0.0f ? x : 0.0f;
    if (act == 2) return x / (1.0f + expf(-x));
    return 0.5f * x * (1.0f + erff(x * 0.70710678118654752440f));
}

static void linear(const lin_t* l, const float* x, float* y)
{
    for (int o = 0; o < l->out; o++) {
        double acc = l->b[o];
        const float* w = l->w + (size_t)o * (size_t)l->in;
        for (int i = 0; i < l->in; i++) acc += (double)w[i] * (double)x[i];
        y[o] = (float)acc;
    }
}

/* Proiezione L2-normalizzata di un embedding grezzo (hidden) in `out` (proj). 0 ok. */
static int project(const gmb_clm_head_t* h, const sub_t* s, const float* x, float* out)
{
    float* a = (float*)malloc((size_t)s->width * sizeof(float));
    float* t = (float*)malloc((size_t)s->width * sizeof(float));
    if (!a || !t) { free(a); free(t); return -1; }
    linear(&s->inp, x, a);
    for (int i = 0; i < s->width; i++) a[i] = activate(h->act, a[i]);
    for (int k = 0; k < s->nh; k++) {
        linear(&s->hid[k], a, t);
        if (s->ln_w) {
            double mean = 0, var = 0;
            for (int i = 0; i < s->width; i++) mean += t[i];
            mean /= s->width;
            for (int i = 0; i < s->width; i++) var += (t[i] - mean) * (t[i] - mean);
            var /= s->width;
            double inv = 1.0 / sqrt(var + 1e-5);
            for (int i = 0; i < s->width; i++)
                t[i] = (float)((t[i] - mean) * inv * s->ln_w[(size_t)k * (size_t)s->width + (size_t)i]
                               + s->ln_b[(size_t)k * (size_t)s->width + (size_t)i]);
        }
        for (int i = 0; i < s->width; i++) {
            float hv = activate(h->act, t[i]);
            a[i] = h->residual ? a[i] + hv : hv;
        }
    }
    linear(&s->outl, a, out);
    free(a); free(t);
    double nn = 0;
    for (int i = 0; i < s->proj; i++) nn += (double)out[i] * (double)out[i];
    nn = sqrt(nn);
    if (nn < 1e-12) nn = 1e-12;
    for (int i = 0; i < s->proj; i++) out[i] = (float)(out[i] / nn);
    return 0;
}

int gmb_clm_score(const gmb_clm_head_t* h, const float* state_emb, const float* cands, int n, float* probs)
{
    if (!h || !state_emb || !cands || !probs || n < 1) return -1;
    int P = h->s.proj, H = h->s.hidden;
    float* zs = (float*)malloc((size_t)P * sizeof(float));
    float* zc = (float*)malloc((size_t)P * sizeof(float));
    float* lg = (float*)malloc((size_t)n * sizeof(float));
    if (!zs || !zc || !lg) { free(zs); free(zc); free(lg); return -1; }
    int rc = project(h, &h->s, state_emb, zs);
    for (int i = 0; i < n && rc == 0; i++) {
        rc = project(h, &h->a, cands + (size_t)i * (size_t)H, zc);
        double dot = 0;
        for (int j = 0; j < P; j++) dot += (double)zs[j] * (double)zc[j];
        lg[i] = (float)((double)h->scale * dot);
    }
    if (rc == 0) {
        float mx = lg[0];
        for (int i = 1; i < n; i++) if (lg[i] > mx) mx = lg[i];
        double sum = 0;
        for (int i = 0; i < n; i++) { probs[i] = expf(lg[i] - mx); sum += probs[i]; }
        for (int i = 0; i < n; i++) probs[i] = (float)(probs[i] / sum);
    }
    free(zs); free(zc); free(lg);
    return rc;
}

int gmb_clm_decide_local(const gmb_clm_head_t* head, gmb_clm_embed_fn embed, void* ctx,
                         const char* state, const char* instructions, gmb_decision_kind_t kind,
                         const gmb_decision_option_t* options, int n, gmb_decision_result_t* out,
                         char* err, size_t errcap)
{
    if (!head || !embed || !options || n < 1 || n > GMB_DECISION_MAX_OPTIONS) {
        set_err(err, errcap, "clm: %s", "head, encoder o dominio mancanti");
        return -1;
    }
    int H = head->s.hidden;
    float* emb = (float*)malloc((size_t)(n + 1) * (size_t)H * sizeof(float));
    char* st = gmb_clm_state_text(state, instructions);
    if (!emb || !st) { free(emb); free(st); return -1; }
    int rc = embed(st, emb, H, ctx);
    free(st);
    for (int i = 0; i < n && rc == 0; i++) {
        sb_t b = {0};
        if (kind == GMB_DECISION_NOUL) {
            sb_str(&b, opt_text(&options[i])); sb_str(&b, ": ");
            sb_str(&b, instructions ? instructions : "");
        } else sb_str(&b, opt_text(&options[i]));
        char* ct = sb_take(&b);
        rc = ct ? embed(ct, emb + (size_t)(i + 1) * (size_t)H, H, ctx) : -1;
        free(ct);
    }
    if (rc != 0) { free(emb); set_err(err, errcap, "clm: %s", "encoder: embedding fallito"); return -1; }
    float pr[GMB_DECISION_MAX_OPTIONS];
    rc = gmb_clm_score(head, emb, emb + H, n, pr);
    free(emb);
    if (rc != 0) { set_err(err, errcap, "clm: %s", "punteggio fallito"); return -1; }
    int top = 0;
    for (int i = 1; i < n; i++) if (pr[i] > pr[top]) top = i;
    fill(out, kind, &options[top], pr[top], gmb_clm_confidence(pr, n));
    return 1;
}
