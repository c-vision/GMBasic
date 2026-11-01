/* ============================================================================
 * gmb_http2.c — server HTTP/2 (RFC 9113): framing, SETTINGS, HEADERS/DATA, flow control
 * minimo, una richiesta attiva alla volta. HPACK in gmb_hpack.c.
 *
 * Scelte dichiarate (e perché):
 *  - UNA richiesta alla volta: il multiplexing cambia il modello di concorrenza (una coda di
 *    stream per connessione) ed è un blocco a sé; con un solo stream attivo il protocollo è
 *    comunque valido (RFC 9113 §5.1: gli stream sono indipendenti, non obbligatoriamente
 *    concorrenti dal lato server).
 *  - Corpo della richiesta in memoria con tetto: un corpo più grande viene rifiutato con
 *    RST_STREAM (nessun troncamento silenzioso).
 *  - Il WINDOW_UPDATE del client per i DATI che riceviamo si manda subito: senza, un corpo
 *    più grande di 64 KB si bloccherebbe (il client aspetta credito).
 *  - I frame che non riguardano lo stream corrente (PRIORITY, RST_STREAM di altri stream...)
 *    vengono processati o ignorati, mai persi in mezzo a una risposta.
 * ============================================================================ */
#include "gmb_http2.h"
#include "gmb_hpack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define H2_TYPE_DATA          0x0
#define H2_TYPE_HEADERS       0x1
#define H2_TYPE_PRIORITY      0x2
#define H2_TYPE_RST_STREAM    0x3
#define H2_TYPE_SETTINGS      0x4
#define H2_TYPE_PUSH_PROMISE  0x5
#define H2_TYPE_PING          0x6
#define H2_TYPE_GOAWAY        0x7
#define H2_TYPE_WINDOW_UPDATE 0x8
#define H2_TYPE_CONTINUATION  0x9

#define H2_FLAG_END_STREAM  0x1
#define H2_FLAG_ACK         0x1
#define H2_FLAG_END_HEADERS 0x4
#define H2_FLAG_PADDED      0x8
#define H2_FLAG_PRIORITY    0x20

#define H2_SETTINGS_HEADER_TABLE_SIZE 0x1
#define H2_SETTINGS_ENABLE_PUSH       0x2
#define H2_SETTINGS_MAX_CONCURRENT    0x3
#define H2_SETTINGS_INITIAL_WINDOW    0x4
#define H2_SETTINGS_MAX_FRAME_SIZE    0x5
#define H2_SETTINGS_MAX_HEADER_LIST   0x6

#define H2_ERR_NO_ERROR            0x0
#define H2_ERR_PROTOCOL            0x1
#define H2_ERR_INTERNAL            0x2
#define H2_ERR_FLOW_CONTROL        0x3
#define H2_ERR_FRAME_SIZE          0x6
#define H2_ERR_REFUSED_STREAM      0x7
#define H2_ERR_CANCEL              0x8
#define H2_ERR_COMPRESSION         0x9

#define H2_LOCAL_MAX_FRAME     16384
#define H2_LOCAL_WINDOW        65535
#define H2_MAX_HEADER_BLOCK    (64 * 1024)
#define H2_MAX_BODY            (1024 * 1024)

/* Aggiunta SATURANTE al credito di invio. I client mandano WINDOW_UPDATE enormi (spesso
 * 2^31-1): sommandoli in un int32 si va in overflow e il credito diventa NEGATIVO — così il
 * server si metteva ad aspettare un WINDOW_UPDATE che aveva già ricevuto (risposta troncata
 * a 65535 byte). */
static int32_t h2_win_add(int32_t cur, uint32_t inc)
{
    int64_t v = (int64_t)cur + (int64_t)inc;
    if (v > 0x7FFFFFFFLL) v = 0x7FFFFFFFLL;
    if (v < 0) v = 0x7FFFFFFFLL;
    return (int32_t)v;
}

static const char H2_PREFACE[24] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

