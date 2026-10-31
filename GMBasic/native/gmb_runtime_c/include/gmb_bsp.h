/* ============================================================================
 * gmb_bsp.h — transpilazione .bsp (HTML + <% %>) → BASIC per gmb_expr.
 * Non emette IMPORT/ON ERROR/END/handler: quelli restano nel pipeline C#.
 * ============================================================================ */
#ifndef GMB_BSP_H
#define GMB_BSP_H

#include "gmb_expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Transpila src BSP in BASIC. *out_bas è malloc (caller free). */
int gmb_bsp_compile(const char* src, char** out_bas);

/* compile + exec_capture. *out_html è malloc (caller free). */
int gmb_bsp_render(const char* src, void* env,
                   gmb_expr_get_fn get, gmb_expr_set_fn set,
                   gmb_expr_call_fn call, char** out_html);

/* Equivalente nativo di TryServeBspPage: path URL `.bsp` sotto dir, query `k=v`
 * seminata nell'env. `host` (header HTTP Host della richiesta, o NULL) è seminato
 * come REQ.Headers["Host"] così le pagine BSP possono auto-localizzare il registry
 * (es. "http://" + REQ.Headers.Get("Host")). 1 = gestita (200/403/500, *out_html
 * malloc), 0 = non .bsp o file assente (il caller può fare fallback statico/404). */
int gmb_bsp_try_serve(const char* dir, const char* url_path, const char* query,
                      const char* host, char** out_html, int* out_status);

/* Renderer VM (registrato da gmb_vm_c): esegue la pagina precompilata (sidecar
 * chunk A1 "<file>.bsp.chunk") con la VM BASIC completa. 1 = gestita
 * (*out_html malloc, *out_status), 0 = sidecar assente/non fresco → try_serve
 * prosegue con il fallback gmb_expr. */
typedef int (*gmb_bsp_vm_render_fn)(const char* bsp_path, const char* url_path,
                                    const char* query, const char* host,
                                    char** out_html, int* out_status);
void gmb_bsp_set_vm_renderer(gmb_bsp_vm_render_fn fn);

#ifdef __cplusplus
}
#endif

#endif /* GMB_BSP_H */
