/* gmb_worker.c — AI WORKER + AI STATE (P3 AI-Runtime consolidato, asse C).
 * Worker lifecycle + workspace per-task + state save/load + contatori run-level.
 * SPECCHIO ESATTO del C# AiWorkerRuntime (gmb_context.c già copre budget/view). */
#include "gmb_worker.h"
#include "gmb_strbuf.h"   /* vista curata del run (AI WORKER "<TASK>") */
#include "gmb_context.h"   /* normalizzazione budget/policy della vista (P2) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "gmb_compat_win.h"   /* mkdir a 2 argomenti (solo su Windows) */

#define GMB_WKR_MAX 64

typedef struct {
    int used;
    char name[128];
    long long budget;
    char workspace[512];
} gmb_worker_entry;

static gmb_worker_entry g_wkr[GMB_WKR_MAX];
static int g_current = -1;          /* worker corrente: `AI WORKER "<TASK>"` agisce su questo */
static long long g_rounds;
static long long g_rescued;
static long long g_spend;

static void wkr_default_workspace(const char* name, char* out, size_t cap)
{
    snprintf(out, cap, "work/%s", name ? name : "w");
}

static gmb_worker_entry* wkr_current(void)
{
    if (g_current < 0 || g_current >= GMB_WKR_MAX) return NULL;
    return g_wkr[g_current].used ? &g_wkr[g_current] : NULL;
}

/* estrae "workspace":"…" / "budget":N da opts (stesse chiavi del C# AiWorkerRuntime). */
static void wkr_parse_opts(const char* opts, char* workspace, size_t wcap, long long* budget)
{
    *budget = 0;
    if (!opts || !*opts) return;
    const char* b = strstr(opts, "\"budget\"");
    if (b) { const char* c = strchr(b, ':'); if (c) *budget = strtoll(c + 1, NULL, 10); }
    const char* w = strstr(opts, "\"workspace\"");
    if (w) {
        const char* c = strchr(w, ':');
        if (c) {
            const char* q1 = strchr(c, '"');
            if (q1) { const char* q2 = strchr(q1 + 1, '"');
                if (q2) { size_t n = (size_t)(q2 - q1 - 1); if (n >= wcap) n = wcap - 1;
                    memcpy(workspace, q1 + 1, n); workspace[n] = 0; } }
        }
    }
}

static gmb_worker_entry* wkr_find(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < GMB_WKR_MAX; i++)
        if (g_wkr[i].used && strcmp(g_wkr[i].name, name) == 0) return &g_wkr[i];
    for (int i = 0; i < GMB_WKR_MAX; i++)
        if (!g_wkr[i].used) {
            g_wkr[i].used = 1;
            snprintf(g_wkr[i].name, sizeof(g_wkr[i].name), "%s", name);
            g_wkr[i].budget = 0;
            return &g_wkr[i];
        }
    return NULL;
}