struct gmb_h2_conn {
    gmb_hpack_ctx_t* hpack;
    int32_t  peer_max_frame;     /* MAX_FRAME_SIZE del client (default 16384) */
    int32_t  peer_initial_window;/* SETTINGS_INITIAL_WINDOW_SIZE del client: credito di OGNI
                                  * nuovo stream. Prima si ripartiva sempre da 65535 e si
                                  * ignorava il valore del client (curl ne annuncia uno grande):
                                  * la risposta si fermava a 64 KB aspettando un WINDOW_UPDATE. */
    int32_t  send_window;        /* credito per inviare sul nostro stream */
    int32_t  recv_window;        /* credito che il client ha per inviarci */
    int32_t  stream_send_window; /* finestra dello stream corrente */
    int32_t  stream_id;
    int      last_stream_id;     /* per GOAWAY */
    /* Byte già letti dal chiamante (dopo il preface) e non ancora consumati. */
    const unsigned char* pending;
    size_t pending_len;
    size_t pending_off;
};

gmb_h2_conn_t* gmb_h2_new(void)
{
    gmb_h2_conn_t* c = (gmb_h2_conn_t*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->hpack = gmb_hpack_new();
    if (!c->hpack) { free(c); return NULL; }
    c->peer_max_frame = H2_LOCAL_MAX_FRAME;
    c->peer_initial_window = H2_LOCAL_WINDOW;
    c->send_window = H2_LOCAL_WINDOW;
    c->stream_send_window = H2_LOCAL_WINDOW;
    c->recv_window = H2_LOCAL_WINDOW;
    c->stream_id = 0;
    return c;
}

void gmb_h2_free(gmb_h2_conn_t* c)
{
    if (!c) return;
    gmb_hpack_free(c->hpack);
    free(c);
}

int gmb_h2_preface_state(const char* buf, size_t len)
{
    size_t n = len < sizeof(H2_PREFACE) ? len : sizeof(H2_PREFACE);
    if (n == 0) return -1;
    if (memcmp(buf, H2_PREFACE, n) != 0) return 0;
    return len >= sizeof(H2_PREFACE) ? 1 : -1;
}

/* ── I/O ────────────────────────────────────────────────────────────────────── */
static int h2_write(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx, const void* data, size_t len)
{
    (void)c;
    if (len == 0) return 0;
    return io->send(ctx, data, len);
}

static int h2_read_exact(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx, void* buf, size_t len)
{
    size_t got = 0;
    while (got < len)
    {
        /* prima si consumano i byte che il chiamante aveva già letto (prefase + eventuale
         * primo frame arrivato insieme: capita spesso con curl) */
        if (c->pending_off < c->pending_len)
        {
            size_t avail = c->pending_len - c->pending_off;
            size_t take = avail < (len - got) ? avail : (len - got);
            memcpy((char*)buf + got, c->pending + c->pending_off, take);
            c->pending_off += take;
            got += take;
            continue;
        }
        {
            long n = io->recv(ctx, (char*)buf + got, len - got);
            if (n <= 0) return -1;
            got += (size_t)n;
        }
    }
    return 0;
}

static int h2_send_frame(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx, int type, int flags,
                         int32_t stream, const void* payload, size_t len)
{
    unsigned char head[9];
    head[0] = (unsigned char)((len >> 16) & 0xFF);
    head[1] = (unsigned char)((len >> 8) & 0xFF);
    head[2] = (unsigned char)(len & 0xFF);
    head[3] = (unsigned char)type;
    head[4] = (unsigned char)flags;
    head[5] = (unsigned char)(((uint32_t)stream >> 24) & 0x7F);
    head[6] = (unsigned char)(((uint32_t)stream >> 16) & 0xFF);
    head[7] = (unsigned char)(((uint32_t)stream >> 8) & 0xFF);
    head[8] = (unsigned char)((uint32_t)stream & 0xFF);
    if (h2_write(c, io, ctx, head, sizeof head) != 0) return -1;
    if (len && h2_write(c, io, ctx, payload, len) != 0) return -1;
    return 0;
}

static void h2_goaway(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx, uint32_t code)
{
    unsigned char p[8];
    p[0] = (unsigned char)((c->last_stream_id >> 24) & 0x7F);
    p[1] = (unsigned char)((c->last_stream_id >> 16) & 0xFF);
    p[2] = (unsigned char)((c->last_stream_id >> 8) & 0xFF);
    p[3] = (unsigned char)(c->last_stream_id & 0xFF);
    p[4] = (unsigned char)((code >> 24) & 0xFF);
    p[5] = (unsigned char)((code >> 16) & 0xFF);
    p[6] = (unsigned char)((code >> 8) & 0xFF);
    p[7] = (unsigned char)(code & 0xFF);
    h2_send_frame(c, io, ctx, H2_TYPE_GOAWAY, 0, 0, p, sizeof p);
}

static void h2_rst(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx, int32_t stream, uint32_t code)
{
    unsigned char p[4];
    p[0] = (unsigned char)((code >> 24) & 0xFF);
    p[1] = (unsigned char)((code >> 16) & 0xFF);
    p[2] = (unsigned char)((code >> 8) & 0xFF);
    p[3] = (unsigned char)(code & 0xFF);
    h2_send_frame(c, io, ctx, H2_TYPE_RST_STREAM, 0, stream, p, sizeof p);
}

static int h2_send_settings(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx)
{
    unsigned char p[18];
    size_t n = 0;
    /* MAX_CONCURRENT_STREAMS = 1: dichiara la portata reale (una richiesta alla volta). */
    p[n++] = 0; p[n++] = H2_SETTINGS_MAX_CONCURRENT;
    p[n++] = 0; p[n++] = 0; p[n++] = 0; p[n++] = 1;
    /* INITIAL_WINDOW_SIZE = 65535 (default, esplicitato) */
    p[n++] = 0; p[n++] = H2_SETTINGS_INITIAL_WINDOW;
    p[n++] = 0; p[n++] = 0; p[n++] = 0xFF; p[n++] = 0xFF;
    return h2_send_frame(c, io, ctx, H2_TYPE_SETTINGS, 0, 0, p, n);
}

/* ── Richiesta in costruzione ──────────────────────────────────────────────── */
typedef struct {
    char method[16];
    char path[2048];
    char query[2048];
    char authority[256];
    char* names[64];
    char* values[64];
    int n_headers;
    char* body;
    size_t body_len;
    size_t body_cap;
} h2_request_t;

static void h2_req_free(h2_request_t* r)
{
    int i;
    for (i = 0; i < r->n_headers; i++) { free(r->names[i]); free(r->values[i]); }
    r->n_headers = 0;
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
    r->body_cap = 0;
}

typedef struct { h2_request_t* req; int error; } h2_pair_ctx_t;

static void h2_on_pair(void* user, const char* name, size_t nl, const char* value, size_t vl)
{
    h2_pair_ctx_t* pc = (h2_pair_ctx_t*)user;
    h2_request_t* r = pc->req;

    if (nl > 0 && name[0] == ':')
    {
        if (nl == 7 && memcmp(name, ":method", 7) == 0)
        {
            size_t n = vl < sizeof(r->method) - 1 ? vl : sizeof(r->method) - 1;
            memcpy(r->method, value, n); r->method[n] = 0;
        }
        else if (nl == 5 && memcmp(name, ":path", 5) == 0)
        {
            const char* q = memchr(value, '?', vl);
            size_t plen = q ? (size_t)(q - value) : vl;
            size_t n = plen < sizeof(r->path) - 1 ? plen : sizeof(r->path) - 1;
            memcpy(r->path, value, n); r->path[n] = 0;
            if (q)
            {
                size_t qlen = vl - plen - 1;
                size_t qn = qlen < sizeof(r->query) - 1 ? qlen : sizeof(r->query) - 1;
                memcpy(r->query, q + 1, qn); r->query[qn] = 0;
            }
            else r->query[0] = 0;
        }
        else if (nl == 10 && memcmp(name, ":authority", 10) == 0)
        {
            size_t n = vl < sizeof(r->authority) - 1 ? vl : sizeof(r->authority) - 1;
            memcpy(r->authority, value, n); r->authority[n] = 0;
        }
        /* :scheme e altri pseudo-header: non servono al dispatch */
        return;
    }

    if (r->n_headers < 64)
    {
        char* n = (char*)malloc(nl + 1);
        char* v = (char*)malloc(vl + 1);
        if (n && v)
        {
            memcpy(n, name, nl); n[nl] = 0;
            memcpy(v, value, vl); v[vl] = 0;
            r->names[r->n_headers] = n;
            r->values[r->n_headers] = v;
            r->n_headers++;
        }
        else { free(n); free(v); pc->error = 1; }
    }
}

/* ── Lettura di un frame ───────────────────────────────────────────────────── */
typedef struct {
    int type;
    int flags;
    int32_t stream;
    unsigned char* payload;
    size_t len;
} h2_frame_t;

static void h2_frame_free(h2_frame_t* f)
{
    free(f->payload);
    f->payload = NULL;
    f->len = 0;
}

static int h2_read_frame(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx, h2_frame_t* f)
{
    unsigned char head[9];
    size_t len;
    memset(f, 0, sizeof(*f));
    if (h2_read_exact(c, io, ctx, head, sizeof head) != 0) return -1;
    len = ((size_t)head[0] << 16) | ((size_t)head[1] << 8) | (size_t)head[2];
    f->type = head[3];
    f->flags = head[4];
    f->stream = (int32_t)((((uint32_t)head[5] & 0x7F) << 24) | ((uint32_t)head[6] << 16)
                          | ((uint32_t)head[7] << 8) | (uint32_t)head[8]);
    f->len = len;
    if (len > (size_t)H2_LOCAL_MAX_FRAME && f->type != H2_TYPE_SETTINGS)
    {
        /* frame oltre il nostro MAX_FRAME_SIZE: errore di protocollo */
        h2_goaway(c, io, ctx, H2_ERR_FRAME_SIZE);
        return -1;
    }
    if (len)
    {
        f->payload = (unsigned char*)malloc(len);
        if (!f->payload) return -1;
        if (h2_read_exact(c, io, ctx, f->payload, len) != 0) { h2_frame_free(f); return -1; }
    }
    return 0;
}

/* Applica i SETTINGS del client. */
static void h2_apply_settings(gmb_h2_conn_t* c, const unsigned char* p, size_t len)
{
    size_t i = 0;
    while (i + 6 <= len)
    {
        unsigned short id = (unsigned short)((p[i] << 8) | p[i + 1]);
        uint32_t v = ((uint32_t)p[i + 2] << 24) | ((uint32_t)p[i + 3] << 16)
                   | ((uint32_t)p[i + 4] << 8) | (uint32_t)p[i + 5];
        switch (id)
        {
            case H2_SETTINGS_HEADER_TABLE_SIZE: gmb_hpack_set_max_size(c->hpack, v); break;
            case H2_SETTINGS_INITIAL_WINDOW:
                c->peer_initial_window = (int32_t)v;
                c->stream_send_window = (int32_t)v;
                break;
            case H2_SETTINGS_MAX_FRAME_SIZE:
                if (v >= 16384 && v <= 16777215) c->peer_max_frame = (int32_t)v;
                break;
            default: break;
        }
        i += 6;
    }
}

/* Accumula il blocco di header da HEADERS (+ CONTINUATION) e lo decodifica. */
static int h2_read_header_block(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* ctx,
                                h2_frame_t* f, h2_request_t* req, int* end_stream)
{
    unsigned char* block;
    size_t blen = 0, cap = f->len + 256;
    size_t off = 0;
    size_t pad = 0;

    if (f->flags & H2_FLAG_PADDED)
    {
        if (f->len < 1) return -1;
        pad = f->payload[0];
        off = 1;
        if (pad + off > f->len) return -1;
    }
    if (f->flags & H2_FLAG_PRIORITY) off += 5;   /* dipendenza(4) + peso(1) */
    if (off > f->len) return -1;

    block = (unsigned char*)malloc(cap);
    if (!block) return -1;
    memcpy(block, f->payload + off, f->len - off - pad);
    blen = f->len - off - pad;
    *end_stream = (f->flags & H2_FLAG_END_STREAM) ? 1 : 0;

    while (!(f->flags & H2_FLAG_END_HEADERS))
    {
        h2_frame_t cf;
        if (h2_read_frame(c, io, ctx, &cf) != 0) { free(block); return -1; }
        if (cf.type != H2_TYPE_CONTINUATION || cf.stream != f->stream)
        {
            h2_frame_free(&cf);
            free(block);
            h2_goaway(c, io, ctx, H2_ERR_PROTOCOL);
            return -1;
        }
        if (blen + cf.len > H2_MAX_HEADER_BLOCK) { h2_frame_free(&cf); free(block); return -1; }
        if (blen + cf.len > cap)
        {
            unsigned char* nb;
            cap = blen + cf.len + 256;
            nb = (unsigned char*)realloc(block, cap);
            if (!nb) { h2_frame_free(&cf); free(block); return -1; }
            block = nb;
        }
        memcpy(block + blen, cf.payload, cf.len);
        blen += cf.len;
        f->flags = cf.flags;   /* END_HEADERS può stare sull'ultimo CONTINUATION */
        h2_frame_free(&cf);
    }

    {
        h2_pair_ctx_t pc;
        pc.req = req;
        pc.error = 0;
        if (gmb_hpack_decode(c->hpack, block, blen, h2_on_pair, &pc) != 0 || pc.error)
        {
            free(block);
            return -1;
        }
    }
    free(block);
    return 0;
}

static int h2_body_append(h2_request_t* r, const unsigned char* data, size_t len)
{
    if (len == 0) return 0;
    if (r->body_len + len > H2_MAX_BODY) return -1;
    if (r->body_len + len + 1 > r->body_cap)
    {
        size_t ncap = r->body_cap ? r->body_cap : 1024;
        char* nb;
        while (ncap < r->body_len + len + 1) ncap *= 2;
        nb = (char*)realloc(r->body, ncap);
        if (!nb) return -1;
        r->body = nb;
        r->body_cap = ncap;
    }
    memcpy(r->body + r->body_len, data, len);
    r->body_len += len;
    r->body[r->body_len] = 0;
    return 0;
}

/* ── Risposta ──────────────────────────────────────────────────────────────── */
int gmb_h2_respond(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx, int32_t stream,
                   int status, const char* headers_text, const void* body, size_t body_len)
{
    unsigned char block[8192];
    int blen;
    int first = 1;

    blen = gmb_hpack_encode_response(status, headers_text, block, sizeof block);
    if (blen < 0) return -1;

    if (body_len == 0)
    {
        /* un solo frame: HEADERS con END_HEADERS|END_STREAM */
        if (h2_send_frame(c, io, io_ctx, H2_TYPE_HEADERS, H2_FLAG_END_HEADERS | H2_FLAG_END_STREAM,
                          stream, block, (size_t)blen) != 0) return -1;
        return 0;
    }

    if (h2_send_frame(c, io, io_ctx, H2_TYPE_HEADERS, H2_FLAG_END_HEADERS, stream,
                      block, (size_t)blen) != 0) return -1;

    {
        const unsigned char* p = (const unsigned char*)body;
        size_t left = body_len;
        while (left > 0)
        {
            size_t chunk = left;
            int last;
            if (chunk > (size_t)c->peer_max_frame) chunk = (size_t)c->peer_max_frame;
            if (c->send_window <= 0 || c->stream_send_window <= 0)
            {
                /* credito esaurito: si leggono frame finché arriva un WINDOW_UPDATE */
                h2_frame_t f;
                if (h2_read_frame(c, io, io_ctx, &f) != 0) return -1;
                if (f.type == H2_TYPE_WINDOW_UPDATE && f.len >= 4)
                {
                    uint32_t inc = ((uint32_t)f.payload[0] << 24) | ((uint32_t)f.payload[1] << 16)
                                 | ((uint32_t)f.payload[2] << 8) | (uint32_t)f.payload[3];
                    inc &= 0x7FFFFFFFu;
                    if (f.stream == 0) c->send_window = h2_win_add(c->send_window, inc);
                    else if (f.stream == stream) c->stream_send_window = h2_win_add(c->stream_send_window, inc);
                }
                else if (f.type == H2_TYPE_PING && !(f.flags & H2_FLAG_ACK))
                {
                    h2_send_frame(c, io, io_ctx, H2_TYPE_PING, H2_FLAG_ACK, 0, f.payload, f.len);
                }
                else if (f.type == H2_TYPE_RST_STREAM && f.stream == stream)
                {
                    h2_frame_free(&f);
                    return -1;   /* il client ha annullato la richiesta */
                }
                h2_frame_free(&f);
                continue;
            }
            if (chunk > (size_t)c->send_window) chunk = (size_t)c->send_window;
            if (chunk > (size_t)c->stream_send_window) chunk = (size_t)c->stream_send_window;
            last = (chunk == left);
            if (h2_send_frame(c, io, io_ctx, H2_TYPE_DATA, last ? H2_FLAG_END_STREAM : 0,
                              stream, p, chunk) != 0) return -1;
            if (last) c->send_window -= (int32_t)chunk, c->stream_send_window -= (int32_t)chunk;
            else { c->send_window -= (int32_t)chunk; c->stream_send_window -= (int32_t)chunk; }
            p += chunk;
            left -= chunk;
            first = 0;
        }
    }
    (void)first;
    return 0;
}

/* ── Ciclo di servizio ─────────────────────────────────────────────────────── */
int gmb_h2_serve(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx,
                 const unsigned char* initial, size_t initial_len,
                 gmb_h2_request_fn handler, void* user)
{
    c->pending = initial;
    c->pending_len = initial_len;
    c->pending_off = 0;

    if (h2_send_settings(c, io, io_ctx) != 0) return 0;

    for (;;)
    {
        h2_frame_t f;
        h2_request_t req;
        int end_stream = 0;
        int got_headers = 0;

        if (h2_read_frame(c, io, io_ctx, &f) != 0) return 0;

        switch (f.type)
        {
            case H2_TYPE_SETTINGS:
                if (!(f.flags & H2_FLAG_ACK))
                {
                    h2_apply_settings(c, f.payload, f.len);
                    h2_send_frame(c, io, io_ctx, H2_TYPE_SETTINGS, H2_FLAG_ACK, 0, NULL, 0);
                }
                h2_frame_free(&f);
                continue;

            case H2_TYPE_PING:
                if (!(f.flags & H2_FLAG_ACK))
                    h2_send_frame(c, io, io_ctx, H2_TYPE_PING, H2_FLAG_ACK, 0, f.payload, f.len);
                h2_frame_free(&f);
                continue;

            case H2_TYPE_WINDOW_UPDATE:
                if (f.len >= 4)
                {
                    uint32_t inc = ((uint32_t)f.payload[0] << 24) | ((uint32_t)f.payload[1] << 16)
                                 | ((uint32_t)f.payload[2] << 8) | (uint32_t)f.payload[3];
                    inc &= 0x7FFFFFFFu;
                    if (f.stream == 0) c->send_window = h2_win_add(c->send_window, inc);
                    else if (f.stream == c->stream_id) c->stream_send_window = h2_win_add(c->stream_send_window, inc);
                }
                h2_frame_free(&f);
                continue;

            case H2_TYPE_GOAWAY:
                h2_frame_free(&f);
                return 0;

            case H2_TYPE_PRIORITY:
                h2_frame_free(&f);
                continue;

            case H2_TYPE_RST_STREAM:
                h2_frame_free(&f);
                continue;

            case H2_TYPE_HEADERS:
                memset(&req, 0, sizeof req);
                c->stream_id = f.stream;
                c->stream_send_window = c->peer_initial_window;
                c->last_stream_id = f.stream;
                if (h2_read_header_block(c, io, io_ctx, &f, &req, &end_stream) != 0)
                {
                    h2_frame_free(&f);
                    h2_rst(c, io, io_ctx, c->stream_id, H2_ERR_COMPRESSION);
                    h2_req_free(&req);
                    continue;
                }
                h2_frame_free(&f);
                got_headers = 1;
                break;

            case H2_TYPE_DATA:
                /* DATI senza HEADERS per lo stream corrente: protocollo violato */
                h2_frame_free(&f);
                h2_goaway(c, io, io_ctx, H2_ERR_PROTOCOL);
                return 0;

            case H2_TYPE_PUSH_PROMISE:
                h2_frame_free(&f);
                h2_goaway(c, io, io_ctx, H2_ERR_PROTOCOL);
                return 0;

            default:
                h2_frame_free(&f);
                continue;
        }

        if (!got_headers) continue;

        /* corpo: DATA finché END_STREAM (o nessun corpo) */
        while (!end_stream)
        {
            h2_frame_t df;
            if (h2_read_frame(c, io, io_ctx, &df) != 0) { h2_req_free(&req); return 0; }
            if (df.type == H2_TYPE_DATA && df.stream == c->stream_id)
            {
                size_t off = 0, pad = 0;
                if (df.flags & H2_FLAG_PADDED)
                {
                    if (df.len < 1) { h2_frame_free(&df); h2_req_free(&req); return 0; }
                    pad = df.payload[0];
                    off = 1;
                    if (pad + off > df.len) { h2_frame_free(&df); h2_req_free(&req); return 0; }
                }
                if (h2_body_append(&req, df.payload + off, df.len - off - pad) != 0)
                {
                    h2_frame_free(&df);
                    h2_rst(c, io, io_ctx, c->stream_id, H2_ERR_REFUSED_STREAM);
                    h2_req_free(&req);
                    end_stream = 1;
                    break;
                }
                /* credito restituito SUBITO: senza, un corpo > 64 KB si blocca */
                if (df.len > 0)
                {
                    unsigned char wu[4];
                    uint32_t inc = (uint32_t)df.len;
                    wu[0] = (unsigned char)((inc >> 24) & 0xFF);
                    wu[1] = (unsigned char)((inc >> 16) & 0xFF);
                    wu[2] = (unsigned char)((inc >> 8) & 0xFF);
                    wu[3] = (unsigned char)(inc & 0xFF);
                    h2_send_frame(c, io, io_ctx, H2_TYPE_WINDOW_UPDATE, 0, 0, wu, 4);
                }
                end_stream = (df.flags & H2_FLAG_END_STREAM) ? 1 : 0;
                h2_frame_free(&df);
            }
            else if (df.type == H2_TYPE_WINDOW_UPDATE)
            {
                if (df.len >= 4)
                {
                    uint32_t inc = ((uint32_t)df.payload[0] << 24) | ((uint32_t)df.payload[1] << 16)
                                 | ((uint32_t)df.payload[2] << 8) | (uint32_t)df.payload[3];
                    inc &= 0x7FFFFFFFu;
                    if (df.stream == 0) c->send_window = h2_win_add(c->send_window, inc);
                    else c->stream_send_window = h2_win_add(c->stream_send_window, inc);
                }
                h2_frame_free(&df);
            }
            else if (df.type == H2_TYPE_PING)
            {
                if (!(df.flags & H2_FLAG_ACK))
                    h2_send_frame(c, io, io_ctx, H2_TYPE_PING, H2_FLAG_ACK, 0, df.payload, df.len);
                h2_frame_free(&df);
            }
            else if (df.type == H2_TYPE_RST_STREAM && df.stream == c->stream_id)
            {
                h2_frame_free(&df);
                h2_req_free(&req);
                end_stream = 1;
                goto next_request;
            }
            else
            {
                h2_frame_free(&df);
            }
        }

        if (req.method[0] == 0) strcpy(req.method, "GET");
        if (req.path[0] == 0) strcpy(req.path, "/");
        handler(user, c, c->stream_id, req.method, req.path, req.query,
                (const char* const*)req.names, (const char* const*)req.values, req.n_headers,
                req.body ? req.body : "", req.body_len);
    next_request:
        h2_req_free(&req);
    }
    return 0;
}
