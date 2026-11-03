/* gmb_team.c — Registro TEAM in-memory (headless, per VM/C e transpiler C99).
 * TEAM NON è un namespace: i comandi sono `AI TEAM ...`, le letture AI.TEAMLIST/AI.TEAMAGENTS.
 * Speculare a gmb_agent.c (gmb_runtime_c): stesso stile, memoria statica, nessuna dipendenza. */
#include "gmb_runtime.h"
#include "gmb_state_hook.h"   /* hook opzionale di stato (client managed) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define GMB_TEAM_MAX 32
#define GMB_TEAM_MEMBERS_MAX 16
#define GMB_TEAM_CONV_MAX 64

/* ruoli (TeamRole C#): 0 Coordinator, 1 Worker, 2 Critic, 3 Observer */
/* strategie (TeamStrategy C#): 0 Sequential, 1 RoundRobin, 2 Coordinator,
 * 3 Mixture, 4 Distillation, 5 Deliberation */

typedef struct gmb_team_member { char name[64]; int role; } gmb_team_member_t;
typedef struct gmb_team {
    char name[64];
    int active;
    gmb_team_member_t members[GMB_TEAM_MEMBERS_MAX];
    int member_count;
    int strategy;
    char coordinator[64];
    int recursion_rounds;
    char latent_mode[16];
    int latent_thoughts;
    char outer_link_cfg[256];
    char inner_link_cfg[256];
    int max_recursion_tokens;
    char conversation[GMB_TEAM_CONV_MAX][512];
    int conv_count;
    /* estensioni §9 */
    int dag_dmax, dag_lmax, dag_nmax;
    double complexity_threshold;
    char reflection_mode[8];
    char critic_personas[128];
    char judge_agent[64];
    int prune_topk;
    int thinking_enabled;
    int thinking_steps;
} gmb_team_t;

/* forward declarations (definite più avanti, sezione trainer) */
#define GMB_LINK_DIM 768
#define GMB_LINK_HIDDEN 128
#define GMB_LINK_MAX 64
typedef struct gmb_link {
    char key[96];
    int active;
    int steps;
    double last_loss;
    int dim, outdim, hidden;
    int is_outer;
    double* W1; double* B1;
    double* W2; double* B2;
    double* W3;
} gmb_link_t;
static gmb_link_t* link_get_or_create(const char* key, int is_outer, int hidden);
static void link_embed(const char* text, double* out);
static double gmb_gelu(double x);
static int cfg_hidden(gmb_team_t* t, int is_outer, int dflt);
static void record_run(const char* team, int round, long ms, int tokens, int asks);

static gmb_team_t g_teams[GMB_TEAM_MAX];

static gmb_team_t* find_team(const char* name) {
    for (int i = 0; i < GMB_TEAM_MAX; i++)
        if (g_teams[i].active && strcmp(g_teams[i].name, name) == 0) return &g_teams[i];
    return NULL;
}

static gmb_team_member_t* find_member(gmb_team_t* t, const char* agent) {
    for (int i = 0; i < t->member_count; i++)
        if (strcmp(t->members[i].name, agent) == 0) return &t->members[i];
    return NULL;
}

int gmb_team_exists(const char* name) { return find_team(name) != NULL; }

int gmb_team_create(const char* name) {
    { int hr = gmb_state_hook_mutate("team.create", name, NULL, NULL); if (hr >= 0) return hr; }
    if (!name || !*name || find_team(name)) return 0;
    for (int i = 0; i < GMB_TEAM_MAX; i++)
        if (!g_teams[i].active) {
            strncpy(g_teams[i].name, name, 63); g_teams[i].name[63] = '\0';
            g_teams[i].active = 1; g_teams[i].member_count = 0;
            g_teams[i].strategy = 0; g_teams[i].coordinator[0] = '\0';
            g_teams[i].recursion_rounds = 1; g_teams[i].latent_thoughts = 8;
            g_teams[i].max_recursion_tokens = 2048;
            strncpy(g_teams[i].latent_mode, "text", sizeof(g_teams[i].latent_mode) - 1);
            g_teams[i].latent_mode[sizeof(g_teams[i].latent_mode) - 1] = '\0';
            g_teams[i].outer_link_cfg[0] = '\0'; g_teams[i].inner_link_cfg[0] = '\0';
            g_teams[i].conv_count = 0;
            g_teams[i].dag_dmax = 1; g_teams[i].dag_lmax = 3; g_teams[i].dag_nmax = 3;
            g_teams[i].complexity_threshold = 0.5;
            strncpy(g_teams[i].reflection_mode, "off", sizeof(g_teams[i].reflection_mode) - 1);
            g_teams[i].reflection_mode[sizeof(g_teams[i].reflection_mode) - 1] = '\0';
            g_teams[i].critic_personas[0] = '\0'; g_teams[i].judge_agent[0] = '\0';
            g_teams[i].prune_topk = 3; g_teams[i].thinking_enabled = 0; g_teams[i].thinking_steps = 0;
            return 1;
        }
    return 0;
}

