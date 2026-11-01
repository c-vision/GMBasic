/* gmb_agent_loop.c — implementazione del loop Brain→Azione.
 * Ogni step: brain(transcript) → se risponde con un AgentAction JSON viene eseguita
 * (gmb_agent_action_run) e l'osservazione torna nel transcript; se risponde con testo
 * libero (nessuna azione) è la risposta finale e il loop termina. Policy capability
 * applicata sul nome tool prima dell'esecuzione. */
#include "gmb_agent_loop.h"
#include "gmb_runtime.h"   /* gmb_opt_env */
#include "gmb_agent_action.h"
#include "gmb_brain_http.h"
#include "gmb_code_exec.h"
#include "gmb_brain_mlx.h"
#include "gmb_cua_runtime.h"
#include "capability.h"
#include "tool_registry.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* brain GMBasic registrato dall'host/embedding (mai un server esterno di default) */
static gmb_brain_fn g_brain = NULL;
static void* g_brain_ctx = NULL;

/* §4.10 esposizione agenti (lato C): SEAM verso il compilatore per i tool `code.*` (grafo
 * semantico del codice). Il runtime agenti NON linka il compilatore: l'host (VM/C, C99Native)
 * registra qui `gmb_semquery_file`. Assente → errore ESPLICITO (mai no-op). */
static gmb_agent_code_query_fn g_code_query = NULL;
void gmb_agent_code_query_set(gmb_agent_code_query_fn fn) { g_code_query = fn; }

int gmb_agent_brain_register(gmb_brain_fn brain, void* ctx) {
    if (!brain) return -1;
    g_brain = brain;
    g_brain_ctx = ctx;
    return 0;
}
void gmb_agent_brain_clear(void) { g_brain = NULL; g_brain_ctx = NULL; }
int gmb_agent_brain_is_set(void) { return g_brain != NULL; }

/* Directory del modello per il brain MLX: GMB_MLX_MODEL_DIR (env) oppure il modello locale MLX
 * (`.gguf`/cartella) dell'provider COMPLETION attivo registrato da `AI REGISTER`+`AI SET ACTIVE`
 * — parità col routing host C# (MlxLlmCompleter). Weak: se gmb_runtime_c non è linkato resta
 * NULL (nessuna dipendenza forte). */
extern const char* gmb_provider_active_model_path(void) __attribute__((weak));
static const char* mlx_model_dir_effective(void) {
    const char* mdir = gmb_opt_env("GMB_MLX_MODEL_DIR");
    if (mdir && *mdir) return mdir;
    if (gmb_provider_active_model_path) {
        const char* p = gmb_provider_active_model_path();
        if (p && *p) return p;
    }
    return NULL;
}

/* Ask one-shot al brain registrato (testo libero, per CodeGen/UX): invoca g_brain una volta
   con `prompt` e riempie out. Se nessun brain e' registrato ne' MLX configurato -> errore
   esplicito (mai no-op). Ritorna 0 ok. */
int gmb_brain_ask_sync(const char* prompt, char* out, size_t cap) {
    if (!out || cap == 0) return -1;
    out[0] = 0;
    if (!prompt) { snprintf(out, cap, "error: prompt vuoto"); return -1; }
    if (g_brain)
        return g_brain(g_brain_ctx, prompt, out, cap);
    const char* mdir = mlx_model_dir_effective();
    if (mdir && *mdir) {
        if (gmb_brain_mlx_auto(mdir, out, cap) != 0)
            return -1;
        return g_brain ? g_brain(g_brain_ctx, prompt, out, cap) : -1;
    }
    snprintf(out, cap,
        "error: nessun brain registrato — GMB_MLX_MODEL_DIR o gmb_agent_brain_register / GMB_BRAIN_URL.");
    return -1;
}

static void agent_logf(char* log, size_t cap, const char* fmt, ...) {
    size_t p = strlen(log);
    if (p + 16 >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log + p, cap - p, fmt, ap);
    va_end(ap);
}

/* estrae il nome tool (chiave "tool"/"name") dal JSON della risposta per la policy */
static void extract_tool_name(const char* reply, char* out, size_t cap) {
    out[0] = 0;
    const char* key = strstr(reply, "\"tool\"");
    if (!key) key = strstr(reply, "\"name\"");
    if (!key) return;
    const char* colon = strchr(key, ':');
    if (!colon) return;
    const char* v = colon + 1;
    while (*v && (*v == ' ' || *v == '"')) v++;
    const char* s = v;
    while (*v && *v != '"') v++;
    size_t n = (size_t)(v - s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n);
    out[n] = 0;
}

static void trans_add(char* t, size_t tcap, int* tlen, const char* fmt, const char* arg) {
    size_t cur = (size_t)(*tlen);
    if (cur >= tcap) return;
    int w;
    if (arg)
        w = snprintf(t + cur, tcap - cur, fmt, arg);
    else
        w = snprintf(t + cur, tcap - cur, "%s", fmt);
    if (w > 0) *tlen += w;
}

