/* gmb_agent_queue.c — coda FIFO per agente (L1, mondo C) */
#include "gmb_runtime.h"
#include "gmb_state_hook.h"   /* hook opzionale di stato (client managed) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define GMB_AQ_MAX_AGENTS 64
#define GMB_AQ_MAX_QUEUE 1000
#define GMB_AQ_MSG_MAX 64

typedef struct {
    char id[33];
    char from[64];
    char to[64];
    char task[128];
    char input[512];
    double payment;
    long timestamp;
} gmb_aq_msg_t;

typedef struct {
    char agent[64];
    gmb_aq_msg_t msgs[GMB_AQ_MSG_MAX];
    int head, tail, count;
    int paused;
    int stopped;
    pthread_mutex_t lock;
} gmb_aq_t;

static gmb_aq_t g_aqs[GMB_AQ_MAX_AGENTS];
static int g_aq_count = 0;
static pthread_mutex_t g_aq_global_lock = PTHREAD_MUTEX_INITIALIZER;

static gmb_aq_t* find_aq(const char* agent) {
    for (int i=0;i<g_aq_count;i++) if (strcmp(g_aqs[i].agent, agent)==0) return &g_aqs[i];
    return NULL;
}
static gmb_aq_t* find_or_create_aq(const char* agent) {
    gmb_aq_t* q = find_aq(agent);
    if (q) return q;
    pthread_mutex_lock(&g_aq_global_lock);
    q = find_aq(agent);
    if (!q && g_aq_count < GMB_AQ_MAX_AGENTS) {
        q = &g_aqs[g_aq_count++];
        strncpy(q->agent, agent, 63); q->agent[63]='\0';
        q->head=q->tail=q->count=0;
        q->paused=0; q->stopped=0;
        pthread_mutex_init(&q->lock, NULL);
    }
    pthread_mutex_unlock(&g_aq_global_lock);
    return q;
}

static void gen_id(char out[33]) {
    static unsigned long long seq = 0x9E3779B97F4A7C15ULL;
    seq ^= seq << 13; seq ^= seq >> 7; seq ^= seq << 17;
    seq += (unsigned long long)time(NULL) * 0x100000001B3ULL;
    for(int i=0;i<16;i++){ seq ^= seq << 13; seq ^= seq >> 7; seq ^= seq << 17; }
    const char* hex="0123456789abcdef";
    for(int i=0;i<16;i++){ unsigned char b=(seq >> (i*4)) & 0xF; out[i*2]=hex[(b>>4)&0xF]; out[i*2+1]=hex[b&0xF]; }
    out[32]='\0';
}

