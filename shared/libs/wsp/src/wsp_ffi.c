/*
 * wsp_ffi.c — shim ABI piatto per il namespace BASIC WSP (Weight-Space Programming).
 *
 * L'FFI di GMBasic passa solo scalari (LONG = int64, DOUBLE), stringhe (const char*) e array; qui un modello aperto è un
 * HANDLE (massimo 8 vivi per processo, uso sequenziale, non thread-safe) che racchiude: Weight Store, WSP Document (se la
 * famiglia è supportata), overlay con journal e, opzionalmente, una sessione agente. Le funzioni che restituiscono STRING
 * puntano a un buffer dell'handle valido fino alla chiamata successiva. Gli errori: valore di ritorno -1 (o 0 per i
 * predicati) e messaggio in wsp_lasterror(h) (h = 0: errori globali, es. di wsp_loadmodel).
 * Le implementazioni sono le librerie C99 llmwsp_* (sorgenti venduti da GMBasic/gmbllm, vedi build.sh).
 */
#include "llmwsp_agent.h"
#include "llmwsp_analyze.h"
#include "llmwsp_derive.h"
#include "llmwsp_fixture.h"
#include "llmwsp_overlay.h"
#include "llmwsp_verify.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long long i64;
#define MAXH 8

typedef struct {
    llmwsp_store *s; llmwsp_doc *d; llmwsp_overlay *ov; llmwsp_session *ag;
    char dir[1100], err[600], rep[600], str[4200]; char *big; int ag_rc; int ag_open_tr;
} H;

static H *g_h[MAXH];
static char g_err[600];

static H *H_of(i64 h) { return (h >= 1 && h <= MAXH) ? g_h[h - 1] : NULL; }
static void seterr(H *x, const char *m) { snprintf(x ? x->err : g_err, 600, "%s", m); }

const char *wsp_lasterror(i64 h) { H *x = H_of(h); return x ? x->err : g_err; }

i64 wsp_loadmodel(const char *dir) {
    int slot;
    g_err[0] = 0;
    if (!dir || !dir[0]) { seterr(NULL, "percorso vuoto"); return -1; }
    for (slot = 0; slot < MAXH; slot++) if (!g_h[slot]) break;
    if (slot == MAXH) { seterr(NULL, "troppi modelli aperti (massimo 8): manca FREEMODEL"); return -1; }
    H *x = (H *)calloc(1, sizeof *x);
    if (!x) { seterr(NULL, "memoria"); return -1; }
    char e[400];
    if (llmwsp_store_open(dir, &x->s, e, sizeof e) < 0) { snprintf(g_err, sizeof g_err, "%s", e); free(x); return -1; }
    snprintf(x->dir, sizeof x->dir, "%s", dir);
    char cp[1200]; snprintf(cp, sizeof cp, "%s/config.json", dir);
    if (llmwsp_doc_build(x->s, cp, &x->d, e, sizeof e) < 0) { x->d = NULL; snprintf(x->err, sizeof x->err, "documento non costruito (solo indirizzi @asse:da:a:tensore): %s", e); }
    x->ov = llmwsp_ov_new(x->s, x->d, dir);
    g_h[slot] = x;
    return slot + 1;
}

i64 wsp_freemodel(i64 h) {
    H *x = H_of(h);
    if (!x) return -1;
    llmwsp_session_free(x->ag); llmwsp_ov_free(x->ov); llmwsp_doc_free(x->d); llmwsp_store_close(x->s);
    free(x->big); free(x); g_h[h - 1] = NULL;
    return 0;
}

i64 wsp_hasdoc(i64 h) { H *x = H_of(h); return x && x->d ? 1 : 0; }