int gmb_agent_loop_run(gmb_brain_fn brain, void* ctx, const char* agent, const char* goal,
                       int max_steps, char* log, size_t logcap) {
    if (!brain || !log || logcap == 0) return -1;
    log[0] = 0;
    char transcript[32768];
    /* Modalità brain (TOOL|COMPUTER|CODE): GMB_AGENT_MODE o default TOOL.
       TOOL = azioni su tool (browser/computer/ui/files); COMPUTER = solo computer/ui;
       CODE = rispondere con un programma GMBASIC come testo. */
    const char* mode = gmb_opt_env("GMB_AGENT_MODE");
    if (!mode || !*mode) mode = "TOOL";
    const char* mode_rule =
        (strcasecmp(mode, "COMPUTER") == 0) ?
            "Usa SOLO azioni COMPUTER/UI (coordinate o testi UI). Mai tool browser per il compito."
        : (strcasecmp(mode, "CODE") == 0) ?
            "Rispondi SOLO con un programma GMBASIC (codice BASIC, no JSON action)."
        : "Puoi usare azioni tool (browser/computer/ui).";
    int tlen = snprintf(transcript, sizeof(transcript),
        "Compito: %s\n\nMODALITA': %s\nRegola: %s\n"
        "Rispondi SEMPRE o con un'azione {\"action\":{\"tool\":\"...\",\"args\":{...}}}, "
        "oppure con la risposta finale in testo libero.\n\nStorico:\n",
        goal ? goal : "", mode, mode_rule);
    if (tlen < 0) return -1;

    for (int step = 1; step <= max_steps; step++) {
        char reply[8192];
        if (brain(ctx, transcript, reply, sizeof(reply)) != 0) {
            agent_logf(log, logcap, "[step %d] brain errore\n", step);
            return -1;
        }
        if (!reply[0]) break;

        /* transcript limitato: quando quasi pieno compatta tenendo la coda */
        size_t tcur = strlen(transcript);
        if (tcur + strlen(reply) + 600 >= sizeof(transcript)) {
            size_t keep = 16384;
            size_t from = tcur > keep ? tcur - keep : 0;
            memmove(transcript, transcript + from, tcur - from + 1);
            tlen = (int)strlen(transcript);
        }
        trans_add(transcript, sizeof(transcript), &tlen, "assistant: %s\n", reply);

        /* MODALITA' CODE: il brain ha risposto con un programma GMBASIC → eseguilo
           con gmb_code_exec (mondo C) e riporta l'output come risposta finale. */
        if (strcasecmp(mode, "CODE") == 0) {
            /* estrai un eventuale blocco fenced (```basic ... ``` / ``` ... ```) */
            char prog[16384];
            const char* fb = strstr(reply, "```");
            if (fb) {
                const char* f0 = fb + 3;
                const char* nl0 = strchr(f0, '\n');
                const char* start = nl0 ? nl0 + 1 : f0;
                const char* fe = strstr(start, "```");
                size_t n = fe ? (size_t)(fe - start) : strlen(start);
                if (n >= sizeof(prog)) n = sizeof(prog) - 1;
                memcpy(prog, start, n); prog[n] = 0;
            } else { snprintf(prog, sizeof(prog), "%s", reply); }
            char exec_out[16384] = "";
            if (gmb_code_exec(prog, exec_out, sizeof(exec_out)) == 0) {
                agent_logf(log, logcap, "[step %d] CODE eseguito: %s\n", step, exec_out[0] ? exec_out : "(nessun output)");
                return 0;
            }
            agent_logf(log, logcap, "[step %d] CODE errore: %s\n", step, exec_out);
            return -1;
        }

        char obs[4096];
        /* POLICY capability PRIMA dell'esecuzione: un tool negato non viene mai tentato */
        char tool[128] = "";
        extract_tool_name(reply, tool, sizeof(tool));
        if (tool[0] && agent && *agent &&
            gmb_capability_check(agent, tool) == GMB_CAP_DENY) {
            snprintf(obs, sizeof(obs), "{\"obs\":\"permission denied: %s\"}", tool);
            agent_logf(log, logcap, "[step %d] DENY tool=%s\n", step, tool);
            trans_add(transcript, sizeof(transcript), &tlen, "tool-error: %s\n", obs);
            continue;
        }

        int rc = gmb_agent_action_run(reply, obs, sizeof(obs));
        if (rc == 1) {
            agent_logf(log, logcap, "[step %d] finale: %s\n", step, reply);
            return 0;
        }
        if (rc == -1) {
            agent_logf(log, logcap, "[step %d] azione fallita: %s\n", step, obs);
            trans_add(transcript, sizeof(transcript), &tlen, "tool-error: %s\n", obs);
            continue;
        }
        agent_logf(log, logcap, "[step %d] action=%s -> %s\n", step, tool, obs);
        trans_add(transcript, sizeof(transcript), &tlen, "observation: %s\n", obs);
    }
    agent_logf(log, logcap, "max_steps raggiunto (%d)\n", max_steps);
    return 1;
}

/* Fixture completer deterministico (parità FixtureLlmCompleter.Complete C#): usato dal loop
 * CUA (AI.AGENTRUN) quando non c'e' un brain reale configurato — NON un errore. Fonte UNICA:
 * gmb_fixture_llm_complete (runtime condiviso, gmb_agent.c). */
static int gmb_agent_fixture_complete(void* ctx, const char* prompt, char* out, size_t cap) {
    (void)ctx;
    if (!out || cap == 0) return -1;
    gmb_fixture_llm_complete(prompt, out, cap);
    return 0;
}

