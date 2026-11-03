/* ============================================================================
 * gmb_bsp_pinvoke.c — ABI P/Invoke del motore BSP in C (render con la VM/C).
 *
 * Direzione UNICA C# → C (AGENTS §Regola primaria): il runtime C non conosce né
 * referenzia il managed; questo file espone solo entry point `extern "C"` che un
 * client C# (host REPL, server BSP) può chiamare in-process. Nessuna callback
 * verso il C#.
 *
 * Cosa fa: registra il renderer VM/C (gmb_vm_bsp.c) in gmb_bsp e serve una pagina
 * `.bsp` precompilata (sidecar A1 "<pagina>.bsp.chunk", prodotto dalla
 * precompilazione managed `-bsp`) eseguendola in una VM indipendente per
 * richiesta. Il body è l'output dei PRINT (semantica ASP-classic).
 *
 * Memoria: `*out_html` è malloc → liberarla con gmb_bsp_shared_free().
 * Il chiamante NON deve trattenere il puntatore dopo la free.
 * ============================================================================ */
#include "gmb_bsp.h"
#include "gmb_vm_bsp.h"
#include "gmb_state_hook.h"   /* hook opzionale di stato (agenti/team/workflow/memoria) */
/* I loop agentici C (tool code. e tool graph.) vivono in gmb_agent_runtime: la lib BSP li
 * collega al runtime condiviso all'init, come fa gmb_vm_host (senza, AI.AGENTRUNCODE e
 * AI.AGENTRUNGRAPH risponderebbero "seam non registrato" pur essendo disponibili nel motore C). */
#include "gmb_agent_loop.h"


#include <stdlib.h>
#include <stddef.h>

/* Versione dell'ABI (cresce solo con cambi incompatibili). */
int gmb_bsp_shared_abi(void) { return 2; }

/* Registra il renderer VM. Idempotente: si può chiamare a ogni richiesta. */
void gmb_bsp_shared_init(void) { gmb_vm_bsp_register(); gmb_agent_run_code_json_set(gmb_agent_run_code);
    gmb_agent_run_graph_json_set(gmb_agent_run_graph); }

/* Serve un URL `.bsp` sotto `dir` (stessa logica di gmb_bsp_try_serve):
 *   1 = gestita (*out_html malloc, *out_status = codice HTTP)
 *   0 = non è una .bsp / file assente (il chiamante fa fallback statico/404)
 *  -1 = errore interno (out_html NULL) */
int gmb_bsp_shared_serve(const char* dir, const char* url_path, const char* query,
                         const char* host, char** out_html, int* out_status)
{
    if (out_html) *out_html = NULL;
    if (out_status) *out_status = 0;
    if (!dir || !url_path) return -1;
    gmb_vm_bsp_register();   /* sicuro anche se già registrato */
    /* SOLO VM: niente gateway (evita la ricorsione dylib→gateway→dylib) e niente
     * fallback al valutatore: se il sidecar manca, 0 → il chiamante decide. */
    return gmb_bsp_try_serve_vm(dir, url_path, query, host, out_html, out_status);
}

/* Render diretto di una pagina già nota (nessun fallback gmb_expr): utile per
 * benchmark e per chi ha già risolto il path. Stessa semantica di ritorno. */
int gmb_bsp_shared_render(const char* page_path, const char* url_path, const char* query,
                          const char* host, char** out_html, int* out_status)
{
    if (out_html) *out_html = NULL;
    if (out_status) *out_status = 0;
    if (!page_path) return -1;
    gmb_vm_bsp_register();
    return gmb_vm_bsp_render(page_path, url_path, query, host, out_html, out_status);
}

/* Libera un buffer restituito da questa ABI (malloc dello stesso runtime C). */
void gmb_bsp_shared_free(char* p) { free(p); }

/* Registra l'hook OPZIONALE di stato: quando la VM/C è ospitata da un client managed, i
 * getter di agenti/team/workflow/memoria chiedono a lui lo stato (il registro C resta il
 * fallback per gli host senza client). Puntatore OPACO: il C non conosce il managed.
 * fn = NULL rimuove l'hook (comportamento VM/C standalone). */
void gmb_bsp_shared_set_state_hook(gmb_state_query_fn fn, void* ctx)
{
    gmb_state_hook_set(fn, ctx);
}

/* True se un hook di stato è registrato (diagnostica). */
int gmb_bsp_shared_state_hook_active(void) { return gmb_state_hook_active(); }

/* Elenco (CSV) delle pagine che possono essere renderizzate in PARALLELO: il loro stato passa
 * interamente dal client managed (hook), quindi il runtime C non ha stato condiviso da
 * proteggere. Tutte le altre restano serializzate (lato scrittura del rwlock). */
void gmb_bsp_shared_set_concurrent_pages(const char* csv)
{
    gmb_bsp_set_concurrent_pages(csv);
}