double wsp_dim(i64 h, const char *key) {
    H *x = H_of(h);
    if (!x || !key) return -1;
    if (!strcmp(key, "tensors")) return (double)llmwsp_store_count(x->s);
    if (!strcmp(key, "bytes")) return (double)llmwsp_store_total_bytes(x->s);
    if (!strcmp(key, "shards")) return (double)llmwsp_store_file_count(x->s);
    if (!x->d) { seterr(x, "documento non disponibile per questa famiglia"); return -1; }
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    if (!strcmp(key, "layers")) return (double)g.layers;
    if (!strcmp(key, "hidden")) return (double)g.hidden;
    if (!strcmp(key, "heads")) return (double)g.heads;
    if (!strcmp(key, "kv_heads")) return (double)g.kv_heads;
    if (!strcmp(key, "head_dim")) return (double)g.head_dim;
    if (!strcmp(key, "inter")) return (double)g.inter;
    if (!strcmp(key, "vocab")) return (double)g.vocab;
    if (!strcmp(key, "leaves")) return (double)llmwsp_doc_leaf_count(x->d);
    seterr(x, "chiave sconosciuta"); return -1;
}

const char *wsp_modeltype(i64 h) { H *x = H_of(h); if (!x || !x->d) return ""; snprintf(x->str, sizeof x->str, "%s", llmwsp_doc_model_type(x->d)); return x->str; }
i64 wsp_tensorcount(i64 h) { H *x = H_of(h); return x ? (i64)llmwsp_store_count(x->s) : -1; }
const char *wsp_tensorname(i64 h, i64 i) { H *x = H_of(h); const llmwsp_tensor *t = x ? llmwsp_store_at(x->s, (size_t)i) : NULL; if (!t) return ""; snprintf(x->str, sizeof x->str, "%s", t->name); return x->str; }
const char *wsp_tensorinfo(i64 h, const char *name) {
    H *x = H_of(h); const llmwsp_tensor *t = x ? llmwsp_store_find(x->s, name) : NULL;
    if (!t) { if (x) seterr(x, "tensore inesistente"); return ""; }
    size_t n = (size_t)snprintf(x->str, sizeof x->str, "%s [", t->dtype);
    for (int k = 0; k < t->ndim; k++) n += (size_t)snprintf(x->str + n, sizeof x->str - n, "%s%lld", k ? "," : "", (long long)t->shape[k]);
    snprintf(x->str + n, sizeof x->str - n, "] %llu byte", (unsigned long long)t->nbytes);
    return x->str;
}
i64 wsp_leafcount(i64 h) { H *x = H_of(h); return x && x->d ? (i64)llmwsp_doc_leaf_count(x->d) : -1; }
const char *wsp_leafaddr(i64 h, i64 i) { H *x = H_of(h); const llmwsp_leaf *l = x && x->d ? llmwsp_doc_leaf(x->d, (size_t)i) : NULL; if (!l) return ""; snprintf(x->str, sizeof x->str, "%s", l->addr); return x->str; }

i64 wsp_checkdoc(i64 h) {
    H *x = H_of(h); char e[300];
    if (!x || !x->d) { if (x) seterr(x, "documento non disponibile"); return -1; }
    int v = llmwsp_doc_check(x->d, e, sizeof e); x->err[0] = 0;
    if (v) snprintf(x->err, sizeof x->err, "%s", e);
    return v;
}
i64 wsp_docjson(i64 h, const char *path) {
    H *x = H_of(h);
    if (!x || !x->d) return -1;
    char *js = llmwsp_doc_to_json(x->d); FILE *f = js ? fopen(path, "wb") : NULL;
    if (!f) { free(js); seterr(x, "file non scrivibile"); return -1; }
    fputs(js, f); free(js); return fclose(f) == 0 ? 0 : -1;
}
const char *wsp_sha(i64 h, const char *tensor) { H *x = H_of(h); char hx[65]; if (!x || llmwsp_ov_tensor_sha256(x->ov, tensor, hx) < 0) { if (x) seterr(x, "tensore inesistente"); return ""; } snprintf(x->str, sizeof x->str, "%s", hx); return x->str; }

