/* msg_broker_mock.c — broker pub/sub minimale (line protocol) per i test del trasporto tcp.
 *
 * Protocollo (una istruzione per riga, UTF-8, terminata da \n):
 *
 *   C->B   SUB <topic>          sottoscrive il client al topic
 *   C->B   UNSUB                annulla la sottoscrizione
 *   C->B   PUB <topic> <data>   pubblica; il broker consegna e poi conferma col mittente
 *   C->B   ACK <seq>            il consumatore conferma la consegna del messaggio <seq>
 *   C->B   NACK <seq>           il consumatore rifiuta: il broker ri-consegna SUBITO
 *   C->B   PING                 liveness
 *   C->B   STATS                contatori (per i test/probe)
 *   C->B   CLOSE                chiude il client
 *
 *   B->C   MSG <seq> <topic> <data>   consegna (con id, così il client può fare ACK)
 *   B->C   ACK <seq>                  conferma al publisher: la PUB è stata smistata
 *   B->C   PONG                       risposta a PING
 *   B->C   STATS nack=<n> redelivery=<n> inflight=<n> dropped=<n>
 *
 * SEMANTICA DI CONSEGNA (at-least-once, best effort come Disque):
 *   - ogni MSG resta "in volo" finché il client non manda ACK <seq>;
 *   - se l'ACK non arriva entro `retry_ms`, il broker ri-consegna (redelivery++);
 *   - NACK <seq> forza la ri-consegna immediata (nack++);
 *   - dopo `max_attempts` ri-consegne il messaggio viene abbandonato (dropped++).
 *   Il "due contatori" al posto della dead-letter queue è l'idea presa da Disque: la
 *   policy la decide l'applicazione che legge STATS/il contatore, il broker non tiene coda.
 *
 * L'ACK a fine PUB è ciò che rende deterministico il test: quando PUBLISH ritorna, il
 * broker ha già scritto le MSG sui socket dei sottoscrittori.
 *
 * Uso: msg_broker_mock [porta] [retry_ms] [max_attempts]   (default 9200, 300, 3)
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAXC     16
#define MAXP     64
#define MAXDATA  1024
#define MAXLINE  2048

static volatile sig_atomic_t stop = 0;
static void on_sig(int s) { (void)s; stop = 1; }

typedef struct {
    int  fd;
    char topic[64];
    char buf[MAXLINE];
    int  len;
} cli;

/* Un messaggio in volo: `mask` ha il bit i acceso se il client nello slot i non ha ancora
 * mandato ACK per questo <seq>. La data è copiata: se tutti chiudono, l'entry muore. */
typedef struct {
    int           used;
    unsigned long seq;
    char          topic[64];
    char          data[MAXDATA];
    unsigned int  mask;
    long long     due_ms;
    int           attempts;
} pend;

static pend       g_pend[MAXP];
static unsigned long g_seq = 0, g_nack = 0, g_redelivery = 0, g_dropped = 0;

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
}

static int sends(int fd, const char *m, int n)
{
    int o = 0;
    while (o < n) {
        int w = (int)send(fd, m + o, (size_t)(n - o), 0);
        if (w <= 0) return -1;
        o += w;
    }
    return 0;
}

static pend *pend_find(unsigned long seq)
{
    int i;
    for (i = 0; i < MAXP; i++)
        if (g_pend[i].used && g_pend[i].seq == seq) return &g_pend[i];
    return NULL;
}

static pend *pend_alloc(void)
{
    int i;
    for (i = 0; i < MAXP; i++) if (!g_pend[i].used) return &g_pend[i];
    return NULL; /* pieno: il messaggio non è tracciato (best effort) */
}

/* Slot liberato: nessuno attenderà più l'ACK da quel client. */
static void pend_forget_slot(int slot /*, int *dropped*/)
{
    int i;
    unsigned int bit = 1u << slot;
    for (i = 0; i < MAXP; i++) {
        if (!g_pend[i].used) continue;
        g_pend[i].mask &= ~bit;
        if (g_pend[i].mask == 0) g_pend[i].used = 0;
    }
}

