/* gmb_agent.c — Registro agenti AI in-memory (headless, per VM/C e transpiler). */
#include "gmb_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include "gmb_state_hook.h"   /* hook opzionale di stato (client managed) */

char* gmb_hex_encode_string(const uint8_t* data, size_t len); /* gmb_crypto.c */
/* F6 multimodale: gmb_doc_extract vive in gmb_doc_c (header non incluso qui). */
char* gmb_doc_extract(const char* path);

#define GMB_AGENT_MAX 32
#define GMB_AGENT_CFG_MAX 32
#define GMB_AGENT_EVENTS_MAX 64
#define GMB_AGENT_HIST_MAX 64
#define GMB_AGENT_MSG_MAX 512

typedef struct gmb_agent_cfg { char key[64]; char value[512]; } gmb_agent_cfg_t;
typedef struct gmb_agent {
    char name[64];
    int active;
    char id[33];          /* hex 32 (formato GUID senza trattini, parità AgentServer) */
    char model[128];
    char provider[128];   /* vuoto → "(active)" */
    char browser_provider[32]; /* chromium|obscura|auto (default auto) */
    char tool_names[32][128];
    int tool_count;
    double temperature;   /* default 0.7 */
    int max_tokens;       /* default 2048 */
    int history_count;
    char hist_role[GMB_AGENT_HIST_MAX][32];
    char hist_content[GMB_AGENT_HIST_MAX][GMB_AGENT_MSG_MAX];
    gmb_agent_cfg_t cfg[GMB_AGENT_CFG_MAX];
    int cfg_count;
} gmb_agent_t;

static const char* g_agent_permission_default = "YOLO"; /* default v1 (parità C# AgentPermissionLevel.YOLO) */

static gmb_agent_t g_agents[GMB_AGENT_MAX];
static char g_agent_events[GMB_AGENT_EVENTS_MAX][256];
static int g_agent_event_count = 0;
static int g_agent_trace_enabled = 0;
static int g_agent_verbose_enabled = 0;
static uint64_t g_agent_id_seq = 0x9E3779B97F4A7C15ULL; /* seed xorshift */
static pthread_mutex_t g_agent_brain_lock = PTHREAD_MUTEX_INITIALIZER;

static void agent_gen_id(char out[33]) {
    uint64_t s = g_agent_id_seq + (uint64_t)time(NULL) * 0x100000001B3ULL;
    for (int i = 0; i < 4; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; }
    g_agent_id_seq = s;
    uint8_t b[16];
    for (int i = 0; i < 16; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; b[i] = (uint8_t)s; }
    char* h = gmb_hex_encode_string(b, 16);
    if (h) { strncpy(out, h, 32); out[32] = '\0'; free(h); }
    else { for (int i = 0; i < 32; i++) out[i] = '0'; out[32] = '\0'; }
}

/* Timestamp ISO-8601 ":O" (yyyy-MM-ddTHH:mm:ss.fffffff+00:00) come DateTimeOffset C#. */
static void agent_iso_ts(char* out, size_t cap) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    snprintf(out, cap, "%04d-%02d-%02dT%02d:%02d:%02d.%07ld+00:00",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, (long)(ts.tv_nsec / 100));
}

static void agent_log(const char* kind, const char* name, const char* detail) {
    /* Parità AgentEventLog.Record: si registra SOLO con TRACE on (prima sempre → AI.AGENTEVENTS()
     * divergeva a TRACE off). Il ring buffer usa Format(verbose=true):
     * "[AGENT <Kind>] <ts:O> agent=<name> <detail>" (sempre spazio prima di detail). */
    if (!g_agent_trace_enabled) return;
    char ts[64];
    agent_iso_ts(ts, sizeof(ts));
    if (g_agent_event_count < GMB_AGENT_EVENTS_MAX)
        snprintf(g_agent_events[g_agent_event_count++], 256, "[AGENT %s] %s agent=%s %s",
                 kind, ts, name ? name : "", detail ? detail : "");
    /* Echo su stderr: NON-verbose "[AGENT <Kind>] agent=<name>", verbose con ts+detail. */
    if (g_agent_verbose_enabled)
        fprintf(stderr, "[AGENT %s] %s agent=%s %s\n", kind, ts, name ? name : "", detail ? detail : "");
    else
        fprintf(stderr, "[AGENT %s] agent=%s\n", kind, name ? name : "");
}

/* AI AGENT TRACE <ON|OFF|VERBOSE> (parità AgentEventLog.SetMode). */
int gmb_agent_trace(const char* mode) {
    { int hr = gmb_state_hook_mutate("agent.trace", mode, NULL, NULL); if (hr >= 0) return hr; }
    if (!mode || !*mode) return 0;
    if (strcasecmp(mode, "VERBOSE") == 0) { g_agent_trace_enabled = 1; g_agent_verbose_enabled = 1; return 1; }
    if (strcasecmp(mode, "ON") == 0) { g_agent_trace_enabled = 1; g_agent_verbose_enabled = 0; return 1; }
    if (strcasecmp(mode, "OFF") == 0) { g_agent_trace_enabled = 0; g_agent_verbose_enabled = 0; return 1; }
    return 0;
}

static gmb_agent_t* find_agent(const char* name) {
    for (int i = 0; i < GMB_AGENT_MAX; i++)
        if (g_agents[i].active && strcmp(g_agents[i].name, name) == 0) return &g_agents[i];
    return NULL;
}

int gmb_agent_create(const char* name) {
    { int hr = gmb_state_hook_mutate("agent.create", name, NULL, NULL); if (hr >= 0) return hr; }
    if (!name || !*name || find_agent(name)) return 0;
    for (int i = 0; i < GMB_AGENT_MAX; i++)
        if (!g_agents[i].active) {
            strncpy(g_agents[i].name, name, 63); g_agents[i].name[63] = '\0';
            g_agents[i].active = 1; g_agents[i].cfg_count = 0;
            g_agents[i].temperature = 0.7; g_agents[i].max_tokens = 2048;
            g_agents[i].model[0] = '\0'; g_agents[i].provider[0] = '\0';
            strncpy(g_agents[i].browser_provider, "auto", 31); g_agents[i].browser_provider[31] = '\0';
            g_agents[i].history_count = 0; g_agents[i].tool_count = 0;
            agent_gen_id(g_agents[i].id);
            /* applica il default di permesso (AI PERMISSION DEFAULT ...) — parità C# */
            if (g_agent_permission_default && *g_agent_permission_default)
                gmb_agent_set(name, "permission", g_agent_permission_default);
            agent_log("Created", name, "");
            return 1;
        }
    return 0;
}

int gmb_agent_delete(const char* name) {
    { int hr = gmb_state_hook_mutate("agent.delete", name, NULL, NULL); if (hr >= 0) return hr; }
    gmb_agent_t* a = find_agent(name);
    if (!a) return 0;
    a->active = 0;
    agent_log("Deleted", name, "");
    return 1;
}