int gmb_agent_run_goal(const char* agent, const char* goal, int max_steps, char* log, size_t logcap) {
    if (!log || logcap == 0) return -1;
    log[0] = 0;
    /* Parità oracolo IL (IlRuntime.AgentRun) e handler VM/C gmb_bf_agent_run_c1387. */
    if (!goal || !*goal) { snprintf(log, logcap, "AGENT.RUN: goal mancante"); return -1; }
    if (g_brain)
        return gmb_agent_loop_run(g_brain, g_brain_ctx, agent, goal, max_steps, log, logcap);

    /* brain MLX embedded (gmb_host_ai_mlx): GMB_MLX_MODEL_DIR o modello locale dell'active
     * provider registrato da AI REGISTER (auto-wire, parità col routing host C#). */
    const char* mdir = mlx_model_dir_effective();
    if (mdir && *mdir) {
        if (gmb_brain_mlx_auto(mdir, log, logcap) != 0) return -1;
        return gmb_agent_loop_run(g_brain, g_brain_ctx, agent, goal, max_steps, log, logcap);
    }
    /* fallback SOLO se l'operatore ha configurato esplicitamente GMB_BRAIN_URL */
    const char* url = gmb_opt_env("GMB_BRAIN_URL");
    if (url && *url) {
        gmb_brain_http_t* hb = gmb_brain_http_new(url, NULL);
        if (!hb) { snprintf(log, logcap, "brain HTTP: allocazione fallita"); return -1; }
        int rc = gmb_agent_loop_run(gmb_brain_http_complete, hb, agent, goal, max_steps, log, logcap);
        gmb_brain_http_free(hb);
        return rc;
    }

    /* Nessun brain (registrato/MLX/HTTP): errore ESPLICITO, parità con l'oracolo IL
     * (IlRuntime.AgentRun → "brain non disponibile..."). AGENT.RUN NON usa il fixture: il
     * fixture appartiene al loop CUA di AI.AGENTRUN (gmb_agent_run_cua). */
    snprintf(log, logcap,
        "brain non disponibile: registra il completer embedding GMBasic "
        "(gmb_agent_brain_register) o configura GMB_BRAIN_URL.");
    return -1;
}

/* ============================================================================
 * AI.AGENTRUN(name, goal[, rounds]) — loop CUA nativo (parità AgentServer.AskCua +
 * CuaAgentLoop + ToolCallParser + FixtureLlmCompleter del C#). Distinto da AGENT.RUN
 * (gmb_agent_run_goal): l'oracolo C# mappa AI.AGENTRUN su AgentRunCua/AskCua (tool_calls
 * JSON), non sul loop azione {"action":...}.
 * ============================================================================ */

/* Istruzioni tool CUA (parità CuaAgentLoop.ToolInstructions C#). */
static const char* gmb_cua_tool_instructions(void) {
    return "Strumenti disponibili. Per usarne uno rispondi SOLO con il JSON "
           "{\"tool_calls\":[{\"name\":\"<tool>\",\"arguments\":{...}}]}, altrimenti rispondi "
           "con la risposta finale in testo libero.\n"
           "- browser.navigate: Navigate the browser to a URL\n"
           "- browser.snapshot: Snapshot (DOM JSON) of the current page\n"
           "- browser.evaluate: Evaluate a JavaScript expression in the page\n"
           "- browser.click: Click an element by CSS selector\n"
           "- browser.fill: Fill a form field by CSS selector\n"
           "- tool.search: Search available tools by keyword\n"
           "- ui.find: Find a native UI element by text (richiede VM/C: AX)\n"
           "- computer.screenshot: Take a screenshot (richiede VM/C)\n"
           "Esempio: {\"tool_calls\":[{\"name\":\"browser.navigate\","
           "\"arguments\":{\"url\":\"https://example.com\"}}]}";
}

#define GMB_CUA_MAX_CALLS 32
typedef struct { char name[128]; char args[8192]; } gmb_cua_call_t;

/* Copia `s` (trim spazi) in dst. */
static void cua_copy_trim(char* dst, size_t cap, const char* s) {
    if (!dst || cap == 0) return;
    dst[0] = 0;
    if (!s) return;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) n--;
    if (n >= cap) n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = 0;
}

/* Estrae le tool_calls (parità ToolCallParser.TryParse C#): accetta un oggetto JSON
 * {"tool_calls":[{"name":..,"arguments":..}, ...]}, anche in un blocco ```...```; supporta
 * il formato OpenAI {"function":{"name":..,"arguments":..}}. Ritorna il numero di call. */