i64 wsp_readslice(i64 h, const char *addr, double *out, i64 cap) {
    H *x = H_of(h);
    if (!x) return -1;
    const llmwsp_tensor *t; int ax; int64_t a, b;
    if (addr[0] == '@') {
        int axx; long long p, q; int off = 0;
        if (sscanf(addr, "@%d:%lld:%lld:%n", &axx, &p, &q, &off) < 3 || !off || !(t = llmwsp_store_find(x->s, addr + off))) { seterr(x, "indirizzo non valido"); return -1; }
        ax = axx; a = p; b = q;
    } else if (!x->d || llmwsp_doc_resolve(x->d, addr, &t, &ax, &a, &b) < 0) { seterr(x, "indirizzo non risolvibile"); return -1; }
    size_t n = 0;       /* contenuto CORRENTE: overlay se modificato, altrimenti base */
    if (llmwsp_ov_read_f64(x->ov, addr, out, (size_t)cap, &n) < 0) { seterr(x, "lettura fallita (indirizzo, buffer o dtype non supportato)"); return -1; }
    (void)t; (void)ax; (void)a; (void)b;
    return (i64)n;
}

/* ---- analisi ---- */
#define NEEDDOC(x) if (!(x) || !(x)->d) { if (x) seterr(x, "documento non disponibile"); return -1; }
i64 wsp_norms(i64 h, i64 layer, const char *kind, double *out, i64 cap) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    int64_t n = (kind[0] == 'k' || kind[0] == 'v') ? g.kv_heads : g.heads;
    double *q = (double *)calloc((size_t)g.heads, 8), *k = (double *)calloc((size_t)g.kv_heads, 8), *v = (double *)calloc((size_t)g.kv_heads, 8), *o = (double *)calloc((size_t)g.heads, 8);
    i64 rc = -1;
    if (n <= cap && q && k && v && o && llmwsp_an_head_norms(x->s, x->d, (int)layer, q, k, v, o) == 0) {
        const double *src = kind[0] == 'q' ? q : kind[0] == 'k' ? k : kind[0] == 'v' ? v : o;
        memcpy(out, src, (size_t)n * 8); rc = n;
    } else seterr(x, "norme non calcolabili (layer/buffer)");
    free(q); free(k); free(v); free(o); return rc;
}
i64 wsp_copyscore(i64 h, i64 layer, i64 head, const char *circuit, double *out) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    double *wr = (double *)malloc((size_t)g.head_dim * 8), *wi = (double *)malloc((size_t)g.head_dim * 8); i64 rc = -1;
    if (wr && wi && llmwsp_an_circuit_spectrum(x->s, x->d, (int)layer, (int)head, (circuit[0] == 'Q' || circuit[0] == 'q') ? LLMWSP_CIRCUIT_QK : LLMWSP_CIRCUIT_OV, wr, wi) == 0) {
        llmwsp_an_copy_scores(wr, wi, (size_t)g.head_dim, &out[0], &out[1]); rc = 0;
    } else seterr(x, "spettro non calcolabile");
    free(wr); free(wi); return rc;
}
i64 wsp_spectrum(i64 h, i64 layer, i64 head, const char *circuit, double *wr, double *wi, i64 cap) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    if (cap < g.head_dim) { seterr(x, "buffer troppo piccolo"); return -1; }
    if (llmwsp_an_circuit_spectrum(x->s, x->d, (int)layer, (int)head, (circuit[0] == 'Q' || circuit[0] == 'q') ? LLMWSP_CIRCUIT_QK : LLMWSP_CIRCUIT_OV, wr, wi) < 0) { seterr(x, "spettro non calcolabile"); return -1; }
    return g.head_dim;
}
double wsp_compose(i64 h, i64 l1, i64 h1, i64 l2, i64 h2, const char *which) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    double *c = (double *)malloc((size_t)g.head_dim * 8), pk = -1;
    if (!c || llmwsp_an_compose_cosines(x->s, x->d, (int)l1, (int)h1, (int)l2, (int)h2, which[0], c, &pk) < 0) { seterr(x, "composizione non calcolabile"); pk = -1; }
    free(c); return pk;
}
i64 wsp_mlpimportance(i64 h, i64 layer, double *out, i64 cap) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    if (cap < g.inter || llmwsp_mlp_importance(x->s, x->d, (int)layer, out) < 0) { seterr(x, "importanza non calcolabile"); return -1; }
    return g.inter;
}
i64 wsp_kvimportance(i64 h, i64 layer, double *out, i64 cap) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    if (cap < g.kv_heads || llmwsp_kvgroup_importance(x->s, x->d, (int)layer, out) < 0) { seterr(x, "importanza non calcolabile"); return -1; }
    return g.kv_heads;
}
i64 wsp_topk(const double *score, i64 n, i64 k, i64 *out) {
    if (n <= 0 || k <= 0 || k > n) return -1;
    int *tmp = (int *)malloc((size_t)k * sizeof(int));
    if (!tmp) return -1;
    llmwsp_topk_sorted(score, (size_t)n, (size_t)k, tmp);
    for (i64 i = 0; i < k; i++) out[i] = tmp[i];
    free(tmp); return k;
}

