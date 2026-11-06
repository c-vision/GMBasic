/* llmwsp_agent.c — sessione agente: verbi, budget, trascrizione, replay (W9). C99. */
#define _POSIX_C_SOURCE 200809L
#include "llmwsp_agent.h"
#include "llmwsp_analyze.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXBLOB 32
#define MAXVERB 32

struct llmwsp_session {
    const llmwsp_store *s; const llmwsp_doc *d;
    llmwsp_overlay *ov; llmwsp_budget bud; llmwsp_forward_fn fwd; void *fctx;
    int edits, forwards, seq;
    char *bn[MAXBLOB]; double *bv[MAXBLOB]; size_t bl[MAXBLOB]; int nb;
    FILE *tr;
    char *used[MAXVERB]; int nused;
};

llmwsp_session *llmwsp_session_new(const llmwsp_store *s, const llmwsp_doc *d, const char *srcdir, llmwsp_budget b, llmwsp_forward_fn fwd, void *fctx) {
    llmwsp_session *x = (llmwsp_session *)calloc(1, sizeof *x);
    if (!x) return NULL;
    x->s = s; x->d = d; x->bud = b; x->fwd = fwd; x->fctx = fctx; x->ov = llmwsp_ov_new(s, d, srcdir);
    if (!x->ov) { free(x); return NULL; }
    return x;
}
void llmwsp_session_free(llmwsp_session *x) {
    if (!x) return;
    llmwsp_session_transcript_close(x);
    for (int i = 0; i < x->nb; i++) { free(x->bn[i]); free(x->bv[i]); }
    for (int i = 0; i < x->nused; i++) free(x->used[i]);
    llmwsp_ov_free(x->ov); free(x);
}
llmwsp_overlay *llmwsp_session_overlay(llmwsp_session *x) { return x->ov; }
int llmwsp_session_edits_used(const llmwsp_session *x) { return x->edits; }
int llmwsp_session_forwards_used(const llmwsp_session *x) { return x->forwards; }

int llmwsp_session_transcript_open(llmwsp_session *x, const char *path) { x->tr = fopen(path, "wb"); return x->tr ? 0 : -1; }
int llmwsp_session_transcript_close(llmwsp_session *x) { int r = 0; if (x->tr) { r = fclose(x->tr); x->tr = NULL; } return r; }

static int blob_find(const llmwsp_session *x, const char *n) { for (int i = 0; i < x->nb; i++) if (!strcmp(x->bn[i], n)) return i; return -1; }
static void note_verb(llmwsp_session *x, const char *v) {
    for (int i = 0; i < x->nused; i++) if (!strcmp(x->used[i], v)) return;
    if (x->nused < MAXVERB) x->used[x->nused++] = strdup(v);
}

static void jesc(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) { unsigned char c = (unsigned char)*s; if (c == '"' || c == '\\') { fputc('\\', f); fputc((int)c, f); } else if (c == '\n') fputs("\\n", f); else if (c == '\t') fputs("\\t", f); else if (c < 32) fputc(' ', f); else fputc((int)c, f); }
    fputc('"', f);
}

static int report_text(char *out, size_t cap, int rc, const llmwsp_ov_report *r, const char *err) {
    if (rc == 0) snprintf(out, cap, "ok toccati=%lld/%lld rel_norm=%.6g max_abs=%.6g", (long long)r->touched, (long long)r->total, r->rel_norm, r->max_abs);
    else snprintf(out, cap, "ERRORE: %s", err);
    return rc == 0 ? 0 : -1;
}