static void mkdir_p(const char* dir)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", dir ? dir : "work");
    for (char* p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

/* AI WORKER NEW name, opts → registra il worker, crea il workspace, lo rende CORRENTE.
 * Parità col C# AiWorkerRuntime.WorkerNew: opts {"workspace":…,"budget":…}. Prima gli opts
 * erano IGNORATI (budget sempre 0 e workspace sempre work/<name>): la copertura P3 era
 * "in tabella", non semantica. */
double gmb_worker_new(const char* name, const char* opts)
{
    if (!name || !*name) return -1;
    /* ⚠️ wkr_find ALLOCA se assente: la guardia precedente (`if (wkr_find(name)) return -1;`)
     * rendeva gmb_worker_new SEMPRE -1 (il worker non nasceva mai e nessun workspace veniva
     * creato) — copertura P3 "in tabella" mai eseguita. Serve una ricerca NON allocante. */
    for (int i = 0; i < GMB_WKR_MAX; i++)
        if (g_wkr[i].used && strcmp(g_wkr[i].name, name) == 0) return -1;
    gmb_worker_entry* e = wkr_find(name);
    if (!e) return -1;
    wkr_default_workspace(name, e->workspace, sizeof(e->workspace));
    wkr_parse_opts(opts, e->workspace, sizeof(e->workspace), &e->budget);
    g_current = (int)(e - g_wkr);
    mkdir_p(e->workspace);
    return 1;
}

/* AI WORKER RUN name */
double gmb_worker_run(const char* name)
{
    if (!wkr_find(name)) return -1;
    g_rounds++;
    return 1;
}

/* AI STATE SAVE worker, key, value — scrive work/<worker>/state/<key>.txt */
double gmb_state_save(const char* worker, const char* key, const char* value)
{
    if (!wkr_find(worker)) return -1;
    if (!worker || !key) return -1;
    char dir[1024], path[2048];
    gmb_worker_entry* e = wkr_find(worker);
    snprintf(dir, sizeof(dir), "%s/state", e->workspace);
    mkdir_p(dir);
    snprintf(path, sizeof(path), "%s/%s.txt", dir, key);
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fputs(value ? value : "", f);
    fclose(f);
    return 1;
}

/* AI STATE LOAD worker, key */
gmb_string_t gmb_state_load(const char* worker, const char* key)
{
    if (!worker || !key) return gmb_string_alloc("");
    char path[2048];
    { gmb_worker_entry* e = wkr_find(worker);
      snprintf(path, sizeof(path), "%s/state/%s.txt", e->workspace, key); }
    FILE* f = fopen(path, "rb");
    if (!f) return gmb_string_alloc("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return gmb_string_alloc(""); }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return gmb_string_alloc(""); }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = 0;
    fclose(f);
    gmb_string_t r = gmb_string_alloc(buf);
    free(buf);
    return r;
}

/* ── AI WORKER "<TASK>" — INVOCAZIONE PULITA (C1) ────────────────────────────────────
 * Vista curata = SOLO memoria esterna del run (workspace), nell'ordine:
 *   TASK: <task>, poi plan.md, notes.md, solution, e i state/*.txt in ordine alfabetico.
 * Normalizzata dal Context Engine (gmb_ctx_*, chiave "worker:<name>", budget del worker):
 * con policy error + overflow l'invocazione è RIFIUTATA (0) e conta come rescue.
 * Poi gmb_llm_complete (errore esplicito senza provider, stessa sentinella del C#):
 * esito in <ws>/solution, contatori rounds/spend, -2 se il provider ha risposto con errore.
 * Ritorna 1 ok · 0 budget · -1 nessun worker · -2 errore provider (speculare al C#). */
static void wkr_append_file(gmb_strbuf_t* sb, const char* path, const char* label)
{
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = 0; fclose(f);
    while (rd > 0 && (buf[rd - 1] == '\n' || buf[rd - 1] == '\r')) buf[--rd] = 0;
    gmb_strbuf_append(sb, "\n", 1); gmb_strbuf_append(sb, label, strlen(label));
    gmb_strbuf_append(sb, ":\n", 2); gmb_strbuf_append(sb, buf, rd);
    gmb_strbuf_append(sb, "\n", 1);
    free(buf);
}

/* state/*.txt del workspace, in ordine alfabetico (stessa regola del C# BuildCuratedView:
 * OrderBy(Ordinal) sul path → nomi ordinati). Sezione "STATE <key>:" per ogni file. */
static void wkr_append_state(gmb_strbuf_t* sb, const char* workspace)
{
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/state", workspace);
    DIR* d = opendir(dir);
    if (!d) return;
    char names[64][128];
    int n = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL && n < 64) {
        size_t l = strlen(ent->d_name);
        if (l < 4 || l >= 128 || strcmp(ent->d_name + l - 4, ".txt") != 0) continue;
        snprintf(names[n], sizeof(names[n]), "%s", ent->d_name);
        n++;
    }
    closedir(d);
    for (int i = 1; i < n; i++)          /* insertion sort: deterministico, n<=64 */
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char tmp[128]; memcpy(tmp, names[j - 1], sizeof(tmp));
            memcpy(names[j - 1], names[j], sizeof(tmp)); memcpy(names[j], tmp, sizeof(tmp));
        }
    for (int i = 0; i < n; i++) {
        char path[2048], label[160];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        size_t l = strlen(names[i]);
        snprintf(label, sizeof(label), "STATE %.*s", (int)(l - 4), names[i]);
        wkr_append_file(sb, path, label);
    }
}