int gmb_agent_set(const char* name, const char* key, const char* value) {
    { int hr = gmb_state_hook_mutate("agent.set", name, key, value); if (hr >= 0) return hr; }
    gmb_agent_t* a = find_agent(name);
    if (!a) return 0;
    if (key && *key && value) {
        if (strcmp(key, "model") == 0) { strncpy(a->model, value, 127); a->model[127] = '\0'; return 1; }
        if (strcmp(key, "temperature") == 0) { a->temperature = atof(value); return 1; }
        /* "maxTokens" camelCase (Snapshot C#) oltre a "maxtokens" storico. */
        if (strcmp(key, "maxtokens") == 0 || strcmp(key, "maxTokens") == 0) { int m = atoi(value); if (m > 0) a->max_tokens = m; return 1; }
        /* "providerName" (Snapshot C#) oltre a "provider". */
        if (strcmp(key, "provider") == 0 || strcmp(key, "providerName") == 0) { strncpy(a->provider, value, 127); a->provider[127] = '\0'; return 1; }
        if (strcasecmp(key, "browser") == 0 || strcasecmp(key, "browserprovider") == 0) {
            char nb[32]; snprintf(nb, sizeof(nb), "%s", value);
            /* codice numerico %AUTO/%CHROMIUM/%OBSCURA dalla superficie BASIC (0 auto, 1 chromium, 2 obscura) */
            double code = atof(nb);
            if (nb[0] >= '0' && nb[0] <= '9' && code >= 0 && code <= 2 && code == (int)code) {
                int c = (int)code;
                strcpy(nb, c == 1 ? "chromium" : c == 2 ? "obscura" : "auto");
            } else {
                for (char *p = nb; *p; p++) *p = (char)tolower((unsigned char)*p);
                if (strcmp(nb, "chrome") == 0) strcpy(nb, "chromium");
                if (strcmp(nb, "chromium") != 0 && strcmp(nb, "obscura") != 0 && strcmp(nb, "auto") != 0) return 0;
            }
            strncpy(a->browser_provider, nb, 31); a->browser_provider[31] = '\0';
            return 1;
        }
    }
    for (int i = 0; i < a->cfg_count; i++)
        if (strcmp(a->cfg[i].key, key) == 0) { strncpy(a->cfg[i].value, value, 511); a->cfg[i].value[511] = '\0'; return 1; }
    if (a->cfg_count < GMB_AGENT_CFG_MAX) {
        strncpy(a->cfg[a->cfg_count].key, key, 63);
        strncpy(a->cfg[a->cfg_count].value, value, 511);
        a->cfg_count++;
        return 1;
    }
    return 0;
}

int gmb_agent_exists(const char* name) { return find_agent(name) ? 1 : 0; }

/* AI.ASKCHAIN — anello di ragionamento concatenato via Blackboard (parità AskChainedStepCore):
 * legge <prefix>_state, compone il prompt col riassunto precedente, completa (niente transcript
 * cumulato), appende l'artefatto <prefix>_s<count> e aggiorna lo stato. Ritorna la risposta. */
char* gmb_agent_ask_chain(const char* name, const char* prompt, const char* prefix)
{
    { char* r = gmb_state_hook_query3("agent.askchain", name, prompt, prefix); if (r) return r; }
    /* PARITÀ IL (divergenza trovata da gen_AI_AskChain su C99Native): `AgentServer` lancia
     * `Agent '<name>' is not registered.` per un agente inesistente; qui il nome era IGNORATO
     * ((void)name) e si finiva nel completo LLM, che senza modello produce il testo fixture
     * "[...] (insufficient context)" → due testi diversi per lo stesso caso. Validazione
     * esplicita con lo STESSO messaggio (il runner lo rende "ERROR: <msg>"). */
    if (name && *name)
    {
        char names[64][64];
        int n = gmb_agent_list(names, 64), found = 0;
        for (int i = 0; i < n && !found; i++)
            if (strcasecmp(names[i], name) == 0) found = 1;
        if (!found)
        {
            char* err = (char*)malloc(strlen(name) + 48);
            if (!err) return NULL;
            snprintf(err, strlen(name) + 48, "ERROR: Agent '%s' is not registered.", name);
            return err;
        }
    }
    const char* pfx = (prefix && *prefix) ? prefix : "chain";
    const char* step = prompt ? prompt : "";
    char state_key[256];
    snprintf(state_key, sizeof(state_key), "%s_state", pfx);
    char* prior = gmb_blackboard_read(state_key);
    char* full;
    if (prior && *prior)
    {
        size_t need = strlen(step) + strlen(prior) + 64;
        full = (char*)malloc(need);
        if (!full) { free(prior); return NULL; }
        snprintf(full, need, "%s\n\n[stato precedente (blackboard)]\n%s", step, prior);
    }
    else
    {
        full = (char*)malloc(strlen(step) + 1);
        if (!full) { free(prior); return NULL; }
        snprintf(full, strlen(step) + 1, "%s", step);
    }
    char* answer = NULL;
    /* Parità C# AskChainedStepCore via completer (2026-09-23, Parita-5-Backend-Residui §2 —
     * AiAskChainExample): il completer è quello di completamento ATTIVO (mock fixture se
     * fixture, cervello reale altrimenti) — MAI il path enrich (gmb_llm_complete) che sul
     * kind fixture ritorna NoCompleter. Prima la chain chiamava sempre gmb_llm_complete →
     * "Error: nessun provider LLM configurato" mentre IL dava "[prompt] (insufficient
     * context)". Senza provider attivo: stesso errore C# (GetActiveCompletion lancia). */
    {
        const char* ap = gmb_provider_active("completion");
        const char* ak = (ap && *ap) ? gmb_provider_kind(ap) : NULL;
        if (!ap || !*ap)
            answer = strdup("ERROR: No active completion provider available (all failed or none registered).");
        else if (ak && strcmp(ak, "fixture") == 0) {
            size_t need = strlen(full) + 64;
            answer = (char*)malloc(need);
            if (answer) gmb_fixture_llm_complete(full, answer, need);
        }
        else answer = gmb_llm_complete("", full);
    }
    free(full);
    int count = 1;
    if (prior && *prior)
    {
        int nonempty = 0;
        const char* start = prior;
        for (const char* p = prior; ; p++)
        {
            if (*p == '\n' || *p == '\0')
            {
                if (p > start) nonempty++;
                if (*p == '\0') break;
                start = p + 1;
            }
        }
        count = nonempty + 1;
    }
    char step_key[256];
    snprintf(step_key, sizeof(step_key), "%s_s%d", pfx, count);
    gmb_blackboard_write(step_key, answer ? answer : "");
    char* newstate;
    if (prior && *prior)
    {
        size_t need = strlen(prior) + strlen(answer ? answer : "") + 2;
        newstate = (char*)malloc(need);
        if (newstate) snprintf(newstate, need, "%s\n%s", prior, answer ? answer : "");
    }
    else
    {
        size_t need = strlen(answer ? answer : "") + 1;
        newstate = (char*)malloc(need);
        if (newstate) snprintf(newstate, need, "%s", answer ? answer : "");
    }
    gmb_blackboard_write(state_key, newstate ? newstate : "");
    free(prior);
    free(newstate);
    return answer;
}

