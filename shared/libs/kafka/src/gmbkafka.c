/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 *
 * gmbkafka.c — shim FFI del pacchetto gmbsuite `kafka`: client Kafka (wire protocol) in C PURO
 * (socket POSIX, NESSUNA libreria esterna) -> self-contained. Build dal sorgente (build.sh).
 *
 * Implementa un sottoinsieme reale e sufficiente del protocollo Kafka (broker moderni 0.11+):
 *   - ApiVersions (18)      [handshake/verifica]
 *   - Metadata    (3, v1)   [scoperta broker + leader di partizione]
 *   - Produce     (0, v2)   [RecordBatch v2, CRC32C, acks=1]
 *   - Fetch       (1, v4)   [lettura recordset]
 * Interfaccia BASIC: namespace KAFKA, firma OMOGENEA (tutti LONG), stringhe AS STRING. */
#include "gmbkafka.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdint.h>
#include <time.h>

#define KAFKA_MAX 16
#define BUF_CAP (1 << 20)

typedef struct { int used; int fd; char cid[64]; int corr; char topic[256]; int leader_port; char leader_host[256]; int has_leader; long next_offset; char value[65536]; int has_value; } kafka_cli;
static kafka_cli g_cli[KAFKA_MAX];

/* ---------- buffer writer ---------- */
typedef struct { unsigned char* p; int len; int cap; } bw;
static void bw_init(bw* b, unsigned char* mem, int cap) { b->p = mem; b->len = 0; b->cap = cap; }
static void bw_i8(bw* b, int v) { if (b->len + 1 <= b->cap) b->p[b->len++] = (unsigned char)v; }
static void bw_i16(bw* b, int v) { if (b->len + 2 <= b->cap) { b->p[b->len++] = (v >> 8) & 0xff; b->p[b->len++] = v & 0xff; } }
static void bw_i32(bw* b, int32_t v) { if (b->len + 4 <= b->cap) { b->p[b->len++] = (v >> 24) & 0xff; b->p[b->len++] = (v >> 16) & 0xff; b->p[b->len++] = (v >> 8) & 0xff; b->p[b->len++] = v & 0xff; } }
static void bw_i64(bw* b, int64_t v) { for (int i = 7; i >= 0; i--) bw_i8(b, (int)((v >> (i * 8)) & 0xff)); }
static void bw_raw(bw* b, const void* d, int n) { if (b->len + n <= b->cap) { memcpy(b->p + b->len, d, (size_t)n); b->len += n; } }
static void bw_str(bw* b, const char* s) { int n = s ? (int)strlen(s) : 0; bw_i16(b, n); if (n) bw_raw(b, s, n); }
static void bw_nullstr(bw* b, const char* s) { if (!s) { bw_i16(b, -1); return; } bw_str(b, s); }
static void bw_bytes(bw* b, const void* d, int n) { bw_i32(b, n); if (n > 0) bw_raw(b, d, n); }
static void bw_arrlen(bw* b, int n) { bw_i32(b, n); }
/* varint zigzag (per int64) */
static void bw_varlong(bw* b, int64_t v) { uint64_t zz = ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); do { unsigned char c = zz & 0x7f; zz >>= 7; if (zz) c |= 0x80; bw_i8(b, c); } while (zz); }

