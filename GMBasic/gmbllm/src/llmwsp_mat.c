/* llmwsp_mat.c — Materializer (W3). C99 + POSIX; su macOS usa clonefile(). */
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE 1
#define _DEFAULT_SOURCE 1
#include "llmwsp_mat.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/clonefile.h>
#endif

static void E(char *err, size_t cap, const char *fmt, const char *a, const char *b) {
    if (err && cap) snprintf(err, cap, fmt, a ? a : "", b ? b : "");
}

static int has_suffix(const char *s, const char *suf) {
    size_t a = strlen(s), b = strlen(suf);
    return a >= b && strcmp(s + a - b, suf) == 0;
}

static int copy_file(const char *from, const char *to) {
    FILE *a = fopen(from, "rb");
    if (!a) return -1;
    FILE *b = fopen(to, "wb");
    if (!b) { fclose(a); return -1; }
    enum { CH = 1 << 20 };
    char *buf = (char *)malloc(CH);
    int rc = buf ? 0 : -1;
    size_t n;
    while (rc == 0 && (n = fread(buf, 1, CH, a)) > 0) if (fwrite(buf, 1, n, b) != n) rc = -1;
    free(buf); fclose(a);
    if (fclose(b) != 0) rc = -1;
    return rc;
}

/* clone se possibile, altrimenti copia */
static int clone_or_copy(const char *from, const char *to) {
#ifdef __APPLE__
    if (clonefile(from, to, 0) == 0) return 0;
#endif
    return copy_file(from, to);
}

static int edit_index(const llmwsp_edit *e, size_t ne, const char *name) {
    for (size_t i = 0; i < ne; i++) if (strcmp(e[i].name, name) == 0) return (int)i;
    return -1;
}

static int cmp_off(const void *a, const void *b) {
    const llmwsp_tensor *x = *(const llmwsp_tensor *const *)a, *y = *(const llmwsp_tensor *const *)b;
    return x->offset < y->offset ? -1 : x->offset > y->offset;
}

static int dir_is_empty_or_new(const char *d) {
    struct stat st;
    if (stat(d, &st) < 0) return errno == ENOENT;
    if (!S_ISDIR(st.st_mode)) return 0;
    DIR *dp = opendir(d);
    if (!dp) return 0;
    struct dirent *e; int empty = 1;
    while ((e = readdir(dp))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) { empty = 0; break; }
    closedir(dp);
    return empty;
}

static void jq(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) { if (*s == '"' || *s == '\\') fputc('\\', f); fputc(*s, f); }
    fputc('"', f);
}

