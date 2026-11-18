/* gmb_agent_fabric.c — fabric per agenti (L1, mondo C) — pthread+epoll */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* F2: close() — arrivava implicita da un altro header */
#include <pthread.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

/* Fabric: per agente una struttura con inbox/outbox/control chan (simile a GPM) */
#define GMB_FABRIC_MAX 32

typedef struct {
    char agent_id[64];
    int inbox_fd;  /* epoll fd per inbox */
    int outbox_fd;
    int control_fd;
    int active;
    pthread_t thread;
} gmb_fabric_t;

static gmb_fabric_t g_fabrics[GMB_FABRIC_MAX];
static int g_fabric_count = 0;
static pthread_mutex_t g_fabric_lock = PTHREAD_MUTEX_INITIALIZER;

int gmb_agent_fabric_create(const char* agent_id){
    if(!agent_id||!*agent_id) return 0;
    pthread_mutex_lock(&g_fabric_lock);
    for(int i=0;i<g_fabric_count;i++) if(strcmp(g_fabrics[i].agent_id, agent_id)==0){ pthread_mutex_unlock(&g_fabric_lock); return 0; }
    if(g_fabric_count>=GMB_FABRIC_MAX){ pthread_mutex_unlock(&g_fabric_lock); return 0; }
    gmb_fabric_t* f=&g_fabrics[g_fabric_count++];
    strncpy(f->agent_id, agent_id, 63); f->agent_id[63]='\0';
#ifdef __linux__
    f->inbox_fd = epoll_create1(0);
    f->outbox_fd = epoll_create1(0);
    f->control_fd = epoll_create1(0);
#else
    f->inbox_fd = f->outbox_fd = f->control_fd = -1;
#endif
    f->active=1;
    pthread_mutex_unlock(&g_fabric_lock);
    /* Crea anche la coda per l'agente */
    gmb_agent_queue_create(agent_id);
    return 1;
}
int gmb_agent_send(const char* agent, const char* from, const char* task, const char* input){
    return gmb_agent_queue_send(agent, from, task, input, 0);
}
char* gmb_agent_receive(const char* agent){
    return gmb_agent_queue_receive(agent);
}
int gmb_agent_stop(const char* agent){ return gmb_agent_queue_stop(agent); }
int gmb_agent_pause(const char* agent){ return gmb_agent_queue_pause(agent); }
int gmb_agent_resume(const char* agent){ return gmb_agent_queue_resume(agent); }
int gmb_channel_create_select(const char* channel){ return gmb_bus_channel_create(channel); }
int gmb_agent_fabric_destroy(const char* agent_id){
    pthread_mutex_lock(&g_fabric_lock);
    for(int i=0;i<g_fabric_count;i++) if(strcmp(g_fabrics[i].agent_id, agent_id)==0){
#ifdef __linux__
        if(g_fabrics[i].inbox_fd>=0) close(g_fabrics[i].inbox_fd);
        if(g_fabrics[i].outbox_fd>=0) close(g_fabrics[i].outbox_fd);
        if(g_fabrics[i].control_fd>=0) close(g_fabrics[i].control_fd);
#endif
        g_fabrics[i]=g_fabrics[g_fabric_count-1];
        g_fabric_count--;
        pthread_mutex_unlock(&g_fabric_lock);
        return 1;
    }
    pthread_mutex_unlock(&g_fabric_lock);
    return 0;
}
/* GO SELECT coordinatore (parity C# AgentQueueManager.DequeueBlocking poll): interroga le
 * code degli agenti dati finche' una ha un messaggio (o timeout). Ritorna l'indice
 * dell'agente vincente e copia il messaggio JSON in out (cap); -1 su timeout. */
