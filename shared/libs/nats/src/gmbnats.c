/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbnats.c — shim FFI del pacchetto gmbsuite `nats`: client NATS (protocollo testuale) in C
 * PURO (socket POSIX, NESSUNA libreria esterna) -> self-contained. Build dal sorgente (build.sh).
 *
 * Interfaccia BASIC (namespace NATS, .inc): firma OMOGENEA (tutti LONG), stringhe AS STRING.
 * Ultimo messaggio ricevuto in stato interno (LASTSUBJECT/LASTPAYLOAD). */
#include "gmbnats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

#define NATS_MAX 32
typedef struct { int used; int fd; int next_sid; char subject[512]; char payload[16384]; int has_msg; } nats_cli;
static nats_cli g_cli[NATS_MAX];

int gmbnats_available(void) { return 1; }

static int na_write(int fd, const char* p, int n) { int o = 0; while (o < n) { ssize_t w = write(fd, p + o, (size_t)(n - o)); if (w <= 0) return -1; o += (int)w; } return 0; }
static int na_readline(int fd, char* buf, int cap) {          /* riga fino a \n (senza \r\n); -1 err */
    int n = 0;
    for (;;) {
        char ch;
        ssize_t r = read(fd, &ch, 1);
        if (r <= 0) return -1;
        if (ch == '\n') break;
        if (ch != '\r' && n < cap - 1) buf[n++] = ch;
    }
    buf[n] = 0;
    return n;
}
static int na_readn(int fd, char* buf, int n) { int o = 0; while (o < n) { ssize_t r = read(fd, buf + o, (size_t)(n - o)); if (r <= 0) return -1; o += (int)r; } return 0; }

int gmbnats_connect(const char* host, long port, const char* name)
{
    if (!host) return -1;
    int slot = -1;
    for (int i = 0; i < NATS_MAX; i++) if (!g_cli[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    char ps[16]; snprintf(ps, sizeof(ps), "%ld", port > 0 ? port : 4222);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) { close(fd); freeaddrinfo(res); return -1; }
    freeaddrinfo(res);
    char line[8192];
    if (na_readline(fd, line, sizeof(line)) < 0 || strncmp(line, "INFO", 4) != 0) { close(fd); return -1; }
    char cmd[512];
    int cl = snprintf(cmd, sizeof(cmd), "CONNECT {\"verbose\":false,\"pedantic\":false,\"name\":\"%s\"}\r\n", (name && *name) ? name : "gmbasic");
    if (na_write(fd, cmd, cl) != 0) { close(fd); return -1; }
    /* legge la risposta (INFO aggiornato / +OK / -ERR) */
    if (na_readline(fd, line, sizeof(line)) < 0) { close(fd); return -1; }
    if (strncmp(line, "-ERR", 4) == 0) { close(fd); return -1; }

    g_cli[slot].used = 1; g_cli[slot].fd = fd; g_cli[slot].next_sid = 1; g_cli[slot].has_msg = 0;
    return slot + 1;
}
static nats_cli* naget(long h) { return (h >= 1 && h <= NATS_MAX && g_cli[h - 1].used) ? &g_cli[h - 1] : NULL; }

int gmbnats_subscribe(long h, const char* subject)
{
    nats_cli* c = naget(h); if (!c || !subject) return -1;
    char cmd[600];
    int l = snprintf(cmd, sizeof(cmd), "SUB %s %d\r\n", subject, c->next_sid++);
    return na_write(c->fd, cmd, l);
}

int gmbnats_publish(long h, const char* subject, const char* payload)
{
    nats_cli* c = naget(h); if (!c || !subject) return -1;
    const char* pl = payload ? payload : "";
    int pln = (int)strlen(pl);
    char hdr[640];
    int l = snprintf(hdr, sizeof(hdr), "PUB %s %d\r\n", subject, pln);
    if (na_write(c->fd, hdr, l) != 0) return -1;
    if (pln > 0 && na_write(c->fd, pl, pln) != 0) return -1;
    return na_write(c->fd, "\r\n", 2);
}

/* Attende un MSG (o gestisce PING) fino a timeout_ms. 1 msg, 0 timeout, -1 err. */
int gmbnats_receive(long h, long timeout_ms)
{
    nats_cli* c = naget(h); if (!c) return -1;
    if (timeout_ms < 0) timeout_ms = 0;
    for (;;) {
        fd_set rf; FD_ZERO(&rf); FD_SET(c->fd, &rf);
        struct timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
        int s = select(c->fd + 1, &rf, NULL, NULL, &tv);
        if (s == 0) return 0;
        if (s < 0) { if (errno == EINTR) continue; return -1; }
        char line[8192];
        if (na_readline(c->fd, line, sizeof(line)) < 0) return -1;
        if (strncmp(line, "MSG ", 4) == 0) {
            /* MSG <subject> <sid> [reply] <#bytes> */
            char subj[512]; int sid = 0, nbytes = 0;
            /* parse: subject poi sid poi eventuale reply poi len */
            char* p = line + 4;
            char* sp = strchr(p, ' ');
            if (!sp) continue;
            int sl = (int)(sp - p); if (sl > (int)sizeof(subj) - 1) sl = (int)sizeof(subj) - 1;
            memcpy(subj, p, (size_t)sl); subj[sl] = 0;
            p = sp + 1;
            /* sid */
            sid = atoi(p);
            sp = strchr(p, ' ');
            if (!sp) continue;
            p = sp + 1;
            /* resto: [reply] len → ultimo token = len */
            char* lastsp = strrchr(p, ' ');
            nbytes = atoi(lastsp ? lastsp + 1 : p);
            (void)sid;
            if (nbytes < 0 || nbytes > (int)sizeof(c->payload) - 1) { /* sovradimensionato: scarta payload */
                char tmp[4096]; int left = nbytes; while (left > 0) { int tk = left < (int)sizeof(tmp) ? left : (int)sizeof(tmp); if (na_readn(c->fd, tmp, tk) != 0) return -1; left -= tk; }
            } else {
                if (nbytes > 0 && na_readn(c->fd, c->payload, nbytes) != 0) return -1;
                c->payload[nbytes] = 0;
                strncpy(c->subject, subj, sizeof(c->subject) - 1); c->subject[sizeof(c->subject) - 1] = 0;
                c->has_msg = 1;
            }
            na_readn(c->fd, line, 2);  /* CRLF finale */
            if (c->has_msg) return 1;
            continue;
        } else if (strncmp(line, "PING", 4) == 0) {
            if (na_write(c->fd, "PONG\r\n", 6) != 0) return -1;
            continue;
        }
        /* +OK / INFO / -ERR / altro: ignora e continua */
    }
}

const char* gmbnats_last_subject(long h) { nats_cli* c = naget(h); return (c && c->has_msg) ? c->subject : ""; }
const char* gmbnats_last_payload(long h) { nats_cli* c = naget(h); return (c && c->has_msg) ? c->payload : ""; }

int gmbnats_disconnect(long h)
{
    nats_cli* c = naget(h); if (!c) return -1;
    close(c->fd);
    memset(c, 0, sizeof(*c));
    return 0;
}