int llmwsp_materialize(const llmwsp_store *s, const char *srcdir, const llmwsp_edit *edits, size_t ne,
                       const char *outdir, llmwsp_mat_stats *st, char *err, size_t errcap) {
    llmwsp_mat_stats z = {0};
    if (!st) st = &z;
    memset(st, 0, sizeof *st);
    if (err && errcap) err[0] = 0;
    /* validazione edit */
    for (size_t i = 0; i < ne; i++) {
        const llmwsp_tensor *t = llmwsp_store_find(s, edits[i].name);
        if (!t) { E(err, errcap, "edit su tensore inesistente: %s%s", edits[i].name, ""); return -1; }
        if (t->nbytes != edits[i].nbytes || (!edits[i].data && t->nbytes)) { E(err, errcap, "edit con dimensione errata: %s%s", edits[i].name, ""); return -1; }
        for (size_t k = 0; k < i; k++) if (strcmp(edits[k].name, edits[i].name) == 0) { E(err, errcap, "edit duplicata: %s%s", edits[i].name, ""); return -1; }
    }
    if (!dir_is_empty_or_new(outdir)) { E(err, errcap, "outdir esiste e non è vuota: %s%s", outdir, ""); return -1; }
    mkdir(outdir, 0755);
    size_t nt = llmwsp_store_count(s);
    char sp[1100], dp[1100];
    FILE *prov = NULL;
    int nsh = llmwsp_store_file_count(s);
    for (int fi = 0; fi < nsh; fi++) {
        const char *fn = llmwsp_store_file_name(s, fi);
        snprintf(sp, sizeof sp, "%s/%s", srcdir, fn);
        snprintf(dp, sizeof dp, "%s/%s", outdir, fn);
        const llmwsp_tensor **in = (const llmwsp_tensor **)malloc((nt ? nt : 1) * sizeof *in);
        size_t c = 0; int touched = 0;
        for (size_t i = 0; i < nt; i++) {
            const llmwsp_tensor *t = llmwsp_store_at(s, i);
            if (t->file == fi) { in[c++] = t; if (edit_index(edits, ne, t->name) >= 0) touched = 1; }
        }
        if (!touched) {
            free(in);
            if (clone_or_copy(sp, dp) < 0) { E(err, errcap, "clone/copia fallita: %s%s", fn, ""); return -1; }
            st->shards_cloned++;
            continue;
        }
        qsort(in, c, sizeof *in, cmp_off);
        llmwsp_wspec *spec = (llmwsp_wspec *)calloc(c, sizeof *spec);
        for (size_t i = 0; i < c; i++) {
            spec[i].name = in[i]->name; spec[i].dtype = in[i]->dtype; spec[i].ndim = in[i]->ndim;
            for (int d = 0; d < in[i]->ndim; d++) spec[i].shape[d] = in[i]->shape[d];
        }
        llmwsp_writer *w = NULL;
        int rc = llmwsp_writer_open(dp, spec, c, &w, err, errcap);
        for (size_t i = 0; rc == 0 && i < c; i++) {
            int ei = edit_index(edits, ne, in[i]->name);
            rc = ei >= 0 ? llmwsp_writer_append(w, edits[ei].data, (size_t)edits[ei].nbytes) : llmwsp_writer_copy(w, s, in[i]);
            if (rc < 0) E(err, errcap, "scrittura fallita: %s%s", in[i]->name, "");
        }
        if (w) { char e2[200]; int r2 = llmwsp_writer_close(w, e2, sizeof e2); if (r2 < 0 && rc == 0) { rc = -1; E(err, errcap, "%s%s", e2, ""); } }
        free(spec); free(in);
        if (rc < 0) return -1;
        st->shards_rewritten++;
    }
    /* file accessori (non ricorsivo): tutto ciò che non è .safetensors */
    DIR *d = opendir(srcdir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.' || has_suffix(e->d_name, ".safetensors")) continue;
            snprintf(sp, sizeof sp, "%s/%s", srcdir, e->d_name);
            struct stat fs;
            if (stat(sp, &fs) < 0 || !S_ISREG(fs.st_mode)) continue;
            snprintf(dp, sizeof dp, "%s/%s", outdir, e->d_name);
            if (strcmp(e->d_name, "provenance.json") == 0) continue;
            if (copy_file(sp, dp) < 0) { closedir(d); E(err, errcap, "copia fallita: %s%s", e->d_name, ""); return -1; }
            st->files_copied++;
        }
        closedir(d);
    }
    st->edited_tensors = (int)ne;
    /* provenance */
    snprintf(dp, sizeof dp, "%s/provenance.json", outdir);
    prov = fopen(dp, "wb");
    if (!prov) { E(err, errcap, "provenance non scrivibile%s%s", "", ""); return -1; }
    fputs("{\"format\":\"gmb.wsp.provenance@1\",\"source\":", prov); jq(prov, srcdir);
    fprintf(prov, ",\"shards_cloned\":%d,\"shards_rewritten\":%d,\"edits\":[", st->shards_cloned, st->shards_rewritten);
    for (size_t i = 0; i < ne; i++) {
        char before[65], after[65];
        llmwsp_tensor_sha256(s, llmwsp_store_find(s, edits[i].name), before);
        llmwsp_sha256_buf(edits[i].data, (size_t)edits[i].nbytes, after);
        fprintf(prov, "%s\n{\"tensor\":", i ? "," : ""); jq(prov, edits[i].name);
        fprintf(prov, ",\"sha256_before\":\"%s\",\"sha256_after\":\"%s\"}", before, after);
    }
    fputs("\n]}\n", prov);
    fclose(prov);
    return 0;
}

int llmwsp_materialize_verify(const llmwsp_store *s, const llmwsp_edit *edits, size_t ne, const char *outdir, char *err, size_t errcap) {
    llmwsp_store *o = NULL;
    if (llmwsp_store_open(outdir, &o, err, errcap) < 0) return -1;
    int viol = 0;
    #define V(...) do { if (!viol && err && errcap) snprintf(err, errcap, __VA_ARGS__); viol++; } while (0)
    if (llmwsp_store_count(o) != llmwsp_store_count(s)) V("numero di tensori diverso");
    char e2[200];
    if (llmwsp_store_check_coverage(o, e2, sizeof e2) != 0) V("copertura: %s", e2);
    for (size_t i = 0; i < llmwsp_store_count(s); i++) {
        const llmwsp_tensor *a = llmwsp_store_at(s, i), *b = llmwsp_store_find(o, a->name);
        if (!b || b->ndim != a->ndim || strcmp(a->dtype, b->dtype) != 0 || b->nbytes != a->nbytes) { V("tensore diverso/mancante: %s", a->name); continue; }
        int same = 1;
        for (int d = 0; d < a->ndim; d++) if (a->shape[d] != b->shape[d]) same = 0;
        if (!same) { V("forma diversa: %s", a->name); continue; }
        char ha[65], hb[65];
        int ei = edit_index(edits, ne, a->name);
        if (ei >= 0) llmwsp_sha256_buf(edits[ei].data, (size_t)edits[ei].nbytes, ha);
        else if (llmwsp_tensor_sha256(s, a, ha) < 0) { V("lettura originale: %s", a->name); continue; }
        if (llmwsp_tensor_sha256(o, b, hb) < 0 || strcmp(ha, hb) != 0) V("contenuto diverso: %s", a->name);
    }
    #undef V
    llmwsp_store_close(o);
    return viol;
}