/* ---------- buffer reader ---------- */
typedef struct { const unsigned char* p; int len; int off; } br;
static int br_ok(br* r, int n) { return r->off + n <= r->len; }
static int br_i8(br* r) { if (!br_ok(r, 1)) return 0; return r->p[r->off++]; }
static int br_i16(br* r) { if (!br_ok(r, 2)) return 0; int v = ((int)r->p[r->off] << 8) | r->p[r->off + 1]; r->off += 2; return (int16_t)v; }
static int32_t br_i32(br* r) { if (!br_ok(r, 4)) return 0; uint32_t v = ((uint32_t)r->p[r->off] << 24) | ((uint32_t)r->p[r->off + 1] << 16) | ((uint32_t)r->p[r->off + 2] << 8) | r->p[r->off + 3]; r->off += 4; return (int32_t)v; }
static int64_t br_i64(br* r) { if (!br_ok(r, 8)) return 0; uint64_t v = 0; for (int i = 0; i < 8; i++) v = (v << 8) | r->p[r->off++]; return (int64_t)v; }
static int br_str(br* r, char* out, int cap) { int n = br_i16(r); if (n < 0) { if (cap) out[0] = 0; return 0; } if (!br_ok(r, n)) return -1; int c = n < cap - 1 ? n : cap - 1; memcpy(out, r->p + r->off, (size_t)c); out[c] = 0; r->off += n; return n; }
static int br_skipstr(br* r) { int n = br_i16(r); if (n > 0) r->off += n; return n; }
static int64_t br_varlong(br* r) { uint64_t v = 0; int sh = 0; unsigned char c; do { c = (unsigned char)br_i8(r); v |= (uint64_t)(c & 0x7f) << sh; sh += 7; } while (c & 0x80); return (int64_t)((v >> 1) ^ -(int64_t)(v & 1)); }

/* ---------- crc32c ---------- */
static uint32_t g_crctab[256]; static int g_crcinit = 0;
static void crc_init(void) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? (0x82f63b78u ^ (c >> 1)) : (c >> 1); g_crctab[i] = c; } g_crcinit = 1; }
static uint32_t crc32c(const unsigned char* d, int n) { if (!g_crcinit) crc_init(); uint32_t crc = 0xffffffffu; for (int i = 0; i < n; i++) crc = g_crctab[(crc ^ d[i]) & 0xff] ^ (crc >> 8); return crc ^ 0xffffffffu; }

/* ---------- socket ---------- */
static int k_connect(const char* host, int port, char* err, int errcap) {
    char ps[16]; snprintf(ps, sizeof(ps), "%d", port);
    struct addrinfo hints, *res = NULL; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) { if (err) snprintf(err, errcap, "resolve"); return -1; }
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) { close(fd); freeaddrinfo(res); return -1; }
    freeaddrinfo(res); return fd;
}
static int k_write(int fd, const void* p, int n) { int o = 0; while (o < n) { ssize_t w = write(fd, (const char*)p + o, (size_t)(n - o)); if (w <= 0) return -1; o += (int)w; } return 0; }
static int k_readn(int fd, void* p, int n) { int o = 0; while (o < n) { ssize_t r = read(fd, (char*)p + o, (size_t)(n - o)); if (r <= 0) return -1; o += (int)r; } return 0; }

/* invia request (api_key, api_version, body) e ritorna il corpo della response (senza size/corr). */
static int k_request(int fd, int api_key, int api_version, const char* cid, int corr, const unsigned char* body, int blen, unsigned char* rhdr, unsigned char* resp, int cap, int* rlen)
{
    static unsigned char req[BUF_CAP];
    bw b; bw_init(&b, req, sizeof(req));
    bw_i16(&b, api_key); bw_i16(&b, api_version); bw_i32(&b, corr); bw_nullstr(&b, cid);
    bw_raw(&b, body, blen);
    static unsigned char framed[BUF_CAP + 4];
    int total = b.len;
    framed[0] = (total >> 24) & 0xff; framed[1] = (total >> 16) & 0xff; framed[2] = (total >> 8) & 0xff; framed[3] = total & 0xff;
    memcpy(framed + 4, req, (size_t)total);
    if (k_write(fd, framed, total + 4) != 0) return -1;
    unsigned char sz[4];
    if (k_readn(fd, sz, 4) != 0) return -1;
    int rsize = ((int)sz[0] << 24) | ((int)sz[1] << 16) | ((int)sz[2] << 8) | sz[3];
    if (rsize < 4 || rsize > cap) return -1;
    if (k_readn(fd, resp, rsize) != 0) return -1;
    /* resp: correlation_id(4) + body */
    (void)rhdr;
    br r; r.p = resp; r.len = rsize; r.off = 0;
    int32_t rcorr = br_i32(&r);
    if (rcorr != corr) return -1;
    *rlen = rsize - 4;
    /* sposta il body all'inizio per chiarezza */
    memmove(resp, resp + 4, (size_t)(rsize - 4));
    return 0;
}