static void wkr_write_file(const char* dir, const char* name, const char* content)
{
    char path[2048];
    mkdir_p(dir);
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE* f = fopen(path, "w");
    if (!f) return;
    fputs(content ? content : "", f);
    fclose(f);
}

double gmb_worker_invoke(const char* task)
{
    gmb_worker_entry* e = wkr_current();
    if (!e) return -1;
    char key[192];
    snprintf(key, sizeof(key), "worker:%s", e->name);
    gmb_strbuf_t sb = gmb_strbuf_new();
    gmb_strbuf_append(&sb, "TASK: ", 6);
    gmb_strbuf_append(&sb, task ? task : "", task ? strlen(task) : 0);
    gmb_strbuf_append(&sb, "\n", 1);
    char path[2048];
    snprintf(path, sizeof(path), "%s/plan.md", e->workspace);    wkr_append_file(&sb, path, "PLAN");
    snprintf(path, sizeof(path), "%s/notes.md", e->workspace);   wkr_append_file(&sb, path, "NOTES");
    snprintf(path, sizeof(path), "%s/solution", e->workspace);   wkr_append_file(&sb, path, "SOLUTION");
    snprintf(path, sizeof(path), "%s/best", e->workspace);       wkr_append_file(&sb, path, "BEST");
    wkr_append_state(&sb, e->workspace);
    if (e->budget > 0) gmb_ctx_set_budget(key, (double)e->budget);
    if (!gmb_ctx_set_view(key, sb.data ? sb.data : "")) {
        char msg[256];
        snprintf(msg, sizeof(msg), "context overflow: %d > budget %lld",
                 (int)strlen(sb.data ? sb.data : ""), (long long)gmb_ctx_budget(key));
        gmb_strbuf_free(&sb);
        g_rounds++; g_rescued++;
        wkr_write_file(e->workspace, "last_error", msg);
        { char sp[2048]; mkdir_p(e->workspace); snprintf(sp, sizeof(sp), "%s/state", e->workspace);
          mkdir_p(sp); wkr_write_file(sp, "last_error.txt", msg); }   /* osservabile: AI.STATELOAD */
        return 0;
    }
    gmb_strbuf_free(&sb);
    gmb_string_t view = gmb_ctx_view(key);
    wkr_write_file(e->workspace, "context.md", view.data ? view.data : "");
    g_rounds++; g_spend += view.data ? (long long)strlen(view.data) : 0;
    char* answer = gmb_llm_complete(
        "Sei un worker isolato: rispondi SOLO con la vista fornita (task/plan/notes/solution). "
        "Non inventare contesto non presente nella vista.",
        view.data ? view.data : "");
    wkr_write_file(e->workspace, "solution", answer ? answer : "");
    { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
      wkr_write_file(sp, "solution.txt", answer ? answer : ""); }    /* osservabile: AI.STATELOAD */
    int rc = 1;
    if (answer && strncmp(answer, "Error:", 6) == 0) {
        g_rescued++;
        wkr_write_file(e->workspace, "last_error", answer);
        char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
        wkr_write_file(sp, "last_error.txt", answer);
        rc = -2;
    }
    free(answer);
    gmb_string_free(&view);
    return (double)rc;
}

/* best-so-far del run: copia <ws>/solution → <ws>/best (+ state/best) se la soluzione è valida.
 * Nel loop multi-round è ciò che rende il run "non ancorato" all'ultimo tentativo (D2). */