const char* gmb_agent_get(const char* name, const char* key) {
    static char out[128];
    gmb_agent_t* a = find_agent(name);
    /* Parità IlRuntime.AiAgentGet: agente inesistente → errore esplicito (prima ""). */
    if (!a) { snprintf(out, sizeof(out), "ERROR: Agent '%s' is not registered.", name ? name : ""); return out; }
    if (key) {
        if (strcasecmp(key, "model") == 0) return a->model;
        if (strcasecmp(key, "provider") == 0) return a->provider;
        if (strcasecmp(key, "browser") == 0 || strcasecmp(key, "browserprovider") == 0) return a->browser_provider;
        if (strcasecmp(key, "temperature") == 0) { snprintf(out, sizeof(out), "%g", a->temperature); return out; }
        /* case-insensitive: il SET accetta "maxTokens" camelCase (Snapshot C#) e la PROPERTY
         * MaxTokens lo usa in lettura → il GET con strcmp("maxtokens") non lo trovava
         * (MaxTokens=0 sul VM/C, AgentObject). */
        if (strcasecmp(key, "maxtokens") == 0) { snprintf(out, sizeof(out), "%d", a->max_tokens); return out; }
        /* Proprietà BOOLEANE (parità AgentServer.Get): "ON"/"OFF" dal valore cfg o default
         * OFF se mai settate. Prima cadevano nel loop cfg → "" (divergenza IL: "OFF"). */
        {
            static const char* const bool_keys[] = {
                "skills", "skillsenabled", "sessionstreaming", "sessionstreamingenabled",
                "clienttools", "clienttoolsenabled", "memory", "memoryenabled",
                "thinking", "thinkingenabled", "plugins", "pluginsenabled",
                "router", "routerenabled"
            };
            for (unsigned bi = 0; bi < sizeof(bool_keys) / sizeof(bool_keys[0]); bi++)
                if (strcasecmp(key, bool_keys[bi]) == 0) {
                    for (int i = 0; i < a->cfg_count; i++)
                        if (strcasecmp(a->cfg[i].key, key) == 0) return a->cfg[i].value;
                    return "OFF";
                }
        }
        /* toolpolicies (plurale) = join delle cfg "toolpolicy" (parità C# ToolPolicies) */
        if (strcmp(key, "toolpolicies") == 0) {
            out[0] = '\0'; int first = 1;
            for (int i = 0; i < a->cfg_count; i++)
                if (strncmp(a->cfg[i].key, "toolpolicy", 10) == 0) {
                    size_t n = strlen(out);
                    const char* v = a->cfg[i].value;
                    const char* bar = strchr(v, '|');
                    /* parità C# ToolPolicies: "tool=policy" (il cfg interno usa "tool|policy"). */
                    if (bar)
                        snprintf(out + n, sizeof(out) - n, "%s%.*s=%s", first ? "" : ",", (int)(bar - v), v, bar + 1);
                    else
                        snprintf(out + n, sizeof(out) - n, "%s%s", first ? "" : ",", v);
                    first = 0;
                }
            return out;
        }
    }
    for (int i = 0; i < a->cfg_count; i++)
        if (strcmp(a->cfg[i].key, key) == 0) return a->cfg[i].value;
    return "";
}

/* Brain LLM reale del mondo C (gmb_agent_runtime: MLX via gmb_host_ai_mlx, o HTTP via
 * GMB_BRAIN_URL). Referenza DEBOLE: se libgmb_agent_runtime è linkata (VM/C, C99Native,
 * emitter C#) risolve e gmb_agent_ask risponde davvero; se non è linkata resta NULL →
 * fallback deterministico. Evita una dipendenza dura runtime_c → agent_runtime. */
extern int gmb_brain_ask_sync(const char* prompt, char* out, size_t cap) __attribute__((weak));
/* System prompt risolto per il render col chat_template (gmb_agent_runtime). Referenza DEBOLE
 * come sopra: se agent_runtime non è linkata resta NULL e non si passa nulla. */
extern void gmb_brain_mlx_set_system(const char* sys) __attribute__((weak));
/* Limite di token nuovi (maxtokens dell'agente) per il brain MLX. Referenza DEBOLE come sopra. */
extern void gmb_brain_mlx_set_max_new(int n) __attribute__((weak));
/* Tool-loop di ASK (gmb_agent_runtime, gmb_agent_loop.c): referenza DEBOLE come sopra. Se
 * agent_runtime non è linkata resta NULL → ASK resta il single-ask invariato. */
extern char* gmb_agent_ask_with_tools(const char* agent, const char* prompt, int max_rounds) __attribute__((weak));

/* Transcript (parità AgentServer.BuildTranscript): "role: content\n" per messaggio, TrimEnd. */
static void gmb_agent_build_transcript(gmb_agent_t* a, char* out, size_t cap) {
    size_t off = 0;
    if (cap == 0) return;
    out[0] = '\0';
    if (!a) return;
    for (int i = 0; i < a->history_count; i++) {
        int w = snprintf(out + off, cap - off, "%s: %s\n", a->hist_role[i], a->hist_content[i]);
        if (w > 0) off += (size_t)w;
        if (off >= cap) { off = cap - 1; out[off] = '\0'; break; }
    }
    while (off > 0 && (out[off - 1] == '\n' || out[off - 1] == '\r'
                       || out[off - 1] == ' ' || out[off - 1] == '\t'))
        out[--off] = '\0';
}

static void gmb_agent_hist_add(gmb_agent_t* a, const char* role, const char* content) {
    if (!a || a->history_count >= GMB_AGENT_HIST_MAX) return;
    snprintf(a->hist_role[a->history_count], sizeof(a->hist_role[0]), "%s", role);
    snprintf(a->hist_content[a->history_count], GMB_AGENT_MSG_MAX, "%s", content ? content : "");
    a->history_count++;
}

/* Fixture LLM deterministica (parità FixtureLlmCompleter.Complete C#): question = testo dopo
 * "Question: " se presente, altrimenti il prompt intero (trim); snippet = testo dopo il primo
 * ") " fino a '\n' (trim). UNICA sorgente per gmb_agent_ask e il loop CUA (AI.AGENTRUN). */
