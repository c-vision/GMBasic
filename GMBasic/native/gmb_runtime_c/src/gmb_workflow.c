/* ============================================================================
 * gmb_workflow.c — runtime WORKFLOW (definizione/step/execute + STATUS/STEPS).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * ============================================================================ */

#include "gmb_runtime.h"
#include <string.h>

/* Registro di workflow: lista concatenata {nome, steps[], count, status, next}.
 * L'inserimento è in TESTA (più recente per primo) → gmb_workflow_find restituisce
 * l'omonimo PIÙ RECENTE, che è la semantica di sostituzione del Dictionary C#
 * (`_workflows[name] = wf`). Le funzioni che elencano (LIST/JSON) percorrono la lista
 * all'indietro per restituire l'ordine di INSERIMENTO, come `WorkflowServer.List()`. */
typedef struct gmb_wf_step {
    char* name;
    char* action;
    int retry;
    int timeoutMs;
} gmb_wf_step_t;

/* Riga dell'execution log (parità WorkflowDefinition.ExecutionLog : List<string>). */
typedef struct gmb_wf_log {
    char* text;
} gmb_wf_log_t;

typedef struct gmb_wf {
    char* name;
    gmb_wf_step_t* steps;
    int step_count;
    /* Stato (parità WorkflowServer C#): "" (in definizione) → "ready"/"empty" (END)
     * → "completed" (EXECUTE). `status == ""` è anche IsBuilding. */
    char* status;
    gmb_wf_log_t* log;      /* execution log (EXECUTE), NULL = vuoto */
    int log_count;
    int log_cap;
    struct gmb_wf* next;
} gmb_wf_t;

static gmb_wf_t* g_workflows = NULL;
static gmb_wf_t* g_current_wf = NULL;

static gmb_wf_t* gmb_workflow_find(const char* name);
static void gmb_wf_free(gmb_wf_t* w);
static void gmb_wf_log_clear(gmb_wf_t* w);
static void gmb_wf_log_add(gmb_wf_t* w, const char* text);
static int  gmb_workflow_remove_name(const char* name);

/* WORKFLOW name: apre una definizione (stato vuoto = in building). */
void gmb_workflow_begin(const char* name) {
    gmb_wf_t* wf = (gmb_wf_t*)calloc(1, sizeof(gmb_wf_t));
    if (!wf) return;
    wf->name = name ? strdup(name) : strdup("");
    wf->status = strdup("");
    wf->next = g_workflows;
    g_workflows = wf;
    g_current_wf = wf;
}

/* STEP name, action: aggiunge uno step al workflow corrente. */
void gmb_workflow_step(const char* name, const char* action) {
    gmb_workflow_step_ex(name, action, 0, 0);
}
void gmb_workflow_step_ex(const char* name, const char* action, int retry, int timeoutMs) {
    if (!g_current_wf) return;
    gmb_wf_step_t* ns = (gmb_wf_step_t*)realloc(g_current_wf->steps,
        (size_t)(g_current_wf->step_count + 1) * sizeof(gmb_wf_step_t));
    if (!ns) return;
    g_current_wf->steps = ns;
    g_current_wf->steps[g_current_wf->step_count].name = name ? strdup(name) : strdup("");
    g_current_wf->steps[g_current_wf->step_count].action = action ? strdup(action) : strdup("");
    g_current_wf->steps[g_current_wf->step_count].retry = retry;
    g_current_wf->steps[g_current_wf->step_count].timeoutMs = timeoutMs;
    g_current_wf->step_count++;
}

/* END WORKFLOW: chiude la definizione corrente (stato ready/empty, parità C#). */
void gmb_workflow_end(void) {
    if (!g_current_wf) return;
    free(g_current_wf->status);
    g_current_wf->status = strdup(g_current_wf->step_count > 0 ? "ready" : "empty");
    g_current_wf = NULL;
}

/* EXECUTE WORKFLOW name: esegue i passi in sequenza (i valori sono dichiarativi) e
 * marca lo stato "completed" (parità WorkflowServer.Execute del C#).
 * Sorgente unica: il corpo è gmb_workflow_execute_ok, questa è solo il wrapper void
 * per le statement BASIC (stessa semantica, nessun duplicato — §0bis). */
void gmb_workflow_execute(const char* name) {
    (void)gmb_workflow_execute_ok(name);
}

static gmb_wf_t* gmb_workflow_find(const char* name) {
    for (gmb_wf_t* w = g_workflows; w; w = w->next)
        if (strcmp(w->name, name) == 0) return w;
    return NULL;
}