int gmb_agent_queue_create(const char* agent) {
    if(!agent || !*agent) return 0;
    if(find_aq(agent)) return 0;
    gmb_aq_t* q = find_or_create_aq(agent);
    return q ? 1 : 0;
}
int gmb_agent_queue_send(const char* agent, const char* from, const char* task, const char* input, double payment) {
    /* `from` e `payment` non sono ponte: l'API managed (AiAgentSend) usa "user" e paga 0. */
    { int hr = gmb_state_hook_mutate("agent.send", agent, task, input); if (hr >= 0) return hr; }
    if(!agent || !*agent) return 0;
    gmb_aq_t* q = find_or_create_aq(agent);
    if(!q) return 0;
    pthread_mutex_lock(&q->lock);
    if(q->stopped || q->count >= GMB_AQ_MAX_QUEUE){ pthread_mutex_unlock(&q->lock); return 0; }
    gmb_aq_msg_t* m = &q->msgs[q->tail];
    gen_id(m->id);
    strncpy(m->from, from?from:"user", 63); m->from[63]='\0';
    strncpy(m->to, agent, 63); m->to[63]='\0';
    strncpy(m->task, task?task:"", 127); m->task[127]='\0';
    strncpy(m->input, input?input:"", 511); m->input[511]='\0';
    m->payment = payment;
    m->timestamp = (long)time(NULL);
    q->tail = (q->tail+1)%GMB_AQ_MSG_MAX;
    q->count++;
    pthread_mutex_unlock(&q->lock);
    return 1;
}
char* gmb_agent_queue_receive(const char* agent) {
    { char* r = gmb_state_hook_query("agent.receive", agent, NULL); if (r) return r; }
    if(!agent || !*agent) return NULL;
    gmb_aq_t* q = find_aq(agent);
    if(!q) return NULL;
    pthread_mutex_lock(&q->lock);
    if(q->paused || q->stopped || q->count==0){ pthread_mutex_unlock(&q->lock); return NULL; }
    gmb_aq_msg_t m = q->msgs[q->head];
    q->head = (q->head+1)%GMB_AQ_MSG_MAX;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    char* out = (char*)malloc(1024);
    if(!out) return NULL;
    /* Parità AgentMessage.ToJson (System.Text.Json C#): Payment senza decimali forzati (%g),
     * Timestamp ISO-8601 UTC ("...Z"), non epoch. Prima: %.2f e %ld → divergenti. */
    char tsbuf[40];
    {
        time_t t = (time_t)m.timestamp;
        struct tm tmv;
        gmtime_r(&t, &tmv);
        snprintf(tsbuf, sizeof(tsbuf), "%04d-%02d-%02dT%02d:%02d:%02d.000000Z",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    }
    snprintf(out, 1024, "{\"Id\":\"%s\",\"From\":\"%s\",\"To\":\"%s\",\"Task\":\"%s\",\"Input\":\"%s\",\"Payment\":%g,\"Timestamp\":\"%s\"}",
        m.id, m.from, m.to, m.task, m.input, m.payment, tsbuf);
    return out;
}
char* gmb_agent_queue_stats(const char* agent) {
    { char* r = gmb_state_hook_query("agent.queuestats", agent, NULL); if (r) return r; }
    gmb_aq_t* q = find_aq(agent);
    int count=0, paused=0, stopped=0;
    if(q){ pthread_mutex_lock(&q->lock); count=q->count; paused=q->paused; stopped=q->stopped; pthread_mutex_unlock(&q->lock); }
    char* out=(char*)malloc(256);
    if(!out) return NULL;
    snprintf(out,256,"{\"agent\":\"%s\",\"count\":%d,\"paused\":%s,\"stopped\":%s}", agent?agent:"", count, paused?"true":"false", stopped?"true":"false");
    return out;
}
int gmb_agent_queue_clear(const char* agent) {
    { int hr = gmb_state_hook_mutate("agent.queueclear", agent, NULL, NULL); if (hr >= 0) return hr; }
    gmb_aq_t* q=find_aq(agent); if(!q) return 0;
    pthread_mutex_lock(&q->lock); q->head=q->tail=q->count=0; pthread_mutex_unlock(&q->lock); return 1;
}
int gmb_agent_queue_pause(const char* agent){
    { int hr = gmb_state_hook_mutate("agent.pause", agent, NULL, NULL); if (hr >= 0) return hr; } gmb_aq_t* q=find_aq(agent); if(!q) return 0; pthread_mutex_lock(&q->lock); q->paused=1; pthread_mutex_unlock(&q->lock); return 1; }
int gmb_agent_queue_resume(const char* agent){
    { int hr = gmb_state_hook_mutate("agent.resume", agent, NULL, NULL); if (hr >= 0) return hr; } gmb_aq_t* q=find_aq(agent); if(!q) return 0; pthread_mutex_lock(&q->lock); q->paused=0; pthread_mutex_unlock(&q->lock); return 1; }
int gmb_agent_queue_stop(const char* agent){
    { int hr = gmb_state_hook_mutate("agent.stop", agent, NULL, NULL); if (hr >= 0) return hr; } gmb_aq_t* q=find_aq(agent); if(!q) return 0; pthread_mutex_lock(&q->lock); q->stopped=1; q->head=q->tail=q->count=0; pthread_mutex_unlock(&q->lock); return 1; }
int gmb_agent_queue_is_paused(const char* agent){ gmb_aq_t* q=find_aq(agent); return q ? q->paused : 0; }
int gmb_agent_queue_is_stopped(const char* agent){ gmb_aq_t* q=find_aq(agent); return q ? q->stopped : 0; }
int gmb_agent_queue_count(const char* agent){ gmb_aq_t* q=find_aq(agent); return q ? q->count : 0; }

/* Wrapper gmb_string_t (per emissione C99, evita leak del char* heap):
 * copia e libera l'allocazione di receive/stats. */
gmb_string_t gmb_agent_queue_receive_s(const char* agent){ char* raw = gmb_agent_queue_receive(agent); gmb_string_t s = gmb_string_alloc(raw ? raw : ""); if (raw) free(raw); return s; }
gmb_string_t gmb_agent_queue_stats_s(const char* agent){ char* raw = gmb_agent_queue_stats(agent); gmb_string_t s = gmb_string_alloc(raw ? raw : ""); if (raw) free(raw); return s; }