void gmb_fixture_llm_complete(const char* user_prompt, char* out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    const char* p = user_prompt ? user_prompt : "";
    const char* qm = strstr(p, "Question: ");
    const char* q = qm ? qm + strlen("Question: ") : p;
    while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
    size_t ql = strlen(q);
    while (ql > 0 && (q[ql-1] == ' ' || q[ql-1] == '\t' || q[ql-1] == '\n' || q[ql-1] == '\r')) ql--;
    char question[4096];
    if (ql >= sizeof(question)) ql = sizeof(question) - 1;
    memcpy(question, q, ql); question[ql] = 0;
    char snippet[2048]; snippet[0] = 0;
    const char* sc = strstr(p, ") ");
    if (sc) {
        sc += 2;
        const char* e = strchr(sc, '\n');
        size_t n = e ? (size_t)(e - sc) : strlen(sc);
        if (n >= sizeof(snippet)) n = sizeof(snippet) - 1;
        memcpy(snippet, sc, n); snippet[n] = 0;
        size_t sn = strlen(snippet);
        while (sn > 0 && (snippet[sn-1] == ' ' || snippet[sn-1] == '\t' || snippet[sn-1] == '\r'))
            snippet[--sn] = 0;
    }
    if (snippet[0]) snprintf(out, cap, "[%s] %s", question, snippet);
    else snprintf(out, cap, "[%s] (insufficient context)", question);
}

/* Fixture tool-calling (parità FixtureToolCallingCompleter.Complete C#): args del tool_call
 * deterministico per i tool noti. */
static const char* gmb_fixture_tool_args(const char* tool) {
    if (strcasecmp(tool, "add_numbers") == 0) return "{\"a\":2,\"b\":2}";
    if (strcasecmp(tool, "web_fetch") == 0) return "{\"url\":\"https://example.com\"}";
    if (strcasecmp(tool, "browser_set_cookie") == 0)
        return "{\"name\":\"session\",\"value\":\"agent-cookie\",\"url\":\"http://127.0.0.1:8099\",\"port\":9222}";
    if (strcasecmp(tool, "browser_navigate") == 0) return "{\"url\":\"https://example.com\",\"port\":9222}";
    if (strcasecmp(tool, "browser_text") == 0) return "{\"port\":9222}";
    return "{}";
}
/* Risposta finale della fixture dopo il round col tool (parità FixtureToolCallingCompleter). */
static const char* gmb_fixture_tool_answer(const char* tool) {
    if (strcasecmp(tool, "add_numbers") == 0) return "The sum is 4.";
    if (strcasecmp(tool, "web_fetch") == 0) return "Page fetched: example.com";
    if (strcasecmp(tool, "browser_set_cookie") == 0) return "Cookie impostato: session=agent-cookie";
    if (strcasecmp(tool, "browser_navigate") == 0) return "Navigazione completata (testo non estratto).";
    if (strcasecmp(tool, "browser_text") == 0) return "La pagina contiene: ";
    return NULL;
}

/* Parità AgentServer.Ask: se il model dell'agente non è impostato, usa il ModelName del
 * completer attivo (FixtureLlmCompleter → "fixture-llm-v1"). Prima restava vuoto →
 * AI.AGENTGET(name,"model") divergeva dal mondo C#. */
static void agent_default_model(gmb_agent_t* a) {
    if (!a || a->model[0]) return;
    const char* prov = gmb_provider_active("completion");
    if (prov && *prov) {
        const char* kind = gmb_provider_kind(prov);
        if (kind && strcmp(kind, "fixture") == 0)
            snprintf(a->model, sizeof(a->model), "fixture-llm-v1");
        else
            snprintf(a->model, sizeof(a->model), "%s", prov);
    }
}