/* ---- edit ---- */
static i64 edit_done(H *x, int rc, const llmwsp_ov_report *r, const char *e) {
    if (rc == 0) { snprintf(x->rep, sizeof x->rep, "toccati=%lld/%lld rel_norm=%.6g max_abs=%.6g", (long long)r->touched, (long long)r->total, r->rel_norm, r->max_abs); x->err[0] = 0; return 0; }
    snprintf(x->err, sizeof x->err, "%s", e); x->rep[0] = 0; return -1;
}
const char *wsp_lastreport(i64 h) { H *x = H_of(h); return x ? x->rep : ""; }
i64 wsp_scaleslice(i64 h, const char *addr, double alpha) { H *x = H_of(h); if (!x) return -1; llmwsp_ov_report r; char e[300]; int rc = llmwsp_ov_scale(x->ov, addr, alpha, &r, e, sizeof e); return edit_done(x, rc, &r, e); }
i64 wsp_zeroslice(i64 h, const char *addr) { H *x = H_of(h); if (!x) return -1; llmwsp_ov_report r; char e[300]; int rc = llmwsp_ov_zero(x->ov, addr, &r, e, sizeof e); return edit_done(x, rc, &r, e); }
i64 wsp_setvalues(i64 h, const char *addr, const double *v, i64 n) { H *x = H_of(h); if (!x) return -1; llmwsp_ov_report r; char e[300]; int rc = llmwsp_ov_set(x->ov, addr, v, (size_t)n, &r, e, sizeof e); return edit_done(x, rc, &r, e); }
i64 wsp_adddelta(i64 h, const char *addr, const double *v, i64 n) { H *x = H_of(h); if (!x) return -1; llmwsp_ov_report r; char e[300]; int rc = llmwsp_ov_adddelta(x->ov, addr, v, (size_t)n, &r, e, sizeof e); return edit_done(x, rc, &r, e); }
i64 wsp_copyslice(i64 h, const char *dst, const char *src) { H *x = H_of(h); if (!x) return -1; llmwsp_ov_report r; char e[300]; int rc = llmwsp_ov_copy(x->ov, dst, src, &r, e, sizeof e); return edit_done(x, rc, &r, e); }
i64 wsp_projectout(i64 h, const char *addr, const double *dirs, i64 ndirs, i64 len, i64 vec_axis, i64 flags) {
    H *x = H_of(h); if (!x) return -1; llmwsp_ov_report r; char e[300];
    int rc = llmwsp_ov_projectout(x->ov, addr, dirs, (size_t)ndirs, (size_t)len, (int)vec_axis, (int)flags, &r, e, sizeof e);
    return edit_done(x, rc, &r, e);
}