static void wkr_promote_best(gmb_worker_entry* e)
{
    char sol[2048], dst[2048], dir[2048];
    snprintf(sol, sizeof(sol), "%s/solution", e->workspace);
    FILE* f = fopen(sol, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = 0; fclose(f);
    if (strncmp(buf, "Error:", 6) != 0) {
        snprintf(dst, sizeof(dst), "%s/best", e->workspace);
        FILE* o = fopen(dst, "w");
        if (o) { fwrite(buf, 1, rd, o); fclose(o); }
        /* P4 "best-so-far checkpoint+artifact": anche come ARTIFACT del run (ArtifactStore). */
        { char art[192]; snprintf(art, sizeof(art), "best:%s", e->name); gmb_artifact_put(art, dst); }
        snprintf(dir, sizeof(dir), "%s/state", e->workspace); mkdir_p(dir);
        wkr_write_file(dir, "best.txt", buf);
    }
    free(buf);
}

/* Ricostruisce la vista curata del run (TASK + plan/notes/solution/best + state/*).
 * Usata sia dall'invocazione singola sia dal LOOP (che la ricostruisce ad ogni round). */
static void wkr_build_view(gmb_worker_entry* e, const char* task, gmb_strbuf_t* sb)
{
    char path[2048];
    gmb_strbuf_append(sb, "TASK: ", 6);
    gmb_strbuf_append(sb, task ? task : "", task ? strlen(task) : 0);
    gmb_strbuf_append(sb, "\n", 1);
    snprintf(path, sizeof(path), "%s/plan.md", e->workspace);    wkr_append_file(sb, path, "PLAN");
    snprintf(path, sizeof(path), "%s/notes.md", e->workspace);   wkr_append_file(sb, path, "NOTES");
    snprintf(path, sizeof(path), "%s/solution", e->workspace);   wkr_append_file(sb, path, "SOLUTION");
    snprintf(path, sizeof(path), "%s/best", e->workspace);       wkr_append_file(sb, path, "BEST");
    wkr_append_state(sb, e->workspace);
}

/* AI WORKER LOOP "<TASK>"[, rounds] — LOOP ISOLATO multi-round (assi C + D1).
 * Fino a N round (default 3), ognuno con la vista curata aggiornata dal round precedente e con
 * budget CUMULATIVO del run: il budget del worker è il tetto dell'intero run (ogni round vede
 * `budget - speso` come cap). Stop espliciti: -1 nessun worker · -2 errore provider · 0 budget
 * del run esaurito (last_error "run budget exhausted: …") · 1 tutti i round completati.
 * Speculare a AiWorkerRuntime.WorkerLoop (C#). */
double gmb_worker_loop(const char* task, double rounds, const char* require)
{
    gmb_worker_entry* e = wkr_current();
    if (!e) return -1;
    int has_audit = require && *require;
    char key[192];
    snprintf(key, sizeof(key), "worker:%s", e->name);
    if (e->budget > 0) gmb_ctx_set_budget(key, (double)e->budget);
    long long run_budget = (long long)gmb_ctx_budget(key);
    int n = rounds > 0 ? (int)rounds : 3;
    long long run_spent = 0;
    for (int round = 1; round <= n; round++) {
        gmb_strbuf_t sb = gmb_strbuf_new();
        wkr_build_view(e, task, &sb);
        long long raw_len = (long long)strlen(sb.data ? sb.data : "");
        if (run_budget > 0) {
            long long remaining = run_budget - run_spent;
            if (remaining <= 0 || (remaining < raw_len && strcmp(gmb_ctx_get_policy(key), "error") == 0)) {
                char msg[192];
                snprintf(msg, sizeof(msg), "run budget exhausted: spent=%lld budget=%lld",
                         (long long)run_spent, (long long)run_budget);
                gmb_strbuf_free(&sb);
                g_rescued++;
                wkr_write_file(e->workspace, "last_error", msg);
                { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
                  wkr_write_file(sp, "last_error.txt", msg); }
                return 0;
            }
            gmb_ctx_set_budget(key, (double)remaining);   /* cap del round = residuo */
        }
        if (!gmb_ctx_set_view(key, sb.data ? sb.data : "")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "context overflow: %d > budget %lld",
                     (int)raw_len, (long long)gmb_ctx_budget(key));
            gmb_strbuf_free(&sb);
            g_rounds++; g_rescued++;
            wkr_write_file(e->workspace, "last_error", msg);
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
              wkr_write_file(sp, "last_error.txt", msg); }
            return 0;
        }
        gmb_strbuf_free(&sb);
        gmb_string_t view = gmb_ctx_view(key);
        wkr_write_file(e->workspace, "context.md", view.data ? view.data : "");
        g_rounds++; run_spent += view.data ? (long long)strlen(view.data) : 0;
        g_spend += view.data ? (long long)strlen(view.data) : 0;
        char* answer = gmb_llm_complete(
            "Sei un worker isolato: rispondi SOLO con la vista fornita (task/plan/notes/solution). "
            "Non inventare contesto non presente nella vista.",
            view.data ? view.data : "");
        wkr_write_file(e->workspace, "solution", answer ? answer : "");
        { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
          wkr_write_file(sp, "solution.txt", answer ? answer : ""); }
        int is_err = answer && strncmp(answer, "Error:", 6) == 0;
        free(answer);
        gmb_string_free(&view);
        if (is_err) {
            g_rescued++;
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace);
              /* last_error = testo della solution (errore provider) */
              char sol[2048]; snprintf(sol, sizeof(sol), "%s/state/solution.txt", e->workspace);
              FILE* f = fopen(sol, "rb"); if (f) { fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
                char* b=(char*)malloc((size_t)sz+1); if (b) { size_t r=fread(b,1,(size_t)sz,f); b[r]=0;
                wkr_write_file(e->workspace, "last_error", b); wkr_write_file(sp, "last_error.txt", b); free(b);} fclose(f);} }
            if (!has_audit) return -2;
            /* con audit: si giudica anche la risposta-errore (sotto); se l'audit la rifiuta → -2 */
        }
        wkr_promote_best(e);
        if (!has_audit) continue;
        /* ── AUDIT indipendente (REQUIRE con RESULT$ = risposta), rework informato ──────
         * L'esito dell'audit entra in `<ws>/state/audit_error.txt` → il round successivo lo
         * vede nella vista curata (sezione `STATE audit_error`): il rework non è cieco. */
        {
            char sol[2048];
            snprintf(sol, sizeof(sol), "%s/state/solution.txt", e->workspace);
            FILE* f = fopen(sol, "rb");
            char* ans = NULL;
            if (f) {
                fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                ans = (char*)malloc((size_t)(sz > 0 ? sz : 0) + 1);
                if (ans) { size_t r = fread(ans, 1, (size_t)(sz > 0 ? sz : 0), f); ans[r] = 0; }
                fclose(f);
            }
            if (!ans) ans = strdup("");
            if (gmb_verify_require(require, ans)) {
                wkr_write_file(e->workspace, "best", ans);
                char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
                wkr_write_file(sp, "best.txt", ans);
                wkr_write_file(sp, "verified.txt", "1");
                wkr_write_file(sp, "audit_error.txt", "");
                wkr_write_file(e->workspace, "last_error", "");
                free(ans);
                return 1;   /* VERIFICATO: stop anticipato (l'auditor ha accettato la risposta) */
            }
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
              wkr_write_file(sp, "audit_error.txt", ans); }
            free(ans);
        }
        if (is_err) return -2;   /* audit rifiutato + risposta in errore: stop esplicito */
    }
    if (!has_audit) return 1;
    {
        char msg[512];
        char sp[2048];
        snprintf(sp, sizeof(sp), "%s/state/audit_error.txt", e->workspace);
        FILE* f = fopen(sp, "rb");
        char detail[256] = "";
        if (f) { size_t r = fread(detail, 1, sizeof(detail) - 1, f); detail[r] = 0; fclose(f); }
        snprintf(msg, sizeof(msg), "audit failed after %d round(s): %s", n, detail);
        g_rescued++;
        wkr_write_file(e->workspace, "last_error", msg);
        { char sd[2048]; snprintf(sd, sizeof(sd), "%s/state", e->workspace); mkdir_p(sd);
          wkr_write_file(sd, "last_error.txt", msg); }
    }
    return -3;
}