/* STATUS("name") → stato del workflow (parità WorkflowServer.GetStatus): il valore del
 * campo status ("", "ready", "empty", "completed"); workflow assente → "". */
gmb_string_t gmb_workflow_status(const char* name) {
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return gmb_string_alloc("");
    return gmb_string_alloc(w->status ? w->status : "");
}

/* LIST() → "name1\nname2\n..." (parità IlRuntime.WorkflowList = string.Join("\n", nomi),
 * ordine di INSERIMENTO come WorkflowServer.List()). */
gmb_string_t gmb_workflow_list(void) {
    int n = 0;
    for (gmb_wf_t* w = g_workflows; w; w = w->next) n++;
    gmb_wf_t** v = (gmb_wf_t**)calloc((size_t)(n > 0 ? n : 1), sizeof(gmb_wf_t*));
    if (!v) return gmb_string_alloc("");
    int i = n;
    for (gmb_wf_t* w = g_workflows; w && i > 0; w = w->next) v[--i] = w;  /* inversa = inserimento */
    size_t total = 1;
    for (int k = 0; k < n; k++) total += strlen(v[k]->name) + 1;
    char* buf = (char*)malloc(total);
    if (!buf) { free(v); return gmb_string_alloc(""); }
    size_t off = 0;
    for (int k = 0; k < n; k++) {
        int w = snprintf(buf + off, total - off, "%s%s", v[k]->name, k + 1 < n ? "\n" : "");
        if (w > 0) off += (size_t)w;
    }
    free(v);
    gmb_string_t res = gmb_string_alloc(buf);
    free(buf);
    return res;
}

/* STEPS("name") → "name1,name2,..." (parità WorkflowServer.GetSteps: i nomi dei passi
 * separati da virgola). */
gmb_string_t gmb_workflow_steps(const char* name) {
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return gmb_string_alloc("");
    size_t total = 0;
    for (int i = 0; i < w->step_count; i++)
        total += strlen(w->steps[i].name) + 1;
    char* buf = (char*)malloc(total + 1);
    if (!buf) return gmb_string_alloc("");
    size_t off = 0;
    for (int i = 0; i < w->step_count; i++) {
        int n = snprintf(buf + off, total + 1 - off, "%s%s", w->steps[i].name, i + 1 < w->step_count ? "," : "");
        if (n > 0) off += (size_t)n;
    }
    gmb_string_t res = gmb_string_alloc(buf);
    free(buf);
    return res;
}

/* ============================================================================
 * SUPERFICE HTTP (webapp /api/workflows*) — porting del 2026-09-30.
 * Le funzioni sotto espongono a C gli equivalenti di Basic.Workflow.WorkflowServer
 * che mancavano (delete, execution log, API per-nome, JSON). Sorgente unica con
 * quanto sopra: riusano gmb_workflow_find / gmb_workflow_list / gmb_workflow_steps.
 * ========================================================================== */

/* Libera un workflow e la sua memoria (nessun double-free: scollega dalla lista). */
static void gmb_wf_free(gmb_wf_t* w) {
    if (!w) return;
    for (int i = 0; i < w->step_count; i++) {
        free(w->steps[i].name);
        free(w->steps[i].action);
    }
    free(w->steps);
    gmb_wf_log_clear(w);
    free(w->log);
    free(w->name);
    free(w->status);
    free(w);
}

static void gmb_wf_log_clear(gmb_wf_t* w) {
    if (!w || !w->log) return;
    for (int i = 0; i < w->log_count; i++) free(w->log[i].text);
    w->log_count = 0;
}

static void gmb_wf_log_add(gmb_wf_t* w, const char* text) {
    if (!w) return;
    if (w->log_count >= w->log_cap) {
        int nc = w->log_cap > 0 ? w->log_cap * 2 : 8;
        gmb_wf_log_t* nl = (gmb_wf_log_t*)realloc(w->log, (size_t)nc * sizeof(gmb_wf_log_t));
        if (!nl) return;
        w->log = nl;
        w->log_cap = nc;
    }
    w->log[w->log_count].text = strdup(text ? text : "");
    if (w->log[w->log_count].text) w->log_count++;
}

/* Rimuove dalla lista il PRIMO nodo con quel nome (lista in testa = più recente).
 * Ritorna 1 se rimosso. Usato da begin_checked (sostituzione omonima, parità
 * Dictionary) e da gmb_workflow_delete. */
