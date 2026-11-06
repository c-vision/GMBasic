/* llmwsp_verify.c — verifica, gate e provenienza (W8). C99. */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_verify.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void first(llmwsp_verify_report *r, const char *m, const char *a) { if (!r->first_error[0]) snprintf(r->first_error, sizeof r->first_error, "%s%s%s", m, a ? ": " : "", a ? a : ""); }

/* conta i non finiti in un tensore float, a blocchi */
static int64_t count_nonfinite(const llmwsp_store *s, const llmwsp_tensor *t) {
    int k = !strcmp(t->dtype, "F32") ? 4 : (!strcmp(t->dtype, "BF16") || !strcmp(t->dtype, "F16")) ? 2 : 0;
    if (!k) return 0;
    enum { CH = 1 << 20 };
    unsigned char *b = (unsigned char *)malloc(CH);
    if (!b) return -1;
    int64_t bad = 0;
    for (uint64_t off = 0; off < t->nbytes; off += CH) {
        uint64_t n = t->nbytes - off < CH ? t->nbytes - off : CH;
        if (llmwsp_read_bytes(s, t, off, n, b) < 0) { free(b); return -1; }
        for (uint64_t i = 0; i + (uint64_t)k <= n; i += (uint64_t)k) {
            if (k == 4) { uint32_t v; memcpy(&v, b + i, 4); if ((v & 0x7f800000u) == 0x7f800000u) bad++; }
            else { uint16_t v; memcpy(&v, b + i, 2); if (!strcmp(t->dtype, "BF16") ? ((v & 0x7f80u) == 0x7f80u) : ((v & 0x7c00u) == 0x7c00u)) bad++; }
        }
    }
    free(b);
    return bad;
}

int llmwsp_verify_dir(const char *dir, const llmwsp_verify_opts *o, llmwsp_verify_report *r) {
    memset(r, 0, sizeof *r);
    llmwsp_store *s; char err[300];
    if (llmwsp_store_open(dir, &s, err, sizeof err) < 0) { first(r, "checkpoint non apribile", err); return -1; }
    r->tensors = llmwsp_store_count(s);
    r->coverage_violations = llmwsp_store_check_coverage(s, err, sizeof err);
    if (r->coverage_violations) first(r, "copertura", err);
    if (o && o->check_doc) {
        char cp[1100]; llmwsp_doc *d;
        snprintf(cp, sizeof cp, "%s/config.json", dir);
        if (llmwsp_doc_build(s, cp, &d, err, sizeof err) < 0) { r->doc_violations = 1; first(r, "documento non costruibile", err); }
        else { r->doc_violations = llmwsp_doc_check(d, err, sizeof err); if (r->doc_violations) first(r, "documento", err); llmwsp_doc_free(d); }
    }
    if (o && o->check_finite)
        for (size_t i = 0; i < r->tensors; i++) {
            int64_t b = count_nonfinite(s, llmwsp_store_at(s, i));
            if (b < 0) { first(r, "lettura fallita", llmwsp_store_at(s, i)->name); r->nonfinite_tensors++; }
            else if (b > 0) { r->nonfinite_tensors++; r->nonfinite_values += b; first(r, "valori non finiti in", llmwsp_store_at(s, i)->name); }
        }
    if (o && o->base) {
        const llmwsp_store *bs = o->base;
        if (llmwsp_store_count(bs) != r->tensors) { r->shape_mismatch++; first(r, "numero di tensori diverso dalla base", NULL); }
        for (size_t i = 0; i < llmwsp_store_count(bs); i++) {
            const llmwsp_tensor *x = llmwsp_store_at(bs, i), *y = llmwsp_store_find(s, x->name);
            if (!y || y->ndim != x->ndim || strcmp(y->dtype, x->dtype) != 0 || y->nbytes != x->nbytes) { r->shape_mismatch++; first(r, "tensore mancante o forma/dtype diversi", x->name); continue; }
            int same = 1; for (int d = 0; d < x->ndim; d++) if (x->shape[d] != y->shape[d]) same = 0;
            if (!same) { r->shape_mismatch++; first(r, "forma diversa", x->name); continue; }
            char h1[65], h2[65];
            if (llmwsp_tensor_sha256(bs, x, h1) < 0 || llmwsp_tensor_sha256(s, y, h2) < 0) { first(r, "lettura fallita", x->name); r->shape_mismatch++; continue; }
            if (strcmp(h1, h2) != 0) {
                r->changed++;
                int ok = 0; for (size_t k = 0; k < o->n_allowed; k++) if (!strcmp(o->allowed_changed[k], x->name)) { ok = 1; break; }
                if (!ok) { r->unexpected_changed++; first(r, "tensore cambiato e non ammesso", x->name); }
            }
        }
    }
    r->ok = !r->doc_violations && !r->coverage_violations && !r->nonfinite_tensors && !r->shape_mismatch && !r->unexpected_changed;
    llmwsp_store_close(s);
    return 0;
}