/* ---- journal / scrittura ---- */
i64 wsp_undoedit(i64 h) { H *x = H_of(h); return x && llmwsp_ov_undo(x->ov) == 0 ? 0 : -1; }
i64 wsp_redoedit(i64 h) { H *x = H_of(h); return x && llmwsp_ov_redo(x->ov) == 0 ? 0 : -1; }
i64 wsp_checkpoint(i64 h, const char *name) { H *x = H_of(h); return x && llmwsp_ov_checkpoint(x->ov, name) == 0 ? 0 : -1; }
i64 wsp_rollback(i64 h, const char *name) { H *x = H_of(h); if (!x) return -1; if (llmwsp_ov_rollback(x->ov, name) < 0) { seterr(x, "rollback impossibile"); return -1; } return 0; }
i64 wsp_journalpos(i64 h) { H *x = H_of(h); return x ? (i64)llmwsp_ov_journal_pos(x->ov) : -1; }
i64 wsp_journallen(i64 h) { H *x = H_of(h); return x ? (i64)llmwsp_ov_journal_len(x->ov) : -1; }
const char *wsp_summary(i64 h) { H *x = H_of(h); if (!x) return ""; free(x->big); x->big = llmwsp_ov_summary(x->ov); return x->big ? x->big : ""; }
i64 wsp_diffcount(i64 h) { H *x = H_of(h); size_t n = 0; return x && llmwsp_ov_diff(x->ov, NULL, 0, &n) == 0 ? (i64)n : -1; }
i64 wsp_materialize(i64 h, const char *outdir) { H *x = H_of(h); if (!x) return -1; llmwsp_mat_stats st; char e[300]; if (llmwsp_ov_materialize(x->ov, outdir, &st, e, sizeof e) < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; } return st.shards_rewritten; }
i64 wsp_saverecipe(i64 h, const char *dir) { H *x = H_of(h); char e[300]; if (!x) return -1; if (llmwsp_ov_recipe(x->ov, dir, e, sizeof e) < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; } return 0; }
i64 wsp_replayrecipe(i64 h, const char *dir) { H *x = H_of(h); char e[300]; if (!x) return -1; if (llmwsp_ov_replay(x->ov, dir, e, sizeof e) < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; } return 0; }
i64 wsp_exportpatch(i64 h, const char *path) { H *x = H_of(h); size_t b = 0; char e[300]; if (!x) return -1; if (llmwsp_ov_export_patch(x->ov, path, &b, e, sizeof e) < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; } return (i64)b; }
i64 wsp_applypatch(i64 h, const char *path) { H *x = H_of(h); char e[300]; if (!x) return -1; if (llmwsp_ov_apply_patch(x->ov, path, e, sizeof e) < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; } return 0; }

/* ---- chirurgia di architettura: gli array LONG (int64) sono convertiti; nl/kvm/neuk = 0 disattivano la voce ---- */
i64 wsp_derive(i64 h, const char *outdir, const i64 *layer_map, i64 nl, const i64 *kv_keep, i64 kvm, const i64 *neuron_keep, i64 neuk) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_dims g; llmwsp_doc_dims(x->d, &g);
    size_t nout = nl > 0 ? (size_t)nl : (size_t)g.layers;
    int *lm = nl > 0 ? (int *)malloc((size_t)nl * sizeof(int)) : NULL, *kk = kvm > 0 ? (int *)malloc(nout * (size_t)kvm * sizeof(int)) : NULL, *nk = neuk > 0 ? (int *)malloc(nout * (size_t)neuk * sizeof(int)) : NULL;
    for (i64 i = 0; lm && i < nl; i++) lm[i] = (int)layer_map[i];
    for (size_t i = 0; kk && i < nout * (size_t)kvm; i++) kk[i] = (int)kv_keep[i];
    for (size_t i = 0; nk && i < nout * (size_t)neuk; i++) nk[i] = (int)neuron_keep[i];
    llmwsp_derive_ops ops; memset(&ops, 0, sizeof ops);
    ops.layer_map = lm; ops.nl = (size_t)(nl > 0 ? nl : 0); ops.kv_keep = kk; ops.kv_m = (size_t)(kvm > 0 ? kvm : 0); ops.neuron_keep = nk; ops.neu_k = (size_t)(neuk > 0 ? neuk : 0);
    char e[400]; llmwsp_derive_report rep;
    int rc = llmwsp_derive(x->s, x->d, x->dir, &ops, outdir, &rep, e, sizeof e);
    free(lm); free(kk); free(nk);
    if (rc < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; }
    snprintf(x->rep, sizeof x->rep, "layer=%zu tensori=%zu byte %llu -> %llu", rep.out_layers, rep.tensors, (unsigned long long)rep.bytes_before, (unsigned long long)rep.bytes_after);
    return 0;
}

