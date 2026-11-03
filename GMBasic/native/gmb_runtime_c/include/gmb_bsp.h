/* ============================================================================
 * gmb_bsp.h — transpilazione .bsp (HTML + <% %>) → BASIC per il COMPILATORE
 * completo (VM/C). Non esiste un percorso "ridotto": la pagina viene sempre
 * compilata in un chunk A1 ed eseguita dalla VM.
 * ============================================================================ */
#ifndef GMB_BSP_H
#define GMB_BSP_H

#include "gmb_expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Transpila src BSP in BASIC per il COMPILATORE completo (VM/C): emette le
 * direttive IMPORT della pagina, quindi `DASHBOARD.*`, `AI.*`, … si risolvono.
 * *out_bas è malloc (caller free). */
int gmb_bsp_compile_full(const char* src, char** out_bas);

/* Equivalente nativo di TryServeBspPage: path URL `.bsp` sotto dir, servito dal
 * motore BSP COMPLETO (chunk A1 + VM/C via libgmb_bsp). `host` (header HTTP Host
 * della richiesta, o NULL) è seminato come REQ.Headers["Host"] così le pagine
 * possono auto-localizzarsi. 1 = gestita (200/403/500, *out_html malloc),
 * 0 = non .bsp o file assente. Se la pagina esiste ma chunk/dylib mancano:
 * 500 esplicito (nessun rendering alternativo). */
int gmb_bsp_try_serve(const char* dir, const char* url_path, const char* query,
                      const char* host, char** out_html, int* out_status);
/* Solo renderer VM (senza gateway): usato dalla dylib libgmb_bsp per evitare ricorsioni. */
int gmb_bsp_try_serve_vm(const char* dir, const char* url_path, const char* query,
                        const char* host, char** out_html, int* out_status);

/* Renderer BSP COMPLETO via `libgmb_bsp` (VM/C, pagine precompilate in chunk A1):
 * 1 = gestita, 0 = dylib/sidecar assente → il chiamante usa gmb_expr/fallback. */
int gmb_bsp_gateway_serve(const char* dir, const char* url_path, const char* query,
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