char* gmb_agent_ask(const char* name, const char* prompt) {
    { char* r = gmb_state_hook_query("agent.ask", name, prompt); if (r) return r; }
    /* Tool-call path (`tool: <nome>`): verifica la toolpolicy dell'agente; se deny emette
     * PermissionDenied con il dettaglio (parità AgentPermissionService/AgentServer). Il nome
     * nel dettaglio è quello PIENO del tool (chiave policy), come ToolCall.Name C#. */
    char fulltool[128]; fulltool[0] = 0;
    char toolargs[512]; toolargs[0] = 0;
    const char* toolfixans = NULL;
    if (prompt && strncasecmp(prompt, "tool:", 5) == 0) {
        const char* tool = prompt + 5;
        while (*tool == ' ' || *tool == '\t') tool++;
        char tooln[128]; int ti = 0;
        while (tool[ti] && tool[ti] != ' ' && tool[ti] != ',' && ti < 127) { tooln[ti] = tool[ti]; ti++; }
        tooln[ti] = 0;
        if (tooln[0]) {
            snprintf(fulltool, sizeof(fulltool), "%s", tooln);
            gmb_agent_t* ta = find_agent(name);
            const char* pol = NULL; char polbuf[576]; char det[768];
            if (ta) for (int i = 0; i < ta->cfg_count; i++)
                if (strncmp(ta->cfg[i].key, "toolpolicy", 10) == 0) {
                    const char* bar = strchr(ta->cfg[i].value, '|');
                    if (bar) {
                        size_t tl = (size_t)(bar - ta->cfg[i].value);
                        /* vince l'ULTIMA policy (override allow→deny); match esatto o per
                         * substring (l'ask usa il nome corto "cookie" vs "browser_set_cookie"). */
                        if (strncasecmp(ta->cfg[i].value, tooln, tl) == 0 || strstr(ta->cfg[i].value, tooln) != NULL) {
                            snprintf(polbuf, sizeof(polbuf), "%s", bar + 1); pol = polbuf;
                            snprintf(fulltool, sizeof(fulltool), "%.*s", (int)tl, ta->cfg[i].value);
                        }
                    }
                }
            snprintf(toolargs, sizeof(toolargs), "%s", gmb_fixture_tool_args(fulltool));
            toolfixans = gmb_fixture_tool_answer(fulltool);
            if (pol && strcasecmp(pol, "deny") == 0) {
                snprintf(det, sizeof(det), "tool=%s (sandbox)", fulltool);
                agent_log("PermissionDenied", name, det);
                snprintf(det, sizeof(det), "tool=%s args=%s result=ERROR: permesso negato (sandbox) per il tool '%s'.",
                         fulltool, toolargs, fulltool);
                agent_log("ToolCalled", name, det);
            } else {
                snprintf(det, sizeof(det), "tool=%s args=%s", fulltool, toolargs);
                agent_log("ToolCalled", name, det);
            }
        }
    }
    gmb_agent_t* a = find_agent(name);
    /* Parità AgentServer.Ask: appende il messaggio USER prima di costruire il transcript. */
    gmb_agent_hist_add(a, "user", prompt ? prompt : "");

    /* Prompt per il brain. Per il motore MLX/DS4 replico il template C# (ChatTranscripts.
     * BuildDeepSeek + AgentRecord.ResolveSystemPrompt) così il modello reale riceve lo STESSO
     * input del mondo C# → parità sul modello reale SENZA bridge (AGENTS §0/§10 invariato).
     * Sistema: SystemPrompt dell'agente + blocco "## Agent log folder" (LogPath default ai-logs/).
     * Layout: <｜begin▁of▁sentence｜><｜User｜>{sys}{user}<｜Assistant｜>. */
    char brain_prompt_buf[16384];
    const char* brain_prompt = prompt ? prompt : "";
    {
        const char* mp = gmb_provider_active_model_path();
        if (mp && *mp) {
            /* System prompt RISOLTO dell'agente (parità AgentRecord.ResolveSystemPrompt C#):
             * SystemPrompt agente + blocco "## Agent log folder". La seconda riga è il LogPath
             * REALE (AiConfig.LogPath → GmbiPaths.LogsDir del config, default "ai-logs"); la
             * terza è testo letterale C# (con "ai-logs/" hardcoded).
             * Passato al brain (referenza DEBOLE) per il render col chat_template del modello. */
            const char* sp = gmb_agent_get(name, "systemprompt");
            extern void gmb_paths_logs_dir(char* out, size_t cap);
            char logsdir[512]; logsdir[0] = 0;
            gmb_paths_logs_dir(logsdir, sizeof logsdir);
            if (!logsdir[0]) snprintf(logsdir, sizeof logsdir, "ai-logs");
            /* Thinking effort (parità MlxProviders: config provider "thinkLevel" 1..100,
             * port di --think-level di DS4 / DeepSeek V4.1): se l'provider COMPLETION attivo
             * ha thinkLevel>0, antepone al system lo STESSO prefisso del mondo C#
             * (ds4_deepseek41_reasoning_effort_text). 0/assente = nessun prefisso. */
            char effort[256]; effort[0] = 0;
            {
                const char* ap = gmb_provider_active("completion");
                const char* tls = (ap && *ap) ? gmb_ai_get_config(ap, "thinkLevel") : NULL;
                int tlvl = (tls && *tls) ? atoi(tls) : 0;
                if (tlvl > 100) tlvl = 100;
                if (tlvl > 0)
                    snprintf(effort, sizeof(effort),
                        "Reasoning Effort: %d (range 1-100, the higher the value, the more thorough the reasoning)\n\n",
                        tlvl);
            }
            char sysbuf[8192];
            snprintf(sysbuf, sizeof(sysbuf),
                "%s%s\n\n## Agent log folder\n"
                "Scrivi i log nella cartella: `%s`\n"
                "(impostata con AI LOG PATH; default ai-logs/)\n",
                effort, sp ? sp : "", logsdir);
            if (gmb_brain_mlx_set_system) gmb_brain_mlx_set_system(sysbuf);

            /* maxtokens dell'agente → limite di generazione del brain (parità agent.MaxTokens C#).
             * Assente/non numerico → 0 = default del brain (MAX_NEW). */
            if (gmb_brain_mlx_set_max_new) {
                const char* mt = gmb_agent_get(name, "maxtokens");
                int mn = (mt && *mt) ? atoi(mt) : 0;
                gmb_brain_mlx_set_max_new(mn);
            }

            /* DS4 (.gguf): prompt pre-formattato verbatim, byte-identico a prima (nessuna
             * regressione). Il brain lo usa tal quale (transcript inizia col BOS). */
            size_t ml = strlen(mp);
            if (ml > 5 && strcasecmp(mp + ml - 5, ".gguf") == 0) {
                snprintf(brain_prompt_buf, sizeof(brain_prompt_buf),
                    "<｜begin▁of▁sentence｜><｜User｜>%s%s<｜Assistant｜>", sysbuf, prompt ? prompt : "");
                brain_prompt = brain_prompt_buf;
            }
        }
    }

    /* Tool-loop (parità AgentServer.Ask → AgentToolLoop.AskWithTools C#): parte SOLO se
     * l'agente ha tool ESPLICITI (AI AGENT REGISTER TOOL) oppure il prompt matcha il trigger
     * di un plugin/skill noto (PluginIndex.MatchByTrigger). I tool del registry da soli NON
     * bastano (parità Fase 1 C#: il loop non deve degradare le risposte semplici). Senza tool
     * → nel path single-ask invariato. Se gmb_agent_ask_with_tools non è linkata (agent_runtime
     * assente) il weak è NULL e si prosegue col single-ask. */
    if (gmb_agent_ask_with_tools && a && (a->tool_count > 0 || llmengine_plugin_match_trigger(prompt))) {
        pthread_mutex_lock(&g_agent_brain_lock);
        char* tout = gmb_agent_ask_with_tools(name, prompt, 0);
        pthread_mutex_unlock(&g_agent_brain_lock);
        if (tout) {
            gmb_agent_hist_add(a, "assistant", tout);
            char asked[8192];
            snprintf(asked, sizeof(asked), "prompt=%s answer=%s", prompt ? prompt : "", tout);
            agent_log("Asked", name, asked);
            agent_default_model(a);
            return tout;
        }
        /* tout == NULL → nessun brain al primo round: prosegue col path single-ask. */
    }

    /* Brain LLM reale (MLX/HTTP), serializzato (non thread-safe). gmb_brain_ask_sync
     * auto-configura il brain MLX dal provider attivo (o GMB_MLX_MODEL_DIR) e ritorna !=0 se
     * nessun provider. */
    if (gmb_brain_ask_sync) {
        const size_t cap = 65536;
        char* buf = (char*)malloc(cap);
        if (buf) {
            pthread_mutex_lock(&g_agent_brain_lock);
            int rc = gmb_brain_ask_sync(brain_prompt, buf, cap);
            pthread_mutex_unlock(&g_agent_brain_lock);
            if (rc == 0) {
                gmb_agent_hist_add(a, "assistant", buf);
                char asked[8192]; snprintf(asked, sizeof(asked), "prompt=%s answer=%s", prompt ? prompt : "", buf);
                agent_log("Asked", name, asked);
                return buf;
            }
        }
        free(buf);
    }

    /* Fallback deterministico = FixtureLlmCompleter.Complete (fonte unica: gmb_fixture_llm_complete).
     * Sul path tool la fixture tool-calling ritorna la risposta del round col tool. */
    char transcript[8192];
    gmb_agent_build_transcript(a, transcript, sizeof(transcript));
    size_t need = strlen(transcript) * 2 + 64;
    char* out = (char*)malloc(need);
    if (!out) return NULL;
    if (toolfixans) snprintf(out, need, "%s", toolfixans);
    else gmb_fixture_llm_complete(transcript, out, need);
    gmb_agent_hist_add(a, "assistant", out);
    {
        char asked[8192];
        snprintf(asked, sizeof(asked), "prompt=%s answer=%s", prompt ? prompt : "", out);
        agent_log("Asked", name, asked);
    }
    agent_default_model(a);
    return out;
}