int gmb_agent_fabric_select(const char* const* agents, int n, char* out, int cap, int timeout_ms){
    if(!agents || n <= 0 || !out || cap <= 0) return -1;
    long deadline = gmb_ticks_ms() + (timeout_ms >= 0 ? timeout_ms : 5000);
    while (gmb_ticks_ms() < deadline) {
        for(int i=0;i<n;i++){
            if(!agents[i] || !agents[i][0]) continue;
            char* msg = gmb_agent_queue_receive(agents[i]);
            if(msg){
                snprintf(out, (size_t)cap, "%s", msg);
                free(msg);
                return i;
            }
        }
        gmb_sleep_ms(10);
    }
    return -1;
}

/* ── ON INSERT EXECUTE AGENT (fase C, parity C# AgentInsertTrigger): registry (db|coll) →
 * agente+template; fire dopo gmb_aiv_index_text. Mai stub: contatore + esecuzione reale. ── */
#define GMB_AITRIG_MAX 16
typedef struct { char db[64]; char coll[64]; char agent[64]; char prompt[512]; long fired; } gmb_aitrig_t;
static gmb_aitrig_t g_aitrig[GMB_AITRIG_MAX];
static int g_aitrig_count = 0;

int gmb_agent_insert_trigger_register(const char* db, const char* coll, const char* agent, const char* prompt){
    if(!db || !db[0] || !coll || !coll[0] || !agent || !agent[0]) return 0;
    if(g_aitrig_count >= GMB_AITRIG_MAX) return 0;
    for(int i=0;i<g_aitrig_count;i++) if(strcmp(g_aitrig[i].db,db)==0 && strcmp(g_aitrig[i].coll,coll)==0) return 0;
    gmb_aitrig_t* t=&g_aitrig[g_aitrig_count++];
    snprintf(t->db,sizeof(t->db),"%s",db); snprintf(t->coll,sizeof(t->coll),"%s",coll);
    snprintf(t->agent,sizeof(t->agent),"%s",agent);
    snprintf(t->prompt,sizeof(t->prompt),"%s", prompt&&prompt[0]?prompt:"Elabora il documento inserito: {text}");
    t->fired=0;
    return 1;
}
int gmb_agent_insert_trigger_unregister(const char* db, const char* coll){
    for(int i=0;i<g_aitrig_count;i++) if(strcmp(g_aitrig[i].db,db)==0 && strcmp(g_aitrig[i].coll,coll)==0){
        memmove(&g_aitrig[i],&g_aitrig[i+1],(size_t)(g_aitrig_count-i-1)*sizeof(gmb_aitrig_t));
        g_aitrig_count--; return 1;
    }
    return 0;
}
long gmb_agent_insert_trigger_fired(const char* db, const char* coll){
    for(int i=0;i<g_aitrig_count;i++) if(strcmp(g_aitrig[i].db,db)==0 && strcmp(g_aitrig[i].coll,coll)==0) return g_aitrig[i].fired;
    return 0;
}
int gmb_agent_insert_trigger_fire(const char* db, const char* coll, const char* source, const char* text){
    int fired=0;
    for(int i=0;i<g_aitrig_count;i++){
        if(strcmp(g_aitrig[i].db,db)==0 && strcmp(g_aitrig[i].coll,coll)==0){
            char prompt[1024]; snprintf(prompt,sizeof(prompt),"%s",g_aitrig[i].prompt);
            char tmp[1024]; snprintf(tmp,sizeof(tmp),"%s",prompt);
            char* s=strstr(tmp,"{text}"); if(s && text) { snprintf(prompt,sizeof(prompt),"%.*s%.900s%s",(int)(s-tmp),tmp,text,s+6); }
            s=strstr(prompt,"{source}"); if(s && source) { char tmp2[1024]; snprintf(tmp2,sizeof(tmp2),"%s",prompt); snprintf(prompt,sizeof(prompt),"%.*s%s%s",(int)(s-tmp2),tmp2,source,s+8); }
            g_aitrig[i].fired++;
            gmb_agent_ask(g_aitrig[i].agent, prompt);
            fired++;
        }
    }
    return fired;
}
void gmb_agent_insert_trigger_clear(void){ g_aitrig_count=0; }
