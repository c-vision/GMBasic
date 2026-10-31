/* gmb_goal.c — Goal persistente + idle continuation (mondo C).
 *
 * Orchestrazione agenti (A4): obiettivo persistente del thread; quando l'agente va idle il
 * runtime può riprenderlo. Registro deterministico (obiettivo, attivo, passi); gmb_goal_tick
 * rappresenta un impulso di continuazione (l'aggancio GPM è un follow-on). Parita' con
 * Basic.Agents.GoalRegistry (C#): stesso formato JSON. Runtime CONDIVISO (AGENTS §0bis).
 */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define GMB_GOAL_OBJ_MAX 1024
#define GMB_GOAL_AGENT_MAX 256

static char g_goal_objective[GMB_GOAL_OBJ_MAX];
static int g_goal_active = 0;
static int g_goal_steps = 0;
static char g_goal_agent[GMB_GOAL_AGENT_MAX];
static char g_goal_last[2048];
static pthread_mutex_t g_goal_lock = PTHREAD_MUTEX_INITIALIZER;

/* Associa un agente al goal (idle continuation). 1 = ok, 0 = nome vuoto. */
int gmb_goal_agent(const char* name)
{
    if (!name || !*name) return 0;
    pthread_mutex_lock(&g_goal_lock);
    snprintf(g_goal_agent, sizeof(g_goal_agent), "%s", name);
    pthread_mutex_unlock(&g_goal_lock);
    return 1;
}

int gmb_goal_set(const char* objective)
{
    if (!objective || !*objective) return 0;
    pthread_mutex_lock(&g_goal_lock);
    snprintf(g_goal_objective, GMB_GOAL_OBJ_MAX, "%s", objective);
    g_goal_active = 1;
    g_goal_steps = 0;
    pthread_mutex_unlock(&g_goal_lock);
    return 1;
}

/* Obiettivo corrente (malloc; "" se nessuno). */
char* gmb_goal_get(void)
{
    char* out = (char*)malloc(GMB_GOAL_OBJ_MAX);
    if (!out) return NULL;
    pthread_mutex_lock(&g_goal_lock);
    snprintf(out, GMB_GOAL_OBJ_MAX, "%s", g_goal_objective);
    pthread_mutex_unlock(&g_goal_lock);
    return out;
}

static void gmb_goal_escape(const char* s, char* out, size_t cap)
{
    size_t o = 0;
    for (const char* p = s ? s : ""; *p && o + 2 < cap; p++)
    {
        char c = *p;
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else out[o++] = c;
    }
    out[o] = '\0';
}

char* gmb_goal_status(void)
{
    size_t cap = GMB_GOAL_OBJ_MAX * 2 + 2048 * 2 + 256;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;
    pthread_mutex_lock(&g_goal_lock);
    char eo[GMB_GOAL_OBJ_MAX * 2];
    char ea[GMB_GOAL_AGENT_MAX * 2];
    char el[2048 * 2];
    gmb_goal_escape(g_goal_objective, eo, sizeof(eo));
    gmb_goal_escape(g_goal_agent, ea, sizeof(ea));
    gmb_goal_escape(g_goal_last, el, sizeof(el));
    snprintf(buf, cap, "{\"objective\":\"%s\",\"active\":%s,\"steps\":%d,\"agent\":\"%s\",\"last\":\"%s\"}",
        eo, g_goal_active ? "true" : "false", g_goal_steps, ea, el);
    pthread_mutex_unlock(&g_goal_lock);
    return buf;
}

/* Impulso di continuazione idle: se attivo incrementa i passi e ritorna il conteggio.
 * Se è associato un agente, lo interroga con l'obiettivo (aggancio GPM: gmb_agent_ask). */
double gmb_goal_tick(void)
{
    pthread_mutex_lock(&g_goal_lock);
    if (!g_goal_active) { pthread_mutex_unlock(&g_goal_lock); return 0.0; }
    g_goal_steps++;
    double r = (double)g_goal_steps;
    char obj[GMB_GOAL_OBJ_MAX];
    char agent[GMB_GOAL_AGENT_MAX];
    snprintf(obj, sizeof(obj), "%s", g_goal_objective);
    snprintf(agent, sizeof(agent), "%s", g_goal_agent);
    pthread_mutex_unlock(&g_goal_lock);
    if (agent[0] && gmb_agent_exists(agent))
    {
        char* reply = gmb_agent_ask(agent, obj);
        pthread_mutex_lock(&g_goal_lock);
        snprintf(g_goal_last, sizeof(g_goal_last), "%s", reply ? reply : "");
        pthread_mutex_unlock(&g_goal_lock);
        free(reply);
    }
    return r;
}

int gmb_goal_clear(void)
{
    pthread_mutex_lock(&g_goal_lock);
    int had = (g_goal_active || g_goal_objective[0]) ? 1 : 0;
    g_goal_objective[0] = '\0';
    g_goal_active = 0;
    g_goal_steps = 0;
    g_goal_agent[0] = '\0';
    g_goal_last[0] = '\0';
    pthread_mutex_unlock(&g_goal_lock);
    return had;
}