/* Consegna `p` a tutti i client dello slot ancora in attesa. */
static void pend_deliver(pend *p, cli *c)
{
    int k;
    char out[MAXDATA + 128];
    int on;
    if (!p || !p->used) return;
    on = snprintf(out, sizeof(out), "MSG %lu %s %s\n", p->seq, p->topic, p->data);
    if (on <= 0 || on >= (int)sizeof(out)) return;
    for (k = 0; k < MAXC; k++) {
        if (!(p->mask & (1u << k))) continue;
        if (c[k].fd < 0) { p->mask &= ~(1u << k); continue; }
        if (sends(c[k].fd, out, on) != 0) {
            close(c[k].fd);
            c[k].fd = -1;
            c[k].topic[0] = 0;
            p->mask &= ~(1u << k);
        }
    }
    if (p->mask == 0) p->used = 0;
}

/* Ritenta i messaggi scaduti: è il retry-after lato broker. */
static void pend_tick(cli *c, int retry_ms, int max_attempts)
{
    int i;
    long long t = now_ms();
    for (i = 0; i < MAXP; i++) {
        pend *p = &g_pend[i];
        if (!p->used || t < p->due_ms) continue;
        p->attempts++;
        g_redelivery++;
        if (p->attempts > max_attempts) {
            g_dropped++;
            p->used = 0;
            continue;
        }
        pend_deliver(p, c);
        if (p->used) p->due_ms = t + retry_ms;
    }
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 9200;
    int retry_ms = argc > 2 ? atoi(argv[2]) : 300;
    int max_attempts = argc > 3 ? atoi(argv[3]) : 3;
    int ls, maxfd, i, slot;
    struct sockaddr_in a;
    cli c[MAXC];

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    signal(SIGPIPE, SIG_IGN);

    if (retry_ms <= 0) retry_ms = 300;
    if (max_attempts < 0) max_attempts = 0;

    for (i = 0; i < MAXC; i++) { c[i].fd = -1; c[i].topic[0] = 0; c[i].len = 0; }
    memset(g_pend, 0, sizeof(g_pend));

    ls = socket(AF_INET, SOCK_STREAM, 0);
    { int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); }
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((unsigned short)port);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) != 0) { perror("bind"); return 1; }
    listen(ls, 8);
    fprintf(stderr, "msg_broker_mock: 127.0.0.1:%d (retry=%dms max_attempts=%d)\n",
            port, retry_ms, max_attempts);

    while (!stop) {
        fd_set rf;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 20000; /* 20ms: granularità sufficiente per retry_ms >= 50 */
        FD_ZERO(&rf);
        FD_SET(ls, &rf);
        maxfd = ls;
        for (i = 0; i < MAXC; i++) if (c[i].fd >= 0) { FD_SET(c[i].fd, &rf); if (c[i].fd > maxfd) maxfd = c[i].fd; }

        if (select(maxfd + 1, &rf, 0, 0, &tv) > 0) {
            if (FD_ISSET(ls, &rf)) {
                int fd = (int)accept(ls, 0, 0);
                if (fd >= 0) {
                    slot = -1;
                    for (i = 0; i < MAXC; i++) if (c[i].fd < 0) { slot = i; break; }
                    if (slot < 0) close(fd);
                    else { c[slot].fd = fd; c[slot].topic[0] = 0; c[slot].len = 0; }
                }
            }
            for (slot = 0; slot < MAXC; slot++) {
                if (c[slot].fd < 0 || !FD_ISSET(c[slot].fd, &rf)) continue;
                {
                    int n = (int)recv(c[slot].fd, c[slot].buf + c[slot].len,
                                      sizeof(c[slot].buf) - (size_t)c[slot].len, 0);
                    if (n <= 0) {
                        pend_forget_slot(slot);
                        close(c[slot].fd);
                        c[slot].fd = -1;
                        c[slot].topic[0] = 0;
                        continue;
                    }
                    c[slot].len += n;
                }
                for (;;) {
                    char *nl = (char *)memchr(c[slot].buf, '\n', (size_t)c[slot].len);
                    int rem;
                    if (!nl) break;
                    *nl = 0;
                    if (strncmp(c[slot].buf, "SUB ", 4) == 0) {
                        snprintf(c[slot].topic, sizeof(c[slot].topic), "%s", c[slot].buf + 4);
                    } else if (strncmp(c[slot].buf, "UNSUB", 5) == 0) {
                        c[slot].topic[0] = 0;
                    } else if (strncmp(c[slot].buf, "PUB ", 4) == 0) {
                        char topic[64];
                        const char *data = "";
                        char *sp = strchr(c[slot].buf + 4, ' ');
                        pend *p;
                        char out[MAXDATA + 128];
                        int on, k;
                        if (sp) { *sp = 0; data = sp + 1; }
                        snprintf(topic, sizeof(topic), "%s", c[slot].buf + 4);
                        g_seq++;
                        on = snprintf(out, sizeof(out), "MSG %lu %s %s\n", g_seq, topic, data);
                        if (on > 0 && on < (int)sizeof(out)) {
                            p = pend_alloc();
                            if (p) {
                                memset(p, 0, sizeof(*p));
                                p->used = 1;
                                p->seq = g_seq;
                                snprintf(p->topic, sizeof(p->topic), "%s", topic);
                                snprintf(p->data, sizeof(p->data), "%s", data);
                                p->attempts = 0;
                                for (k = 0; k < MAXC; k++) {
                                    if (c[k].fd < 0 || strcmp(c[k].topic, topic) != 0) continue;
                                    if (sends(c[k].fd, out, on) != 0) {
                                        pend_forget_slot(k);
                                        close(c[k].fd);
                                        c[k].fd = -1;
                                        c[k].topic[0] = 0;
                                        continue;
                                    }
                                    p->mask |= (1u << k);
                                }
                                if (p->mask == 0) p->used = 0;
                                else p->due_ms = now_ms() + retry_ms;
                            }
                        }
                        /* conferma al publisher: da qui in poi le MSG sono già scritte */
                        { char ack[48]; int al = snprintf(ack, sizeof(ack), "ACK %lu\n", g_seq);
                          if (al > 0) (void)sends(c[slot].fd, ack, al); }
                    } else if (strncmp(c[slot].buf, "ACK ", 4) == 0) {
                        pend *p = pend_find(strtoul(c[slot].buf + 4, NULL, 10));
                        if (p) {
                            p->mask &= ~(1u << slot);
                            if (p->mask == 0) p->used = 0;
                        }
                    } else if (strncmp(c[slot].buf, "NACK ", 5) == 0) {
                        pend *p = pend_find(strtoul(c[slot].buf + 5, NULL, 10));
                        g_nack++;
                        if (p && (p->mask & (1u << slot))) {
                            p->due_ms = now_ms(); /* ri-consegna al prossimo tick */
                        }
                    } else if (strncmp(c[slot].buf, "PING", 4) == 0) {
                        (void)sends(c[slot].fd, "PONG\n", 5);
                    } else if (strncmp(c[slot].buf, "STATS", 5) == 0) {
                        char st[128];
                        int sl = snprintf(st, sizeof(st),
                                          "STATS nack=%lu redelivery=%lu inflight=%d dropped=%lu\n",
                                          g_nack, g_redelivery, 0, g_dropped);
                        if (sl > 0) (void)sends(c[slot].fd, st, sl);
                    } else if (strncmp(c[slot].buf, "CLOSE", 5) == 0) {
                        pend_forget_slot(slot);
                        close(c[slot].fd);
                        c[slot].fd = -1;
                        c[slot].topic[0] = 0;
                        break;
                    }
                    rem = c[slot].len - (int)(nl + 1 - c[slot].buf);
                    memmove(c[slot].buf, nl + 1, (size_t)(rem > 0 ? rem : 0));
                    c[slot].len = rem > 0 ? rem : 0;
                    if (c[slot].fd < 0) break;
                }
            }
        }
        pend_tick(c, retry_ms, max_attempts);
    }

    for (i = 0; i < MAXC; i++) if (c[i].fd >= 0) close(c[i].fd);
    close(ls);
    return 0;
}