/* ---- verifica, hash, gate, provenienza ---- */
i64 wsp_verifymodel(const char *dir, i64 flags) {
    llmwsp_verify_opts o; memset(&o, 0, sizeof o); o.check_doc = (flags & 1) != 0; o.check_finite = (flags & 2) != 0;
    llmwsp_verify_report r;
    g_err[0] = 0;
    if (llmwsp_verify_dir(dir, &o, &r) < 0) { snprintf(g_err, sizeof g_err, "%s", r.first_error); return -1; }
    snprintf(g_err, sizeof g_err, "%s", r.first_error);
    return r.ok ? 0 : (i64)(r.doc_violations + r.coverage_violations + (int)r.nonfinite_tensors + (int)r.shape_mismatch + (int)r.unexpected_changed);
}
const char *wsp_modelhash(i64 h) { H *x = H_of(h); char hx[65]; if (!x || llmwsp_model_hash(x->s, hx) < 0) { if (x) seterr(x, "hash fallito"); return ""; } snprintf(x->str, sizeof x->str, "%s", hx); return x->str; }
i64 wsp_gate(double value, double threshold, i64 le) { llmwsp_gate g = {"gate", value, threshold, le != 0}; return llmwsp_gates_eval(&g, 1, NULL) == 0 ? 1 : 0; }
i64 wsp_provenance(i64 h, const char *outdir, const char *recipe_text, const char *path) {
    H *x = H_of(h); llmwsp_store *so; char e[400]; if (!x) return -1;
    if (llmwsp_store_open(outdir, &so, e, sizeof e) < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; }
    int rc = llmwsp_provenance_write(path, x->s, recipe_text, so, NULL, e, sizeof e);
    llmwsp_store_close(so);
    if (rc < 0) { snprintf(x->err, sizeof x->err, "%s", e); return -1; }
    return 0;
}

/* ---- sessione agente (verbi, budget, trascrizione, replay) ---- */
i64 wsp_agentnew(i64 h, i64 max_edits, i64 max_forwards) {
    H *x = H_of(h); NEEDDOC(x);
    llmwsp_session_free(x->ag);
    llmwsp_budget b = {(int)max_edits, (int)max_forwards};
    x->ag = llmwsp_session_new(x->s, x->d, x->dir, b, NULL, NULL);
    return x->ag ? 0 : -1;
}
const char *wsp_agentexec(i64 h, const char *cmd) {
    H *x = H_of(h); if (!x || !x->ag) { if (x) seterr(x, "sessione non creata (AGENTNEW)"); return ""; }
    free(x->big); x->big = (char *)malloc(1 << 16);
    if (!x->big) return "";
    x->ag_rc = llmwsp_session_exec(x->ag, cmd, x->big, 1 << 16);
    return x->big;
}
i64 wsp_agentrc(i64 h) { H *x = H_of(h); return x ? x->ag_rc : -1; }
i64 wsp_agenttranscript(i64 h, const char *path) { H *x = H_of(h); return x && x->ag && llmwsp_session_transcript_open(x->ag, path) == 0 ? 0 : -1; }
i64 wsp_agentstop(i64 h) { H *x = H_of(h); return x && x->ag && llmwsp_session_transcript_close(x->ag) == 0 ? 0 : -1; }
i64 wsp_agentreplay(i64 h, const char *path) {
    H *x = H_of(h); int mm = 0; char e[300]; if (!x || !x->ag) return -1;
    int rc = llmwsp_replay(x->ag, path, &mm, e, sizeof e);
    snprintf(x->err, sizeof x->err, "%s", rc ? e : "");
    return rc == 0 ? 0 : (rc == -1 ? (i64)mm : -1);
}
i64 wsp_agentrun(i64 h, const char *path) { H *x = H_of(h); return x && x->ag && llmwsp_session_write_run(x->ag, path) == 0 ? 0 : -1; }
i64 wsp_agentedits(i64 h) { H *x = H_of(h); return x && x->ag ? llmwsp_session_edits_used(x->ag) : -1; }

