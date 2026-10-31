/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbmqtt.c — shim FFI del pacchetto gmbsuite `mqtt`: client MQTT 3.1.1 (QoS 0) in C PURO
 * (socket POSIX, nessuna libreria esterna) -> self-contained. Nessuna dipendenza di sistema.
 * Build dal sorgente (build.sh), eseguito dal package manager.
 *
 * Interfaccia BASIC (namespace MQTT, .inc): firma OMOGENEA (tutti LONG), stringhe AS STRING.
 * Ultimo messaggio ricevuto in stato interno (LASTTOPIC/LASTPAYLOAD). */
#include "gmbmqtt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MQTT_MAX 32
typedef struct { int used; int fd; int connected; int next_pid; char topic[512]; char payload[16384]; int has_msg; } mqtt_cli;
static mqtt_cli g_cli[MQTT_MAX];

int gmbmqtt_available(void) { return 1; }

static int mqtt_send_all(int fd, const unsigned char* p, int n) {
    int off = 0;
    while (off < n) { ssize_t w = write(fd, p + off, (size_t)(n - off)); if (w <= 0) return -1; off += (int)w; }
    return 0;
}
static int mqtt_recv_all(int fd, unsigned char* p, int n) {
    int off = 0;
    while (off < n) { ssize_t r = read(fd, p + off, (size_t)(n - off)); if (r <= 0) return -1; off += (int)r; }
    return 0;
}
static int enc_remlen(unsigned char* b, int len) {        /* varint MQTT */
    int n = 0;
    do { unsigned char x = (unsigned char)(len % 128); len /= 128; if (len) x |= 0x80; b[n++] = x; } while (len);
    return n;
}

