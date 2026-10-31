/*
 * gmb_msg.c — messaging generico (namespace MESSAGE). Vedi gmb_msg.h.
 *
 * Trasporti:
 *   - "local"       : bus IN-PROCESS (pub/sub sincrono).
 *   - "tcp://h:p"   : client verso un broker (line protocol):
 *                     C->B:  SUB <topic> | PUB <topic> <data> | UNSUB <topic> | PING
 *                            ACK <seq> | NACK <seq>
 *                     B->C:  MSG <seq> <topic> <data>   (conferma richiesta)
 *                            ACK <seq>                 (conferma della nostra PUB)
 *                     Le consegne si ricevono con gmb_msg_pump().
 *
 * CONFERMA DI CONSEGNA (at-least-once):
 *   - ogni MSG porta un id; dopo il dispatch il client manda ACK <seq>, oppure NACK <seq>
 *     se nessun handler ha consumato il messaggio (il broker lo ri-consegna, retry-after
 *     con backoff);
 *   - il broker conferma anche la PUB col mittente (ACK <seq>), scritta PRIMA delle MSG;
 *     la PUB non attende quella conferma: chi deve osservare l'effetto attende una
 *     condizione propria (attendere qui richiederebbe di pompare dentro la PUBLISH, che
 *     rientrerebbe nell'handler del VM durante la valutazione dell'espressione → SIGSEGV).
 */
#include "gmb_msg.h"
#include "gmb_c99gpm.h"
#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GMB_MSG_MAX_BUS 64
#define GMB_MSG_MAX_SUB 256

typedef struct {
    int  used, is_tcp;
    char uri[128];
    int64_t fd;              /* tcp: handle gmb_net_tcp_* */
    char rbuf[4096]; int rlen;
    /* CONTATORI DI CONSEGNA — letti da MESSAGE.DELIVERY(bus). Per-bus, monotoni, azzerati
     * alla CONNECT (memset del bus). Vedi gmb_msg_delivery_stats(). */
    long long sent, recv, ack, nack, dup, maxseq;
} gmsg_bus;

typedef struct {
    int  used;
    char id[40];
    char topic[64];
    int64_t bus;
    gmb_msg_handler_t fn;
} gmsg_sub;


#ifndef _WIN32
static int64_t tcp_connect(const char *host, int port)
{
    struct addrinfo hints, *res = NULL, *p; char ps[16]; int fd = -1;
    memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    snprintf(ps, sizeof(ps), "%d", port);
    if (getaddrinfo(host, ps, &hints, &res) != 0) return -1;
    for (p = res; p; p = p->ai_next) { fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol); if (fd < 0) continue; if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break; close(fd); fd = -1; }
    freeaddrinfo(res);
    if (fd >= 0) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}
static int tcp_send(int fd, const char *s, int n){ int o=0; while(o<n){ int w=(int)send(fd,s+o,(size_t)(n-o),0); if(w<0){ if(errno==EAGAIN||errno==EWOULDBLOCK){ continue; } return -1; } if(w==0) return -1; o+=w; } return 0; }
static int tcp_recv(int fd, char *buf, int cap){ int n=(int)recv(fd,buf,(size_t)cap,0); if(n<0 && (errno==EAGAIN||errno==EWOULDBLOCK)) return 0; return n; }
static void tcp_close(int fd){ if(fd>=0) close(fd); }
#else
static int64_t tcp_connect(const char *h,int p){(void)h;(void)p;return -1;}
static int tcp_send(int fd,const char*s,int n){(void)fd;(void)s;(void)n;return -1;}
static int tcp_recv(int fd,char*b,int c){(void)fd;(void)b;(void)c;return 0;}
static void tcp_close(int fd){(void)fd;}
#endif

static gmsg_bus g_bus[GMB_MSG_MAX_BUS];
static gmsg_sub g_sub[GMB_MSG_MAX_SUB];
static int g_subseq = 1;

static gmsg_sub *find_sub(const char *id)
{
    int i;
    if (!id) return NULL;
    for (i = 0; i < GMB_MSG_MAX_SUB; i++)
        if (g_sub[i].used && strcmp(g_sub[i].id, id) == 0) return &g_sub[i];
    return NULL;
}

static gmsg_bus *find_bus(int64_t bus)
{
    int i;
    for (i = 0; i < GMB_MSG_MAX_BUS; i++)
        if (g_bus[i].used && (int64_t)(i + 1) == bus) return &g_bus[i];
    return NULL;
}