/* AI WORKER MANAGE [, cycles] [REQUIRE "<expr>"] — MANAGER LOOP (P4).
 * Il manager prende dal task graph il prossimo task lavorabile (gmb_task_next_pending: primo
 * `pending` in ordine alfabetico con dipendenze `done`) e lo assegna al worker con l'invocazione
 * pulita (vista curata + budget CUMULATIVO del run). L'auditor (REQUIRE con RESULT$) giudica:
 * soddisfatta → task `done`; rifiutata → DISSENSO loggato (state/dissent + state/dissents, visibile
 * al ciclo successivo) e si ritenta lo stesso task.
 * Esiti: 1 fatto · 0 budget run · -1 nessun worker · -2 errore provider · -3 cicli esauriti con
 * task pendenti · -4 deadlock di dipendenze. L'ultimo esito è in state/manage_exit.
 * Speculare a AiWorkerRuntime.WorkerManage (C#). */
static void wkr_manage_finish(gmb_worker_entry* e, double exit_code, long long dissents, const char* last_error)
{
    char buf[64], dir[2048];
    snprintf(dir, sizeof(dir), "%s/state", e->workspace); mkdir_p(dir);
    snprintf(buf, sizeof(buf), "%lld", (long long)exit_code);
    wkr_write_file(dir, "manage_exit.txt", buf);
    if (dissents > 0) { snprintf(buf, sizeof(buf), "%lld", dissents); wkr_write_file(dir, "dissents.txt", buf); }
    if (last_error && *last_error) {
        wkr_write_file(e->workspace, "last_error", last_error);
        wkr_write_file(dir, "last_error.txt", last_error);
    }
}