int gmbmqtt_connect(const char* host, long port, const char* client_id)
{
    if (!host) return -1;
    int slot = -1;
    for (int i = 0; i < MQTT_MAX; i++) if (!g_cli[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    char ps[16]; snprintf(ps, sizeof(ps), "%ld", port > 0 ? port : 1883);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) { close(fd); freeaddrinfo(res); return -1; }
    freeaddrinfo(res);

    const char* cid = (client_id && *client_id) ? client_id : "gmbasic";
    unsigned char pkt[512]; int n = 0;
    pkt[n++] = 0x10;                                       /* CONNECT */
    unsigned char rl[4]; int rln = 0;
    int cidlen = (int)strlen(cid);
    /* variable header: "MQTT"(2+4)+level(1)+flags(1)+keepalive(2) = 10 ; payload: 2+cidlen */
    rln = enc_remlen(rl, 10 + 2 + cidlen);
    memcpy(pkt + n, rl, (size_t)rln); n += rln;
    pkt[n++] = 0; pkt[n++] = 4; pkt[n++] = 'M'; pkt[n++] = 'Q'; pkt[n++] = 'T'; pkt[n++] = 'T';
    pkt[n++] = 4;                                          /* level 4 = MQTT 3.1.1 */
    pkt[n++] = 0x02;                                       /* flags: clean session */
    pkt[n++] = 0; pkt[n++] = 60;                           /* keepalive 60s */
    pkt[n++] = (unsigned char)(cidlen >> 8); pkt[n++] = (unsigned char)(cidlen & 0xff);
    memcpy(pkt + n, cid, (size_t)cidlen); n += cidlen;
    if (mqtt_send_all(fd, pkt, n) != 0) { close(fd); return -1; }
    unsigned char ack[4];
    if (mqtt_recv_all(fd, ack, 4) != 0 || ack[0] != 0x20 || ack[3] != 0x00) { close(fd); return -1; }

    g_cli[slot].used = 1; g_cli[slot].fd = fd; g_cli[slot].connected = 1; g_cli[slot].next_pid = 1; g_cli[slot].has_msg = 0;
    return slot + 1;
}

static mqtt_cli* get(long h) { return (h >= 1 && h <= MQTT_MAX && g_cli[h - 1].used) ? &g_cli[h - 1] : NULL; }

int gmbmqtt_publish(long h, const char* topic, const char* payload)
{
    mqtt_cli* c = get(h); if (!c || !topic) return -1;
    const char* pl = payload ? payload : "";
    int tlen = (int)strlen(topic), plen = (int)strlen(pl);
    unsigned char pkt[16384 + 512]; int n = 0;
    pkt[n++] = 0x30;                                       /* PUBLISH QoS0 */
    unsigned char rl[4]; int rln = enc_remlen(rl, 2 + tlen + plen);
    memcpy(pkt + n, rl, (size_t)rln); n += rln;
    pkt[n++] = (unsigned char)(tlen >> 8); pkt[n++] = (unsigned char)(tlen & 0xff);
    memcpy(pkt + n, topic, (size_t)tlen); n += tlen;
    memcpy(pkt + n, pl, (size_t)plen); n += plen;
    return mqtt_send_all(c->fd, pkt, n);
}

int gmbmqtt_subscribe(long h, const char* topic)
{
    mqtt_cli* c = get(h); if (!c || !topic) return -1;
    int tlen = (int)strlen(topic);
    unsigned char pkt[600]; int n = 0;
    pkt[n++] = 0x82;                                       /* SUBSCRIBE */
    unsigned char rl[4]; int rln = enc_remlen(rl, 2 + 2 + tlen + 1);
    memcpy(pkt + n, rl, (size_t)rln); n += rln;
    int pid = c->next_pid++;
    pkt[n++] = (unsigned char)(pid >> 8); pkt[n++] = (unsigned char)(pid & 0xff);
    pkt[n++] = (unsigned char)(tlen >> 8); pkt[n++] = (unsigned char)(tlen & 0xff);
    memcpy(pkt + n, topic, (size_t)tlen); n += tlen;
    pkt[n++] = 0x00;                                       /* QoS 0 */
    if (mqtt_send_all(c->fd, pkt, n) != 0) return -1;
    /* attende SUBACK (0x90) */
    for (;;) {
        unsigned char hd[2];
        if (mqtt_recv_all(c->fd, hd, 1) != 0) return -1;
        if (hd[0] == 0x90) {
            int len = 0, mult = 1, i = 0;
            do { if (mqtt_recv_all(c->fd, hd, 1) != 0) return -1; len += (hd[0] & 0x7f) * mult; mult *= 128; i++; } while ((hd[0] & 0x80) && i < 4);
            unsigned char skip[8]; if (len > 0) { if (mqtt_recv_all(c->fd, skip, len < (int)sizeof(skip) ? len : (int)sizeof(skip)) != 0) return -1; }
            return 0;
        } else {
            int len = 0, mult = 1, i = 0;
            do { if (mqtt_recv_all(c->fd, hd, 1) != 0) return -1; len += (hd[0] & 0x7f) * mult; mult *= 128; i++; } while ((hd[0] & 0x80) && i < 4);
            unsigned char buf[4096]; int left = len; while (left > 0) { int take = left < (int)sizeof(buf) ? left : (int)sizeof(buf); if (mqtt_recv_all(c->fd, buf, take) != 0) return -1; left -= take; }
        }
    }
}

/* Attende un messaggio (PUBLISH) fino a timeout_ms. 1 = ricevuto, 0 = timeout, -1 = errore. */
int gmbmqtt_receive(long h, long timeout_ms)
{
    mqtt_cli* c = get(h); if (!c) return -1;
    if (timeout_ms < 0) timeout_ms = 0;
    for (;;) {
        fd_set rf; FD_ZERO(&rf); FD_SET(c->fd, &rf);
        struct timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
        int s = select(c->fd + 1, &rf, NULL, NULL, &tv);
        if (s == 0) return 0;
        if (s < 0) { if (errno == EINTR) continue; return -1; }
        unsigned char hd;
        if (mqtt_recv_all(c->fd, &hd, 1) != 0) return -1;
        int len = 0, mult = 1, i = 0;
        unsigned char b;
        do { if (mqtt_recv_all(c->fd, &b, 1) != 0) return -1; len += (b & 0x7f) * mult; mult *= 128; i++; } while ((b & 0x80) && i < 4);
        unsigned char* buf = (unsigned char*)malloc((size_t)len + 1);
        if (!buf) return -1;
        if (len > 0 && mqtt_recv_all(c->fd, buf, len) != 0) { free(buf); return -1; }
        int type = hd >> 4;
        if (type == 3 && len >= 2) {                       /* PUBLISH */
            int tlen = (buf[0] << 8) | buf[1];
            if (tlen + 2 <= len) {
                int cp = tlen < (int)sizeof(c->topic) - 1 ? tlen : (int)sizeof(c->topic) - 1;
                memcpy(c->topic, buf + 2, (size_t)cp); c->topic[cp] = 0;
                int plen = len - 2 - tlen;
                int pp = plen < (int)sizeof(c->payload) - 1 ? plen : (int)sizeof(c->payload) - 1;
                memcpy(c->payload, buf + 2 + tlen, (size_t)pp); c->payload[pp] = 0;
                c->has_msg = 1;
                free(buf);
                return 1;
            }
        }
        free(buf);                                          /* altri tipi: ignora e continua */
    }
}

const char* gmbmqtt_last_topic(long h) { mqtt_cli* c = get(h); return (c && c->has_msg) ? c->topic : ""; }
const char* gmbmqtt_last_payload(long h) { mqtt_cli* c = get(h); return (c && c->has_msg) ? c->payload : ""; }

int gmbmqtt_disconnect(long h)
{
    mqtt_cli* c = get(h); if (!c) return -1;
    unsigned char d[2] = { 0xE0, 0x00 };
    mqtt_send_all(c->fd, d, 2);
    close(c->fd);
    memset(c, 0, sizeof(*c));
    return 0;
}