static int gmb_cua_parse_calls(const char* answer, gmb_cua_call_t* calls, int max) {
    if (!answer || !calls || max <= 0) return 0;
    char buf[65536];
    const char* t = answer;
    while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r') t++;
    if (strncmp(t, "```", 3) == 0) {
        const char* nl = strchr(t, '\n');
        const char* fe = nl ? strstr(nl, "```") : NULL;
        if (nl && fe && fe > nl) {
            size_t n = (size_t)(fe - (nl + 1));
            if (n >= sizeof(buf)) n = sizeof(buf) - 1;
            memcpy(buf, nl + 1, n); buf[n] = 0;
            t = buf;
            while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r') t++;
        }
    }
    if (*t != '{') return 0;
    gmb_val_t root = gmb_json_parse(t);
    int n = 0;
    for (int i = 0; i < max; i++) {
        char path[64];
        snprintf(path, sizeof(path), "tool_calls[%d].name", i);
        gmb_string_t nm = gmb_json_path_text(root, path);
        if (!nm.data || !nm.data[0]) {
            gmb_string_free(&nm);
            snprintf(path, sizeof(path), "tool_calls[%d].function.name", i);
            nm = gmb_json_path_text(root, path);
        }
        if (!nm.data || !nm.data[0]) { gmb_string_free(&nm); break; }
        cua_copy_trim(calls[n].name, sizeof(calls[n].name), nm.data);
        gmb_string_free(&nm);
        snprintf(path, sizeof(path), "tool_calls[%d].arguments", i);
        gmb_string_t ar = gmb_json_path_text(root, path);
        if (!ar.data || !ar.data[0]) {
            gmb_string_free(&ar);
            snprintf(path, sizeof(path), "tool_calls[%d].function.arguments", i);
            ar = gmb_json_path_text(root, path);
        }
        cua_copy_trim(calls[n].args, sizeof(calls[n].args), ar.data);
        gmb_string_free(&ar);
        if (!calls[n].args[0]) snprintf(calls[n].args, sizeof(calls[n].args), "{}");
        n++;
    }
    return n;
}

/* Esegue una tool CUA (parità CuaAgentLoop.Execute C#). */
static void gmb_cua_execute(const char* name, const char* args_json, char* out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    gmb_val_t root = gmb_json_parse(args_json && *args_json ? args_json : "{}");
    gmb_string_t a1 = gmb_json_path_text(root, "url");
    gmb_string_t a2 = gmb_json_path_text(root, "expression");
    gmb_string_t a3 = gmb_json_path_text(root, "selector");
    gmb_string_t a4 = gmb_json_path_text(root, "value");
    gmb_string_t a5 = gmb_json_path_text(root, "query");
    gmb_string_t a6 = gmb_json_path_text(root, "text");
    gmb_string_t a7 = gmb_json_path_text(root, "path");
    gmb_string_t r;
    if (strcmp(name, "browser.navigate") == 0)
        r = gmb_cua_browser_navigate(a1.data ? a1.data : "");
    else if (strcmp(name, "browser.snapshot") == 0)
        r = gmb_cua_browser_snapshot();
    else if (strcmp(name, "browser.evaluate") == 0)
        r = gmb_cua_browser_evaluate(a2.data ? a2.data : "");
    else if (strcmp(name, "browser.click") == 0)
        r = gmb_cua_browser_click(a3.data ? a3.data : "");
    else if (strcmp(name, "browser.fill") == 0)
        r = gmb_cua_browser_fill(a3.data ? a3.data : "", a4.data ? a4.data : "");
    else if (strcmp(name, "tool.search") == 0)
        r = gmb_cua_tool_search(a5.data ? a5.data : "");
    else if (strcmp(name, "ui.find") == 0)
        r = gmb_cua_ui_find(a6.data ? a6.data : "");
    else if (strcmp(name, "computer.screenshot") == 0)
        r = gmb_cua_computer_screenshot(a7.data ? a7.data : "");
    else if (name && strncmp(name, "graph.", 6) == 0) {
        /* Tool graph.* sul knowledge graph C (gmb_kg): query e path sono implementati sui
         * algoritmi del grafo condiviso; gli altri tool dell'oracolo C# (neighbors/bfs/dfs/
         * dijkstra/hubs/stats) non hanno ancora un equivalente C → errore ESPLICITO (mai un
         * risultato silenzioso). */
        gmb_string_t gq = gmb_json_path_text(root, "query");
        gmb_string_t gf = gmb_json_path_text(root, "from");
        gmb_string_t gt = gmb_json_path_text(root, "to");
        if (strcmp(name, "graph.query") == 0) {
            char* r0 = gmb_kg_query(gq.data ? gq.data : "");
            r = gmb_string_alloc(r0 ? r0 : "{\"error\":\"query vuota\"}");
            free(r0);
        } else if (strcmp(name, "graph.path") == 0) {
            char spec[600];
            snprintf(spec, sizeof spec, "path %s->%s", gf.data ? gf.data : "", gt.data ? gt.data : "");
            char* r0 = gmb_kg_query(spec);
            r = gmb_string_alloc(r0 ? r0 : "{\"error\":\"path non risolto\"}");
            free(r0);
        } else {
            r = gmb_string_alloc("error: tool graph.* non disponibile su questo host C "
                                 "(neighbors/bfs/dfs/dijkstra/hubs/stats richiedono il brain del runtime managed)");
        }
        gmb_string_free(&gq); gmb_string_free(&gf); gmb_string_free(&gt);
    }
    else if (name && strncmp(name, "code.", 5) == 0) {
        /* §4.10 tool code.* (grafo semantico del codice): via seam verso il compilatore. */
        if (g_code_query == NULL) {
            r = gmb_string_alloc("error: tool code.* non disponibile su questo host C (seam gmb_agent_code_query_set non registrato)");
        } else {
            gmb_string_t path = gmb_json_path_text(root, "path");
            gmb_string_t nm = gmb_json_path_text(root, "name");
            gmb_string_t qy = gmb_json_path_text(root, "query");
            gmb_string_t dp = gmb_json_path_text(root, "depth");
            const char* verb = NULL; const char* arg = NULL; int depth = -1;
            if (strcmp(name, "code.symbol") == 0) { verb = "SYMBOL"; arg = nm.data; }
            else if (strcmp(name, "code.callmap") == 0) { verb = "CALLMAP"; arg = nm.data; }
            else if (strcmp(name, "code.impact") == 0) { verb = "IMPACT"; arg = nm.data; depth = (dp.data && atoi(dp.data) > 0) ? atoi(dp.data) : 32; }
            else if (strcmp(name, "code.context") == 0) { verb = "CONTEXT"; arg = nm.data; depth = (dp.data && atoi(dp.data) > 0) ? atoi(dp.data) : 1; }
            else if (strcmp(name, "code.findsymbol") == 0) { verb = "FINDSYMBOL"; arg = (qy.data && *qy.data) ? qy.data : nm.data; }
            else if (strcmp(name, "code.fileoutline") == 0) {
                verb = "FILEOUTLINE"; const char* pp = (path.data && *path.data) ? path.data : ""; const char* bb = strrchr(pp, '/'); arg = bb ? bb + 1 : pp;
            }
            else if (strcmp(name, "code.repomap") == 0) { verb = "REPOMAP"; arg = NULL; }
            char res[8192];
            if (verb == NULL) r = gmb_string_alloc("error: tool code non riconosciuto");
            else if (path.data == NULL || !*path.data) r = gmb_string_alloc("error: manca 'path' (file .bas)");
            else if (g_code_query(path.data, verb, arg, depth, res, sizeof res) == 0) r = gmb_string_alloc(res);
            else r = gmb_string_alloc("error: query semantica fallita (file non compilabile?)");
            gmb_string_free(&path); gmb_string_free(&nm); gmb_string_free(&qy); gmb_string_free(&dp);
        }
    }
    else {
        /* Tool del registry (es. registrato dall'host via gmb_tool_registry_register_simple):
         * eseguito dall'executor esterno se presente (seam C, parità IMcpToolDispatcher C#).
         * Senza executor → errore ESPLICITO, mai no-op/stub. */
        char texec[8192];
        int rx = gmb_tool_registry_execute(name, args_json, texec, sizeof(texec));
        if (rx == 1) {
            r = gmb_string_alloc(texec);
        } else {
            char m[256];
            if (rx == 0)
                snprintf(m, sizeof(m),
                    "error: tool '%s' non disponibile su questo host C (usa AGENT.RUN su VM/C per "
                    "ui.*/computer.*)", name ? name : "");
            else
                snprintf(m, sizeof(m),
                    "error: nessun executor registrato per il tool '%s' "
                    "(gmb_tool_registry_set_executor)", name ? name : "");
            r = gmb_string_alloc(m);
        }
    }
    snprintf(out, cap, "%s", r.data ? r.data : "");
    gmb_string_free(&r);
    gmb_string_free(&a1); gmb_string_free(&a2); gmb_string_free(&a3); gmb_string_free(&a4);
    gmb_string_free(&a5); gmb_string_free(&a6); gmb_string_free(&a7);
}

