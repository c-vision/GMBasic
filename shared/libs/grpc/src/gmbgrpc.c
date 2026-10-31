/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbgrpc.c — shim FFI del pacchetto gmbsuite `grpc`: client gRPC UNARIO in C PURO
 * (socket POSIX, HTTP/2 + HPACK minimale, framing gRPC): NESSUNA libreria esterna -> self-contained.
 * Build dal sorgente (build.sh).
 *
 * Il payload del messaggio è scambiato in ESADECIMALE (i byte protobuf contengono NUL). Complementare
 * al pacchetto `protobuf` (per costruire/leggere i messaggi). Interfaccia BASIC: namespace GRPC. */
#include "gmbgrpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdint.h>

static char g_resp[65536];
static long g_status = -1;

int gmbgrpc_available(void) { return 1; }

static int gr_write(int fd, const void* p, int n) { int o = 0; while (o < n) { ssize_t w = write(fd, (const char*)p + o, (size_t)(n - o)); if (w <= 0) return -1; o += (int)w; } return 0; }
static int gr_readn(int fd, void* p, int n) { int o = 0; while (o < n) { ssize_t r = read(fd, (char*)p + o, (size_t)(n - o)); if (r <= 0) return -1; o += (int)r; } return 0; }

static int gr_connect(const char* host, int port)
{
    char ps[16]; snprintf(ps, sizeof(ps), "%d", port);
    struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) { close(fd); freeaddrinfo(res); return -1; }
    freeaddrinfo(res); return fd;
}

/* HTTP/2 frame: length(3) type(1) flags(1) stream(4) payload. */
static int h2_frame(int fd, int type, int flags, int stream, const unsigned char* payload, int len)
{
    unsigned char h[9];
    h[0] = (len >> 16) & 0xff; h[1] = (len >> 8) & 0xff; h[2] = len & 0xff;
    h[3] = (unsigned char)type; h[4] = (unsigned char)flags;
    h[5] = (stream >> 24) & 0xff; h[6] = (stream >> 16) & 0xff; h[7] = (stream >> 8) & 0xff; h[8] = stream & 0xff;
    if (gr_write(fd, h, 9) != 0) return -1;
    if (len > 0 && gr_write(fd, payload, len) != 0) return -1;
    return 0;
}

/* HPACK: literal header field without indexing, new name (semplice e valido). */
static void hpack_int(unsigned char* o, int* n, int v) { if (v < 127) { o[(*n)++] = (unsigned char)v; } else { int first = 127; o[(*n)++] = (unsigned char)first; v -= 127; while (v >= 128) { o[(*n)++] = (unsigned char)((v & 0x7f) | 0x80); v >>= 7; } o[(*n)++] = (unsigned char)v; } }
static void hpack_lit(unsigned char* o, int* n, const char* name, const char* value)
{
    o[(*n)++] = 0x00;
    int nl = (int)strlen(name); hpack_int(o, n, nl); memcpy(o + *n, name, (size_t)nl); *n += nl;
    int vl = (int)strlen(value); hpack_int(o, n, vl); memcpy(o + *n, value, (size_t)vl); *n += vl;
}

static int hexval(char c) { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; }