static int do_exec(llmwsp_session *x, const char *cmdline, char *out, size_t cap, const char *recorded_fwd) {
    char cmd[4096]; snprintf(cmd, sizeof cmd, "%s", cmdline);
    char *save = NULL, *tok[64]; int nt = 0;
    for (char *t = strtok_r(cmd, " \t\r\n", &save); t && nt < 64; t = strtok_r(NULL, " \t\r\n", &save)) tok[nt++] = t;
    out[0] = 0;
    if (nt == 0) { snprintf(out, cap, "ERRORE: comando vuoto"); return -1; }
    const char *v = tok[0]; char err[300] = ""; llmwsp_ov_report rep; memset(&rep, 0, sizeof rep);
    #define IS(n) (!strcmp(v, n))
    #define NEED(k) if (nt < (k)) { snprintf(out, cap, "ERRORE: argomenti insufficienti per %s", v); return -1; }
    /* --- edit (budget) --- */
    int is_edit = IS("SCALE") || IS("ZERO") || IS("SET") || IS("ADDDELTA") || IS("COPY") || IS("PROJECTOUT");
    int is_read = IS("NORMS") || IS("SPECTRUM") || IS("COMPOSE") || IS("JOURNAL") || IS("SUMMARY") || IS("SHA") || IS("DEFINE") || IS("CHECKPOINT") || IS("ROLLBACK") || IS("UNDO") || IS("REDO");
    if (!is_edit && !is_read && !IS("FORWARD")) { snprintf(out, cap, "RIFIUTATO: verbo non ammesso (%s)", v); return 1; }
    note_verb(x, v);
    if (is_edit && x->edits >= x->bud.max_edits) { snprintf(out, cap, "RIFIUTATO: budget di edit esaurito (%d)", x->bud.max_edits); return 1; }
    if (IS("FORWARD") && x->forwards >= x->bud.max_forwards) { snprintf(out, cap, "RIFIUTATO: budget di forward esaurito (%d)", x->bud.max_forwards); return 1; }
    int rc = 0;
    if (IS("SCALE")) { NEED(3); rc = report_text(out, cap, llmwsp_ov_scale(x->ov, tok[1], atof(tok[2]), &rep, err, sizeof err), &rep, err); }
    else if (IS("ZERO")) { NEED(2); rc = report_text(out, cap, llmwsp_ov_zero(x->ov, tok[1], &rep, err, sizeof err), &rep, err); }
    else if (IS("COPY")) { NEED(3); rc = report_text(out, cap, llmwsp_ov_copy(x->ov, tok[1], tok[2], &rep, err, sizeof err), &rep, err); }
    else if (IS("SET") || IS("ADDDELTA")) {
        NEED(3); int b = blob_find(x, tok[2]);
        if (b < 0) { snprintf(out, cap, "ERRORE: vettore '%s' non definito", tok[2]); return -1; }
        rc = report_text(out, cap, IS("SET") ? llmwsp_ov_set(x->ov, tok[1], x->bv[b], x->bl[b], &rep, err, sizeof err) : llmwsp_ov_adddelta(x->ov, tok[1], x->bv[b], x->bl[b], &rep, err, sizeof err), &rep, err);
    } else if (IS("PROJECTOUT")) {
        NEED(6); int b = blob_find(x, tok[5]); size_t len = (size_t)atol(tok[4]);
        if (b < 0 || len == 0 || x->bl[b] % len) { snprintf(out, cap, "ERRORE: vettore non definito o len incompatibile"); return -1; }
        rc = report_text(out, cap, llmwsp_ov_projectout(x->ov, tok[1], x->bv[b], x->bl[b] / len, len, atoi(tok[2]), atoi(tok[3]), &rep, err, sizeof err), &rep, err);
    } else if (IS("DEFINE")) {
        NEED(3);
        if (x->nb >= MAXBLOB && blob_find(x, tok[1]) < 0) { snprintf(out, cap, "ERRORE: troppi vettori"); return -1; }
        int b = blob_find(x, tok[1]); if (b < 0) { b = x->nb++; x->bn[b] = strdup(tok[1]); x->bv[b] = NULL; }
        free(x->bv[b]); x->bl[b] = (size_t)(nt - 2); x->bv[b] = (double *)malloc(x->bl[b] * sizeof(double));
        for (size_t i = 0; i < x->bl[b]; i++) x->bv[b][i] = atof(tok[2 + i]);
        snprintf(out, cap, "ok %s: %zu valori", tok[1], x->bl[b]);
    } else if (IS("CHECKPOINT")) { NEED(2); rc = llmwsp_ov_checkpoint(x->ov, tok[1]); snprintf(out, cap, rc == 0 ? "ok checkpoint '%s' a pos=%zu" : "ERRORE: checkpoint", tok[1], llmwsp_ov_journal_pos(x->ov)); if (rc) rc = -1; }
    else if (IS("ROLLBACK")) { NEED(2); rc = llmwsp_ov_rollback(x->ov, tok[1]); snprintf(out, cap, rc == 0 ? "ok rollback a '%s' pos=%zu" : "ERRORE: rollback impossibile", tok[1], llmwsp_ov_journal_pos(x->ov)); if (rc) rc = -1; }
    else if (IS("UNDO")) { rc = llmwsp_ov_undo(x->ov); snprintf(out, cap, rc == 0 ? "ok pos=%zu" : "ERRORE: niente da annullare", llmwsp_ov_journal_pos(x->ov)); if (rc) rc = -1; }
    else if (IS("REDO")) { rc = llmwsp_ov_redo(x->ov); snprintf(out, cap, rc == 0 ? "ok pos=%zu" : "ERRORE: niente da ripristinare", llmwsp_ov_journal_pos(x->ov)); if (rc) rc = -1; }
    else if (IS("JOURNAL")) snprintf(out, cap, "journal: %zu edit registrate, %zu applicate", llmwsp_ov_journal_len(x->ov), llmwsp_ov_journal_pos(x->ov));
    else if (IS("SUMMARY")) { char *s = llmwsp_ov_summary(x->ov); snprintf(out, cap, "%s", s ? s : "ERRORE"); free(s); }
    else if (IS("SHA")) { NEED(2); char h[65]; if (llmwsp_ov_tensor_sha256(x->ov, tok[1], h) == 0) snprintf(out, cap, "%s", h); else { snprintf(out, cap, "ERRORE: tensore inesistente"); rc = -1; } }
    else if (IS("NORMS")) {
        NEED(2); llmwsp_dims g; llmwsp_doc_dims(x->d, &g); int L = atoi(tok[1]);
        double *on = (double *)malloc((size_t)g.heads * sizeof(double));
        if (!on || llmwsp_an_head_norms(x->s, x->d, L, NULL, NULL, NULL, on) < 0) { snprintf(out, cap, "ERRORE: layer non valido"); free(on); return -1; }
        size_t n = (size_t)snprintf(out, cap, "norme ||O_h||_F layer %d:", L);
        for (int64_t h = 0; h < g.heads && n + 24 < cap; h++) n += (size_t)snprintf(out + n, cap - n, " %.6g", on[h]);
        free(on);
    } else if (IS("SPECTRUM")) {
        NEED(4); llmwsp_dims g; llmwsp_doc_dims(x->d, &g); double *wr = (double *)malloc((size_t)g.head_dim * 8), *wi = (double *)malloc((size_t)g.head_dim * 8);
        int ok = wr && wi && llmwsp_an_circuit_spectrum(x->s, x->d, atoi(tok[1]), atoi(tok[2]), !strcmp(tok[3], "qk") ? LLMWSP_CIRCUIT_QK : LLMWSP_CIRCUIT_OV, wr, wi) == 0;
        if (ok) { double a, b2; llmwsp_an_copy_scores(wr, wi, (size_t)g.head_dim, &a, &b2); snprintf(out, cap, "%s L%s H%s: copy_sum_ratio=%.6g copy_frac_pos=%.6g (RoPE/qk-norm esclusi)", tok[3], tok[1], tok[2], a, b2); }
        else { snprintf(out, cap, "ERRORE: spettro non calcolabile"); rc = -1; }
        free(wr); free(wi);
    } else if (IS("COMPOSE")) {
        NEED(6); llmwsp_dims g; llmwsp_doc_dims(x->d, &g); double *c = (double *)malloc((size_t)g.head_dim * 8), pk = 0;
        if (c && llmwsp_an_compose_cosines(x->s, x->d, atoi(tok[1]), atoi(tok[2]), atoi(tok[3]), atoi(tok[4]), tok[5][0], c, &pk) == 0) snprintf(out, cap, "compose PK=%.6g cos_max=%.6g", pk, c[0]);
        else { snprintf(out, cap, "ERRORE: composizione non calcolabile"); rc = -1; }
        free(c);
    } else if (IS("FORWARD")) {
        const char *args = cmdline + strlen("FORWARD"); while (*args == ' ') args++;
        if (recorded_fwd) snprintf(out, cap, "%s", recorded_fwd);
        else if (!x->fwd) { snprintf(out, cap, "ERRORE: nessun motore di forward collegato"); return -1; }
        else rc = x->fwd(x->fctx, args, out, cap) == 0 ? 0 : -1;
        if (rc == 0) x->forwards++;
        return rc;
    }
    if (rc == 0 && is_edit) x->edits++;
    return rc;
}