int gmb_team_delete(const char* name) {
    { int hr = gmb_state_hook_mutate("team.delete", name, NULL, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(name);
    if (!t) return 0;
    t->active = 0;
    return 1;
}

int gmb_team_add_agent(const char* team, const char* agent) {
    { int hr = gmb_state_hook_mutate("team.add", team, agent, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || !agent || !*agent || find_member(t, agent)) return 0;
    /* Parità C# TeamServer.AddAgent (riga 72): l'agente deve ESISTERE
     * (AgentServer.Instance.Exists). Senza il check il VM/C aggiungeva agenti
     * inesistenti e l'IL/PCode no (divergenza su team_domain.bas: after-add). */
    if (!gmb_agent_exists(agent)) return 0;
    if (t->member_count >= GMB_TEAM_MEMBERS_MAX) return 0;
    strncpy(t->members[t->member_count].name, agent, 63);
    t->members[t->member_count].name[63] = '\0';
    t->members[t->member_count].role = 1; /* Worker */
    t->member_count++;
    return 1;
}

int gmb_team_remove_agent(const char* team, const char* agent) {
    { int hr = gmb_state_hook_mutate("team.remove", team, agent, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    for (int i = 0; i < t->member_count; i++)
        if (strcmp(t->members[i].name, agent) == 0) {
            for (int j = i; j < t->member_count - 1; j++) t->members[j] = t->members[j + 1];
            t->member_count--;
            return 1;
        }
    return 0;
}

static int role_from_str(const char* r) {
    if (!r) return 1;
    /* case-insensitive come TeamRole (Enum.TryParse ignoreCase) del managed. */
    if (strcasecmp(r, "COORDINATOR") == 0) return 0;
    if (strcasecmp(r, "CRITIC") == 0) return 2;
    if (strcasecmp(r, "OBSERVER") == 0) return 3;
    return 1;
}

int gmb_team_set_role(const char* team, const char* agent, const char* role) {
    { int hr = gmb_state_hook_mutate("team.set_role", team, agent, role); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    gmb_team_member_t* m = t ? find_member(t, agent) : NULL;
    if (!m) return 0;
    m->role = role_from_str(role);
    return 1;
}

static int strategy_from_str(const char* s) {
    if (!s) return 0;
    /* case-insensitive: la superficie HTTP manda "Sequential"/"RoundRobin" (enum C#
     * Enum.TryParse ignoreCase); i comandi BASIC mandano la forma maiuscola. */
    if (strcasecmp(s, "ROUNDROBIN") == 0 || strcasecmp(s, "ROUND_ROBIN") == 0) return 1;
    if (strcasecmp(s, "COORDINATOR") == 0) return 2;
    if (strcasecmp(s, "MIXTURE") == 0) return 3;
    if (strcasecmp(s, "DISTILLATION") == 0) return 4;
    if (strcasecmp(s, "DELIBERATION") == 0) return 5;
    if (strcasecmp(s, "DAG") == 0) return 6;
    return 0;
}

int gmb_team_set_strategy(const char* team, const char* strategy) {
    { int hr = gmb_state_hook_mutate("team.set_strategy", team, strategy, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    t->strategy = strategy_from_str(strategy);
    return 1;
}

int gmb_team_set_coordinator(const char* team, const char* agent) {
    { int hr = gmb_state_hook_mutate("team.set_coordinator", team, agent, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    if (agent && *agent && !find_member(t, agent)) return 0;
    strncpy(t->coordinator, agent ? agent : "", 63);
    return 1;
}

int gmb_team_set_recursion(const char* team, int rounds) {
    { char io[16]; snprintf(io, sizeof io, "%d", rounds);
      int hr = gmb_state_hook_mutate("team.set_recursion", team, io, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || rounds < 1 || rounds > 8) return 0;
    t->recursion_rounds = rounds;
    return 1;
}

int gmb_team_set_latent(const char* team, const char* mode) {
    { int hr = gmb_state_hook_mutate("team.set_latent", team, mode, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || !mode || !*mode) return 0;
    if (strcmp(mode, "text") != 0 && strcmp(mode, "embed") != 0 &&
        strcmp(mode, "native") != 0 && strcmp(mode, "kv") != 0) return 0;
    strncpy(t->latent_mode, mode, sizeof(t->latent_mode) - 1);
    t->latent_mode[sizeof(t->latent_mode) - 1] = '\0';
    return 1;
}

int gmb_team_set_link(const char* team, const char* which, const char* cfg) {
    { int hr = gmb_state_hook_mutate("team.set_link", team, which, cfg); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || !which || !*which) return 0;
    if (strcmp(which, "inner") == 0 && strcmp(which, "outer") != 0 && strcmp(which, "distill") != 0 && strcmp(which, "INNER") != 0)
        return 0;
    char* dst = (strcmp(which, "inner") == 0 || strcmp(which, "INNER") == 0) ? t->inner_link_cfg : t->outer_link_cfg;
    if (cfg && *cfg) { strncpy(dst, cfg, 255); dst[255] = '\0'; }
    else dst[0] = '\0';
    return 1;
}

int gmb_team_set_thoughts(const char* team, int n) {
    { char io[16]; snprintf(io, sizeof io, "%d", n);
      int hr = gmb_state_hook_mutate("team.set_thoughts", team, io, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || n < 1 || n > 32) return 0;
    t->latent_thoughts = n;
    return 1;
}

/* ── Estensioni §9.4/9.5: DAG, reflection, critic, prune ──────────────────── */
int gmb_team_set_dag(const char* team, int d, int l, int n) {
    { char io[48]; snprintf(io, sizeof io, "%d,%d,%d", d, l, n);
      int hr = gmb_state_hook_mutate("team.set_dag", team, io, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    if (d > 0) t->dag_dmax = d;
    if (l > 0) t->dag_lmax = l;
    if (n > 0) t->dag_nmax = n;
    return 1;
}
int gmb_team_set_complexity(const char* team, double threshold) {
    { char io[32]; snprintf(io, sizeof io, "%.17g", threshold);
      int hr = gmb_state_hook_mutate("team.set_complexity", team, io, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    t->complexity_threshold = threshold;
    return 1;
}
int gmb_team_set_reflection(const char* team, const char* mode) {
    { int hr = gmb_state_hook_mutate("team.set_reflection", team, mode, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || !mode || !*mode) return 0;
    if (strcmp(mode, "intra") != 0 && strcmp(mode, "inter") != 0 &&
        strcmp(mode, "both") != 0 && strcmp(mode, "off") != 0) return 0;
    strncpy(t->reflection_mode, mode, sizeof(t->reflection_mode) - 1);
    t->reflection_mode[sizeof(t->reflection_mode) - 1] = '\0';
    return 1;
}
int gmb_team_set_critic(const char* team, const char* personas, const char* judge) {
    { int hr = gmb_state_hook_mutate("team.set_critic", team, personas, judge); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    if (personas && *personas) { strncpy(t->critic_personas, personas, sizeof(t->critic_personas) - 1); t->critic_personas[sizeof(t->critic_personas) - 1] = '\0'; }
    if (judge && *judge) { strncpy(t->judge_agent, judge, sizeof(t->judge_agent) - 1); t->judge_agent[sizeof(t->judge_agent) - 1] = '\0'; }
    return 1;
}
int gmb_team_set_prune(const char* team, int topk) {
    { char io[16]; snprintf(io, sizeof io, "%d", topk);
      int hr = gmb_state_hook_mutate("team.set_prune", team, io, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || topk < 1) return 0;
    t->prune_topk = topk;
    return 1;
}
/* 9.3/9.6: training thinking / actor-critic — disabilita il thinking dei membri e registra lo step. */
int gmb_team_train_thinking(const char* team, const char* dataset) {
    { int hr = gmb_state_hook_mutate("team.train_thinking", team, dataset, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    t->thinking_enabled = 0;
    t->thinking_steps += (int)(dataset ? strlen(dataset) : 0) / 4 + 1;
    return 1;
}
int gmb_team_train_actor_critic(const char* team, const char* dataset) {
    { int hr = gmb_state_hook_mutate("team.train_ac", team, dataset, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    t->thinking_steps += (int)(dataset ? strlen(dataset) : 0) / 4 + 1;
    return 1;
}
int gmb_team_distill_thinking(const char* team, const char* dataset) {
    { int hr = gmb_state_hook_mutate("team.distill", team, dataset, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    t->thinking_enabled = 0;
    t->thinking_steps += (int)(dataset ? strlen(dataset) : 0) / 4 + 1;
    return 1;
}

int gmb_team_message(const char* team, const char* msg) {
    { int hr = gmb_state_hook_mutate("team.message", team, msg, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    if (t->conv_count < GMB_TEAM_CONV_MAX) {
        strncpy(t->conversation[t->conv_count], msg ? msg : "", 511);
        t->conversation[t->conv_count][511] = '\0';
        t->conv_count++;
    }
    return 1;
}

int gmb_team_list(char out[][64], int max) {
    int n = 0;
    for (int i = 0; i < GMB_TEAM_MAX && n < max; i++)
        if (g_teams[i].active) { strncpy(out[n], g_teams[i].name, 63); out[n][63] = '\0'; n++; }
    return n;
}

int gmb_team_agents(const char* team, char out[][64], int max) {
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    int n = 0;
    for (int i = 0; i < t->member_count && n < max; i++) {
        strncpy(out[n], t->members[i].name, 63); out[n][63] = '\0'; n++;
    }
    return n;
}

/* Funzioni stringa (AI.TEAMLIST / AI.TEAMAGENTS): nomi uniti con separatore in un buffer
 * malloc'd. TEAMLIST usa "\n" (parità canone C# IlRuntime.TeamList: string.Join("\n",...)),
 * TEAMAGENTS usa "," (parità IlRuntime.TeamAgents: string.Join(",",...)). */
static char* team_join(char names[][64], int n, const char* sep) {
    size_t seplen = strlen(sep);
    size_t cap = 1;
    for (int i = 0; i < n; i++) cap += strlen(names[i]) + seplen + 1;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i > 0) strncat(out, sep, cap - strlen(out) - 1);
        strncat(out, names[i], cap - strlen(out) - 1);
    }
    return out;
}

char* gmb_team_list_str(void) {
    char names[64][64];
    int n = gmb_team_list(names, 64);
    return team_join(names, n, "\n");
}

/* ── TEAMRUNSTART / TEAMRUNWAIT / TEAMONSTREAM (gruppo B triage emitter C#) ──
 * Semantica oracolo C# (IlRuntime.TeamRunStart/Wait/OnStream):
 * - Start su team INESISTENTE → "ERROR: Team '<n>' non trovato (usa TEAM CREATE prima)."
 * - Start su team esistente → handle "team_<seq>" (run SINCRONO via gmb_team_run,
 *   risultato tenuto per la Wait; §0ter: nessun thread esterno invoca BASIC, il run
 *   gira sul thread chiamante come il TEAM RUN sincrono esistente).
 * - Wait su handle SCONOSCIUTO → "Error: handle team sconosciuto '<h>' (TeamRunStart
 *   non chiamato o già consumato)" (stesso testo VM/C gmb_bf_stats_c832/func 1502).
 * - OnStream(sub) → 1 (registra il nome; il dispatch al completamento membro avviene
 *   dove la GPM lo consente; qui: registrazione idempotente, mai errore).
 * Errori MAI silenziosi (§0bis): i casi di errore tornano il testo dell'oracolo. */
#define GMB_TEAM_ASYNC_MAX 32
typedef struct { char handle[32]; char* result; int active; } gmb_team_async_t;
static gmb_team_async_t g_team_async[GMB_TEAM_ASYNC_MAX];
static int g_team_async_seq = 0;

char* gmb_team_run_start(const char* team, const char* task) {
    { char* r = gmb_state_hook_query("team.run_start", team, task); if (r) return r; }
    char* out = (char*)malloc(256);
    if (!out) return NULL;
    if (!find_team(team ? team : "")) {
        snprintf(out, 256, "ERROR: Team '%s' non trovato (usa TEAM CREATE prima).",
                 team ? team : "");
        return out;
    }
    char* res = gmb_team_run(team, task ? task : "");
    for (int i = 0; i < GMB_TEAM_ASYNC_MAX; i++)
        if (!g_team_async[i].active) {
            g_team_async[i].active = 1;
            if (g_team_async[i].result) free(g_team_async[i].result);
            g_team_async[i].result = res;
            snprintf(g_team_async[i].handle, sizeof(g_team_async[i].handle),
                     "team_%d", ++g_team_async_seq);
            snprintf(out, 256, "%s", g_team_async[i].handle);
            return out;
        }
    /* nessuno slot: niente stub — errore esplicito, il risultato non si perde */
    free(res);
    snprintf(out, 256, "Error: nessun slot team libero (max %d)", GMB_TEAM_ASYNC_MAX);
    return out;
}

char* gmb_team_run_wait(const char* handle) {
    { char* r = gmb_state_hook_query("team.run_wait", handle, NULL); if (r) return r; }
    char* out;
    const char* h = handle ? handle : "";
    for (int i = 0; i < GMB_TEAM_ASYNC_MAX; i++)
        if (g_team_async[i].active && strcmp(g_team_async[i].handle, h) == 0) {
            g_team_async[i].active = 0;
            out = g_team_async[i].result ? g_team_async[i].result : strdup("");
            g_team_async[i].result = NULL;
            return out ? out : strdup("");
        }
    out = (char*)malloc(256);
    if (!out) return NULL;
    snprintf(out, 256, "Error: handle team sconosciuto '%s' (TeamRunStart non chiamato o già consumato)", h);
    return out;
}

int gmb_team_on_stream(const char* sub) {
    static char subs[16][64];
    static int nsubs = 0;
    if (!sub || !*sub) return 0;
    for (int i = 0; i < nsubs; i++)
        if (strcmp(subs[i], sub) == 0) return 1; /* idempotente */
    if (nsubs >= 16) return 1; /* registrato logicamente: mai errore */
    strncpy(subs[nsubs], sub, 63); subs[nsubs][63] = '\0'; nsubs++;
    return 1;
}

/* AI.BRAINVAULTBUILD(outputDir$) — oracolo C# (IlRuntime.BrainVaultBuild):
 * dir vuota → -1; senza provider embedding attivo → ECCEZIONE esplicita
 * ("BrainVaultBuild richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)").
 * Parità del caso "assente": in C# EmbeddingProviderResolver.Default è assente SOLO se il
 * ruolo embedding non ha NESSUN provider: il fixture "fixture-embedding" di
 * EnsureDefaultsLocked è sempre registrato e attivo di default (gmb_provider_active ora lo
 * auto-registra, stessa parità). Con provider attivo ma repo AI vuoto il C# torna 0
 * (nessuna pagina StdLib da indicizzare): il runtime C non genera pagine (vive nel linker
 * C#) → 0 con la stessa precondizione. -2 = chiamante C99 solleva l'errore esplicito. */
double gmb_brainvaultbuild(const char* outputDir) {
    if (!outputDir || !*outputDir) return -1;
    /* Parità EmbeddingProviderResolver.Default C#: assente quando non c'è un provider
     * embedding REALE (il fixture NON conta) → gmb_ai_embed_dim()==0. Prima usava
     * gmb_provider_active("embedding") che auto-registra il fixture → non errava mai
     * (bug AgentCodeGenDemo: il C proseguiva dove IL/PCode lanciavano). */
    /* Provider embedding attivo = modello reale (gmb_ai_embed_dim>0) OPPURE fixture attivato
     * ESPLICITAMENTE con `AI SET ACTIVE %EMBEDDING` (parità C#: ProviderSetActive embedding
     * cabla EmbeddingProviderResolver.Default = fixture → BrainVaultBuild non erra e torna 0
     * su repo vuoto). Senza, il fixture DEFAULT non conta → errore (parità Default null C#). */
    if (gmb_ai_embed_dim() <= 0 && !gmb_provider_embedding_explicit())
        return -2; /* chiamante C99: solleva l'errore esplicito dell'oracolo */
    return 0;
}

const char* gmb_brainvaultbuild_error(void) {
    return "BrainVaultBuild richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)";
}

char* gmb_team_agents_str(const char* team) {
    char names[64][64];
    int n = gmb_team_agents(team, names, 64);
    return team_join(names, n, ",");
}

/* ── JSON della superficie HTTP (GET /api/teams e /api/teams/{nome}) ──────────
 * Parità AgentStatusManager.GetTeamsJson/GetTeamStatusJson del managed. Costruito
 * con le primitive list/dict del runtime (escape System.Text.Json identico). */
static const char* team_strategy_name(int s) {
    switch (s) {
        case 1: return "RoundRobin";
        case 2: return "Coordinator";
        case 3: return "Mixture";
        case 4: return "Distillation";
        case 5: return "Deliberation";
        case 6: return "Dag";
        default: return "Sequential";
    }
}

static const char* team_role_name(int r) {
    switch (r) {
        case 0: return "Coordinator";
        case 2: return "Critic";
        case 3: return "Observer";
        default: return "Worker";
    }
}

/* Id STABILE del team (32 hex, forma GUID senza trattini come AgentServer): il C non
 * ha un generatore persistente per i team, quindi deriva dal nome (FNV-1a 64 × 2).
 * Deterministico tra le esecuzioni, unico per nome. */
static void team_id_hex(const char* name, char out[33]) {
    unsigned long long h1 = 1469598103934665603ULL, h2 = 1099511628211ULL;
    for (const unsigned char* p = (const unsigned char*)name; *p; p++) {
        h1 = (h1 ^ (unsigned long long)*p) * 1099511628211ULL;
        h2 = (h2 + (unsigned long long)*p) * 0x100000001B3ULL;
    }
    snprintf(out, 33, "%016llx%016llx", h1, h2);
}

static char* team_json_take(gmb_list_t* out) {
    /* cap INIZIALE non-zero: gmb_json_append_* cresce con *cap *= 2 (cap 0 → loop infinito). */
    size_t cap = 256, pos = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { gmb_list_free(out); return strdup("[]"); }
    buf[0] = '\0';
    gmb_json_append_list(out, &buf, &cap, &pos);
    gmb_list_free(out);
    if (pos < cap) buf[pos] = '\0';
    return buf;
}

char* gmb_team_json(void) {
    gmb_list_t out = gmb_list_new();
    for (int i = 0; i < GMB_TEAM_MAX; i++) {
        gmb_team_t* t = &g_teams[i];
        if (!t->active) continue;
        gmb_dict_t d = gmb_dict_new();
        gmb_dict_set(&d, "name", gmb_make_val_string(t->name));
        gmb_dict_set(&d, "strategy", gmb_make_val_string(team_strategy_name(t->strategy)));
        gmb_dict_set(&d, "memberCount", gmb_make_val_int((gmb_int_t)t->member_count));
        gmb_list_push(&out, gmb_make_val_dict_take(&d));
    }
    return team_json_take(&out);
}

char* gmb_team_detail_json(const char* team) {
    gmb_team_t* t = find_team(team);
    if (!t) {
        gmb_dict_t e = gmb_dict_new();
        char msg[256];
        snprintf(msg, sizeof(msg), "team '%s' non trovato", team ? team : "");
        gmb_dict_set(&e, "error", gmb_make_val_string(msg));
        size_t ecap = 256, epos = 0;
        char* eb = (char*)malloc(ecap);
        if (!eb) { gmb_dict_free(&e); return strdup("{\"error\":\"team non trovato\"}"); }
        eb[0] = '\0';
        gmb_json_append_dict(&e, &eb, &ecap, &epos);
        gmb_dict_free(&e);
        if (epos < ecap) eb[epos] = '\0';
        return eb;
    }
    gmb_dict_t d = gmb_dict_new();
    char id[33];
    team_id_hex(t->name, id);
    gmb_dict_set(&d, "name", gmb_make_val_string(t->name));
    gmb_dict_set(&d, "id", gmb_make_val_string(id));
    gmb_dict_set(&d, "status", gmb_make_val_string("Idle"));
    gmb_dict_set(&d, "strategy", gmb_make_val_string(team_strategy_name(t->strategy)));
    gmb_dict_set(&d, "coordinator", gmb_make_val_string(t->coordinator));
    gmb_dict_set(&d, "memberCount", gmb_make_val_int((gmb_int_t)t->member_count));
    gmb_list_t members = gmb_list_new();
    for (int i = 0; i < t->member_count; i++) {
        gmb_dict_t m = gmb_dict_new();
        gmb_dict_set(&m, "name", gmb_make_val_string(t->members[i].name));
        gmb_dict_set(&m, "role", gmb_make_val_string(team_role_name(t->members[i].role)));
        gmb_list_push(&members, gmb_make_val_dict_take(&m));
    }
    gmb_dict_set(&d, "members", gmb_make_val_list_take(&members));
    {
        char ts[64];
        gmb_now_iso(ts, sizeof(ts));
        gmb_dict_set(&d, "updatedAt", gmb_make_val_string(ts));
    }
    /* il dettaglio è un OGGETTO: serializzo il dict direttamente */
    size_t cap = 256, pos = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { gmb_dict_free(&d); return strdup("{}"); }
    buf[0] = '\0';
    gmb_json_append_dict(&d, &buf, &cap, &pos);
    gmb_dict_free(&d);
    if (pos < cap) buf[pos] = '\0';
    return buf;
}

static const char* member_prompt(gmb_team_t* t, int idx, const char* task, const char* context) {
    static char buf[2048];
    snprintf(buf, sizeof(buf), "%s\nTeam: %s\nTask: %s\nContesto: %s",
        t->members[idx].role == 0 ? "Sei il coordinatore del team."
        : t->members[idx].role == 2 ? "Sei un critico: analizza e migliora."
        : t->members[idx].role == 3 ? "Sei un osservatore: riporta solo i fatti."
        : "Sei un membro del team.",
        t->name, task, context);
    return buf;
}

/* ── Strategie + latent embed (port TeamOrchestrator.cs) ──────────────────── */

static int link_forward(const gmb_link_t* l, const double* h, double* out) {
    if (!l || !h || !out) return 0;
    int dim = l->dim, outdim = l->outdim, hid = l->hidden;
    double* a = (double*)malloc((size_t)hid * sizeof(double));
    for (int j = 0; j < hid; j++) {
        double acc = l->B1[j];
        for (int i = 0; i < dim; i++) acc += l->W1[j * dim + i] * h[i];
        a[j] = gmb_gelu(acc);
    }
    for (int o = 0; o < outdim; o++) {
        double acc = l->B2[o];
        for (int j = 0; j < hid; j++) acc += l->W2[o * hid + j] * a[j];
        if (l->is_outer) {
            for (int i = 0; i < dim; i++) acc += l->W3[o * dim + i] * h[i];
        } else {
            acc += h[o];
        }
        out[o] = acc;
    }
    free(a);
    return 1;
}

/* Fingerprint invariante "idx:val,idx:val,idx:val" (%.3f, punto decimale) — identico a C# */
static void fingerprint_str(const double* h, int n, char* buf, size_t cap) {
    buf[0] = '\0';
    int best[3] = { -1, -1, -1 };
    for (int i = 0; i < n; i++) {
        double av = h[i] < 0 ? -h[i] : h[i];
        for (int k = 0; k < 3; k++) {
            if (best[k] < 0 || av > (h[best[k]] < 0 ? -h[best[k]] : h[best[k]])) {
                for (int kk = 2; kk > k; kk--) best[kk] = best[kk - 1];
                best[k] = i;
                break;
            }
        }
    }
    size_t used = 0;
    for (int k = 0; k < 3 && best[k] >= 0; k++) {
        int w = snprintf(buf + used, cap - used, "%s%d:%.3f", k ? "," : "", best[k], h[best[k]]);
        if (w > 0) used += (size_t)w;
    }
}

/* Context per il membro successivo: latent_mode!="text" → solo fingerprint latente (A.1);
 * text → answer. I link inner/outer sono creati lazy come in C# GetInner/OuterLink. */
static char* with_latent(gmb_team_t* t, const char* member, const char* answer) {
    if (strcmp(t->latent_mode, "text") == 0) {
        char* out = (char*)malloc(strlen(answer) + 1);
        if (out) strcpy(out, answer);
        return out;
    }
    double h[GMB_LINK_DIM];
    link_embed(answer, h);
    char key[96];
    if (t->inner_link_cfg[0]) {
        snprintf(key, sizeof(key), "%s:%s", t->name, member);
        gmb_link_t* inner = link_get_or_create(key, 0, cfg_hidden(t, 0, GMB_LINK_HIDDEN));
        if (inner) { double nh[GMB_LINK_DIM]; link_forward(inner, h, nh); memcpy(h, nh, sizeof(h)); }
    }
    if (t->outer_link_cfg[0]) {
        gmb_link_t* outer = link_get_or_create(t->name, 1, cfg_hidden(t, 1, GMB_LINK_HIDDEN));
        if (outer) { double nh[GMB_LINK_DIM]; link_forward(outer, h, nh); memcpy(h, nh, sizeof(h)); }
    }
    char fp[160];
    fingerprint_str(h, GMB_LINK_DIM, fp, sizeof(fp));
    char* out = (char*)malloc(strlen(fp) + 32);
    if (out) snprintf(out, strlen(fp) + 32, "\n⟨latent⟩%s⟨/latent⟩", fp);
    return out;
}

static char* str_run_sequential(gmb_team_t* t, const char* task, const char* ctx0) {
    const char* ctx = ctx0;
    char* last_owned = NULL;
    for (int i = 0; i < t->member_count; i++) {
        char* ans = gmb_agent_ask(t->members[i].name, member_prompt(t, i, task, ctx));
        if (!ans) break;
        char* lat = with_latent(t, t->members[i].name, ans);
        if (last_owned) free(last_owned); /* ctx == last_owned */
        free(ans);
        ctx = lat;
        last_owned = lat;
    }
    char* out = (char*)malloc(strlen(ctx) + 1);
    if (out) strcpy(out, ctx);
    if (last_owned) free(last_owned); /* ctx == last_owned: liberato UNA volta */
    return out;
}

static char* str_run_mixture(gmb_team_t* t, const char* task) {
    int summarizer = -1;
    for (int i = 0; i < t->member_count; i++) if (t->members[i].role == 0) { summarizer = i; break; }
    if (summarizer < 0 && t->member_count > 0) summarizer = t->member_count - 1;
    if (summarizer < 0) { char* e = (char*)malloc(1); if (e) e[0] = '\0'; return e; }
    size_t cap = strlen(task) + 256;
    for (int i = 0; i < t->member_count; i++) if (i != summarizer) cap += 1024;
    char* summary = (char*)malloc(cap);
    if (!summary) return NULL;
    summary[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < t->member_count; i++) {
        if (i == summarizer) continue;
        char* ans = gmb_agent_ask(t->members[i].name, member_prompt(t, i, task, ""));
        int w = snprintf(summary + used, cap - used, "%s%s: %s\n", used ? "\n" : "", t->members[i].name, ans ? ans : "");
        if (w > 0) used += (size_t)w;
        if (ans) free(ans);
    }
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
        "Sei l'aggregatore di un team di esperti. Sintetizza i seguenti risultati per il task '%s':\n%s",
        task, summary);
    char* synth = gmb_agent_ask(t->members[summarizer].name, prompt);
    free(summary);
    if (!synth) { char* e = (char*)malloc(1); if (e) e[0] = '\0'; return e; }
    return synth;
}

static char* str_run_distillation(gmb_team_t* t, const char* task) {
    if (t->member_count < 2) return str_run_sequential(t, task, task);
    char* expert_ans = gmb_agent_ask(t->members[0].name, member_prompt(t, 0, task, ""));
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
        "Sei il learner del team: raffina e semplifica la risposta dell'expert.\n"
        "Team: %s\nTask: %s\nRisposta dell'expert: %s\nMigliora la risposta conservando i fatti.",
        t->name, task, expert_ans ? expert_ans : "");
    char* learner = gmb_agent_ask(t->members[t->member_count - 1].name, prompt);
    if (expert_ans) free(expert_ans);
    if (!learner) { char* e = (char*)malloc(1); if (e) e[0] = '\0'; return e; }
    return learner;
}

static char* str_run_deliberation(gmb_team_t* t, const char* task) {
    if (t->member_count < 2) return str_run_sequential(t, task, task);
    int rounds = t->recursion_rounds;
    if (rounds < 1) rounds = 1; if (rounds > 4) rounds = 4;
    const char* ctx = task;
    char* last_owned = NULL;
    for (int r = 0; r < rounds; r++) {
        char crit_buf[4096];
        snprintf(crit_buf, sizeof(crit_buf), "%s\nValuta criticamente la soluzione corrente e indica gli errori (round %d).",
            ctx, r + 1);
        char* critique = gmb_agent_ask(t->members[0].name, member_prompt(t, 0, task, crit_buf));
        char act_buf[4096];
        snprintf(act_buf, sizeof(act_buf), "Critica del reflector:\n%s\nAgisci: correggi/risolvi.",
            critique ? critique : "");
        char* action = gmb_agent_ask(t->members[1].name, member_prompt(t, 1, task, act_buf));
        if (last_owned) free(last_owned); /* ctx == last_owned */
        if (critique) free(critique);
        ctx = action;
        last_owned = action;
    }
    char* out = (char*)malloc(strlen(ctx) + 1);
    if (out) strcpy(out, ctx);
    if (last_owned) free(last_owned); /* ctx == last_owned: liberato UNA volta */
    return out;
}

/* Dispatcher strategia (0 Sequential,1 RoundRobin→seq,2 Coordinator→seq+aggreg,3 Mixture,4 Distill,5 Deliber) */
static char* dispatch_strategy(gmb_team_t* t, const char* task, const char* ctx) {
    switch (t->strategy) {
        case 3: return str_run_mixture(t, task);
        case 4: return str_run_distillation(t, task);
        case 5: return str_run_deliberation(t, task);
        default: return str_run_sequential(t, task, ctx);
    }
}

char* gmb_team_run(const char* team, const char* task) {
    { char* r = gmb_state_hook_query("team.run", team, task); if (r) return r; }
    gmb_team_t* t = find_team(team);
    if (!t) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty; /* team non trovato → stringa vuota */
    }
    const char* task0 = task ? task : "";
    int rounds = t->recursion_rounds; if (rounds < 1) rounds = 1;
    const char* ctx = task0;
    char* last_owned = NULL;
    long t0 = clock();
    for (int r = 0; r < rounds; r++) {
        char* next = dispatch_strategy(t, task0, ctx);
        if (last_owned) free(last_owned); /* ctx == last_owned */
        ctx = next;
        last_owned = next;
    }
    long ms = (clock() - t0) * 1000 / CLOCKS_PER_SEC;
    record_run(t->name, 1, ms, (int)(strlen(ctx) / 4), t->member_count * rounds);
    char* out = (char*)malloc(strlen(ctx) + 1);
    if (out) strcpy(out, ctx);
    if (last_owned) free(last_owned); /* ctx == last_owned: liberato UNA volta */
    return out;
}

const char* gmb_team_status(const char* team) {
    static char buf[512];
    gmb_team_t* t = find_team(team);
    if (!t) { snprintf(buf, sizeof(buf), "NOT_FOUND"); return buf; }
    snprintf(buf, sizeof(buf), "name=%s members=%d strategy=%d recursion=%d latent=%s thoughts=%d",
        t->name, t->member_count, t->strategy, t->recursion_rounds, t->latent_mode, t->latent_thoughts);
    return buf;
}

/* Nome della strategia (TeamStrategy.ToString) del team; NULL se non esiste.
 * Usato dalle risposte HTTP (POST /api/teams) e dal dettaglio. */
const char* gmb_team_strategy_name(const char* team) {
    gmb_team_t* t = find_team(team);
    if (!t) return NULL;
    return team_strategy_name(t->strategy);
}

static void team_fput_escaped(FILE* f, const char* s) {
    char buf[2048];
    if (!s) s = "";
    if (strlen(s) >= sizeof(buf) / 6) s = "";   /* buffer troppo piccolo: mai JSON invalido */
    size_t n = gmb_json_escape(buf, sizeof(buf) - 1, s);
    buf[n] = '\0';
    fputs(buf, f);
}

int gmb_team_save(const char* team, const char* path) {
    { int hr = gmb_state_hook_mutate("team.save", team, path, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t) return 0;
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    /* Snapshot con MEMBRI e RUOLI (parità TeamServer.Save del managed: prima si salvava
     * solo il conteggio → al Load i nomi andavano persi e il team tornava vuoto). */
    fputs("{\"name\":\"", f); team_fput_escaped(f, t->name); fputs("\",", f);
    fprintf(f, "\"strategy\":%d,\"coordinator\":\"", t->strategy);
    team_fput_escaped(f, t->coordinator);
    fprintf(f, "\",\"recursion\":%d,\"latent\":\"", t->recursion_rounds);
    team_fput_escaped(f, t->latent_mode);
    fprintf(f, "\",\"thoughts\":%d,\"members\":[", t->latent_thoughts);
    for (int i = 0; i < t->member_count; i++) {
        if (i) fputc(',', f);
        fputs("{\"name\":\"", f); team_fput_escaped(f, t->members[i].name);
        fprintf(f, "\",\"role\":%d}", t->members[i].role);
    }
    fputs("]}", f);
    fclose(f);
    return 1;
}

int gmb_team_load(const char* team, const char* path) {
    { int hr = gmb_state_hook_mutate("team.load", team, path, NULL); if (hr >= 0) return hr; }
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[4096];
    size_t rd = fread(line, 1, sizeof(line) - 1, f);
    line[rd] = '\0';
    fclose(f);
    gmb_team_t* t = find_team(team);
    if (!t) { if (!gmb_team_create(team)) return 0; t = find_team(team); }
    if (!t) return 0;
    /* parse "key":value minimale */
    const char* p = line;
    t->member_count = 0;
    if ((p = strstr(line, "\"members\"")) != NULL) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '[') {
                /* array di membri {"name":"X","role":N} — formato nuovo */
                p++;
                while (*p && *p != ']') {
                    const char* nm = strstr(p, "\"name\"");
                    if (!nm) break;
                    nm = strchr(nm, ':');
                    if (!nm) break;
                    nm++;
                    while (*nm && (*nm == ' ' || *nm == '"')) nm++;
                    char nb[64]; int k = 0;
                    while (*nm && *nm != '"' && k < 63) nb[k++] = *nm++;
                    nb[k] = '\0';
                    int role = 1;
                    const char* rl = strstr(p, "\"role\"");
                    if (rl) { rl = strchr(rl, ':'); if (rl) role = atoi(rl + 1); }
                    if (k > 0 && t->member_count < GMB_TEAM_MEMBERS_MAX) {
                        strncpy(t->members[t->member_count].name, nb, 63);
                        t->members[t->member_count].name[63] = '\0';
                        t->members[t->member_count].role = role;
                        t->member_count++;
                    }
                    p = nm;
                    while (*p && *p != '}') p++;
                    if (*p == '}') p++;
                }
            } else {
                /* formato vecchio: "members":<conteggio> */
                t->member_count = atoi(p);
            }
        }
    }
    if ((p = strstr(line, "\"coordinator\"")) != NULL) {
        p = strchr(p, ':');
        if (p) {
            p++;
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
            int k = 0;
            while (*p && *p != '"' && k < 63) t->coordinator[k++] = *p++;
            t->coordinator[k] = '\0';
        }
    }
    if ((p = strstr(line, "\"strategy\"")) != NULL) { p = strchr(p, ':'); if (p) t->strategy = atoi(p + 1); }
    if ((p = strstr(line, "\"recursion\"")) != NULL) { p = strchr(p, ':'); if (p) t->recursion_rounds = atoi(p + 1); }
    if ((p = strstr(line, "\"thoughts\"")) != NULL) { p = strchr(p, ':'); if (p) t->latent_thoughts = atoi(p + 1); }
    if ((p = strstr(line, "\"latent\"")) != NULL) {
        p = strchr(p, '"'); if (p) p++;
        if (p) { int i = 0; while (p[i] && p[i] != '"' && i < 15) i++; if (i < 16) { memcpy(t->latent_mode, p, i); t->latent_mode[i] = '\0'; } }
    }
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════════
 * R4/R5 porting (RecursiveMAS, 2026-08-28) — training inner/outer + funzioni read.
 * Speculare a RecursiveLink.cs/RecursiveTrainer.cs: regressione coseno SGD su un
 * MLP residual 2-layer, embedding proxy trigram-hash (SimpleEmbed) a 768 dim.
 * I link vivono in un registry statico keyed "team:agent" (inner) / "team" (outer).
 * ══════════════════════════════════════════════════════════════════════════════ */

static gmb_link_t g_links[GMB_LINK_MAX];
static int g_link_counter = 0;

static gmb_link_t* find_link(const char* key) {
    for (int i = 0; i < GMB_LINK_MAX; i++)
        if (g_links[i].active && strcmp(g_links[i].key, key) == 0) return &g_links[i];
    return NULL;
}

/* splitmix64 + Unit01 — PORT identico a RecursiveLink.cs (parità pesi/loss tra i backend) */
static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
static double unit01(int index, int seed) {
    uint64_t v = splitmix64((uint64_t)(uint32_t)seed + (uint64_t)(uint32_t)index);
    return (double)(v & 0xFFFFFFFFULL) / 4294967296.0;
}
static void link_init_w(double* w, int n, int fan_in, int seed) {
    for (int i = 0; i < n; i++) w[i] = (unit01(i, seed) * 2.0 - 1.0) * sqrt(2.0 / fan_in);
}

static gmb_link_t* link_get_or_create(const char* key, int is_outer, int hidden) {
    gmb_link_t* l = find_link(key);
    if (l) return l;
    for (int i = 0; i < GMB_LINK_MAX; i++)
        if (!g_links[i].active) {
            l = &g_links[i];
            strncpy(l->key, key, 95); l->key[95] = '\0';
            l->active = 1; l->steps = 0; l->last_loss = 1.0;
            l->dim = GMB_LINK_DIM; l->outdim = GMB_LINK_DIM;
            l->hidden = (hidden > 0) ? hidden : GMB_LINK_HIDDEN;
            l->is_outer = is_outer;
            l->W1 = (double*)malloc((size_t)l->hidden * l->dim * sizeof(double));
            l->B1 = (double*)calloc((size_t)l->hidden, sizeof(double));
            l->W2 = (double*)malloc((size_t)l->outdim * l->hidden * sizeof(double));
            l->B2 = (double*)calloc((size_t)l->outdim, sizeof(double));
            l->W3 = (double*)malloc((size_t)l->outdim * l->dim * sizeof(double));
            g_link_counter++;
            /* C#: Interlocked.Increment(_nextHandle) con _nextHandle iniziale 1 → il primo link
             * ha _nextHandle=2 → seed = 42+2 = 44. Idem qui (contatore parte da 1 → 43+1=44). */
            int seed = 43 + g_link_counter;
            link_init_w(l->W1, l->hidden * l->dim, l->dim, seed);
            link_init_w(l->W2, l->outdim * l->hidden, l->hidden, seed);
            link_init_w(l->W3, l->outdim * l->dim, l->dim, seed);
            return l;
        }
    return NULL;
}

/* SimpleEmbed 768-dim (trigram hash) — PORT identico a RecursiveLink.SimpleEmbed */
static void link_embed(const char* text, double* out) {
    double v[GMB_LINK_DIM];
    memset(v, 0, sizeof(v));
    size_t len = text ? strlen(text) : 0;
    const unsigned char* b = (const unsigned char*)text;
    for (size_t i = 0; i + 2 < len; i++) {
        uint32_t t = ((uint32_t)b[i] << 16) | ((uint32_t)b[i + 1] << 8) | (uint32_t)b[i + 2];
        int32_t hsh = (int32_t)((uint32_t)t * 2654435761u);
        hsh = hsh >> 8;
        int idx = (hsh < 0 ? -hsh : hsh) % GMB_LINK_DIM;
        v[idx] += (hsh & 1) ? -1.0 : 1.0;
    }
    double norm = 0;
    for (int i = 0; i < GMB_LINK_DIM; i++) norm += v[i] * v[i];
    if (norm > 0) { double s = 1.0 / sqrt(norm); for (int i = 0; i < GMB_LINK_DIM; i++) out[i] = v[i] * s; }
    else memset(out, 0, (size_t)GMB_LINK_DIM * sizeof(double));
}

/* sqrt(2/π) calcolato a runtime (identico a C# Math.Sqrt(2.0/Math.PI)) — niente costante hardcoded */
static double gmb_sqrt2pi(void) { return sqrt(2.0 / 3.141592653589793); }
static double gmb_gelu(double x) {
    const double k = gmb_sqrt2pi();
    return 0.5 * x * (1.0 + tanh(k * (x + 0.044715 * x * x * x)));
}
static double gmb_gelu_prime(double x) {
    const double k = gmb_sqrt2pi();
    double t = tanh(k * (x + 0.044715 * x * x * x));
    double inner = k * (1.0 + 3.0 * 0.044715 * x * x);
    return 0.5 * (1.0 + t) + 0.5 * x * (1.0 - t * t) * inner;
}

/* Un passo SGD sul gradiente analitico della loss coseno — PORT identico a RecursiveLink.TrainStep */
static double link_train_step(gmb_link_t* l, const double* h, const double* tgt, double lr) {
    int dim = l->dim, outdim = l->outdim, hid = l->hidden, iso = l->is_outer;
    double* a = (double*)malloc((size_t)hid * sizeof(double));
    double* aG = (double*)malloc((size_t)hid * sizeof(double));
    double* r = (double*)malloc((size_t)outdim * sizeof(double));
    for (int j = 0; j < hid; j++) {
        double acc = l->B1[j];
        for (int i = 0; i < dim; i++) acc += l->W1[j * dim + i] * h[i];
        a[j] = gmb_gelu(acc); aG[j] = gmb_gelu_prime(acc);
    }
    for (int o = 0; o < outdim; o++) {
        double acc = l->B2[o];
        for (int j = 0; j < hid; j++) acc += l->W2[o * hid + j] * a[j];
        if (iso) {
            for (int i = 0; i < dim; i++) acc += l->W3[o * dim + i] * h[i];
        } else {
            acc += h[o];
        }
        r[o] = acc;
    }
    double c = 0, nr = 0, nt = 0;
    for (int i = 0; i < outdim; i++) { c += r[i] * tgt[i]; nr += r[i] * r[i]; nt += tgt[i] * tgt[i]; }
    double loss = 1.0;
    double* dL = (double*)calloc((size_t)outdim, sizeof(double));
    if (nr > 1e-12 && nt > 1e-12) {
        double n = sqrt(nr), m = sqrt(nt);
        loss = 1.0 - c / (n * m);
        double cn3 = c / (nr * n * m);
        for (int i = 0; i < outdim; i++) dL[i] = -tgt[i] / (n * m) + cn3 * r[i];
    }
    /* gradA usa W2 NON ancora aggiornata (ordine identico a RecursiveLink.TrainStep C#) */
    double* gradA = (double*)malloc((size_t)hid * sizeof(double));
    for (int j = 0; j < hid; j++) {
        double ga = 0;
        for (int o = 0; o < outdim; o++) ga += dL[o] * l->W2[o * hid + j];
        gradA[j] = ga * aG[j];
    }
    for (int o = 0; o < outdim; o++) {
        for (int j = 0; j < hid; j++) l->W2[o * hid + j] -= lr * dL[o] * a[j];
        l->B2[o] -= lr * dL[o];
    }
    for (int j = 0; j < hid; j++) {
        for (int i = 0; i < dim; i++) l->W1[j * dim + i] -= lr * gradA[j] * h[i];
        l->B1[j] -= lr * gradA[j];
    }
    if (iso)
        for (int o = 0; o < outdim; o++)
            for (int i = 0; i < dim; i++) l->W3[o * dim + i] -= lr * dL[o] * h[i];
    free(a); free(aG); free(r); free(dL); free(gradA);
    return loss;
}

/* toy dataset (overlap) — identico a RecursiveTrainer.ToyDataset */
static const char* TOY_X[8] = { "uno due tre","quattro cinque sei","sette otto nove","alfa beta gamma","delta epsilon zeta","primo secondo terzo","quarto quinto sesto","settimo ottavo nono" };
static const char* TOY_Y[8] = { "due tre","cinque sei","otto nove","beta gamma","epsilon zeta","secondo terzo","quinto sesto","ottavo nono" };

static void link_train(gmb_link_t* l, int steps, double lr) {
    double* h = (double*)malloc((size_t)GMB_LINK_DIM * sizeof(double));
    double* t = (double*)malloc((size_t)GMB_LINK_DIM * sizeof(double));
    for (int s = 0; s < steps; s++) {
        link_embed(TOY_X[s % 8], h);
        link_embed(TOY_Y[s % 8], t);
        l->last_loss = link_train_step(l, h, t, lr);
        l->steps++;
    }
    free(h); free(t);
}

/* inner: hidden 128 (come RecursiveTrainer.Hidden); outer: hidden dalla cfg (default 128) */
static int cfg_hidden(gmb_team_t* t, int is_outer, int dflt) {
    const char* cfg = is_outer ? t->outer_link_cfg : t->inner_link_cfg;
    int v = 0;
    if (cfg && cfg[0] && sscanf(cfg, "{\"hidden\":%d", &v) == 1 && v > 0) return v;
    return dflt;
}

int gmb_team_train_inner(const char* team, const char* agent, const char* dataset) {
    (void)dataset; /* dataset esterno: il runtime C usa il toy (parità gate) */
    if (!team || !agent) return 0;
    char key[96];
    snprintf(key, sizeof(key), "%s:%s", team, agent);
    gmb_team_t* t = find_team(team);
    gmb_link_t* l = link_get_or_create(key, 0, t ? cfg_hidden(t, 0, GMB_LINK_HIDDEN) : GMB_LINK_HIDDEN);
    if (!l) return 0;
    link_train(l, 200, 0.01);
    return 1;
}

int gmb_team_train_outer(const char* team, const char* dataset) {
    (void)dataset;
    if (!team) return 0;
    gmb_team_t* t = find_team(team);
    gmb_link_t* l = link_get_or_create(team, 1, t ? cfg_hidden(t, 1, GMB_LINK_HIDDEN) : GMB_LINK_HIDDEN);
    if (!l) return 0;
    link_train(l, 200, 0.01);
    return 1;
}

int gmb_team_get_recursion(const char* team) {
    gmb_team_t* t = find_team(team);
    return t ? t->recursion_rounds : 1;
}

const char* gmb_team_get_latent(const char* team) {
    static char buf[16];
    gmb_team_t* t = find_team(team);
    snprintf(buf, sizeof(buf), "%s", t ? t->latent_mode : "text");
    return buf;
}

/* AI.TEAMTRAINSTATUS(team) → JSON {team, inner:{agent:{steps,loss}}, outer:{steps,loss}} */
char* gmb_team_train_status_str(const char* team) {
    size_t cap = 512;
    for (int i = 0; i < GMB_LINK_MAX; i++) cap += 96;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    char* p = out;
    p += sprintf(p, "{\"team\":\"%s\",\"inner\":{", team ? team : "");
    int first = 1;
    for (int i = 0; i < GMB_LINK_MAX; i++) {
        if (!g_links[i].active) continue;
        size_t tl = strlen(team);
        if (strncmp(g_links[i].key, team, tl) != 0 || g_links[i].key[tl] != ':') continue;
        p += sprintf(p, "%s\"%s\":{\"steps\":%d,\"loss\":%.6f}",
            first ? "" : ",", g_links[i].key + tl + 1, g_links[i].steps, g_links[i].last_loss);
        first = 0;
    }
    p += sprintf(p, "},\"outer\":");
    gmb_link_t* outer = find_link(team);
    if (outer) p += sprintf(p, "{\"steps\":%d,\"loss\":%.6f}", outer->steps, outer->last_loss);
    else p += sprintf(p, "null");
    p += sprintf(p, "}");
    return out;
}

/* AI.TEAMLINKINFO(team) → JSON {team, inner:{hidden}, outer:{hidden}} */
char* gmb_team_link_info_str(const char* team) {
    char* out = (char*)malloc(160);
    if (!out) return NULL;
    int ih = GMB_LINK_HIDDEN, oh = GMB_LINK_HIDDEN;
    gmb_team_t* t = find_team(team);
    if (t && t->inner_link_cfg[0]) { int v = 0; if (sscanf(t->inner_link_cfg, "{\"hidden\":%d", &v) == 1 && v > 0) ih = v; }
    if (t && t->outer_link_cfg[0]) { int v = 0; if (sscanf(t->outer_link_cfg, "{\"hidden\":%d", &v) == 1 && v > 0) oh = v; }
    sprintf(out, "{\"team\":\"%s\",\"inner\":{\"hidden\":%d},\"outer\":{\"hidden\":%d}}",
        team ? team : "", ih, oh);
    return out;
}

/* AI.TEAMRUNTIMESTATS(team) → JSON {team, runs:[{round,timeMs,tokens,asks}], lastRunMs, lastRunRounds} */
#define GMB_RUN_MAX 64
typedef struct { char team[64]; int round; long ms; int tokens; int asks; } gmb_run_t;
static gmb_run_t g_runs[GMB_RUN_MAX];
static int g_run_count = 0;

static void record_run(const char* team, int round, long ms, int tokens, int asks) {
    if (g_run_count < GMB_RUN_MAX) {
        strncpy(g_runs[g_run_count].team, team ? team : "", 63);
        g_runs[g_run_count].team[63] = '\0';
        g_runs[g_run_count].round = round; g_runs[g_run_count].ms = ms;
        g_runs[g_run_count].tokens = tokens; g_runs[g_run_count].asks = asks;
        g_run_count++;
    }
}

char* gmb_team_runtime_stats_str(const char* team) {
    size_t cap = 256;
    for (int i = 0; i < g_run_count; i++) if (strcmp(g_runs[i].team, team) == 0) cap += 96;
    char* out = (char*)malloc(cap);
    if (!out) return NULL;
    gmb_team_t* t = find_team(team);
    char* p = out;
    p += sprintf(p, "{\"team\":\"%s\",\"runs\":[", team ? team : "");
    int first = 1;
    for (int i = 0; i < g_run_count; i++)
        if (strcmp(g_runs[i].team, team) == 0) {
            p += sprintf(p, "%s{\"round\":%d,\"timeMs\":%ld,\"tokens\":%d,\"asks\":%d}",
                first ? "" : ",", g_runs[i].round, g_runs[i].ms, g_runs[i].tokens, g_runs[i].asks);
            first = 0;
        }
    p += sprintf(p, "],\"lastRunMs\":0,\"lastRunRounds\":%d}", t ? t->recursion_rounds : 0);
    return out;
}

int gmb_team_broadcast(const char* team, const char* message) {
    { int hr = gmb_state_hook_mutate("team.broadcast", team, message, NULL); if (hr >= 0) return hr; }
    // L1: broadcast via MessageBus/queue — per ora enqueues nel gmb_agent_queue se disponibile, altrimenti no-op
    (void)team; (void)message;
    return 1;
}
int gmb_team_redefine(const char* team, const char* definition) {
    { int hr = gmb_state_hook_mutate("team.redefine", team, definition, NULL); if (hr >= 0) return hr; }
    gmb_team_t* t = find_team(team);
    if (!t || !definition) return 0;
    // definition CSV "A1,A2,A3" → sostituisce i membri
    t->member_count = 0;
    char buf[512];
    strncpy(buf, definition, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    char* tok = strtok(buf, ",");
    while (tok && t->member_count < GMB_TEAM_MEMBERS_MAX) {
        while (*tok==' '||*tok=='\t') tok++;
        char* end = tok + strlen(tok) -1;
        while (end>tok && (*end==' '||*end=='\t')) *end--='\0';
        if (*tok) {
            strncpy(t->members[t->member_count].name, tok, 63);
            t->members[t->member_count].name[63]='\0';
            t->members[t->member_count].role = 1; // Worker
            t->member_count++;
        }
        tok = strtok(NULL, ",");
    }
    return 1;
}