int64_t gmb_msg_connect(const char *uri)
{
    int i, slot = -1;
    for (i = 0; i < GMB_MSG_MAX_BUS; i++) if (!g_bus[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    memset(&g_bus[slot], 0, sizeof(g_bus[slot]));
    g_bus[slot].used = 1; g_bus[slot].fd = -1;
    snprintf(g_bus[slot].uri, sizeof(g_bus[slot].uri), "%s", uri ? uri : "");
    if (uri && strncmp(uri, "tcp://", 6) == 0) {
        char host[96]; int port = 0; const char *p = uri + 6; char *colon;
        snprintf(host, sizeof(host), "%s", p);
        colon = strchr(host, ':');
        if (colon) { *colon = 0; port = atoi(colon + 1); }
        g_bus[slot].fd = tcp_connect(host, port);
        if (g_bus[slot].fd < 0) {
            /* CONNECT FALLITA ⇒ BUS LOCALE, non "bus inesistente": stessa semantica delle gambe
             * managed (IlRuntime.MessageConnect, commit 6ba3691 «connect fallita = bus LOCALE»).
             * Il bus resta VALIDO con la PROPRIA identità (slot+1): è questo che rende impossibile
             * la consegna cross-bus. Prima il fallimento restituiva -1 per ogni CONNECT, quindi
             * tutti i bus falliti condividevano l'handle -1 e la PUBLISH cadeva nel ramo locale
             * servendo le subscription di un ALTRO bus → messaggio consegnato su un trasporto mai
             * aperto (falso verde di rete). Vedi
             * Bugs/(solved) - VM-C-Message-Publish-Consegna-Locale-Su-Bus-Assente. */
            g_bus[slot].is_tcp = 0;
            return (int64_t)(slot + 1);
        }
        g_bus[slot].is_tcp = 1;
    }
    return (int64_t)(slot + 1);
}

/* Il bus esiste ed è un trasporto remoto (tcp) realmente connesso? Serve a chi deve distinguere
 * un bus locale (o una CONNECT tcp fallita, che ripiega sul locale) da un bus con socket: la VM/C
 * usa questa per non avviare la goroutine di ricezione su un bus locale. 0 se il bus non esiste. */
int gmb_msg_bus_is_tcp(int64_t bus)
{
    gmsg_bus *b = find_bus(bus);
    return (b && b->is_tcp && b->fd >= 0) ? 1 : 0;
}

int64_t gmb_msg_subscribe(int64_t bus, const char *topic, const char *alias)
{
    int i;
    gmsg_bus *b = find_bus(bus);
    /* BUS INESISTENTE (handle non valido, es. -1 perché nessuno slot è disponibile): nessuna
     * subscription. La CONNECT tcp FALLITA non produce più un handle -1 (ripiega su un bus
     * locale con identità propria), quindi qui non finiscono più le subscription "orfane" che
     * il ramo locale della PUBLISH ritrovava per handle. Vedi
     * Bugs/(solved) - VM-C-Message-Publish-Consegna-Locale-Su-Bus-Assente. */
    if (!b) return -1;
    for (i = 0; i < GMB_MSG_MAX_SUB; i++) {
        if (!g_sub[i].used) {
            g_sub[i].used = 1;
            g_sub[i].bus = bus;
            if (alias && alias[0]) snprintf(g_sub[i].id, sizeof(g_sub[i].id), "%s", alias);
            else snprintf(g_sub[i].id, sizeof(g_sub[i].id), "s%d", g_subseq++);
            snprintf(g_sub[i].topic, sizeof(g_sub[i].topic), "%s", topic ? topic : "");
            g_sub[i].fn = NULL;
            if (b && b->is_tcp && b->fd >= 0) {
                char msg[160]; int n = snprintf(msg, sizeof(msg), "SUB %s\n", topic ? topic : "");
                (void)tcp_send((int)b->fd, msg, n);
            }
            return g_subseq;
        }
    }
    return -1;
}

void gmb_msg_unsubscribe(const char *sub)
{
    int i;
    if (!sub || !sub[0]) { for (i = 0; i < GMB_MSG_MAX_SUB; i++) g_sub[i].used = 0; return; }
    { gmsg_sub *s = find_sub(sub); if (s) s->used = 0; }
}

void gmb_msg_publish(int64_t bus, const char *topic, const char *data)
{
    int i;
    gmsg_bus *b = find_bus(bus);
    /* TRE casi distinti, non due (il terzo era il bug):
     *   1. handle NON VALIDO (bus mai creato / slot esaurito) → no-op. Mai consegna: un bus che
     *      non esiste non può consegnare.
     *   2. bus REMOTO (tcp) → scrittura verso il broker.
     *   3. bus LOCALE (MESSAGE CONNECT "local", oppure CONNECT tcp FALLITA che ripiega sul
     *      locale) → fan-out in-process sincrono, SOLO sulle subscription di QUESTO bus.
     * Il bug era che ogni CONNECT tcp fallita restituiva -1: tutti i bus falliti avevano lo
     * stesso handle, e il fan-out locale (che filtra per `s->bus == bus`) serviva le subscription
     * di un altro bus. Ora ogni fallimento ha il proprio id, quindi il caso 1 e il caso 3 sono
     * separati anche a livello di identità. Vedi
     * Bugs/(solved) - VM-C-Message-Publish-Consegna-Locale-Su-Bus-Assente. */
    if (!b) return;
    if (b->is_tcp && b->fd >= 0) {
        char msg[2048]; int n = snprintf(msg, sizeof(msg), "PUB %s %s\n", topic ? topic : "", data ? data : "");
        /* Lo smistamento è del broker; l'attesa della conferma resta FUORI da qui: pompare
         * dentro la PUBLISH rientra nell'handler del VM (vm_msg_cb) mentre l'espressione è in
         * corso → SIGSEGV (misurato, gamba VM/C). Il broker manda comunque ACK <seq> al
         * mittente; la conferma di consegna che conta è quella del consumatore (ACK/NACK
         * sotto), e chi deve attendere l'effetto lo fa su una condizione propria. */
        if (n > 0 && tcp_send((int)b->fd, msg, n) == 0) b->sent++;   /* contatore di consegna */
        return;
    }
    for (i = 0; i < GMB_MSG_MAX_SUB; i++) {
        gmsg_sub *s = &g_sub[i];
        if (!s->used || s->bus != bus) continue;
        if (s->topic[0] && (!topic || strcmp(s->topic, topic) != 0)) continue;
        if (!s->fn) continue;
        s->fn(s->id, topic ? topic : "", data ? data : "");
    }
}

/* Consegna i messaggi in arrivo dal broker: chiamare periodicamente. */
int gmb_msg_pump(int64_t bus)
{
    gmsg_bus *b = find_bus(bus);
    int delivered = 0;
    if (!b || !b->is_tcp || b->fd < 0) return 0;
    for (;;) {
        /* Drena TUTTE le righe già in buffer, poi recv. (Prima si processava una sola riga
         * per recv: se una recv conteneva "OK\nMSG ...\n", la MSG restava in buffer e non
         * veniva mai ri-scansionata → segnali persi sul bus tcp.) */
        char *nl = (char *)memchr(b->rbuf, '\n', (size_t)b->rlen);
        if (!nl) {
            int n = tcp_recv((int)b->fd, b->rbuf + b->rlen, (int)(sizeof(b->rbuf) - (size_t)b->rlen));
            if (n <= 0) break;
            b->rlen += n;
            continue;
        }
        {
            char *line = b->rbuf;
            int len = (int)(nl - b->rbuf);
            *nl = 0;
            if (strncmp(line, "MSG ", 4) == 0) {
                /* Due formati: "MSG <seq> <topic> <data>" (broker con conferma) e il legacy
                 * "MSG <topic> <data>". L'id è il primo token se è interamente numerico. */
                char *p = line + 4;
                char *sp1 = strchr(p, ' ');
                char topic[64];
                const char *data = "";
                unsigned long seq = 0; int has_seq = 0, hits = 0;
                if (sp1) {
                    char *end = NULL;
                    unsigned long v;
                    *sp1 = 0;
                    v = (unsigned long)strtoul(p, &end, 10);
                    if (end && *end == 0 && end != p) { seq = v; has_seq = 1; }
                    p = sp1 + 1;
                    if (has_seq) {
                        char *sp2 = strchr(p, ' ');
                        if (sp2) { *sp2 = 0; data = sp2 + 1; }
                        snprintf(topic, sizeof(topic), "%s", p);
                    } else {
                        snprintf(topic, sizeof(topic), "%s", line + 4); /* topic = token prima dello spazio */
                        data = p;
                    }
                } else snprintf(topic, sizeof(topic), "%s", p);
                { int i; for (i = 0; i < GMB_MSG_MAX_SUB; i++) {
                    gmsg_sub *s = &g_sub[i];
                    if (!s->used || s->bus != bus) continue;
                    if (s->topic[0] && strcmp(s->topic, topic) != 0) continue;
                    if (s->fn) { s->fn(s->id, topic, data); delivered++; hits++; }
                } }
                /* Conferma: ACK se qualcuno ha consumato, NACK se il messaggio è rimasto orfano
                 * (il broker lo ri-consegna: è la semantica "competing consumer"). */
                if (has_seq) {
                    char cb[48];
                    int cn = snprintf(cb, sizeof(cb), "%s %lu\n", hits > 0 ? "ACK" : "NACK", seq);
                    gmsg_bus *bb = find_bus(bus);
                    /* CONTATORI: recv = messaggi ricevuti (con id o legacy), dup = id già visto
                     * (= ri-consegna del broker), ack/nack = conferme inviate. Duplicato ⇔ l'id
                     * non supera il massimo già visto: il broker numera in modo monotono. */
                    if (bb) {
                        bb->recv++;
                        if ((long long)seq <= bb->maxseq) bb->dup++;
                        else bb->maxseq = (long long)seq;
                        if (hits > 0) bb->ack++; else bb->nack++;
                    }
                    if (cn > 0 && bb && bb->is_tcp && bb->fd >= 0) (void)tcp_send((int)bb->fd, cb, cn);
                } else {
                    gmsg_bus *bb = find_bus(bus);
                    if (bb) bb->recv++;   /* legacy senza id: nessuna conferma possibile */
                }
            }
            /* "ACK <seq>" (conferma del broker alla nostra PUB) non richiede stato: il broker
             * l'ha già scritta prima delle MSG, la si ignora qui. */
            { int rem = b->rlen - (len + 1); memmove(b->rbuf, nl + 1, (size_t)(rem > 0 ? rem : 0)); b->rlen = rem > 0 ? rem : 0; }
        }
    }
    return delivered;
}

void gmb_msg_ping(int64_t bus)
{
    gmsg_bus *b = find_bus(bus);
    if (b && b->is_tcp && b->fd >= 0) { const char *p = "PING\n"; (void)tcp_send((int)b->fd, p, 5); }
}

void gmb_msg_close(int64_t bus)
{
    int i;
    gmsg_bus *b = find_bus(bus);
    for (i = 0; i < GMB_MSG_MAX_SUB; i++) if (g_sub[i].used && g_sub[i].bus == bus) g_sub[i].used = 0;
    if (b) { if (b->is_tcp && b->fd >= 0) { const char *c = "CLOSE\n"; (void)tcp_send((int)b->fd, c, 6); } b->used = 0; }
}

void gmb_msg_reg_handler(const char *sub, gmb_msg_handler_t fn)
{
    gmsg_sub *s = find_sub(sub);
    if (s) s->fn = fn;
}

int gmb_msg_delivery_stats(int64_t bus, char *out, size_t cap)
{
    gmsg_bus *b = find_bus(bus);
    if (!out || cap == 0) return 0;
    if (!b) { snprintf(out, cap, "sent=0 recv=0 ack=0 nack=0 dup=0 last=0"); return 0; }
    snprintf(out, cap, "sent=%lld recv=%lld ack=%lld nack=%lld dup=%lld last=%lld",
             b->sent, b->recv, b->ack, b->nack, b->dup, b->maxseq);
    return 1;
}

/* ---- RECEIVE goroutine GPM (park-based) per il trasporto tcp ---- */
static void gmb_msg_recv_loop(void *arg)
{
    int64_t bus = (int64_t)(intptr_t)arg;
    /* termina quando il bus è chiuso (MESSAGE CLOSE): così la GPM non resta attiva per sempre */
    while (find_bus(bus)) {
        (void)gmb_msg_pump(bus);
        gmb_yield_ticks(20);   /* park breve: il worker non si blocca (pattern WAITKEY park) */
    }
}

void gmb_msg_start_recv(int64_t bus)
{
    static int started[GMB_MSG_MAX_BUS];
    gmsg_bus *b = find_bus(bus);
    if (bus <= 0 || bus > GMB_MSG_MAX_BUS || !b || !b->is_tcp) return;
    if (started[bus - 1]) return;
    started[bus - 1] = 1;
    gmb_task_spawn(gmb_msg_recv_loop, (void *)(intptr_t)bus);
}

void gmb_msg_pump_all(void)
{
    int i;
    for (i = 0; i < GMB_MSG_MAX_BUS; i++)
        if (g_bus[i].used && g_bus[i].is_tcp) (void)gmb_msg_pump((int64_t)(i + 1));
}