int llmwsp_session_exec(llmwsp_session *x, const char *cmd, char *out, size_t cap) {
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    int rc = do_exec(x, cmd, out, cap, NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (x->tr) {
        char h[65]; llmwsp_sha256_buf(out, strlen(out), h);
        fprintf(x->tr, "{\"seq\":%d,\"cmd\":", ++x->seq); jesc(x->tr, cmd);
        fprintf(x->tr, ",\"status\":%d,\"out_sha256\":\"%s\",\"out\":", rc, h); jesc(x->tr, out);
        fprintf(x->tr, ",\"ms\":%.1f}\n", (double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6); fflush(x->tr);
    }
    return rc;
}

/* estrae e deescapa la stringa di "key": "..." dalla riga (NULL se assente) */
static char *jstr(const char *line, const char *key) {
    char pat[40]; snprintf(pat, sizeof pat, "\"%s\":\"", key);
    const char *p = strstr(line, pat); if (!p) return NULL;
    p += strlen(pat); char *o = (char *)malloc(strlen(p) + 1); size_t k = 0;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) { p++; o[k++] = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p; p++; } else o[k++] = *p++; }
    o[k] = 0; return o;
}

int llmwsp_replay(llmwsp_session *x, const char *path, int *mismatch, char *err, size_t ec) {
    FILE *f = fopen(path, "rb");
    if (!f) { if (err && ec) snprintf(err, ec, "trascrizione illeggibile: %s", path); return -2; }
    char *line = (char *)malloc(1 << 20), *out = (char *)malloc(1 << 17); int n = 0, rc = 0;
    if (mismatch) *mismatch = 0;
    while (fgets(line, 1 << 20, f)) {
        char *cmd = jstr(line, "cmd"), *rec = jstr(line, "out"); const char *sp = strstr(line, "\"status\":");
        if (!cmd || !rec || !sp) { free(cmd); free(rec); rc = -2; if (err && ec) snprintf(err, ec, "riga %d malformata", n + 1); break; }
        n++; int status = atoi(sp + 9);
        int is_fwd = !strncmp(cmd, "FORWARD", 7);
        int got = do_exec(x, cmd, out, 1 << 17, is_fwd ? rec : NULL);
        if (got != status || strcmp(out, rec) != 0) {
            if (mismatch) *mismatch = n;
            if (err && ec) snprintf(err, ec, "divergenza al comando %d (%.60s): stato %d/%d", n, cmd, got, status);
            free(cmd); free(rec); rc = -1; break;
        }
        free(cmd); free(rec);
    }
    free(line); free(out); fclose(f);
    return rc;
}

int llmwsp_session_write_run(const llmwsp_session *x, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "{\"format\":\"gmb.wsp.run@1\",\"audit\":{\"tool_free\":true,\"network\":false,\"shell\":false,\"verbs_used\":[");
    for (int i = 0; i < x->nused; i++) fprintf(f, "%s\"%s\"", i ? "," : "", x->used[i]);
    fprintf(f, "]},\"budget\":{\"max_edits\":%d,\"max_forwards\":%d},\"used\":{\"edits\":%d,\"forwards\":%d,\"commands\":%d}}\n", x->bud.max_edits, x->bud.max_forwards, x->edits, x->forwards, x->seq);
    return fclose(f);
}