/* ---- fixture a verità nota (per test e sample autosufficienti) ---- */
i64 wsp_fixture(const char *kind, const char *outdir, i64 seed) {
    llmwsp_fx_truth t; char e[300];
    g_err[0] = 0;
    if (llmwsp_fixture_compile(kind, outdir, (unsigned)seed, &t, e, sizeof e) < 0) { snprintf(g_err, sizeof g_err, "%s", e); return -1; }
    return 0;
}

/* ---- pulizia confinata: rimuove SOLO file prodotti da WSP (whitelist per suffisso) e la directory se resta vuota ---- */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
static int ends(const char *s, const char *x) { size_t a = strlen(s), b = strlen(x); return a >= b && !strcmp(s + a - b, x); }
static int wsp_owned(const char *n) {
    static const char *sfx[] = {".safetensors", ".json", ".txt", ".f64", ".jsonl", ".wsppatch", ".gmbderive", ".gmbarch", ".jinja", NULL};
    for (int i = 0; sfx[i]; i++) if (ends(n, sfx[i])) return 1;
    return 0;
}
i64 wsp_removeoutput(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;                                   /* non esiste: niente da fare */
    struct dirent *e; i64 n = 0; char p[1300];
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(p, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) { if (!strcmp(e->d_name, "arch")) { n += wsp_removeoutput(p); } continue; }
        if (wsp_owned(e->d_name) && unlink(p) == 0) n++;
    }
    closedir(d);
    rmdir(dir);                                         /* fallisce (e va bene) se contiene altro */
    return n;
}

#include <stdint.h>
/* Shim FFI (estensioni automatiche degli array, ffi_ext_shim.pl): firma con int64_t dopo ogni array. */
i64 wsp_adddelta__ffi(i64 h, const char *addr, const double *v, int64_t v_ext, i64 n) { return wsp_adddelta(h, addr, v, n); }
i64 wsp_copyscore__ffi(i64 h, i64 layer, i64 head, const char *circuit, double *out, int64_t out_ext) { return wsp_copyscore(h, layer, head, circuit, out); }
i64 wsp_derive__ffi(i64 h, const char *outdir, const i64 *layer_map, int64_t layer_map_ext, i64 nl, const i64 *kv_keep, int64_t kv_keep_ext, i64 kvm, const i64 *neuron_keep, int64_t neuron_keep_ext, i64 neuk) { return wsp_derive(h, outdir, layer_map, nl, kv_keep, kvm, neuron_keep, neuk); }
i64 wsp_kvimportance__ffi(i64 h, i64 layer, double *out, int64_t out_ext, i64 cap) { return wsp_kvimportance(h, layer, out, cap); }
i64 wsp_mlpimportance__ffi(i64 h, i64 layer, double *out, int64_t out_ext, i64 cap) { return wsp_mlpimportance(h, layer, out, cap); }
i64 wsp_norms__ffi(i64 h, i64 layer, const char *kind, double *out, int64_t out_ext, i64 cap) { return wsp_norms(h, layer, kind, out, cap); }
i64 wsp_projectout__ffi(i64 h, const char *addr, const double *dirs, int64_t dirs_ext, i64 ndirs, i64 len, i64 vec_axis, i64 flags) { return wsp_projectout(h, addr, dirs, ndirs, len, vec_axis, flags); }
i64 wsp_readslice__ffi(i64 h, const char *addr, double *out, int64_t out_ext, i64 cap) { return wsp_readslice(h, addr, out, cap); }
i64 wsp_setvalues__ffi(i64 h, const char *addr, const double *v, int64_t v_ext, i64 n) { return wsp_setvalues(h, addr, v, n); }
i64 wsp_spectrum__ffi(i64 h, i64 layer, i64 head, const char *circuit, double *wr, int64_t wr_ext, double *wi, int64_t wi_ext, i64 cap) { return wsp_spectrum(h, layer, head, circuit, wr, wi, cap); }
i64 wsp_topk__ffi(const double *score, int64_t score_ext, i64 n, i64 k, i64 *out, int64_t out_ext) { return wsp_topk(score, n, k, out); }
