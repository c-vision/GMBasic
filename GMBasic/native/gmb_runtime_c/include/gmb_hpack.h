/* ============================================================================
 * gmb_hpack.h — HPACK (RFC 7541): decodifica dei blocchi di header di HTTP/2 e
 * codifica delle risposte del nostro server.
 *
 * Perché esiste: HTTP/2 non manda header testuali ma una codifica binaria con stato
 * (tabella statica + tabella DINAMICA condivisa lungo la connessione) e, facoltativamente,
 * Huffman. È la parte di HTTP/2 che non si può evitare: senza HPACK non si legge nemmeno
 * una richiesta. Le tabelle (257 codici Huffman, 61 voci statiche) sono GENERATE dalla RFC
 * con tools/py/gen_hpack_huffman.py — niente codici scritti a mano.
 * ============================================================================ */
#ifndef GMB_HPACK_H
#define GMB_HPACK_H

#include <stddef.h>

typedef struct gmb_hpack_ctx gmb_hpack_ctx_t;

/* Riceve una coppia header decodificata. name/value NON sono NUL-terminati. */
typedef void (*gmb_hpack_pair_fn)(void* user, const char* name, size_t name_len,
                                  const char* value, size_t value_len);

/* Nuovo contesto (tabella dinamica vuota, limite 4096 byte come da default RFC). */
gmb_hpack_ctx_t* gmb_hpack_new(void);
void gmb_hpack_free(gmb_hpack_ctx_t* h);

/* Il client può cambiare il limite della tabella dinamica via SETTINGS_HEADER_TABLE_SIZE:
 * il contesto lo rispetta (le voci in eccesso vengono sfrattate). */
void gmb_hpack_set_max_size(gmb_hpack_ctx_t* h, size_t max);

/* Decodifica un blocco di header (i byte di HEADERS o CONTIUNATION sono già accodati dal
 * chiamante). Ritorna 0 = ok, -1 = blocco malformato (il chiamante risponde con
 * COMPRESSION_ERROR). Le coppie vengono consegnate in ordine a `on_pair`. */
int gmb_hpack_decode(gmb_hpack_ctx_t* h, const unsigned char* data, size_t len,
                     gmb_hpack_pair_fn on_pair, void* user);

/* Codifica una RISPOSTA: status + header in forma testuale "Nome: Valore\n" (la stessa
 * rappresentazione interna usata dall'HTTP/1.1). Non emette mai `connection`,
 * `transfer-encoding`, `content-length` (li governa il framing) né `date` duplicato.
 * Ritorna la lunghezza scritta in `out`, oppure -1 se il buffer non basta. */
int gmb_hpack_encode_response(int status, const char* headers_text,
                              unsigned char* out, size_t cap);

#endif /* GMB_HPACK_H */
