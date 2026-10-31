/* ============================================================================
 * gmb_strbuf.c — accumulatore di stringhe growable (pattern §11 C99-TODO).
 * Parte di gmb_runtime_c: libreria runtime C99 multi-file per BASIC transpilato.
 * Usato SOLO dal codegen C99 per il pattern `s$ = s$ + expr` riconosciuto in un
 * FOR: il buffer non è mai esposto come gmb_string_t durante il loop, quindi
 * nessun rischio di aliasing (vedi Todo/gmb_string_t-Ownership-Refcounting-TODO).
 * ============================================================================ */

#include "gmb_strbuf.h"
#include <stdlib.h>
#include <string.h>

gmb_strbuf_t gmb_strbuf_new(void) {
    gmb_strbuf_t b;
    b.data = NULL;
    b.length = 0;
    b.capacity = 0;
    return b;
}

/* Append con raddoppio geometrico della capacità (stesso pattern di sds /
 * gmb_str_append_inplace della VM/C). NULL/len==0 sono no-op. */
void gmb_strbuf_append(gmb_strbuf_t* b, const char* data, size_t len) {
    if (!b || !data || len == 0) return;
    size_t needed = b->length + len;
    if (needed + 1 > b->capacity) {
        size_t cap = b->capacity ? b->capacity : 16;
        while (cap < needed + 1) cap *= 2;
        char* nd = (char*)realloc(b->data, cap);
        if (!nd) return; /* OOM: perde l'append, non il programma */
        b->data = nd;
        b->capacity = cap;
    }
    memcpy(b->data + b->length, data, len);
    b->length = needed;
    b->data[b->length] = '\0';
}

/* Trasferisce la proprietà del buffer al gmb_string_t (nessuna copia extra)
 * e azzera b. Su buffer vuoto alloca una stringa vuota valida. */
gmb_string_t gmb_strbuf_to_string(gmb_strbuf_t* b) {
    gmb_string_t s;
    if (!b || !b->data) {
        s.data = (char*)calloc(1, 1);
        s.length = 0;
        s.ref_count = 1;
        if (b) { b->data = NULL; b->length = 0; b->capacity = 0; }
        return s;
    }
    s.data = b->data;
    s.length = b->length;
    s.ref_count = 1;
    b->data = NULL;
    b->length = 0;
    b->capacity = 0;
    return s;
}

void gmb_strbuf_free(gmb_strbuf_t* b) {
    if (!b) return;
    free(b->data);
    b->data = NULL;
    b->length = 0;
    b->capacity = 0;
}
