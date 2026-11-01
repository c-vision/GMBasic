/* ============================================================================
 * gmb_http2.h — server HTTP/2 (RFC 9113) in C per le gambe native (VM/C, C99Native).
 *
 * Portata attuale (milestone 1):
 *   - h2c (cleartext, prior knowledge) e ALPN "h2" su TLS;
 *   - framing completo di base: SETTINGS (+ACK), HEADERS/CONTINUATION, DATA,
 *     WINDOW_UPDATE, PING, RST_STREAM, GOAWAY, PRIORITY/ignorato;
 *   - HPACK in decodifica (tabella statica + dinamica + Huffman) e in codifica (risposta);
 *   - UNA richiesta attiva alla volta per connessione (i successivi arrivano in sequenza):
 *     RFC-permesso, e toglie di mezzo il multiplexing — quello è la milestone 3;
 *   - corpo della richiesta in memoria (limite), risposta in memoria.
 *
 * NON ancora: multiplexing vero (più stream concorrenti), server push (deprecato dai
 * browser), priorità, streaming/SSE sopra HTTP/2 (i DATA frame progressivi sono la
 * milestone 2), WebSocket-over-HTTP/2 (RFC 8441).
 * ============================================================================ */
#ifndef GMB_HTTP2_H
#define GMB_HTTP2_H

#include <stddef.h>
#include <stdint.h>

typedef struct gmb_h2_conn gmb_h2_conn_t;

/* Trasporto: `recv` blocca fino a che ci sono dati (ritorna >0 letti, 0 = chiuso). */
typedef struct {
    int (*send)(void* ctx, const void* data, size_t len);
    long (*recv)(void* ctx, void* buf, size_t len);
} gmb_h2_io_t;

/* Una richiesta completa è pronta. La risposta si invia con gmb_h2_respond(). */
typedef void (*gmb_h2_request_fn)(void* user, gmb_h2_conn_t* c, int32_t stream_id,
                                  const char* method, const char* path, const char* query,
                                  const char* const* names, const char* const* values, int n_headers,
                                  const char* body, size_t body_len);

/* Rileva il connection preface h2c: 1 = sì, 0 = no, -1 = servono altri byte. */
int gmb_h2_preface_state(const char* buf, size_t len);

/* Ciclo di servizio. `initial`/`initial_len` = byte già letti DOPO il preface (può essere 0).
 * Ritorna 0 quando la connessione è finita (il chiamante chiude il socket). */
int gmb_h2_serve(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx,
                 const unsigned char* initial, size_t initial_len,
                 gmb_h2_request_fn handler, void* user);

gmb_h2_conn_t* gmb_h2_new(void);
void gmb_h2_free(gmb_h2_conn_t* c);

/* Invia una risposta COMPLETA. `headers_text` usa la forma interna "Nome: Valore\n"
 * (la stessa dell'HTTP/1.1). Con `body_len == 0` chiude lo stream nella HEADERS. */
int gmb_h2_respond(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx, int32_t stream_id,
                   int status, const char* headers_text, const void* body, size_t body_len);

/* ── Risposta in STREAMING (SSE, pagine lunghe, progress) ─────────────────────
 * `Start` manda solo la HEADERS (lo stream resta aperto), `Write` manda DATA frame man mano
 * che il codice BASIC produce i byte, `End` chiude con un DATA vuoto + END_STREAM.
 * Vanno chiamati dal thread che sta eseguendo l'handler: mentre l'handler gira il thread di
 * connessione è fermo in attesa, quindi lettura e scrittura dei frame non si sovrappongono.
 * `Write` rispetta il flow control: se il credito è esaurito legge i WINDOW_UPDATE. */
int gmb_h2_start_stream(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx, int32_t stream_id,
                        int status, const char* headers_text);
int gmb_h2_write_data(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx, int32_t stream_id,
                      const void* data, size_t len);
int gmb_h2_end_stream(gmb_h2_conn_t* c, const gmb_h2_io_t* io, void* io_ctx, int32_t stream_id);

/* Tetto del corpo di richiesta (0 = default 1 MB). Oltre il tetto: 413 + RST_STREAM. */
void gmb_h2_set_max_body(gmb_h2_conn_t* c, int64_t bytes);

#endif /* GMB_HTTP2_H */
