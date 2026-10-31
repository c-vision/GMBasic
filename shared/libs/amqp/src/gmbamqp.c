/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbamqp.c — shim FFI del pacchetto gmbsuite `amqp`: client AMQP 0-9-1 (RabbitMQ) in C PURO
 * (socket POSIX, autenticazione SASL PLAIN, NESSUNA libreria esterna) -> self-contained.
 * Build dal sorgente (build.sh).
 *
 * Percorso minimo: handshake (Start/Start-Ok, Tune/Tune-Ok, Open/Open-Ok), Channel.Open,
 * Queue.Declare, Basic.Publish (+ content header + body), Basic.Get (+ content), Disconnect.
 * Interfaccia BASIC: namespace AMQP, firma OMOGENEA (tutti LONG), stringhe AS STRING. */
#include "gmbamqp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdint.h>

#define AMQP_MAX 32
typedef struct { int used; int fd; char body[65536]; char rk[512]; int has_msg; long last_size; } amqp_cli;
static amqp_cli g_cli[AMQP_MAX];

static uint16_t rd16(const unsigned char* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const unsigned char* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint64_t rd64(const unsigned char* p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v = (v << 8) | p[i]; return v; }
static void wr8(unsigned char* b, int* o, unsigned v) { b[(*o)++] = (unsigned char)v; }
static void wr16(unsigned char* b, int* o, unsigned v) { b[*o] = (v >> 8) & 0xff; b[*o + 1] = v & 0xff; *o += 2; }
static void wr32(unsigned char* b, int* o, uint32_t v) { b[*o] = (v >> 24) & 0xff; b[*o + 1] = (v >> 16) & 0xff; b[*o + 2] = (v >> 8) & 0xff; b[*o + 3] = v & 0xff; *o += 4; }
static void wr64(unsigned char* b, int* o, uint64_t v) { for (int i = 7; i >= 0; i--) b[(*o)++] = (unsigned char)((v >> (i * 8)) & 0xff); }
static void wr_shortstr(unsigned char* b, int* o, const char* s) { int n = s ? (int)strlen(s) : 0; if (n > 255) n = 255; wr8(b, o, (unsigned)n); if (n) memcpy(b + *o, s, (size_t)n); *o += n; }
static void wr_longstr(unsigned char* b, int* o, const void* s, int n) { wr32(b, o, (uint32_t)n); if (n) memcpy(b + *o, s, (size_t)n); *o += n; }
static void wr_table_empty(unsigned char* b, int* o) { wr32(b, o, 0); }

static int am_write(int fd, const void* p, int n) { int o = 0; while (o < n) { ssize_t w = write(fd, (const char*)p + o, (size_t)(n - o)); if (w <= 0) return -1; o += (int)w; } return 0; }
static int am_readn(int fd, void* p, int n) { int o = 0; while (o < n) { ssize_t r = read(fd, (char*)p + o, (size_t)(n - o)); if (r <= 0) return -1; o += (int)r; } return 0; }

static int send_frame(int fd, int type, int channel, const unsigned char* payload, int len)
{
    unsigned char hdr[7];
    hdr[0] = (unsigned char)type; hdr[1] = (unsigned char)(channel >> 8); hdr[2] = (unsigned char)(channel & 0xff);
    hdr[3] = (unsigned char)(len >> 24); hdr[4] = (unsigned char)(len >> 16); hdr[5] = (unsigned char)(len >> 8); hdr[6] = (unsigned char)(len & 0xff);
    if (am_write(fd, hdr, 7) != 0) return -1;
    if (len > 0 && am_write(fd, payload, len) != 0) return -1;
    unsigned char end = 0xCE;
    return am_write(fd, &end, 1);
}

/* legge un frame completo; ritorna 0 ok. */
static int read_frame(int fd, int* type, int* channel, unsigned char* buf, int cap, int* len)
{
    unsigned char hdr[7];
    if (am_readn(fd, hdr, 7) != 0) return -1;
    *type = hdr[0]; *channel = (hdr[1] << 8) | hdr[2];
    uint32_t sz = ((uint32_t)hdr[3] << 24) | ((uint32_t)hdr[4] << 16) | ((uint32_t)hdr[5] << 8) | hdr[6];
    if ((int)sz > cap) return -1;
    if (sz > 0 && am_readn(fd, buf, (int)sz) != 0) return -1;
    unsigned char end = 0;
    if (am_readn(fd, &end, 1) != 0 || end != 0xCE) return -1;
    *len = (int)sz;
    return 0;
}

/* invia un metodo su un canale: payload = class/method + args. */
static int send_method(int fd, int channel, int cls, int method, const unsigned char* args, int alen)
{
    unsigned char payload[16384];
    int o = 0;
    wr16(payload, &o, (unsigned)cls); wr16(payload, &o, (unsigned)method);
    if (alen > 0) { memcpy(payload + o, args, (size_t)alen); o += alen; }
    return send_frame(fd, 1, channel, payload, o);
}

int gmbamqp_available(void) { return 1; }

int gmbamqp_connect(const char* host, long port, const char* user, const char* pass)
{
    if (!host) return -1;
    int slot = -1;
    for (int i = 0; i < AMQP_MAX; i++) if (!g_cli[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    char ps[16]; snprintf(ps, sizeof(ps), "%ld", port > 0 ? port : 5672);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) { close(fd); freeaddrinfo(res); return -1; }
    freeaddrinfo(res);

    static const unsigned char proto[8] = { 'A','M','Q','P', 0, 0, 9, 1 };
    if (am_write(fd, proto, 8) != 0) { close(fd); return -1; }

    unsigned char buf[16384];
    int type, ch, len;

    /* Connection.Start (10,10) */
    if (read_frame(fd, &type, &ch, buf, sizeof(buf), &len) != 0) { close(fd); return -1; }
    if (type != 1 || rd16(buf) != 10 || rd16(buf + 2) != 10) { close(fd); return -1; }

    /* Connection.Start-Ok (10,11): client-properties(table), mechanism(shortstr=PLAIN),
       response(longstr=\0user\0pass), locale(shortstr=en_US) */
    {
        unsigned char args[1024]; int o = 0;
        wr_table_empty(args, &o);
        wr_shortstr(args, &o, "PLAIN");
        char resp[600]; int rl = 0;
        resp[rl++] = 0; const char* u = user ? user : "guest";
        int ul = (int)strlen(u); memcpy(resp + rl, u, (size_t)ul); rl += ul;
        resp[rl++] = 0; const char* pw = pass ? pass : "guest";
        int pl = (int)strlen(pw); memcpy(resp + rl, pw, (size_t)pl); rl += pl;
        wr_longstr(args, &o, resp, rl);
        wr_shortstr(args, &o, "en_US");
        if (send_method(fd, 0, 10, 11, args, o) != 0) { close(fd); return -1; }
    }

    /* Connection.Tune (10,30) */
    if (read_frame(fd, &type, &ch, buf, sizeof(buf), &len) != 0) { close(fd); return -1; }
    if (type != 1 || rd16(buf) != 10 || rd16(buf + 2) != 30) { close(fd); return -1; }
    {
        int p = 4;
        unsigned channel_max = rd16(buf + p); p += 2;
        uint32_t frame_max = rd32(buf + p); p += 4;
        unsigned heartbeat = rd16(buf + p); p += 2;
        if (frame_max == 0) frame_max = 131072;
        unsigned char args[16]; int o = 0;
        wr16(args, &o, channel_max); wr32(args, &o, frame_max); wr16(args, &o, heartbeat);
        if (send_method(fd, 0, 10, 31, args, o) != 0) { close(fd); return -1; }
    }

    /* Connection.Open (10,40) */
    {
        unsigned char args[64]; int o = 0;
        wr_shortstr(args, &o, "/");      /* virtual-host */
        wr_shortstr(args, &o, "");       /* capabilities */
        wr8(args, &o, 0);                /* insist = 0 */
        if (send_method(fd, 0, 10, 40, args, o) != 0) { close(fd); return -1; }
    }
    /* Connection.Open-Ok (10,41) */
    if (read_frame(fd, &type, &ch, buf, sizeof(buf), &len) != 0) { close(fd); return -1; }
    if (type != 1 || rd16(buf) != 10 || rd16(buf + 2) != 41) { close(fd); return -1; }

    /* Channel.Open (20,10) su canale 1 */
    { unsigned char args[8]; int o = 0; wr_shortstr(args, &o, ""); if (send_method(fd, 1, 20, 10, args, o) != 0) { close(fd); return -1; } }
    if (read_frame(fd, &type, &ch, buf, sizeof(buf), &len) != 0) { close(fd); return -1; }
    if (type != 1 || rd16(buf) != 20 || rd16(buf + 2) != 11) { close(fd); return -1; }

    g_cli[slot].used = 1; g_cli[slot].fd = fd; g_cli[slot].has_msg = 0; g_cli[slot].last_size = 0;
    return slot + 1;
}
static amqp_cli* aget(long h) { return (h >= 1 && h <= AMQP_MAX && g_cli[h - 1].used) ? &g_cli[h - 1] : NULL; }

/* legge e scarta frame fino a incontrare (class,method) atteso su canale; ritorna payload len (>=0). */
static int wait_method(int fd, int want_ch, int cls, int method, unsigned char* buf, int cap)
{
    for (;;) {
        int type, ch, len;
        if (read_frame(fd, &type, &ch, buf, cap, &len) != 0) return -1;
        if (type == 8) continue;            /* heartbeat */
        if (type != 1) continue;
        if (len < 4) continue;
        if ((want_ch < 0 || ch == want_ch) && rd16(buf) == (unsigned)cls && rd16(buf + 2) == (unsigned)method) return len;
        if (rd16(buf) == 10 && rd16(buf + 2) == 50) return -2;   /* Connection.Close */
        /* altrimenti ignora */
    }
}

int gmbamqp_declare_queue(long h, const char* queue)
{
    amqp_cli* c = aget(h); if (!c || !queue) return -1;
    unsigned char args[300]; int o = 0;
    wr16(args, &o, 0);              /* reserved=0 */
    wr_shortstr(args, &o, queue);
    wr8(args, &o, 0);               /* passive|durable|exclusive|auto-delete|no-wait = 0 (bit consecutivi: 1 ottetto) */
    wr_table_empty(args, &o);       /* arguments */
    if (send_method(c->fd, 1, 50, 10, args, o) != 0) return -1;
    unsigned char buf[8192];
    return (wait_method(c->fd, 1, 50, 11, buf, sizeof(buf)) >= 0) ? 0 : -1;
}

int gmbamqp_publish(long h, const char* exchange, const char* routing_key, const char* body)
{
    amqp_cli* c = aget(h); if (!c) return -1;
    if (!exchange) exchange = "";
    if (!routing_key) routing_key = "";
    const char* b = body ? body : "";
    int bl = (int)strlen(b);

    {   /* Basic.Publish (60,40) */
        unsigned char args[600]; int o = 0;
        wr16(args, &o, 0);                    /* reserved */
        wr_shortstr(args, &o, exchange);
        wr_shortstr(args, &o, routing_key);
        wr8(args, &o, 0);                     /* mandatory|immediate = 0 (bit consecutivi: 1 ottetto) */
        if (send_method(c->fd, 1, 60, 40, args, o) != 0) return -1;
    }
    {   /* Content header (frame 2): class-id(2), weight(2), body-size(LONG LONG 8), property-flags(2) */
        unsigned char hdr[64]; int o = 0;
        wr16(hdr, &o, 60);                    /* class-id */
        wr16(hdr, &o, 0);                     /* weight */
        wr64(hdr, &o, (uint64_t)bl);          /* body size (64-bit) */
        wr16(hdr, &o, 0);                     /* property flags = 0 (no properties) */
        if (send_frame(c->fd, 2, 1, hdr, o) != 0) return -1;
    }
    if (bl > 0 && send_frame(c->fd, 3, 1, (const unsigned char*)b, bl) != 0) return -1;
    return 0;
}

/* Basic.Get; 1 messaggio, 0 coda vuota, -1 errore. */
int gmbamqp_get(long h, const char* queue, long timeout_ms)
{
    amqp_cli* c = aget(h); if (!c || !queue) return -1;
    {   /* Basic.Get (60,70): reserved(2), queue(shortstr), no-ack(bit=1) */
        unsigned char args[300]; int o = 0;
        wr16(args, &o, 0);
        wr_shortstr(args, &o, queue);
        wr8(args, &o, 1);                     /* no-ack = true */
        if (send_method(c->fd, 1, 60, 70, args, o) != 0) return -1;
    }
    for (;;) {
        fd_set rf; FD_ZERO(&rf); FD_SET(c->fd, &rf);
        struct timeval tv; tv.tv_sec = (timeout_ms > 0 ? timeout_ms : 0) / 1000; tv.tv_usec = ((timeout_ms > 0 ? timeout_ms : 0) % 1000) * 1000;
        int s = select(c->fd + 1, &rf, NULL, NULL, &tv);
        if (s == 0) return 0;
        if (s < 0) { if (errno == EINTR) continue; return -1; }
        int type, ch, len;
        unsigned char buf[16384];
        if (read_frame(c->fd, &type, &ch, buf, sizeof(buf), &len) != 0) return -1;
        if (type == 8) continue;              /* heartbeat */
        if (type != 1 || len < 4) continue;
        int cls = rd16(buf), m = rd16(buf + 2);
        if (cls == 60 && m == 71) {           /* Basic.Get-Ok */
            int p = 4;
            p += 8;                           /* delivery-tag (longlong) */
            p += 1;                           /* redelivered bit */
            int exl = buf[p]; p += 1 + exl;   /* exchange */
            int rkl = buf[p]; int rl = rkl > 511 ? 511 : rkl; memcpy(c->rk, buf + p + 1, (size_t)rl); c->rk[rl] = 0; p += 1 + rkl;
            /* message-count (4) segue */
            /* ora Content Header (frame 2) e Body (frame 3) */
            int t2, ch2, l2;
            if (read_frame(c->fd, &t2, &ch2, buf, sizeof(buf), &l2) != 0) return -1;
            if (t2 != 2 || l2 < 14) return -1;
            uint64_t body_size = rd64(buf + 4);   /* body-size = LONG LONG (8 byte) */
            /* successivo: Body */
            int t3, ch3, l3;
            if (read_frame(c->fd, &t3, &ch3, buf, sizeof(buf), &l3) != 0) return -1;
            if (t3 != 3) return -1;
            uint32_t cp = body_size < (uint32_t)sizeof(c->body) - 1 ? body_size : (uint32_t)sizeof(c->body) - 1;
            memcpy(c->body, buf, cp); c->body[cp] = 0;
            c->has_msg = 1; c->last_size = (long)body_size;
            return 1;
        }
        if (cls == 60 && m == 72) return 0;   /* Basic.Get-Empty */
        if (cls == 10 && m == 50) return -1;  /* Connection.Close */
        /* altro: ignora */
    }
}

const char* gmbamqp_last_body(long h) { amqp_cli* c = aget(h); return (c && c->has_msg) ? c->body : ""; }
const char* gmbamqp_last_routingkey(long h) { amqp_cli* c = aget(h); return (c && c->has_msg) ? c->rk : ""; }
long gmbamqp_last_size(long h) { amqp_cli* c = aget(h); return c ? c->last_size : 0; }

int gmbamqp_disconnect(long h)
{
    amqp_cli* c = aget(h); if (!c) return -1;
    /* Connection.Close (10,50): reply-code(short=200), reply-text(shortstr=OK), class-id(short=0), method-id(short=0) */
    unsigned char args[64]; int o = 0;
    wr16(args, &o, 200); wr_shortstr(args, &o, "OK"); wr16(args, &o, 0); wr16(args, &o, 0);
    send_method(c->fd, 0, 10, 50, args, o);   /* best-effort: non attende Close-Ok */
    close(c->fd);
    memset(c, 0, sizeof(*c));
    return 0;
}