int llmwsp_gates_eval(const llmwsp_gate *g, size_t n, char **json_out) {
    int fails = 0; size_t cap = 512 + n * 256, len = 0; char *b = (char *)malloc(cap);
    if (b) len += (size_t)snprintf(b, cap, "{\"gates\":[");
    for (size_t i = 0; i < n; i++) {
        int pass = isfinite(g[i].value) && isfinite(g[i].threshold) && (g[i].le ? g[i].value <= g[i].threshold : g[i].value >= g[i].threshold);
        if (!pass) fails++;
        if (b) len += (size_t)snprintf(b + len, cap - len, "%s{\"name\":\"%s\",\"value\":%.9g,\"op\":\"%s\",\"threshold\":%.9g,\"verdict\":\"%s\"}", i ? "," : "", g[i].name, g[i].value, g[i].le ? "<=" : ">=", g[i].threshold, pass ? "PASS" : "FAIL");
    }
    if (b) snprintf(b + len, cap - len, "],\"failed\":%d,\"accepted\":%s}", fails, fails ? "false" : "true");
    if (json_out) *json_out = b; else free(b);
    return fails;
}

int llmwsp_model_hash(const llmwsp_store *s, char hex[65]) {
    size_t n = llmwsp_store_count(s), cap = 0, m = 0;
    for (size_t i = 0; i < n; i++) cap += strlen(llmwsp_store_at(s, i)->name) + 66;
    char *c = (char *)malloc(cap + 1);
    if (!c) return -1;
    for (size_t i = 0; i < n; i++) {                       /* "<nome>\0<sha256>\n", in ordine di nome */
        const llmwsp_tensor *t = llmwsp_store_at(s, i); char h[65];
        if (llmwsp_tensor_sha256(s, t, h) < 0) { free(c); return -1; }
        size_t L = strlen(t->name); memcpy(c + m, t->name, L); m += L; c[m++] = 0; memcpy(c + m, h, 64); m += 64; c[m++] = '\n';
    }
    llmwsp_sha256_buf(c, m, hex);
    free(c);
    return 0;
}

int llmwsp_provenance_write(const char *path, const llmwsp_store *base, const char *recipe, const llmwsp_store *out, const char *gates, char *err, size_t ec) {
    char h0[65], h2[65], hops[65];
    if (llmwsp_model_hash(base, h0) < 0 || llmwsp_model_hash(out, h2) < 0) { if (err && ec) snprintf(err, ec, "hash del modello fallito"); return -1; }
    size_t rl = recipe ? strlen(recipe) : 0; char *cat = (char *)malloc(64 + rl + 1);
    memcpy(cat, h0, 64); if (rl) memcpy(cat + 64, recipe, rl);
    llmwsp_sha256_buf(cat, 64 + rl, hops); free(cat);
    FILE *f = fopen(path, "wb");
    if (!f) { if (err && ec) snprintf(err, ec, "provenance non scrivibile: %s", path); return -1; }
    fprintf(f, "{\"format\":\"gmb.wsp.chain@1\",\"chain\":[\n {\"stage\":\"base\",\"hash\":\"%s\"},\n {\"stage\":\"ops\",\"hash\":\"%s\",\"definition\":\"sha256(base_hash || recipe_text)\"},\n {\"stage\":\"output\",\"hash\":\"%s\"}\n],\"recipe\":\"", h0, hops, h2);
    for (const char *p = recipe ? recipe : ""; *p; p++) { if (*p == '"' || *p == '\\') fputc('\\', f); if (*p == '\n') fputs("\\n", f); else fputc(*p, f); }
    fprintf(f, "\",\"gates\":%s}\n", gates ? gates : "null");
    return fclose(f);
}