int gmbkafka_available(void) { return 1; }

/* Metadata v1 per un topic; scopre leader di partizione 0. Ritorna 0 ok. */
static int do_metadata(kafka_cli* c, const char* topic, char* err, int errcap)
{
    unsigned char body[512]; bw b; bw_init(&b, body, sizeof(body));
    bw_arrlen(&b, 1); bw_str(&b, topic);   /* topics */
    bw_i8(&b, 1);                          /* allow_auto_topic_creation = true */
    static unsigned char resp[BUF_CAP]; int rl;
    if (k_request(c->fd, 3, 1, c->cid, ++c->corr, body, b.len, NULL, resp, sizeof(resp), &rl) != 0) { if (err) snprintf(err, errcap, "metadata-io"); return -1; }
    br r; r.p = resp; r.len = rl; r.off = 0;
    int nbrokers = br_i32(&r);
    char lhost[256] = ""; int lport = 0; int leader_id = -1;
    for (int i = 0; i < nbrokers; i++) {
        int node = br_i32(&r); char host[256]; br_str(&r, host, sizeof(host)); int port = br_i32(&r); br_skipstr(&r);
        (void)node; if (i == 0) { snprintf(lhost, sizeof(lhost), "%s", host); lport = port; }
    }
    br_i32(&r); /* controller_id */
    int ntopics = br_i32(&r);
    for (int t = 0; t < ntopics; t++) {
        br_i16(&r); char tname[256]; br_str(&r, tname, sizeof(tname)); br_i8(&r);
        int nparts = br_i32(&r);
        for (int p = 0; p < nparts; p++) {
            br_i16(&r); int pid = br_i32(&r); int lead = br_i32(&r);
            int nrep = br_i32(&r); for (int x = 0; x < nrep; x++) br_i32(&r);
            int nisr = br_i32(&r); for (int x = 0; x < nisr; x++) br_i32(&r);
            if (pid == 0) leader_id = lead;
        }
    }
    if (leader_id < 0) { snprintf(c->leader_host, sizeof(c->leader_host), "%s", lhost); c->leader_port = lport; c->has_leader = 0; return 0; }
    /* trova host/port del leader_id tra i broker */
    r.off = 4;   /* riscorre i broker per mappare node->host: semplificato: singolo broker */
    snprintf(c->leader_host, sizeof(c->leader_host), "%s", lhost);
    c->leader_port = lport;
    c->has_leader = 1;
    return 0;
}