static int gmb_workflow_remove_name(const char* name) {
    if (!name) return 0;
    gmb_wf_t* prev = NULL;
    for (gmb_wf_t* w = g_workflows; w; prev = w, w = w->next) {
        if (strcmp(w->name, name) != 0) continue;
        if (prev) prev->next = w->next; else g_workflows = w->next;
        if (g_current_wf == w) g_current_wf = NULL;
        gmb_wf_free(w);
        return 1;
    }
    return 0;
}

/* EXISTS("name") → 1 se il workflow è registrato (in definizione compresa). */
int gmb_workflow_exists(const char* name) {
    if (!name || !*name) return 0;
    return gmb_workflow_find(name) ? 1 : 0;
}

/* BEGIN_CHECKED("name") → 1 se la definizione è stata aperta, 0 se è già in corso
 * una definizione (parità WorkflowServer.BeginDefinition → la rotta HTTP risponde
 * 409). Un omonimo esistente viene SOSTITUITO conservando la sua POSIZIONE nel
 * registro (parità `_workflows[name] = wf` di un Dictionary: la chiave resta al suo
 * posto, l'ordine di List() non cambia) e con stato/passi/log azzerati. */
int gmb_workflow_begin_checked(const char* name) {
    if (!name || !*name) return 0;
    if (g_current_wf) return 0;                 /* già in definizione → 409 */
    gmb_wf_t* old = gmb_workflow_find(name);
    if (old) {
        /* riusa il nodo: azzera i contenuti, tasto indietro la definizione */
        for (int i = 0; i < old->step_count; i++) {
            free(old->steps[i].name);
            free(old->steps[i].action);
        }
        free(old->steps); old->steps = NULL; old->step_count = 0;
        gmb_wf_log_clear(old);
        free(old->status);
        old->status = strdup("");
        g_current_wf = old;
        return 1;
    }
    gmb_workflow_begin(name);
    return 1;
}


/* ADD_STEP_BY_NAME(wf, step, action, retry, timeoutMs) → 1 ok / 0 se il nome del
 * workflow è vuoto, lo step è vuoto, il workflow non esiste o è in costruzione
 * (parità WorkflowServer.AddStepToWorkflow → la rotta HTTP risponde 404).
 * Se il workflow è la definizione aperta, lo step entra nel builder corrente;
 * altrimenti, se lo stato è "empty"/"completed"/"failed" torna "ready". */
int gmb_workflow_add_step_by_name(const char* wf, const char* step,
                                  const char* action, int retry, int timeoutMs) {
    if (!wf || !*wf || !step || !*step) return 0;
    gmb_wf_t* w = gmb_workflow_find(wf);
    if (!w) return 0;
    if (w != g_current_wf && (!w->status || !*w->status)) return 0;  /* IsBuilding */
    gmb_wf_step_t* ns = (gmb_wf_step_t*)realloc(w->steps,
        (size_t)(w->step_count + 1) * sizeof(gmb_wf_step_t));
    if (!ns) return 0;
    w->steps = ns;
    w->steps[w->step_count].name = strdup(step);
    w->steps[w->step_count].action = strdup(action ? action : "");
    w->steps[w->step_count].retry = retry;
    w->steps[w->step_count].timeoutMs = timeoutMs;
    w->step_count++;
    if (w != g_current_wf && w->status &&
        (strcmp(w->status, "empty") == 0 || strcmp(w->status, "completed") == 0 ||
         strcmp(w->status, "failed") == 0)) {
        free(w->status);
        w->status = strdup("ready");
    }
    return 1;
}

/* DELETE("name") → 1 se rimosso, 0 se assente o in costruzione (parità
 * WorkflowServer.Delete → la rotta HTTP risponde 404). */
int gmb_workflow_delete(const char* name) {
    if (!name || !*name) return 0;
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return 0;
    if (w == g_current_wf) return 0;            /* in costruzione → non cancellare */
    return gmb_workflow_remove_name(name);
}

/* EXECUTE_OK("name") → 1 eseguito, 0 se assente o in costruzione (parità
 * WorkflowServer.Execute). Senza callback onStep (come la rotta HTTP) ogni passo
 * succeeds al primo tentativo e il log riceve una riga per passo, con lo stesso
 * testo del C#: "[i] nome: azione" e, se retry+1 > 1, il suffisso
 * "(retry=R, timeout=Tms, attempts=1)". */