/* Loop CUA (parità AgentServer.AskCua): brain → tool_calls → esecuzione → osservazione.
 * Ritorna la risposta finale (malloc'd, mai NULL). */
static char* gmb_agent_run_cua_loop(gmb_brain_fn brain, void* ctx, const char* name,
                                    const char* goal, int rounds, char* transcript, size_t tcap) {
    (void)name; (void)goal;
    if (!brain || !transcript || tcap == 0) return strdup("AI.AGENTRUN: memoria");
    if (rounds <= 0) rounds = 5;
    char last[65536]; last[0] = 0;
    char* attempted_key[GMB_CUA_MAX_CALLS * 4];
    int attempted_cnt[GMB_CUA_MAX_CALLS * 4];
    int attempted_n = 0;
    for (int round = 0; round < rounds; round++) {
        char answer[65536];
        answer[0] = 0;
        if (brain(ctx, transcript, answer, sizeof(answer)) != 0)
            return strdup("error: brain non disponibile per AI.AGENTRUN");
        if (answer[0] && strcmp(answer, last) == 0)
            return strdup("Loop rilevato: risposta identica senza progresso. Interrompo.");
        snprintf(last, sizeof(last), "%s", answer);
        gmb_cua_call_t calls[GMB_CUA_MAX_CALLS];
        int ncalls = gmb_cua_parse_calls(answer, calls, GMB_CUA_MAX_CALLS);
        if (ncalls == 0) return strdup(answer);
        size_t tl = strlen(transcript);
        if (tl + strlen(answer) + 16 < tcap)
            snprintf(transcript + tl, tcap - tl, "\nassistant: %s", answer);
        for (int i = 0; i < ncalls; i++) {
            char key[8320];
            snprintf(key, sizeof(key), "%s|%s", calls[i].name, calls[i].args);
            int idx = -1;
            for (int k = 0; k < attempted_n; k++)
                if (strcmp(attempted_key[k], key) == 0) { idx = k; break; }
            if (idx < 0) {
                idx = attempted_n < (int)(sizeof(attempted_key)/sizeof(attempted_key[0])) ? attempted_n++ : 0;
                attempted_key[idx] = strdup(key);
                attempted_cnt[idx] = 0;
            }
            attempted_cnt[idx]++;
            if (attempted_cnt[idx] >= 3) {
                char m[256];
                snprintf(m, sizeof(m),
                    "Loop rilevato: l'azione '%s' tentata 3 volte. Interrompo.", calls[i].name);
                return strdup(m);
            }
            char obs[8192];
            gmb_cua_execute(calls[i].name, calls[i].args, obs, sizeof(obs));
            tl = strlen(transcript);
            if (tl + strlen(calls[i].name) + strlen(obs) + 32 < tcap)
                snprintf(transcript + tl, tcap - tl, "\ntool: [%s] %s", calls[i].name, obs);
        }
    }
    char m[128];
    snprintf(m, sizeof(m), "Tool-calling loop exceeded %d rounds.", rounds);
    return strdup(m);
}

