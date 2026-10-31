/* gmb_doc_internal.h — helper interni condivisi gmb_doc.c / gmb_doc_pdf.c.
 * Buffer di stringa C semplice (gmb_buf) + estrazione estensione. */
#ifndef GMB_DOC_INTERNAL_H
#define GMB_DOC_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char* p; size_t len; size_t cap; } gmb_buf;

static inline void buf_init(gmb_buf* b, size_t cap) { b->cap = cap ? cap : 256; b->len = 0; b->p = (char*)malloc(b->cap); if (b->p) b->p[0] = 0; }
static inline void buf_free(gmb_buf* b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static inline void buf_appn(gmb_buf* b, const char* s, size_t n)
{
    if (!b->p || !s) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2; while (nc < b->len + n + 1) nc *= 2;
        char* np = (char*)realloc(b->p, nc); if (!np) return; b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = 0;
}
static inline void buf_app(gmb_buf* b, const char* s) { if (s) buf_appn(b, s, strlen(s)); }
static inline void buf_ch(gmb_buf* b, char c) { buf_appn(b, &c, 1); }
static inline char* buf_take(gmb_buf* b) { char* r = b->p ? b->p : strdup(""); b->p = NULL; return r; }

static inline const char* gmb_doc_ext(const char* path)
{
    const char* dot = strrchr(path, '.');
    return (dot && dot[1]) ? dot + 1 : "";
}

#endif /* GMB_DOC_INTERNAL_H */
