/* ============================================================================
 * gmb_strbuf.h — accumulatore di stringhe growable NON esposto a BASIC.
 * Usato dal codegen C99 per i loop `s$ = s$ + expr` riconosciuti (piano
 * gmb_string_t-Ownership-Refcounting-TODO §11): per costruzione non è mai
 * una variabile BASIC assegnabile, quindi zero rischio di aliasing.
 * ============================================================================ */
#ifndef GMB_STRBUF_H
#define GMB_STRBUF_H

#include <stddef.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { char* data; size_t length; size_t capacity; } gmb_strbuf_t;

gmb_strbuf_t gmb_strbuf_new(void);
void         gmb_strbuf_append(gmb_strbuf_t* b, const char* data, size_t len);
/* Trasferisce la proprietà del buffer al gmb_string_t (nessuna copia extra)
 * e azzera b. Su buffer vuoto alloca una stringa vuota valida. */
gmb_string_t gmb_strbuf_to_string(gmb_strbuf_t* b);
void         gmb_strbuf_free(gmb_strbuf_t* b);

#ifdef __cplusplus
}
#endif

#endif /* GMB_STRBUF_H */