/* AI.AGENTRUN(name, goal[, rounds]): risolve il brain (registrato/MLX/HTTP/fixture) e gira il
 * loop CUA. Ritorna malloc'd (mai NULL). */
char* gmb_agent_run_cua(const char* name, const char* goal, int rounds) {
    if (!goal || !*goal) return strdup("AI.AGENTRUN: goal mancante");
    if (rounds <= 0) rounds = 5;
    /* transcript = "user: <goal>\n\n<istruzioni tool>" (parità BuildTranscriptFor + userMsg). */
    size_t cap = 65536;
    char* transcript = (char*)malloc(cap);
    if (!transcript) return strdup("AI.AGENTRUN: memoria");
    snprintf(transcript, cap, "user: %s\n\n%s", goal, gmb_cua_tool_instructions());
    const char* mdir = mlx_model_dir_effective();
    const char* url = gmb_opt_env("GMB_BRAIN_URL");
    char* out;
    if (g_brain)
        out = gmb_agent_run_cua_loop(g_brain, g_brain_ctx, name, goal, rounds, transcript, cap);
    else if (mdir && *mdir) {
        char err[1024]; err[0] = 0;
        if (gmb_brain_mlx_auto(mdir, err, sizeof(err)) != 0) {
            free(transcript);
            return strdup(err[0] ? err : "error: brain MLX non disponibile");
        }
        out = gmb_agent_run_cua_loop(g_brain, g_brain_ctx, name, goal, rounds, transcript, cap);
    } else if (url && *url) {
        gmb_brain_http_t* hb = gmb_brain_http_new(url, NULL);
        if (!hb) { free(transcript); return strdup("brain HTTP: allocazione fallita"); }
        out = gmb_agent_run_cua_loop(gmb_brain_http_complete, hb, name, goal, rounds, transcript, cap);
        gmb_brain_http_free(hb);
    } else {
        /* Nessun provider reale: fixture deterministica (parità FixtureLlmCompleter C#). */
        out = gmb_agent_run_cua_loop(gmb_agent_fixture_complete, NULL, name, goal, rounds, transcript, cap);
    }
    free(transcript);
    return out;
}

/* AI.AGENTRUN(name, goal[, rounds]) — bug 2026-09-08 (BackendVMC/C99/C99Native-GPM-Goroutine-
 * Swarm-Agent, BackendIL-GPM): mai portato al mondo C, mancava PROPRIO in native/ (non solo
 * "tipizzato male", vedi audit). Wrapper malloc/free attorno a gmb_agent_run_goal — che GIÀ
 * accetta un nome agente esplicito (usato con "" da AGENT.RUN, il costrutto senza nome) — stesso
 * pattern inline di gmb_bf_agent_run_c1387 in gmb_vm_c/gmb_builtin.c, estratto qui per essere
 * riusabile da VM/C, C99Native senza triplicare la logica malloc/log/free.
 * Ritorna una stringa malloc'd (il chiamante fa free), mai NULL. */
/* gmb_agent_exists vive nel runtime condiviso (gmb_runtime.h, non incluso qui): dichiarazione
 * locale, stessa convenzione di gmb_doc_extract in gmb_agent_file.c. */
int gmb_agent_exists(const char* name);

char* gmb_agent_run_named(const char* name, const char* goal, int max_steps) {
    /* Parità con l'oracolo IL (AgentServer.AskGraph/Run -> "Agent '<n>' is not registered.") e con
     * il handler VM/C: un agente INESISTENTE è un errore esplicito, non si può girare il loop.
     * Prima la funzione ignorava il nome e cadeva nel loop/fixture (riprodotto da gen_AI_AgentRun,
     * EXPECT "ERROR: Agent 'x' is not registered." con parità verde su IL/PCode/VM-C). */
    if (name && *name && !gmb_agent_exists(name)) {
        size_t n = strlen(name) + 48;
        char* m = (char*)malloc(n);
        if (m) snprintf(m, n, "ERROR: Agent '%s' is not registered.", name);
        return m ? m : strdup("ERROR: Agent non registrato.");
    }
    if (max_steps <= 0) max_steps = 5;
    return gmb_agent_run_cua(name ? name : "", goal, max_steps);
}

/* ============================================================================
 * Tool-loop di AI AGENT ASK (parità AgentToolLoop.AskWithTools C#): round
 * brain → parse tool_calls (gmb_cua_parse_calls) → esecuzione (gmb_cua_execute)
 * → osservazione nel messaggio successivo, fino a testo finale o cap round.
 * Riusa parse/execute del loop CUA (nessuna duplicazione, §0bis).
 * ============================================================================ */

/* Default come AgentToolLoop.DefaultMaxRounds (C#). */
#define GMB_ASK_TOOLS_MAX_ROUNDS 5