int gmb_agent_list(char out[][64], int max) {
    int n = 0;
    for (int i = 0; i < GMB_AGENT_MAX && n < max; i++)
        if (g_agents[i].active) { strncpy(out[n], g_agents[i].name, 63); out[n][63] = '\0'; n++; }
    return n;
}

const char* gmb_agent_status(const char* name) {
    static char buf[512];
    gmb_agent_t* a = find_agent(name);
    if (!a) { snprintf(buf, sizeof(buf), "NOT_FOUND"); return buf; }
    /* formato esatto AgentServer.GetStatusSummary (parità PCodeVM/IL) */
    snprintf(buf, sizeof(buf),
        "name=%s id=%s model=%s provider=%s browser=%s temperature=%g maxTokens=%d historyCount=%d toolCount=%d",
        a->name, a->id, a->model, a->provider[0] ? a->provider : "(active)", a->browser_provider,
        a->temperature, a->max_tokens, a->history_count, a->tool_count);
    return buf;
}

int gmb_agent_events(char out[][256], int max) {
    int n = g_agent_event_count < max ? g_agent_event_count : max;
    for (int i = 0; i < n; i++) strncpy(out[i], g_agent_events[i], 255);
    return n;
}
/* AI.AGENTEVENTS() → eventi come stringa nuova-riga-separata (variante stringa per il getter). */
char* gmb_agent_events_str(void) {
    if (g_agent_event_count <= 0) return strdup("");
    size_t cap = 32 + (size_t)g_agent_event_count * 260;
    char* b = (char*)malloc(cap); if (!b) return strdup("");
    b[0] = '\0';
    for (int i = 0; i < g_agent_event_count && i < 64; i++) {
        if (i) strncat(b, "\n", cap - strlen(b) - 1);
        strncat(b, g_agent_events[i], cap - strlen(b) - 1);
    }
    return b;
}

int gmb_agent_save(const char* name, const char* path) {
    { int hr = gmb_state_hook_mutate("agent.save", name, path, NULL); if (hr >= 0) return hr; }
    gmb_agent_t* a = find_agent(name);
    if (!a) return 0;
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "{\"name\":\"%s\",\"configs\":%d", a->name, a->cfg_count);
    for (int i = 0; i < a->cfg_count; i++) fprintf(f, ",\"%s\":\"%s\"", a->cfg[i].key, a->cfg[i].value);
    /* Campi dedicati (parità Snapshot C#: model/temperature/maxTokens/systemPrompt/...):
     * senza, LOAD li perdeva (gmb_agent_set li tiene fuori da cfg[]). Chiavi camelCase
     * come il C#; gmb_agent_set le mappa ai campi (maxTokens/providerName accettati). */
    fprintf(f, ",\"model\":\"%s\"", a->model);
    fprintf(f, ",\"temperature\":%g", a->temperature);
    fprintf(f, ",\"maxTokens\":%d", a->max_tokens);
    fprintf(f, ",\"providerName\":\"%s\"", a->provider);
    fprintf(f, ",\"browserProvider\":\"%s\"", a->browser_provider);
    fprintf(f, "}");
    fclose(f);
    /* HISTORY in un SIDECAR binario `<path>.hist` (parità AgentServer.Save → AgentRecord.History):
     * senza, dopo LOAD l'agente riparte senza conversazione e il transcript del successivo ASK
     * diverge (AgentBasic.bas). Binario per non dipendere dal parser JSON semplificato (i
     * contenuti possono avere virgolette/newline). */
    char histpath[2200]; snprintf(histpath, sizeof(histpath), "%s.hist", path);
    FILE* hf = fopen(histpath, "wb");
    if (hf) {
        fwrite(&a->history_count, sizeof(int), 1, hf);
        for (int i = 0; i < a->history_count; i++) {
            int rl = (int)strlen(a->hist_role[i]);
            int cl = (int)strlen(a->hist_content[i]);
            fwrite(&rl, sizeof(int), 1, hf);
            fwrite(&cl, sizeof(int), 1, hf);
            fwrite(a->hist_role[i], 1, (size_t)rl, hf);
            fwrite(a->hist_content[i], 1, (size_t)cl, hf);
        }
        fclose(hf);
    }
    return 1;
}

int gmb_agent_load(const char* name, const char* path) {
    { int hr = gmb_state_hook_mutate("agent.load", name, path, NULL); if (hr >= 0) return hr; }
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[4096];
    size_t rd = fread(line, 1, sizeof(line) - 1, f);
    line[rd] = '\0';
    fclose(f);
    if (!gmb_agent_create(name)) {
        /* se esiste gia', reset */
        gmb_agent_t* a = find_agent(name);
        if (!a) return 0;
        a->cfg_count = 0;
        a->history_count = 0;
    }
    /* parse piatto "key":value — il file SAVE scrive coppie piatte separate da virgole.
     * (Prima: lo scanner non consumava la virgoletta di chiusura del VALORE, così a ogni
     * coppia la successiva veniva letta come chiave="," e la sua chiave vera veniva saltata:
     * metà dei config persi + chiavi spazzatura "," — AgentBasic non lo vedeva perché non
     * rileggeva i config dopo LOAD.) Valori: stringa quotata o numero nudo. */
    char* p = line;
    while (*p) {
        char* q1 = strchr(p, '"');
        if (!q1) break;
        char* q2 = strchr(q1 + 1, '"');
        if (!q2 || (size_t)(q2 - q1 - 1) >= sizeof(((gmb_agent_t*)0)->cfg[0].key)) { p = q1 + 1; continue; }
        char* c = q2 + 1;
        while (*c == ' ') c++;
        if (*c != ':') { p = q2 + 1; continue; }
        c++;
        while (*c == ' ') c++;
        char key[64], val[512];
        size_t kl = (size_t)(q2 - q1 - 1);
        memcpy(key, q1 + 1, kl); key[kl] = '\0';
        if (*c == '"') {
            char* v2 = strchr(c + 1, '"');
            if (!v2) break;
            size_t vl = (size_t)(v2 - c - 1);
            if (vl >= sizeof(val)) vl = sizeof(val) - 1;
            memcpy(val, c + 1, vl); val[vl] = '\0';
            p = v2 + 1;
        } else {
            size_t vi = 0;
            while (*c && *c != ',' && *c != '}' && vi + 1 < sizeof(val)) val[vi++] = *c++;
            val[vi] = '\0';
            p = c;
        }
        if (key[0] && strcmp(key, "name") != 0 && strcmp(key, "configs") != 0)
            gmb_agent_set(name, key, val);
    }
    /* HISTORY dal sidecar binario `<path>.hist` (parità AgentServer.Load → AgentRecord.History). */
    char histpath[2200]; snprintf(histpath, sizeof(histpath), "%s.hist", path);
    FILE* hf = fopen(histpath, "rb");
    if (hf) {
        int hn = 0;
        if (fread(&hn, sizeof(int), 1, hf) == 1) {
            gmb_agent_t* a = find_agent(name);
            if (a && hn >= 0) {
                if (hn > GMB_AGENT_HIST_MAX) hn = GMB_AGENT_HIST_MAX;
                a->history_count = 0;
                for (int i = 0; i < hn; i++) {
                    int rl = 0, cl = 0;
                    if (fread(&rl, sizeof(int), 1, hf) != 1 || fread(&cl, sizeof(int), 1, hf) != 1) break;
                    if (rl < 0 || rl > (int)sizeof(a->hist_role[0]) - 1 || cl < 0 || cl >= GMB_AGENT_MSG_MAX) break;
                    if (rl > 0) { if (fread(a->hist_role[i], 1, (size_t)rl, hf) != (size_t)rl) break; }
                    a->hist_role[i][rl] = '\0';
                    if (cl > 0) { if (fread(a->hist_content[i], 1, (size_t)cl, hf) != (size_t)cl) break; }
                    a->hist_content[i][cl] = '\0';
                    a->history_count++;
                }
            }
        }
        fclose(hf);
    }
    return 1;
}