double gmb_worker_manage(double cycles, const char* require, const char* plan)
{
    gmb_worker_entry* e = wkr_current();
    if (!e) return -1;
    int has_audit = require && *require;
    char key[192];
    snprintf(key, sizeof(key), "worker:%s", e->name);
    if (e->budget > 0) gmb_ctx_set_budget(key, (double)e->budget);
    long long run_budget = (long long)gmb_ctx_budget(key);
    int n = cycles > 0 ? (int)cycles : 3;
    /* SCOPING su piano (`AI WORKER MANAGE "<plan>"`): il piano deve esistere ed essere APPROVATO
     * (un piano in bozza non è eseguibile) → esito esplicito -5; il set di lavoro è quello del piano. */
    char plan_tasks[1024];
    plan_tasks[0] = 0;
    if (plan && *plan) {
        if (!gmb_plan_is_approved(plan)) {
            char msg[192];
            snprintf(msg, sizeof(msg), "piano '%s' inesistente o non approvato (AI PLAN APPROVE)", plan);
            wkr_manage_finish(e, -5, 0, msg);
            return -5;
        }
        if (!gmb_plan_tasks(plan, plan_tasks, sizeof(plan_tasks))) {
            wkr_manage_finish(e, -5, 0, "piano inesistente");
            return -5;
        }
        if (!*plan_tasks) { wkr_manage_finish(e, 1, 0, ""); return 1; }   /* piano senza task */
    }
    long long run_spent = 0, dissents = 0;
    /* dimensioni locali: le costanti del taskgraph vivono nel suo .c (gmb_task_next_pending
     * scrive su buffer del chiamante — vedi gmb_runtime.h). */
    char task_id[128], task_desc[512];
    for (int cycle = 1; cycle <= n; cycle++) {
        int deadlock = 0;
        if (!gmb_task_next_pending_filtered(plan_tasks[0] ? plan_tasks : NULL,
                task_id, sizeof(task_id), task_desc, sizeof(task_desc), &deadlock)) {
            const char* msg = deadlock ? "task graph: dipendenze non soddisfatte (deadlock)" : "";
            wkr_manage_finish(e, deadlock ? -4 : 1, dissents, msg);
            return deadlock ? -4 : 1;
        }
        char task_label[128 + 512 + 8];
        if (task_desc[0]) snprintf(task_label, sizeof(task_label), "%s — %s", task_id, task_desc);
        else snprintf(task_label, sizeof(task_label), "%s", task_id);
        gmb_strbuf_t sb = gmb_strbuf_new();
        wkr_build_view(e, task_label, &sb);
        long long raw_len = (long long)strlen(sb.data ? sb.data : "");
        if (run_budget > 0) {
            long long remaining = run_budget - run_spent;
            if (remaining <= 0 || (remaining < raw_len && strcmp(gmb_ctx_get_policy(key), "error") == 0)) {
                char msg[192];
                snprintf(msg, sizeof(msg), "run budget exhausted: spent=%lld budget=%lld",
                         (long long)run_spent, (long long)run_budget);
                gmb_strbuf_free(&sb);
                g_rescued++;
                wkr_manage_finish(e, 0, dissents, msg);
                return 0;
            }
            gmb_ctx_set_budget(key, (double)remaining);
        }
        if (!gmb_ctx_set_view(key, sb.data ? sb.data : "")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "context overflow: %d > budget %lld",
                     (int)raw_len, (long long)gmb_ctx_budget(key));
            gmb_strbuf_free(&sb);
            g_rounds++; g_rescued++;
            wkr_manage_finish(e, 0, dissents, msg);
            return 0;
        }
        gmb_strbuf_free(&sb);
        gmb_string_t view = gmb_ctx_view(key);
        wkr_write_file(e->workspace, "context.md", view.data ? view.data : "");
        g_rounds++; run_spent += view.data ? (long long)strlen(view.data) : 0;
        g_spend += view.data ? (long long)strlen(view.data) : 0;
        char* answer = gmb_llm_complete(
            "Sei un worker isolato: rispondi SOLO con la vista fornita (task/plan/notes/solution). "
            "Non inventare contesto non presente nella vista.",
            view.data ? view.data : "");
        wkr_write_file(e->workspace, "solution", answer ? answer : "");
        { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
          wkr_write_file(sp, "solution.txt", answer ? answer : ""); }
        int is_err = answer && strncmp(answer, "Error:", 6) == 0;
        gmb_string_free(&view);
        if (has_audit && gmb_verify_require(require, answer ? answer : "")) {
            gmb_task_set_status(task_id, "done", answer ? answer : "");
            wkr_promote_best(e);
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
              wkr_write_file(sp, "verified.txt", "1"); wkr_write_file(sp, "manager_task.txt", task_id); }
            wkr_write_file(e->workspace, "last_error", "");
            free(answer);
            continue;   /* task successivo */
        }
        if (has_audit) {
            char buf[64];
            dissents++;
            gmb_task_set_status(task_id, "pending", "");
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
              wkr_write_file(sp, "dissent.txt", answer ? answer : "");
              snprintf(buf, sizeof(buf), "%lld", dissents); wkr_write_file(sp, "dissents.txt", buf); }
        }
        if (is_err) {
            g_rescued++;
            wkr_write_file(e->workspace, "last_error", answer ? answer : "");
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
              wkr_write_file(sp, "last_error.txt", answer ? answer : ""); }
            wkr_manage_finish(e, -2, dissents, answer ? answer : "");
            free(answer);
            return -2;
        }
        if (!has_audit) {
            gmb_task_set_status(task_id, "done", answer ? answer : "");
            wkr_promote_best(e);
            { char sp[2048]; snprintf(sp, sizeof(sp), "%s/state", e->workspace); mkdir_p(sp);
              wkr_write_file(sp, "manager_task.txt", task_id); }
        }
        free(answer);
    }
    {
        int deadlock = 0;
        char pid[128], pdesc[512];
        int more = gmb_task_next_pending_filtered(plan_tasks[0] ? plan_tasks : NULL,
                pid, sizeof(pid), pdesc, sizeof(pdesc), &deadlock);
        if (!more && !deadlock) { wkr_manage_finish(e, 1, dissents, ""); return 1; }
        char fail[256];
        snprintf(fail, sizeof(fail), "%s",
                 deadlock ? "task graph: dipendenze non soddisfatte (deadlock)"
                          : "manager loop: cicli esauriti con task ancora pending");
        g_rescued++;
        wkr_manage_finish(e, deadlock ? -4 : -3, dissents, fail);
        return deadlock ? -4 : -3;
    }
}

/* metriche run-level */
double gmb_worker_rounds(void) { return (double)g_rounds; }
double gmb_worker_rescued(void) { return (double)g_rescued; }
double gmb_worker_spend(void) { return (double)g_spend; }
