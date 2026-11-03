/* gmb_bsp_internal.h — buffer e helper condivisi fra gmb_bsp.c (serve) e
 * gmb_bsp_compile.c (traduzione .bsp → BASIC). Uso interno al runtime. */
#ifndef GMB_BSP_INTERNAL_H
#define GMB_BSP_INTERNAL_H
#include <stddef.h>

typedef struct { char* p; size_t n; size_t cap; } gmb_bsp_buf;

int  gmb_bsp_buf_grow(gmb_bsp_buf* b, size_t extra);
void gmb_bsp_buf_app(gmb_bsp_buf* b, const char* s);
void gmb_bsp_buf_napp(gmb_bsp_buf* b, const char* s, size_t k);
void gmb_bsp_buf_ch(gmb_bsp_buf* b, char ch);

#endif