int gmb_workflow_execute_ok(const char* name) {
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return 0;
    if (!w->status || !*w->status) return 0;    /* IsBuilding → false, stato intatto */
    free(w->status);
    w->status = strdup("running");
    gmb_wf_log_clear(w);
    for (int i = 0; i < w->step_count; i++) {
        int maxAttempts = w->steps[i].retry + 1 > 1 ? w->steps[i].retry + 1 : 1;
        char line[1024];
        if (maxAttempts > 1)
            snprintf(line, sizeof(line), "[%d] %s: %s (retry=%d, timeout=%dms, attempts=1)",
                     i, w->steps[i].name, w->steps[i].action,
                     w->steps[i].retry, w->steps[i].timeoutMs);
        else
            snprintf(line, sizeof(line), "[%d] %s: %s",
                     i, w->steps[i].name, w->steps[i].action);
        gmb_wf_log_add(w, line);
    }
    free(w->status);
    w->status = strdup("completed");
    return 1;
}

/* ── Helper JSON (parità System.Text.Json: chiavi e ordine dei campi del C#) ── */

/* Accoda `"key":"<escaped>"` in un buffer growable.
 * gmb_json_escape NON NUL-termina: va usata scrivendo direttamente nel buffer di
 * destinazione e facendo avanzare l'offset (convenzione di gmb_rag_search.c). */