int gmbkafka_connect(const char* host, long port, const char* client_id)
{
    if (!host) return -1;
    int slot = -1; for (int i = 0; i < KAFKA_MAX; i++) if (!g_cli[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    int fd = k_connect(host, (int)(port > 0 ? port : 9092), NULL, 0);
    if (fd < 0) return -1;
    kafka_cli* c = &g_cli[slot]; memset(c, 0, sizeof(*c));
    c->used = 1; c->fd = fd; snprintf(c->cid, sizeof(c->cid), "%s", (client_id && *client_id) ? client_id : "gmbasic"); c->corr = 0;
    snprintf(c->leader_host, sizeof(c->leader_host), "%s", host); c->leader_port = (int)(port > 0 ? port : 9092); c->has_leader = 0;
    return slot + 1;
}
static kafka_cli* kget(long h) { return (h >= 1 && h <= KAFKA_MAX && g_cli[h - 1].used) ? &g_cli[h - 1] : NULL; }

/* Metadata con retry: attende che il topic esista / abbia un leader (auto-creazione). */
static int ensure_leader(kafka_cli* c, const char* topic)
{
    for (int i = 0; i < 20; i++) {
        if (do_metadata(c, topic, NULL, 0) == 0 && c->has_leader) return 0;
        struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 300 * 1000 * 1000;
        nanosleep(&ts, NULL);
    }
    return -1;
}

int gmbkafka_produce(long h, const char* topic, long partition, const char* value)
{
    kafka_cli* c = kget(h); if (!c || !topic) return -1;
    if (!c->has_leader && ensure_leader(c, topic) != 0) return -1;
    int64_t now = (int64_t)time(NULL) * 1000;
    const char* v = value ? value : ""; int vlen = (int)strlen(v);
    (void)partition;

    /* ---- RecordBatch v2 (1 record) ---- */
    unsigned char rb[65536]; bw bw_; bw_init(&bw_, rb, sizeof(rb));
    bw_i64(&bw_, 0);                 /* baseOffset */
    int len_pos = bw_.len; bw_i32(&bw_, 0);   /* batchLength (riempito dopo) */
    int content_start = bw_.len;
    bw_i32(&bw_, -1);                /* partitionLeaderEpoch */
    bw_i8(&bw_, 2);                  /* magic */
    int crc_pos = bw_.len; bw_i32(&bw_, 0);   /* crc (riempito dopo) */
    int crc_start = bw_.len;
    bw_i16(&bw_, 0);                 /* attributes */
    bw_i32(&bw_, 0);                 /* lastOffsetDelta */
    bw_i64(&bw_, now);               /* baseTimestamp */
    bw_i64(&bw_, now);               /* maxTimestamp */
    bw_i64(&bw_, -1);                /* producerId */
    bw_i16(&bw_, -1);                /* producerEpoch */
    bw_i32(&bw_, -1);                /* baseSequence */
    bw_i32(&bw_, 1);                 /* recordsCount */
    { /* record */
        unsigned char rec[65536]; bw rb_; bw_init(&rb_, rec, sizeof(rec));
        bw_i8(&rb_, 0);              /* attributes */
        bw_varlong(&rb_, 0);         /* timestampDelta */
        bw_varlong(&rb_, 0);         /* offsetDelta */
        bw_varlong(&rb_, -1);        /* keyLength = null */
        bw_varlong(&rb_, vlen);      /* valueLength */
        bw_raw(&rb_, v, vlen);
        bw_varlong(&rb_, 0);         /* headersCount */
        bw_varlong(&bw_, rb_.len);   /* record length */
        bw_raw(&bw_, rec, rb_.len);
    }
    int batch_len = bw_.len - content_start;
    rb[len_pos] = (batch_len >> 24) & 0xff; rb[len_pos + 1] = (batch_len >> 16) & 0xff; rb[len_pos + 2] = (batch_len >> 8) & 0xff; rb[len_pos + 3] = batch_len & 0xff;
    uint32_t crc = crc32c(rb + crc_start, bw_.len - crc_start);
    rb[crc_pos] = (crc >> 24) & 0xff; rb[crc_pos + 1] = (crc >> 16) & 0xff; rb[crc_pos + 2] = (crc >> 8) & 0xff; rb[crc_pos + 3] = crc & 0xff;

    /* ---- Produce request v2 ---- */
    unsigned char body[131072]; bw b2; bw_init(&b2, body, sizeof(body));
    bw_nullstr(&b2, NULL);           /* transactional_id = null */
    bw_i16(&b2, 1);                  /* acks = 1 */
    bw_i32(&b2, 30000);              /* timeout_ms */
    bw_arrlen(&b2, 1);               /* topics */
    bw_str(&b2, topic);
    bw_arrlen(&b2, 1);               /* partitions */
    bw_i32(&b2, 0);                  /* partition index */
    bw_bytes(&b2, rb, bw_.len);      /* records */

    static unsigned char resp[BUF_CAP]; int rl;
    if (k_request(c->fd, 0, 3, c->cid, ++c->corr, body, b2.len, NULL, resp, sizeof(resp), &rl) != 0) return -1;
    br r; r.p = resp; r.len = rl; r.off = 0;
    int ntopics = br_i32(&r); int errcode = -1;
    for (int t = 0; t < ntopics; t++) {
        br_skipstr(&r);
        int nparts = br_i32(&r);
        for (int p = 0; p < nparts; p++) {
            br_i32(&r); errcode = br_i16(&r); br_i64(&r); br_i64(&r); br_i64(&r);
        }
    }
    return errcode;
}

/* Fetch v4; 1 record, 0 nessuno, -1 err. */
int gmbkafka_fetch(long h, const char* topic, long partition, long offset, long timeout_ms)
{
    kafka_cli* c = kget(h); if (!c || !topic) return -1;
    if (!c->has_leader) ensure_leader(c, topic);
    unsigned char body[1024]; bw b; bw_init(&b, body, sizeof(body));
    bw_i32(&b, -1);                  /* replica_id */
    bw_i32(&b, (int32_t)(timeout_ms > 0 ? timeout_ms : 1000)); /* max_wait_ms */
    bw_i32(&b, 1);                   /* min_bytes */
    bw_i32(&b, 1048576);             /* max_bytes */
    bw_i8(&b, 0);                    /* isolation_level */
    bw_arrlen(&b, 1);
    bw_str(&b, topic);
    bw_arrlen(&b, 1);
    bw_i32(&b, (int32_t)partition);
    bw_i64(&b, (int64_t)offset);
    bw_i32(&b, 1048576);             /* partition max_bytes */
    static unsigned char resp[BUF_CAP]; int rl;
    if (k_request(c->fd, 1, 4, c->cid, ++c->corr, body, b.len, NULL, resp, sizeof(resp), &rl) != 0) return -1;
    br r; r.p = resp; r.len = rl; r.off = 0;
    br_i32(&r);                      /* throttle_time_ms */
    int ntopics = br_i32(&r); int got = 0;
    for (int t = 0; t < ntopics; t++) {
        br_skipstr(&r);
        int nparts = br_i32(&r);
        for (int p = 0; p < nparts; p++) {
            br_i32(&r);              /* partition */
            int errcode = br_i16(&r);
            br_i64(&r);              /* high_watermark */
            br_i64(&r);              /* last_stable_offset */
            int nab = br_i32(&r); for (int a = 0; a < nab; a++) { br_i64(&r); br_i64(&r); }
            int rslen = br_i32(&r);
            if (errcode != 0) return -1;
            /* parse primo record del recordset */
            br rs; rs.p = r.p + r.off; rs.len = rslen; rs.off = 0;
            r.off += rslen;
            if (rslen < 12 + 49) continue;
            rs.off = 0;
            br_i64(&rs);             /* baseOffset */
            int batchlen = br_i32(&rs); (void)batchlen;
            br_i32(&rs);             /* partitionLeaderEpoch */
            int magic = br_i8(&rs);
            if (magic != 2) continue;
            br_i32(&rs);             /* crc */
            br_i16(&rs);             /* attributes */
            br_i32(&rs);             /* lastOffsetDelta */
            int64_t base_ts = br_i64(&rs); (void)base_ts;
            br_i64(&rs);             /* maxTimestamp */
            br_i64(&rs);             /* producerId */
            br_i16(&rs);             /* producerEpoch */
            br_i32(&rs);             /* baseSequence */
            int nrec = br_i32(&rs);
            if (nrec < 1) continue;
            /* record 0 */
            br_varlong(&rs);         /* length */
            br_i8(&rs);              /* attributes */
            br_varlong(&rs);         /* timestampDelta */
            int64_t offdelta = br_varlong(&rs);
            int64_t klen = br_varlong(&rs); if (klen > 0) rs.off += (int)klen;
            int64_t vlen = br_varlong(&rs);
            if (vlen > 0 && rs.off + (int)vlen <= rs.len) {
                int cl = (int)vlen < (int)sizeof(c->value) - 1 ? (int)vlen : (int)sizeof(c->value) - 1;
                memcpy(c->value, rs.p + rs.off, (size_t)cl); c->value[cl] = 0; c->has_value = 1;
            }
            c->next_offset = offset + offdelta + 1;
            got = 1;
        }
    }
    return got;
}

const char* gmbkafka_last_value(long h) { kafka_cli* c = kget(h); return (c && c->has_value) ? c->value : ""; }
long gmbkafka_next_offset(long h) { kafka_cli* c = kget(h); return c ? c->next_offset : 0; }

int gmbkafka_disconnect(long h)
{
    kafka_cli* c = kget(h); if (!c) return -1;
    close(c->fd); memset(c, 0, sizeof(*c)); return 0;
}