int gmb_agent_register_tool(const char* name, const char* tool) {
    { int hr = gmb_state_hook_mutate("agent.registertool", name, tool, NULL); if (hr >= 0) return hr; }
    gmb_agent_t* a = find_agent(name);
    if (!a || !tool || !*tool) return 0;
    for (int i = 0; i < a->tool_count; i++)
        if (strcmp(a->tool_names[i], tool) == 0) return 1;
    if (a->tool_count >= 32) return 0;
    strncpy(a->tool_names[a->tool_count], tool, 127);
    a->tool_names[a->tool_count][127] = '\0';
    a->tool_count++;
    return 1;
}

/* --- PERMESSI (Sandbox/Supervised/YOLO) — porting del modello C# AgentPermissionService --- */
int gmb_agent_permission_default(const char* level) {
    { int hr = gmb_state_hook_mutate("agent.permission_default", level, NULL, NULL); if (hr >= 0) return hr; }
    if (!level || !*level) return 0;
    g_agent_permission_default = level;
    return 1;
}
const char* gmb_agent_permission_default_get(void) { return g_agent_permission_default; }

int gmb_agent_permission(const char* name, const char* level) {
    { int hr = gmb_state_hook_mutate("agent.permission", name, level, NULL); if (hr >= 0) return hr; }
    if (!level || !*level) return 0;
    /* Normalizza al nome enum C# (AgentPermissionLevel.ToString): Sandbox/Supervised/YOLO. */
    const char* norm = level;
    if (strcasecmp(level, "sandbox") == 0) norm = "Sandbox";
    else if (strcasecmp(level, "supervised") == 0) norm = "Supervised";
    else if (strcasecmp(level, "yolo") == 0) norm = "YOLO";
    return gmb_agent_set(name, "permission", norm);
}

int gmb_agent_tool_policy(const char* name, const char* tool, const char* policy) {
    { int hr = gmb_state_hook_mutate("agent.toolpolicy", name, tool, policy); if (hr >= 0) return hr; }
    if (!tool || !*tool || !policy || !*policy) return 0;
    char buf[576];
    snprintf(buf, sizeof(buf), "%s|%s", tool, policy);
    return gmb_agent_set(name, "toolpolicy", buf);
}

/* AI.AGENTSWARM(n, prefix, task, maxConcurrency) — parita' IlRuntime.AiAgentSwarm (C#).
 * Crea (se mancano) gli agenti prefix0..prefixN-1, esegue gmb_agent_ask su ciascuno e
 * concatena i risultati con '\n' (una riga per agente). L'output osservabile e' identico
 * al C# (che raccoglie per indice): gli agenti sono indipendenti, quindi l'esecuzione qui
 * e' sequenziale ma l'ordine dei risultati e' lo stesso (results[i]).
 *
 * NB (limite del mondo C, come AI.AGENTRUN): gmb_agent_ask risponde col fallback
 * deterministico "[user: ...] (insufficient context)" (parita' FixtureLlmCompleter C#
 * quando non c'e' un provider LLM attivo). Con un LLM reale il C# produrrebbe risposte
 * vere mentre il C resta sul fallback: e' il gap runtime agenti del mondo C, tracciato.
 * Ritorna una stringa malloc'd (il chiamante fa free), mai NULL. */
char* gmb_agent_swarm(double n, const char* prefix, const char* task, int max_concurrency) {
    { char nm[32], cc[16]; snprintf(nm, sizeof nm, "%.0f", n); snprintf(cc, sizeof cc, "%d", max_concurrency);
      char io[56]; snprintf(io, sizeof io, "%s,%s", nm, cc);
      char* r = gmb_state_hook_query3("agent.swarm", prefix, task, io); if (r) return r; }
    int count = (int)n;
    if (count < 0) count = 0;
    if (count == 0) return strdup("");
    if (!prefix || !*prefix) return strdup("Error: agentPrefix mancante");
    if (!task || !*task) return strdup("Error: task mancante");
    if (max_concurrency < 1) max_concurrency = 8;
    (void)max_concurrency; /* esecuzione sequenziale: bound di concorrenza non osservabile
                            * nell'output (agenti indipendenti, risultati per indice) */

    size_t cap = 4096;
    char* out = (char*)malloc(cap);
    if (!out) return strdup("AI.AGENTSWARM: memoria");
    out[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < count; i++) {
        char name[80];
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        if (!find_agent(name)) gmb_agent_create(name);
        char* res = gmb_agent_ask(name, task);
        if (!res) res = strdup("(errore)");
        size_t need = len + strlen(res) + 2;
        if (need > cap) { cap = need * 2; char* nb = (char*)realloc(out, cap); if (!nb) { free(res); free(out); return strdup("AI.AGENTSWARM: memoria"); } out = nb; }
        if (len > 0) { out[len++] = '\n'; out[len] = '\0'; }
        strcpy(out + len, res);
        len += strlen(res);
        free(res);
    }
    return out;
}

/* -- Sessione KV dell'agente: AI.PREFILL/DECODE/CACHE/BENCHMARK ---------------
 * I backend C non hanno una sessione KV (AgentServer + IKvCacheManager sono C#):
 * queste funzioni ritornano l'ERRORE ESPLICITO del contratto IL quando l'agente
 * non ha una sessione — mai un risultato silenzioso. Stesso pattern di
 * gmb_bf_benchresult_c1258 (VM/C). Contratti allineati a
 * common/AI/Agents/AgentServer.cs (Prefill/Generate/ClearCache/TrimCache/Benchmark). */

