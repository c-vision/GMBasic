/* gmb_message_bus.c — MessageBus Pub/Sub per canali (L1, mondo C) */
#include "gmb_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define GMB_MB_MAX_CHANNELS 64
#define GMB_MB_MAX_SUBS 16

typedef void (*gmb_mb_handler_t)(const char* channel, const char* from, const char* task, const char* input, void* userdata);

typedef struct {
    gmb_mb_handler_t handler;
    void* userdata;
} gmb_mb_sub_t;

typedef struct {
    char channel[64];
    gmb_mb_sub_t subs[GMB_MB_MAX_SUBS];
    int sub_count;
    pthread_mutex_t lock;
} gmb_mb_channel_t;

static gmb_mb_channel_t g_channels[GMB_MB_MAX_CHANNELS];
static int g_channel_count = 0;
static pthread_mutex_t g_mb_lock = PTHREAD_MUTEX_INITIALIZER;

static gmb_mb_channel_t* find_channel(const char* channel){
    for(int i=0;i<g_channel_count;i++) if(strcmp(g_channels[i].channel, channel)==0) return &g_channels[i];
    return NULL;
}
static gmb_mb_channel_t* find_or_create_channel(const char* channel){
    gmb_mb_channel_t* c=find_channel(channel);
    if(c) return c;
    pthread_mutex_lock(&g_mb_lock);
    c=find_channel(channel);
    if(!c && g_channel_count < GMB_MB_MAX_CHANNELS){
        c=&g_channels[g_channel_count++];
        strncpy(c->channel, channel, 63); c->channel[63]='\0';
        c->sub_count=0;
        pthread_mutex_init(&c->lock, NULL);
    }
    pthread_mutex_unlock(&g_mb_lock);
    return c;
}

int gmb_bus_channel_create(const char* channel){
    if(!channel||!*channel) return 0;
    find_or_create_channel(channel);
    return 1;
}
int gmb_channel_subscribe(const char* channel, gmb_mb_handler_t handler, void* userdata){
    if(!channel||!handler) return 0;
    gmb_mb_channel_t* c=find_or_create_channel(channel);
    if(!c) return 0;
    pthread_mutex_lock(&c->lock);
    if(c->sub_count >= GMB_MB_MAX_SUBS){ pthread_mutex_unlock(&c->lock); return 0; }
    for(int i=0;i<c->sub_count;i++) if(c->subs[i].handler==handler && c->subs[i].userdata==userdata){ pthread_mutex_unlock(&c->lock); return 0; }
    c->subs[c->sub_count].handler=handler;
    c->subs[c->sub_count].userdata=userdata;
    c->sub_count++;
    pthread_mutex_unlock(&c->lock);
    return 1;
}
int gmb_channel_unsubscribe(const char* channel, gmb_mb_handler_t handler, void* userdata){
    if(!channel||!handler) return 0;
    gmb_mb_channel_t* c=find_channel(channel);
    if(!c) return 0;
    pthread_mutex_lock(&c->lock);
    for(int i=0;i<c->sub_count;i++) if(c->subs[i].handler==handler && c->subs[i].userdata==userdata){
        c->subs[i]=c->subs[c->sub_count-1];
        c->sub_count--;
        if(c->sub_count==0){ pthread_mutex_lock(&g_mb_lock); /* keep channel but empty */ pthread_mutex_unlock(&g_mb_lock); }
        pthread_mutex_unlock(&c->lock);
        return 1;
    }
    pthread_mutex_unlock(&c->lock);
    return 0;
}
int gmb_bus_channel_publish(const char* channel, const char* message){
    if(!channel) return 0;
    gmb_mb_channel_t* c=find_channel(channel);
    /* Auto-create del canale su PUBLISH (parità IlRuntime.AiChannelPublish → MessageBus.Publish,
     * che crea il canale se assente): senza, pubblicare su un canale mai creato dava 0 mentre
     * l'oracolo dà 1 (divergenza trovata da gen_AI_ChannelPublish sul C99Native). */
    if(!c){ gmb_bus_channel_create(channel); c=find_channel(channel); }
    if(!c) return 0;
    gmb_mb_sub_t snapshot[GMB_MB_MAX_SUBS];
    int n=0;
    pthread_mutex_lock(&c->lock);
    n=c->sub_count;
    for(int i=0;i<n;i++) snapshot[i]=c->subs[i];
    pthread_mutex_unlock(&c->lock);
    for(int i=0;i<n;i++) snapshot[i].handler(channel, "user", "publish", message?message:"", snapshot[i].userdata);
    return n>0 ? n : 1;
}
int gmb_bus_publish_ex(const char* channel, const char* from, const char* task, const char* input){
    if(!channel) return 0;
    gmb_mb_channel_t* c=find_channel(channel);
    if(!c) return 0;
    gmb_mb_sub_t snapshot[GMB_MB_MAX_SUBS];
    int n=0;
    pthread_mutex_lock(&c->lock);
    n=c->sub_count;
    for(int i=0;i<n;i++) snapshot[i]=c->subs[i];
    pthread_mutex_unlock(&c->lock);
    for(int i=0;i<n;i++) snapshot[i].handler(channel, from?from:"user", task?task:"", input?input:"", snapshot[i].userdata);
    return n;
}
int gmb_channel_subscriber_count(const char* channel){
    gmb_mb_channel_t* c=find_channel(channel);
    return c ? c->sub_count : 0;
}
int gmb_channel_has_subscribers(const char* channel){
    gmb_mb_channel_t* c=find_channel(channel);
    return c && c->sub_count>0;
}
void gmb_channel_clear(const char* channel){
    if(!channel) return;
    pthread_mutex_lock(&g_mb_lock);
    for(int i=0;i<g_channel_count;i++) if(strcmp(g_channels[i].channel, channel)==0){
        pthread_mutex_lock(&g_channels[i].lock);
        g_channels[i].sub_count=0;
        pthread_mutex_unlock(&g_channels[i].lock);
        break;
    }
    pthread_mutex_unlock(&g_mb_lock);
}
