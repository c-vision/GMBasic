/* ============================================================================
 * gmb_vm_bsp.h — pagine BSP sulla VM/C (§BSP-VM).
 * Il server HTTP C (gmb_network.c → gmb_bsp_try_serve) chiama il renderer qui
 * registrato: esegue la pagina precompilata (sidecar "<file>.bsp.chunk",
 * formato A1 — prodotto dalla precompilazione managed) in una VM indipendente
 * per richiesta, seminando i globali REQ/QUERY/FORM/SESSION per nome e
 * catturando il PRINT come corpo HTML. Semantica ASP-classic: nessun RES.Send
 * esplicito → l'output dei PRINT È la risposta.
 * ============================================================================ */
#ifndef GMB_VM_BSP_H
#define GMB_VM_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Registra il renderer VM in gmb_bsp (gmb_runtime_c). Da chiamare all'avvio
 * dell'host (gmb_vm_host) prima di HTTP SERVER. */
void gmb_vm_bsp_register(void);

/* Elenco (CSV, es. "index.bsp,canvas.bsp") delle pagine renderizzabili in parallelo: il loro
 * stato arriva tutto dal client managed (hook), quindi la VM/C non condivide stato mutabile. */
void gmb_bsp_set_concurrent_pages(const char* csv);

/* Firma compatibile con gmb_bsp_vm_render_fn: 1 = gestita (*out_html malloc,
 * *out_status), 0 = sidecar assente/non fresco (fallback gmb_expr). `host` =
 * header HTTP Host della richiesta (o NULL), seminato come REQ.Headers["Host"]. */
int gmb_vm_bsp_render(const char* bsp_path, const char* url_path,
                      const char* query, const char* host,
                      char** out_html, int* out_status);

#ifdef __cplusplus
}
#endif

#endif /* GMB_VM_BSP_H */