static void agent_json_escape(char* out, size_t cap, const char* s)
{
    size_t o = 0;
    if (!s) s = "";
    for (const char* p = s; *p && o + 2 < cap; p++) {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = *p; }
        else if (*p == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else out[o++] = *p;
    }
    out[o] = 0;
}

char* gmb_agent_prefill_json(const char* name, const char* role, const char* text)
{
    (void)role;
    if (!text || !*text) return strdup("{\"error\":\"testo prefill vuoto\"}");
    if (!name || !*name) return strdup("{\"error\":\"nome agente vuoto\"}");
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non risolve a un agente con sessione KV (agente non registrato o non avviato)\"}", esc);
    return strdup(buf);
}

char* gmb_agent_decode_json(const char* name, int max_tokens)
{
    (void)max_tokens;
    if (!name || !*name) return strdup("{\"error\":\"nome agente vuoto\"}");
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non risolve a un agente con sessione KV (agente non registrato o non avviato)\"}", esc);
    return strdup(buf);
}

char* gmb_agent_cache_clear_json(const char* name)
{
    if (!name || !*name) return strdup("{\"error\":\"nome agente vuoto\"}");
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non risolve a un agente con sessione KV cache-manager (agente non registrato, non avviato o completer fixture)\"}", esc);
    return strdup(buf);
}

char* gmb_agent_cache_trim_json(const char* name, int drop_tokens)
{
    if (!name || !*name) return strdup("{\"error\":\"nome agente vuoto\"}");
    if (drop_tokens < 0) {
        char b[128];
        snprintf(b, sizeof(b), "{\"error\":\"drop non valido: '%d'\"}", drop_tokens);
        return strdup(b);
    }
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non risolve a un agente con sessione KV cache-manager (agente non registrato, non avviato o completer fixture)\"}", esc);
    return strdup(buf);
}

char* gmb_agent_benchmark_json(const char* name, const char* mode, int iterations, const char* text)
{
    (void)iterations; (void)text;
    if (!name || !*name) return strdup("{\"error\":\"nome agente vuoto\"}");
    /* Validazione modalità PRIMA (contratto AgentServer.Benchmark): mode upper. */
    char m[32]; int i = 0;
    if (mode) { for (const char* p = mode; *p && i < 31; p++) m[i++] = (char)toupper((unsigned char)*p); }
    m[i] = 0;
    if (strcmp(m, "PREFILL") != 0 && strcmp(m, "DECODE") != 0 && strcmp(m, "ALL") != 0) {
        char esc[128]; agent_json_escape(esc, sizeof(esc), mode ? mode : "");
        char b[256];
        snprintf(b, sizeof(b), "{\"error\":\"modalità benchmark non valida: '%s' (attese PREFILL|DECODE|ALL)\"}", esc);
        return strdup(b);
    }
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non risolve a un agente con sessione KV (agente non registrato o non avviato)\"}", esc);
    return strdup(buf);
}

/* AI.BENCHRESULT(alias) — ultimo benchmark registrato. I backend C non hanno una sessione
 * KV, quindi nessun benchmark è mai registrato → errore esplicito del contratto IL
 * (AgentServer.GetBenchResult). Condivisa da VM/C (c1258) e C99Native. */
char* gmb_agent_benchresult_json(const char* name)
{
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non ha ancora un benchmark\"}", esc);
    return strdup(buf);
}

/* AI.PREFILLSTATS(alias) / AI.DECODESTATS(alias) — ultima misura singola di prefill/decode.
 * I backend C non hanno una sessione KV, quindi nessuna misura è mai registrata → errore
 * esplicito del contratto IL (AgentServer.GetPrefillStats/GetDecodeStats). Condivise da VM/C
 * (codici 837/838), C99Native (§0bis: una sola implementazione). */
char* gmb_agent_prefill_stats_json(const char* name)
{
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non ha ancora misure di prefill\"}", esc);
    return strdup(buf);
}

char* gmb_agent_decode_stats_json(const char* name)
{
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":\"'%s' non ha ancora misure di decode\"}", esc);
    return strdup(buf);
}

/* AI.AGENTRUNGRAPH/RAG(alias, prompt, rounds) — il loop graph/rag vive in C# (AgentServer.
 * AskGraph/AskRag); i backend C non lo hanno. Parità del caso "agente non registrato" con
 * l'oracolo (AgentServer solleva → "ERROR: Agent '<name>' is not registered."); per un agente
 * registrato il loop non è eseguibile in C → errore esplicito (mai silenzioso). backend_label
 * è il nome del backend chiamante (VM/C, C99 headless). Condivisa da VM/C (1394/1395),
 * C99Native (§0bis: una sola implementazione). */
static gmb_agent_graph_run_fn g_agent_graph_run = NULL;
void gmb_agent_run_graph_json_set(gmb_agent_graph_run_fn fn) { g_agent_graph_run = fn; }

char* gmb_agent_run_graph_json(const char* name, const char* prompt, int rounds)
{
    if (g_agent_graph_run != NULL) return g_agent_graph_run(name, prompt, rounds);
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    char buf[512];
    if (!name || !*name || !gmb_agent_exists(name))
        snprintf(buf, sizeof(buf), "ERROR: Agent '%s' is not registered.", esc);
    else
        snprintf(buf, sizeof(buf), "ERROR: AgentRunGraph non supportato su questo host C "
                                   "(seam gmb_agent_run_graph_json_set non registrato).");
    return strdup(buf);
}

/* §4.10 esposizione agenti: AI.AGENTRUNCODE. Il loop `code.*` reale vive nel runtime agenti
 * (`gmb_agent_run_code`, gmb_agent_loop.c); il runtime condiviso NON lo linka → lo chiama via
 * SEAM registrato dall'host che linka entrambi (gmb_vm_c, C99Native). Emitter/backend usano
 * SOLO questo wrapper (uniforme, come gmb_agent_run_graph_json). Assente → errore ESPLICITO. */
static gmb_agent_code_run_fn g_agent_code_run = NULL;
void gmb_agent_run_code_json_set(gmb_agent_code_run_fn fn) { g_agent_code_run = fn; }
char* gmb_agent_run_code_json(const char* name, const char* prompt, int rounds) {
    if (g_agent_code_run != NULL) return g_agent_code_run(name, prompt, rounds);
    char esc[256]; agent_json_escape(esc, sizeof(esc), name);
    if (!name || !*name || !gmb_agent_exists(name)) {
        char buf[512]; snprintf(buf, sizeof(buf), "ERROR: Agent '%s' is not registered.", esc); return strdup(buf);
    }
    return strdup("ERROR: AgentRunCode non supportato su questo backend (seam non registrato).");
}