/* Aggiunge il preambolo tool (parità AgentToolLoop.BuildSystemPrompt C#): intestazione +
 * una riga "- <name>: <description>" per ogni tool del registry. Ritorna il nuovo offset. */
static size_t ask_tools_append_list(char* out, size_t cap, size_t off) {
    gmb_tool_manifest_t tools[GMB_TOOL_MAX];
    int n = gmb_tool_registry_list(tools, GMB_TOOL_MAX);
    if (n <= 0) return off;
    off += (size_t)snprintf(out + off, cap - off,
        "\n\n## Available tools (respond with JSON {\"tool_calls\":[...]} to invoke):\n");
    for (int i = 0; i < n && off + 1 < cap; i++)
        off += (size_t)snprintf(out + off, cap - off, "- %s: %s\n", tools[i].name, tools[i].description);
    return off;
}

char* gmb_agent_ask_with_tools(const char* agent, const char* prompt, int max_rounds) {
    (void)agent;
    if (max_rounds <= 0) max_rounds = GMB_ASK_TOOLS_MAX_ROUNDS;
    size_t cap = 65536;
    char* transcript = (char*)malloc(cap);
    if (!transcript) return NULL;
    int tl = snprintf(transcript, cap, "user: %s", prompt ? prompt : "");
    if (tl < 0) { free(transcript); return NULL; }
    (void)ask_tools_append_list(transcript, cap, (size_t)tl);

    char last[65536]; last[0] = 0;
    for (int round = 0; round < max_rounds; round++) {
        char answer[65536];
        answer[0] = 0;
        if (gmb_brain_ask_sync(transcript, answer, sizeof(answer)) != 0) {
            /* Nessun brain al PRIMO round: il chiamante torna al path single-ask (nessun danno).
             * Brain perso a metà loop: risposta finale esplicita, mai loop infinito. */
            free(transcript);
            if (round == 0) return NULL;
            return strdup(answer[0] ? answer : "error: brain non disponibile durante il tool-loop");
        }
        /* State probe (parità AgentToolLoop C#): output identico → nessun progresso. */
        if (answer[0] && strcmp(answer, last) == 0) {
            free(transcript);
            return strdup("Loop rilevato: risposta identica senza progresso. Interrompo.");
        }
        snprintf(last, sizeof(last), "%s", answer);

        gmb_cua_call_t calls[GMB_CUA_MAX_CALLS];
        int ncalls = gmb_cua_parse_calls(answer, calls, GMB_CUA_MAX_CALLS);
        if (ncalls == 0) {
            /* Nessuna tool_call: è la risposta finale (o parse fallito → finale, mai loop). */
            free(transcript);
            return strdup(answer);
        }

        size_t cl = strlen(transcript);
        if (cl + strlen(answer) + 16 < cap)
            snprintf(transcript + cl, cap - cl, "\nassistant: %s", answer);
        for (int i = 0; i < ncalls; i++) {
            char obs[8192];
            gmb_cua_execute(calls[i].name, calls[i].args, obs, sizeof(obs));
            cl = strlen(transcript);
            if (cl + strlen(calls[i].name) + strlen(obs) + 32 < cap)
                snprintf(transcript + cl, cap - cl, "\ntool: [%s] %s", calls[i].name, obs);
        }
    }
    free(transcript);
    char m[128];
    snprintf(m, sizeof(m), "Tool-calling loop exceeded %d rounds.", max_rounds);
    return strdup(m);
}

/* ============================================================================
 * §4.10 esposizione agenti (lato C): tool-loop `code.*` sul grafo semantico del
 * codice (parità SemanticAgentLoop C#). Round brain → parse tool_calls → esecuzione
 * (gmb_cua_execute, ramo code.*) → osservazione. Richiede il seam code.* registrato
 * (gmb_agent_code_query_set) e un brain. Errore esplicito se l'agente non esiste.
 * ============================================================================ */
static size_t ask_tools_append_code(char* out, size_t cap, size_t off) {
    return off + (size_t)snprintf(out + off, cap - off,
        "\n\n## Available tools (respond with JSON {\"tool_calls\":[...]} to invoke):\n"
        "- code.symbol: definizione di un simbolo del file: '<kind> <label>'.\n"
        "- code.callmap: callers/callees diretti: 'callers=[...] callees=[...]'.\n"
        "- code.impact: impatto transitivo (chi e' impattato), fino a 'depth'.\n"
        "- code.context: simbolo + vicini (callers/callees) fino a 'depth'.\n"
        "- code.findsymbol: simboli il cui nome contiene 'query'.\n"
        "- code.fileoutline: outline del file (simboli definiti).\n"
        "- code.repomap: mappa completa delle chiamate del file.\n");
}

/* Istruzioni dei tool graph.* (parità GraphAgentLoop.ToolInstructions). */
static size_t ask_tools_append_graph(char* out, size_t cap, size_t off) {
    return off + (size_t)snprintf(out + off, cap - off,
        "\n\n## Available tools (respond with JSON {\"tool_calls\":[...]} to invoke):\n"
        "- graph.query: cerca nel knowledge graph i nodi che contengono 'query' e ritorna il vicinato.\n"
        "- graph.path: percorso più corto fra 'from' e 'to' del grafo.\n"
        "Altri strumenti graph.* (neighbors/bfs/dfs/dijkstra/hubs/stats) non sono disponibili su questo host.\n");
}