int gmbgrpc_call(const char* host, long port, const char* path, const char* req_hex)
{
    g_resp[0] = 0; g_status = -1;
    if (!host || !path) return -1;
    int fd = gr_connect(host, (int)(port > 0 ? port : 80));
    if (fd < 0) return -1;

    /* decodifica il payload protobuf (hex) */
    unsigned char msg[65536]; int mlen = 0;
    const char* hx = req_hex ? req_hex : "";
    int hn = (int)strlen(hx);
    for (int i = 0; i + 1 < hn; i += 2) { int a = hexval(hx[i]), b = hexval(hx[i + 1]); if (a < 0 || b < 0) { close(fd); return -1; } msg[mlen++] = (unsigned char)((a << 4) | b); }

    /* client connection preface + SETTINGS */
    static const char preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    if (gr_write(fd, preface, (int)strlen(preface)) != 0) { close(fd); return -1; }
    if (h2_frame(fd, 0x4, 0x0, 0, NULL, 0) != 0) { close(fd); return -1; }

    /* HEADERS (stream 1) */
    unsigned char hp[4096]; int hn2 = 0;
    char authority[300]; snprintf(authority, sizeof(authority), "%s:%ld", host, port > 0 ? port : 80);
    hpack_lit(hp, &hn2, ":method", "POST");
    hpack_lit(hp, &hn2, ":scheme", "http");
    hpack_lit(hp, &hn2, ":path", path);
    hpack_lit(hp, &hn2, ":authority", authority);
    hpack_lit(hp, &hn2, "content-type", "application/grpc");
    hpack_lit(hp, &hn2, "te", "trailers");
    hpack_lit(hp, &hn2, "user-agent", "gmbasic-grpc/1.0");
    if (h2_frame(fd, 0x1, 0x4 /*END_HEADERS*/, 1, hp, hn2) != 0) { close(fd); return -1; }

    /* DATA (stream 1): gRPC frame = compressed(1)=0 + length(4) + message */
    unsigned char data[65536 + 5]; int dn = 0;
    data[dn++] = 0; data[dn++] = (mlen >> 24) & 0xff; data[dn++] = (mlen >> 16) & 0xff; data[dn++] = (mlen >> 8) & 0xff; data[dn++] = mlen & 0xff;
    memcpy(data + dn, msg, (size_t)mlen); dn += mlen;
    if (h2_frame(fd, 0x0, 0x1 /*END_STREAM*/, 1, data, dn) != 0) { close(fd); return -1; }

    /* leggi le frame in risposta */
    unsigned char acc[65536 + 32]; int accn = 0; int done = 0; long grpc_status = 0; int have_status = 0;
    while (!done) {
        unsigned char fh[9];
        if (gr_readn(fd, fh, 9) != 0) break;
        int flen = ((int)fh[0] << 16) | ((int)fh[1] << 8) | fh[2];
        int ftype = fh[3], fflags = fh[4];
        int fstream = ((fh[5] & 0x7f) << 24) | (fh[6] << 16) | (fh[7] << 8) | fh[8];
        unsigned char payload[65536];
        if (flen > 0) { if (flen > (int)sizeof(payload)) { /* drena */ int left = flen; unsigned char t[4096]; while (left > 0) { int tk = left < (int)sizeof(t) ? left : (int)sizeof(t); if (gr_readn(fd, t, tk) != 0) break; left -= tk; } continue; } if (gr_readn(fd, payload, flen) != 0) break; }
        if (ftype == 0x4) { if (!(fflags & 0x1)) h2_frame(fd, 0x4, 0x1, 0, NULL, 0); }             /* SETTINGS -> ACK */
        else if (ftype == 0x6) { if (!(fflags & 0x1)) h2_frame(fd, 0x6, 0x1, 0, payload, flen); }  /* PING -> ACK */
        else if (ftype == 0x7) { break; }                                                         /* GOAWAY */
        else if (ftype == 0x0) { if (accn + flen <= (int)sizeof(acc)) { memcpy(acc + accn, payload, (size_t)flen); accn += flen; } if (fflags & 0x1) done = 1; }
        else if (ftype == 0x1) { /* HEADERS (possibili trailers con grpc-status) - decodifica grezza: cerca "grpc-status" */ 
            for (int i = 0; i + 11 < flen; i++) if (memcmp(payload + i, "grpc-status", 11) == 0) { int j = i + 11; long st = 0; while (j < flen && payload[j] >= '0' && payload[j] <= '9') { st = st * 10 + (payload[j] - '0'); j++; } grpc_status = st; have_status = 1; }
            if (fflags & 0x1) done = 1;
        }
        (void)fstream;
    }
    close(fd);

    /* estrai il messaggio gRPC (5-byte prefix + payload) dal DATA accumulato */
    if (accn >= 5) {
        int compress = acc[0];
        int plen = ((int)acc[1] << 24) | ((int)acc[2] << 16) | ((int)acc[3] << 8) | acc[4];
        (void)compress;
        if (plen >= 0 && 5 + plen <= accn) {
            char* o = g_resp; static const char* HX = "0123456789abcdef";
            for (int i = 0; i < plen && o - g_resp < (int)sizeof(g_resp) - 3; i++) { unsigned char by = acc[5 + i]; *o++ = HX[(by >> 4) & 0xf]; *o++ = HX[by & 0xf]; }
            *o = 0;
        }
    }
    g_status = have_status ? grpc_status : 0;
    return (int)g_status;
}

const char* gmbgrpc_last_response(void) { return g_resp; }
long gmbgrpc_last_status(void) { return g_status; }