static void wf_json_kv_str(char** buf, size_t* len, size_t* cap,
                           const char* key, const char* val) {
    size_t vlen = val ? strlen(val) : 0;
    size_t need = strlen(key) + vlen * 6 + 16;
    if (*len + need + 1 > *cap) {
        size_t nc = *cap > 0 ? *cap : 256;
        while (*len + need + 1 > nc) nc *= 2;
        char* nb = (char*)realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    *len += (size_t)snprintf(*buf + *len, *cap - *len, "\"%s\":\"", key);
    if (val) *len += gmb_json_escape(*buf + *len, *cap - *len, val);
    *len += (size_t)snprintf(*buf + *len, *cap - *len, "\"");
}

/* Accoda un array JSON di stringhe già presenti in memoria. */
static void wf_json_str_array(char** buf, size_t* len, size_t* cap,
                              char* const* items, int n) {
    *len += (size_t)snprintf(*buf + *len, *cap - *len, "[");
    for (int i = 0; i < n; i++) {
        size_t need = strlen(items[i]) * 6 + 8;
        if (*len + need + 1 > *cap) {
            size_t nc = *cap > 0 ? *cap : 256;
            while (*len + need + 1 > nc) nc *= 2;
            char* nb = (char*)realloc(*buf, nc);
            if (!nb) return;
            *buf = nb; *cap = nc;
        }
        *len += (size_t)snprintf(*buf + *len, *cap - *len, "%s\"", i ? "," : "");
        *len += gmb_json_escape(*buf + *len, *cap - *len, items[i]);
        *len += (size_t)snprintf(*buf + *len, *cap - *len, "\"");
    }
    *len += (size_t)snprintf(*buf + *len, *cap - *len, "]");
}


/* SUM JSON di un workflow: {"name":..,"status":..,"steps":["a","b"]} — la stessa
 * forma che il C# produce con `new { name, status, steps = … }`. */
static char* wf_summary_json(const gmb_wf_t* w) {
    size_t cap = 256, len = 0;
    char* b = (char*)malloc(cap);
    if (!b) return NULL;
    b[0] = 0;
    len = (size_t)snprintf(b, cap, "{");
    wf_json_kv_str(&b, &len, &cap, "name", w->name);
    len += (size_t)snprintf(b + len, cap - len, ",");
    wf_json_kv_str(&b, &len, &cap, "status", w->status ? w->status : "");
    len += (size_t)snprintf(b + len, cap - len, ",");
    len += (size_t)snprintf(b + len, cap - len, "\"steps\":");
    {
        char* names[64];
        int n = w->step_count < 64 ? w->step_count : 64;
        for (int i = 0; i < n; i++) names[i] = w->steps[i].name;
        wf_json_str_array(&b, &len, &cap, names, n);
    }
    len += (size_t)snprintf(b + len, cap - len, "}");
    return b;
}


/* LIST_JSON() → [{"name":..,"status":..,"steps":[..]}, …] in ordine di inserimento
 * (parità `WorkflowServer.List()` → Select(new { name, status, steps })). */
char* gmb_workflow_list_json(void) {
    int n = 0;
    for (gmb_wf_t* w = g_workflows; w; w = w->next) n++;
    gmb_wf_t** v = (gmb_wf_t**)calloc((size_t)(n > 0 ? n : 1), sizeof(gmb_wf_t*));
    if (!v) return strdup("[]");
    int i = n;
    for (gmb_wf_t* w = g_workflows; w && i > 0; w = w->next) v[--i] = w;
    size_t cap = 256, len = 0;
    char* b = (char*)malloc(cap);
    if (!b) { free(v); return strdup("[]"); }
    b[0] = 0;
    len = (size_t)snprintf(b, cap, "[");
    for (int k = 0; k < n; k++) {
        char* one = wf_summary_json(v[k]);
        if (!one) continue;
        size_t ol = strlen(one);
        if (len + ol + 3 > cap) {
            size_t nc = cap;
            while (len + ol + 3 > nc) nc *= 2;
            char* nb = (char*)realloc(b, nc);
            if (!nb) { free(one); break; }
            b = nb; cap = nc;
        }
        len += (size_t)snprintf(b + len, cap - len, "%s%s", k ? "," : "", one);
        free(one);
    }
    if (len + 3 > cap) {
        char* nb = (char*)realloc(b, cap + 3);
        if (nb) { b = nb; cap += 3; }
    }
    snprintf(b + len, cap - len, "]");
    free(v);
    return b;
}

/* SUMMARY_JSON("name") → {"name":..,"status":..,"steps":[..]} o NULL se assente. */
char* gmb_workflow_summary_json(const char* name) {
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return NULL;
    return wf_summary_json(w);
}

/* STEPS_DETAIL_JSON("name") → [{"Name":..,"Action":..,"RetryCount":..,"TimeoutMs":..}]
 * con le MAIUSCOLE dei nomi di membro: è la serializzazione C# del tipo anonimo
 * `new { s.Name, s.Action, s.RetryCount, s.TimeoutMs }` usata da POST …/steps. */
char* gmb_workflow_steps_detail_json(const char* name) {
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return NULL;
    size_t cap = 256, len = 0;
    char* b = (char*)malloc(cap);
    if (!b) return NULL;
    b[0] = 0;
    len = (size_t)snprintf(b, cap, "[");
    for (int i = 0; i < w->step_count; i++) {
        const char* sn = w->steps[i].name ? w->steps[i].name : "";
        const char* sa = w->steps[i].action ? w->steps[i].action : "";
        size_t need = (strlen(sn) + strlen(sa)) * 6 + 96;
        if (len + need + 1 > cap) {
            size_t nc = cap > 0 ? cap : 256;
            while (len + need + 1 > nc) nc *= 2;
            char* nb = (char*)realloc(b, nc);
            if (!nb) return b;
            b = nb; cap = nc;
        }
        len += (size_t)snprintf(b + len, cap - len, "%s{\"Name\":\"", i ? "," : "");
        len += gmb_json_escape(b + len, cap - len, sn);
        len += (size_t)snprintf(b + len, cap - len, "\",\"Action\":\"");
        len += gmb_json_escape(b + len, cap - len, sa);
        len += (size_t)snprintf(b + len, cap - len, "\",\"RetryCount\":%d,\"TimeoutMs\":%d}",
                                w->steps[i].retry, w->steps[i].timeoutMs);
    }
    len += (size_t)snprintf(b + len, cap - len, "]");
    return b;
}


/* LOG_JSON("name") → ["[0] step: action", …] (parità ExecutionLog serializzato da
 * System.Text.Json). Workflow assente → NULL (la rotta risponde 404). */
char* gmb_wf_log_json(const char* name) {
    gmb_wf_t* w = gmb_workflow_find(name);
    if (!w) return NULL;
    size_t cap = 256, len = 0;
    char* b = (char*)malloc(cap);
    if (!b) return NULL;
    b[0] = 0;
    len = (size_t)snprintf(b, cap, "[");
    for (int i = 0; i < w->log_count; i++) {
        const char* t = w->log[i].text ? w->log[i].text : "";
        size_t need = strlen(t) * 6 + 8;
        if (len + need + 1 > cap) {
            size_t nc = cap > 0 ? cap : 256;
            while (len + need + 1 > nc) nc *= 2;
            char* nb = (char*)realloc(b, nc);
            if (!nb) return b;
            b = nb; cap = nc;
        }
        len += (size_t)snprintf(b + len, cap - len, "%s\"", i ? "," : "");
        len += gmb_json_escape(b + len, cap - len, t);
        len += (size_t)snprintf(b + len, cap - len, "\"");
    }
    len += (size_t)snprintf(b + len, cap - len, "]");
    return b;
}