/* AI.AGENTRUNGRAPH: loop agentico sui tool graph.* (parità AgentServer.AskGraph del managed).
 * Stessa struttura del loop code.*: brain → parse tool_calls → esecuzione (gmb_cua_execute,
 * ramo graph.*) → osservazione. Errore esplicito se l'agente non esiste o il brain manca. */
char* gmb_agent_run_graph(const char* name, const char* prompt, int rounds) {
    if (name && *name && !gmb_agent_exists(name)) {
        size_t n = strlen(name) + 48;
        char* m = (char*)malloc(n);
        if (m) snprintf(m, n, "ERROR: Agent '%s' is not registered.", name);
        return m ? m : strdup("ERROR: Agent non registrato.");
    }
    if (rounds <= 0) rounds = GMB_ASK_TOOLS_MAX_ROUNDS;
    size_t cap = 65536;
    char* transcript = (char*)malloc(cap);
    if (transcript == NULL) return strdup("error: memoria");
    int tl = snprintf(transcript, cap, "user: %s", prompt ? prompt : "");
    if (tl < 0) { free(transcript); return strdup("error: transcript"); }
    (void)ask_tools_append_graph(transcript, cap, (size_t)tl);

    char last[65536]; last[0] = 0;
    for (int round = 0; round < rounds; round++) {
        char answer[65536]; answer[0] = 0;
        if (gmb_brain_ask_sync(transcript, answer, sizeof(answer)) != 0) {
            free(transcript);
            return strdup(answer[0] ? answer : "error: brain non disponibile durante il graph tool-loop");
        }
        if (answer[0] && strcmp(answer, last) == 0) {
            free(transcript);
            return strdup("Loop rilevato: risposta identica senza progresso. Interrompo.");
        }
        snprintf(last, sizeof(last), "%s", answer);
        gmb_cua_call_t calls[GMB_CUA_MAX_CALLS];
        int ncalls = gmb_cua_parse_calls(answer, calls, GMB_CUA_MAX_CALLS);
        if (ncalls == 0) { free(transcript); return strdup(answer); }
        size_t cl = strlen(transcript);
        if (cl + strlen(answer) + 16 < cap)
            snprintf(transcript + cl, cap - cl, "\nassistant: %s", answer);
        for (int i = 0; i < ncalls; i++) {
            char obs[8192];
            gmb_cua_execute(calls[i].name, calls[i].args, obs, sizeof(obs));
            cl = strlen(transcript);
            if (cl + strlen(calls[i].name) + strlen(obs) + 32 < cap)
                snprintf(transcript + cl, cap - cl, "\ntool: [%s] %s", calls[i].name, obs);
        }
    }
    free(transcript);
    char m[128];
    snprintf(m, sizeof(m), "Tool-calling loop exceeded %d rounds.", rounds);
    return strdup(m);
}

char* gmb_agent_run_code(const char* name, const char* prompt, int rounds) {
    if (name && *name && !gmb_agent_exists(name)) {
        size_t n = strlen(name) + 48;
        char* m = (char*)malloc(n);
        if (m) snprintf(m, n, "ERROR: Agent '%s' is not registered.", name);
        return m ? m : strdup("ERROR: Agent non registrato.");
    }
    if (rounds <= 0) rounds = GMB_ASK_TOOLS_MAX_ROUNDS;
    size_t cap = 65536;
    char* transcript = (char*)malloc(cap);
    if (transcript == NULL) return strdup("error: memoria");
    int tl = snprintf(transcript, cap, "user: %s", prompt ? prompt : "");
    if (tl < 0) { free(transcript); return strdup("error: transcript"); }
    (void)ask_tools_append_code(transcript, cap, (size_t)tl);

    char last[65536]; last[0] = 0;
    for (int round = 0; round < rounds; round++) {
        char answer[65536]; answer[0] = 0;
        if (gmb_brain_ask_sync(transcript, answer, sizeof(answer)) != 0) {
            free(transcript);
            return strdup(answer[0] ? answer : "error: brain non disponibile durante il code tool-loop");
        }
        if (answer[0] && strcmp(answer, last) == 0) {
            free(transcript);
            return strdup("Loop rilevato: risposta identica senza progresso. Interrompo.");
        }
        snprintf(last, sizeof(last), "%s", answer);
        gmb_cua_call_t calls[GMB_CUA_MAX_CALLS];
        int ncalls = gmb_cua_parse_calls(answer, calls, GMB_CUA_MAX_CALLS);
        if (ncalls == 0) { free(transcript); return strdup(answer); }
        size_t cl = strlen(transcript);
        if (cl + strlen(answer) + 16 < cap)
            snprintf(transcript + cl, cap - cl, "\nassistant: %s", answer);
        for (int i = 0; i < ncalls; i++) {
            char obs[8192];
            gmb_cua_execute(calls[i].name, calls[i].args, obs, sizeof(obs));
            cl = strlen(transcript);
            if (cl + strlen(calls[i].name) + strlen(obs) + 32 < cap)
                snprintf(transcript + cl, cap - cl, "\ntool: [%s] %s", calls[i].name, obs);
        }
    }
    free(transcript);
    char m[128];
    snprintf(m, sizeof(m), "Tool-calling loop exceeded %d rounds.", rounds);
    return strdup(m);
}
